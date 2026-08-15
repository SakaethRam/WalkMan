#include "walkman/detection.hpp"

#include "walkman/analysis.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace walkman {
namespace {

struct EvidenceRegion {
    std::size_t start{0};
    std::size_t end{0};
    DamageType type{DamageType::Unknown};
    double confidence{0.0};
};

void addRuns(const AudioData& audio, const std::vector<bool>& mask,
             std::size_t minimum, DamageType type, double confidence,
             std::vector<EvidenceRegion>& output) {
    std::size_t runStart = 0;
    bool active = false;
    for (std::size_t i = 0; i <= mask.size(); ++i) {
        const bool current = i < mask.size() && mask[i];
        if (current && !active) {
            runStart = i;
            active = true;
        } else if (!current && active) {
            if (i - runStart >= minimum) {
                output.push_back({runStart, i, type, confidence});
            }
            active = false;
        }
    }
    (void)audio;
}

DamageType chooseType(const std::vector<DamageType>& evidence) {
    for (const auto type : {DamageType::ZeroDropout, DamageType::Clipping,
                            DamageType::NoiseBurst, DamageType::Discontinuity,
                            DamageType::SampleGap}) {
        if (std::find(evidence.begin(), evidence.end(), type) != evidence.end()) {
            return type;
        }
    }
    return evidence.empty() ? DamageType::Unknown : evidence.front();
}

} // namespace

std::string toString(DamageType type) {
    switch (type) {
    case DamageType::ZeroDropout:
        return "ZERO_DROPOUT";
    case DamageType::Clipping:
        return "CLIPPING";
    case DamageType::NoiseBurst:
        return "NOISE_BURST";
    case DamageType::Discontinuity:
        return "DISCONTINUITY";
    case DamageType::SampleGap:
        return "SAMPLE_GAP";
    case DamageType::Unknown:
        return "UNKNOWN";
    }
    return "UNKNOWN";
}

DamageDetector::DamageDetector(DetectionConfig config) : config_(config) {}

std::vector<AudioRegion> DamageDetector::detect(const AudioData& audio) const {
    if (!audio.valid()) {
        return {};
    }
    const std::size_t frames = audio.frameCount();
    std::vector<EvidenceRegion> evidence;
    std::vector<bool> zero(frames, false);
    std::vector<bool> clipped(frames, false);
    std::vector<bool> discontinuous(frames, false);

    for (std::size_t frame = 0; frame < frames; ++frame) {
        double framePeak = 0.0;
        for (std::size_t channel = 0; channel < audio.channels; ++channel) {
            framePeak = std::max(framePeak, std::abs(audio.sample(frame, channel)));
            if (frame > 0 &&
                std::abs(audio.sample(frame, channel) -
                         audio.sample(frame - 1, channel)) >
                    config_.discontinuityThreshold) {
                discontinuous[frame] = true;
            }
        }
        zero[frame] = framePeak < config_.zeroThreshold;
        clipped[frame] = framePeak >= config_.clippingThreshold;
    }

    addRuns(audio, zero, config_.minZeroRun, DamageType::ZeroDropout, 0.96, evidence);
    addRuns(audio, clipped, config_.minClippingRun, DamageType::Clipping, 0.90, evidence);
    addRuns(audio, discontinuous, 1, DamageType::Discontinuity, 0.72, evidence);

    const std::size_t window = std::max<std::size_t>(4, config_.anomalyWindow);
    const double globalRms = rms(audio, 0, frames);
    const double globalZcr = zeroCrossingRate(audio, 0, frames);
    std::vector<bool> anomaly(frames, false);
    for (std::size_t begin = 0; begin < frames; begin += window / 2U) {
        const std::size_t end = std::min(frames, begin + window);
        const double localRms = rms(audio, begin, end);
        const double localZcr = zeroCrossingRate(audio, begin, end);
        const bool loud = globalRms > 1e-5 && localRms > globalRms * config_.anomalyRmsMultiplier;
        const bool noisy = globalZcr > 1e-5 && localZcr > globalZcr * config_.anomalyZcrMultiplier;
        if (loud && noisy) {
            std::fill(anomaly.begin() + static_cast<std::ptrdiff_t>(begin),
                      anomaly.begin() + static_cast<std::ptrdiff_t>(end), true);
        }
    }
    addRuns(audio, anomaly, window / 2U, DamageType::NoiseBurst, 0.76, evidence);

    std::sort(evidence.begin(), evidence.end(),
              [](const EvidenceRegion& lhs, const EvidenceRegion& rhs) {
                  return lhs.start < rhs.start;
              });
    std::vector<AudioRegion> merged;
    for (const auto& item : evidence) {
        const std::size_t start = item.start > config_.mergeGap
                                      ? item.start - config_.mergeGap
                                      : 0;
        const std::size_t end = std::min(frames, item.end + config_.mergeGap);
        if (!merged.empty() && start <= merged.back().endSample) {
            auto& current = merged.back();
            current.endSample = std::max(current.endSample, end);
            current.confidence = std::max(current.confidence, item.confidence);
            if (std::find(current.evidence.begin(), current.evidence.end(), item.type) ==
                current.evidence.end()) {
                current.evidence.push_back(item.type);
            }
            current.damageType = chooseType(current.evidence);
        } else {
            merged.push_back({start, end, item.confidence, item.type, {item.type}});
        }
    }
    return merged;
}

} // namespace walkman