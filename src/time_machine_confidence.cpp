#include "walkman/time_machine/confidence.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace walkman::time_machine {
namespace {

double clampUnit(double value) {
    return std::clamp(value, 0.0, 1.0);
}

double scoreToUnit(double score) {
    return clampUnit(score / 100.0);
}

double correlationToUnit(double value) {
    return clampUnit((value + 1.0) / 2.0);
}

double metricContinuity(const ReconstructionHypothesis& candidate) {
    return clampUnit(1.0 - candidate.metrics.rmsError);
}

double spectralContinuity(const ReconstructionHypothesis& candidate) {
    return clampUnit(1.0 - candidate.metrics.spectralError);
}

double referenceEvidence(const ReconstructionHypothesis& candidate) {
    if (candidate.metadata.evaluationMode != "clean-reference") {
        return 0.5;
    }
    return clampUnit((candidate.metrics.snrDb + 5.0) / 35.0);
}

} // namespace

void ConfidenceMap::rebuild(const AudioData& damaged,
                            const ReconstructionHistory& history) {
    bands_.clear();
    if (!damaged.valid()) {
        return;
    }
    const double sampleRate = static_cast<double>(damaged.sampleRate);
    std::size_t cursor = 0;
    for (const RegionId regionId : history.regionIds()) {
        const auto* region = history.region(regionId);
        if (region == nullptr) {
            continue;
        }
        if (region->region.startSample > cursor) {
            const double start = static_cast<double>(cursor) / sampleRate;
            const double end =
                static_cast<double>(region->region.startSample) / sampleRate;
            ConfidenceEvidence evidence;
            evidence.untouched = true;
            evidence.explanation = "Untouched source audio; 100% means original, not guaranteed perfect.";
            bands_.push_back(ConfidenceBand{
                cursor,
                region->region.startSample,
                start,
                end,
                1.0,
                TimelineState::Clean,
                0,
                {},
                {},
                evidence,
            });
        }
        const auto* selected = history.selectedHypothesis(regionId);
        ConfidenceEvidence evidence;
        double confidence = 0.0;
        TimelineState state = TimelineState::Damaged;
        HypothesisId reconstructionId;
        std::string algorithm;
        if (selected != nullptr) {
            evidence = deriveEvidence(*region, *selected);
            confidence = calculateConfidence(evidence);
            reconstructionId = selected->id;
            algorithm = selected->algorithmName;
            state = confidence < 0.55 ? TimelineState::LowConfidence
                                      : TimelineState::Reconstructed;
        } else if (region->reverted) {
            evidence.explanation = "Region reverted to damaged input; no reconstruction is active.";
            state = TimelineState::Reverted;
        } else {
            evidence.explanation = "Damage was detected, but no hypothesis is selected.";
        }
        const double start =
            static_cast<double>(region->region.startSample) / sampleRate;
        const double end =
            static_cast<double>(region->region.endSample) / sampleRate;
        bands_.push_back(ConfidenceBand{
            region->region.startSample,
            region->region.endSample,
            start,
            end,
            confidence,
            state,
            regionId,
            reconstructionId,
            algorithm,
            evidence,
        });
        cursor = std::max(cursor, region->region.endSample);
    }
    if (cursor < damaged.frameCount()) {
        const double start = static_cast<double>(cursor) / sampleRate;
        const double end =
            static_cast<double>(damaged.frameCount()) / sampleRate;
        ConfidenceEvidence evidence;
        evidence.untouched = true;
        evidence.explanation = "Untouched source audio; 100% means original, not guaranteed perfect.";
        bands_.push_back(ConfidenceBand{
            cursor,
            damaged.frameCount(),
            start,
            end,
            1.0,
            TimelineState::Clean,
            0,
            {},
            {},
            evidence,
        });
    }
}

const std::vector<ConfidenceBand>& ConfidenceMap::bands() const noexcept {
    return bands_;
}

double ConfidenceMap::averageConfidence() const noexcept {
    if (bands_.empty()) {
        return 0.0;
    }
    long double weighted = 0.0;
    std::size_t totalSamples = 0;
    for (const auto& band : bands_) {
        const std::size_t length = band.endSample - band.startSample;
        weighted += static_cast<long double>(band.confidence) * length;
        totalSamples += length;
    }
    return totalSamples == 0
               ? 0.0
               : static_cast<double>(weighted / static_cast<long double>(totalSamples));
}

