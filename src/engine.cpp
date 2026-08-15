#include "walkman/engine.hpp"

#include "walkman/evaluation.hpp"
#include "walkman/repair.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace walkman {

std::size_t AudioData::frameCount() const noexcept {
    return channels == 0 ? 0 : samples.size() / channels;
}

bool AudioData::valid() const noexcept {
    return sampleRate > 0 && channels > 0 && !samples.empty() &&
           samples.size() % channels == 0;
}

double AudioData::sample(std::size_t frame, std::size_t channel) const {
    if (frame >= frameCount() || channel >= channels) {
        throw std::out_of_range("Audio frame/channel index out of bounds");
    }
    return samples[frame * channels + channel];
}

double& AudioData::sample(std::size_t frame, std::size_t channel) {
    if (frame >= frameCount() || channel >= channels) {
        throw std::out_of_range("Audio frame/channel index out of bounds");
    }
    return samples[frame * channels + channel];
}

std::size_t AudioRegion::length() const noexcept {
    return endSample >= startSample ? endSample - startSample : 0;
}

HealingEngine::HealingEngine(HealingConfig config) : config_(config) {}

RepairReport HealingEngine::heal(const AudioData& damaged,
                                 const AudioData* cleanReference,
                                 AudioData& repaired) const {
    if (!damaged.valid()) {
        throw std::invalid_argument("Healing requires valid damaged audio");
    }
    if (cleanReference != nullptr &&
        (cleanReference->sampleRate != damaged.sampleRate ||
         cleanReference->channels != damaged.channels ||
         cleanReference->samples.size() != damaged.samples.size())) {
        throw std::invalid_argument("Clean reference must match damaged audio shape");
    }

    repaired = damaged;
    DamageDetector detector(config_.detection);
    const auto regions = detector.detect(damaged);
    const auto algorithms = makeRepairAlgorithms();
    CandidateEvaluator evaluator;

    RepairReport report;
    report.detectedRegions = regions.size();
    report.regions = regions;
    report.beforeQuality = cleanReference != nullptr
                               ? evaluateQuality(*cleanReference, damaged)
                               : QualityMetrics{};
    for (const auto& region : regions) {
        std::vector<RepairCandidate> candidates;
        for (const auto& algorithm : algorithms) {
            auto samples = algorithm->repair(damaged, region);
            candidates.push_back(evaluator.evaluate(
                damaged, cleanReference, region, algorithm->name(), std::move(samples)));
        }
        if (candidates.empty()) {
            continue;
        }
        const auto best = std::max_element(
            candidates.begin(), candidates.end(),
            [](const RepairCandidate& lhs, const RepairCandidate& rhs) {
                return lhs.score < rhs.score;
            });
        for (std::size_t frame = region.startSample; frame < region.endSample; ++frame) {
            const std::size_t offset = (frame - region.startSample) * damaged.channels;
            for (std::size_t channel = 0; channel < damaged.channels; ++channel) {
                repaired.sample(frame, channel) =
                    best->repairedSamples[offset + channel];
            }
        }
        report.repairedRegions++;
        report.samplesReconstructed += region.length() * damaged.channels;
        report.selectedAlgorithms.push_back(best->algorithmName);
        report.selectedScores.push_back(best->score);
    }
    report.afterQuality = cleanReference != nullptr
                              ? evaluateQuality(*cleanReference, repaired)
                              : QualityMetrics{};
    if (!report.selectedScores.empty()) {
        report.overallIntegrityScore =
            std::accumulate(report.selectedScores.begin(), report.selectedScores.end(), 0.0) /
            static_cast<double>(report.selectedScores.size());
    } else {
        report.overallIntegrityScore = 100.0;
    }
    return report;
}

std::string renderReport(const RepairReport& report, const AudioData& audio,
                         const std::filesystem::path& inputPath,
                         const std::filesystem::path& outputPath) {
    const auto duration = static_cast<double>(audio.frameCount()) /
                          static_cast<double>(audio.sampleRate);
    const auto minutes = static_cast<int>(duration / 60.0);
    const auto seconds = static_cast<int>(duration) % 60;
    std::ostringstream text;
    text << std::fixed << std::setprecision(1);
    text << "\nWALKMAN AUDIO REPAIR REPORT\n"
         << "--------------------------------\n"
         << "Input: " << inputPath.string() << "\n"
         << "Output: " << outputPath.string() << "\n"
         << "Duration: " << std::setfill('0') << std::setw(2) << minutes << ":"
         << std::setw(2) << seconds << "\n"
         << std::setfill(' ') << "Sample Rate: " << audio.sampleRate << " Hz\n"
         << "Channels: " << audio.channels << "\n"
         << "Damaged Regions: " << report.detectedRegions << "\n"
         << "Repaired Regions: " << report.repairedRegions << "\n"
         << "Samples Reconstructed: " << report.samplesReconstructed << "\n";
    for (std::size_t i = 0; i < report.regions.size(); ++i) {
        const auto& region = report.regions[i];
        text << "\nRegion #" << (i + 1) << "\n"
             << "Damage: " << toString(region.damageType) << "\n"
             << "Algorithm: "
             << (i < report.selectedAlgorithms.size() ? report.selectedAlgorithms[i] : "NONE")
             << "\n"
             << "Detection Confidence: " << (region.confidence * 100.0) << "%\n"
             << "Repair Score: "
             << (i < report.selectedScores.size() ? report.selectedScores[i] : 0.0)
             << "/100\n";
    }
    text << "\nOverall Integrity: " << report.overallIntegrityScore << "%\n";
    if (report.beforeQuality.snrDb != 0.0 || report.afterQuality.snrDb != 0.0) {
        text << "SNR Before: " << report.beforeQuality.snrDb << " dB\n"
             << "SNR After: " << report.afterQuality.snrDb << " dB\n"
             << "SNR Improvement: "
             << (report.afterQuality.snrDb - report.beforeQuality.snrDb) << " dB\n"
             << std::setprecision(4)
             << "MSE After: " << report.afterQuality.mse << "\n"
             << "Correlation After: " << report.afterQuality.correlation << "\n"
             << "RMS Error After: " << report.afterQuality.rmsError << "\n";
    } else {
        text << "Reference: none; repair score uses local continuity metrics.\n";
    }
    return text.str();
}

} // namespace walkman