double ConfidenceMap::confidenceAt(std::size_t sample) const noexcept {
    const auto iterator = std::find_if(
        bands_.begin(), bands_.end(),
        [&](const ConfidenceBand& band) {
            return sample >= band.startSample && sample < band.endSample;
        });
    return iterator == bands_.end() ? 0.0 : iterator->confidence;
}

std::string ConfidenceMap::renderTable() const {
    std::ostringstream output;
    output << "CONFIDENCE MAP\n"
           << "---------------------------------------------------------------------\n"
           << "Time              State          Confidence   Evidence\n"
           << "---------------------------------------------------------------------\n";
    output << std::fixed << std::setprecision(1);
    for (const auto& band : bands_) {
        output << formatTimestamp(band.startSeconds) << " - "
               << formatTimestamp(band.endSeconds) << " "
               << std::left << std::setw(15) << toString(band.state)
               << std::right << std::setw(8) << (band.confidence * 100.0)
               << "%   " << band.evidence.explanation << "\n";
    }
    output << "Weighted confidence: " << (averageConfidence() * 100.0) << "%\n";
    return output.str();
}

std::string ConfidenceMap::renderAscii(std::size_t width) const {
    if (bands_.empty() || width == 0) {
        return "Confidence: <empty>\n";
    }
    const std::size_t total = bands_.back().endSample;
    std::string bar(width, ' ');
    for (std::size_t i = 0; i < width; ++i) {
        const std::size_t sample = (i * total) / width;
        const double value = confidenceAt(sample);
        bar[i] = value >= 0.90 ? '#' : value >= 0.70 ? '+' : value >= 0.45 ? '.' : ' ';
    }
    std::ostringstream output;
    output << "Confidence: " << bar << "\n"
           << "Legend: # >= 90%, + >= 70%, . >= 45%, space < 45%\n";
    return output.str();
}

bool ConfidenceMap::validate(std::size_t totalSamples) const noexcept {
    std::size_t cursor = 0;
    for (const auto& band : bands_) {
        if (band.startSample != cursor || band.startSample >= band.endSample ||
            band.endSample > totalSamples || band.confidence < 0.0 ||
            band.confidence > 1.0) {
            return false;
        }
        cursor = band.endSample;
    }
    return bands_.empty() ? totalSamples == 0 : cursor == totalSamples;
}

ConfidenceEvidence ConfidenceMap::deriveEvidence(
    const RegionHypothesisSet& region,
    const ReconstructionHypothesis& selected) {
    ConfidenceEvidence evidence;
    evidence.repairAlgorithmScore = scoreToUnit(selected.qualityScore);
    evidence.localWaveformContinuity =
        correlationToUnit(selected.metrics.correlation);
    evidence.spectralContinuity = spectralContinuity(selected);
    evidence.neighboringSimilarity = metricContinuity(selected);
    evidence.referenceComparison = referenceEvidence(selected);

    if (region.hypotheses.size() <= 1) {
        evidence.candidateAgreement = 0.5;
    } else {
        const double best = region.hypotheses.front().qualityScore;
        double spread = 0.0;
        for (const auto& candidate : region.hypotheses) {
            spread += std::abs(best - candidate.qualityScore);
        }
        spread /= static_cast<double>(region.hypotheses.size());
        evidence.candidateAgreement = clampUnit(1.0 - spread / 100.0);
    }
    std::ostringstream explanation;
    explanation << "Weighted evidence: algorithm "
                << std::round(evidence.repairAlgorithmScore * 100.0) << "%, "
                << "waveform " << std::round(evidence.localWaveformContinuity * 100.0)
                << "%, spectrum " << std::round(evidence.spectralContinuity * 100.0)
                << "%, neighboring signal "
                << std::round(evidence.neighboringSimilarity * 100.0)
                << "%, candidate agreement "
                << std::round(evidence.candidateAgreement * 100.0) << "%.";
    evidence.explanation = explanation.str();
    return evidence;
}

double ConfidenceMap::calculateConfidence(
    const ConfidenceEvidence& evidence) {
    if (evidence.untouched) {
        return 1.0;
    }
    // This is an estimate of plausibility. It deliberately does not call a
    // reconstructed signal "correct", even when a clean reference is present.
    return clampUnit(
        0.30 * evidence.repairAlgorithmScore +
        0.20 * evidence.localWaveformContinuity +
        0.15 * evidence.spectralContinuity +
        0.15 * evidence.neighboringSimilarity +
        0.15 * evidence.candidateAgreement +
        0.05 * evidence.referenceComparison);
}

} // namespace walkman::time_machine