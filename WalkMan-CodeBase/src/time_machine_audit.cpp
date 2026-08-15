#include "walkman/time_machine/audit.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>

namespace walkman::time_machine {
namespace {

bool finiteAndBetween(double value, double low, double high) {
    return std::isfinite(value) && value >= low && value <= high;
}

void addWarning(AuditSummary& summary, const std::string& warning) {
    if (std::find(summary.warnings.begin(), summary.warnings.end(), warning) ==
        summary.warnings.end()) {
        summary.warnings.push_back(warning);
    }
}

void addObservation(AuditSummary& summary, const std::string& observation) {
    summary.observations.push_back(observation);
}

} // namespace

bool AuditSummary::hasWarnings() const noexcept {
    return !warnings.empty();
}

std::size_t AuditSummary::algorithmUseCount(
    const std::string& algorithm) const noexcept {
    const auto iterator = algorithmCounts.find(algorithm);
    return iterator == algorithmCounts.end() ? 0 : iterator->second;
}

AuditSummary ReconstructionAudit::run(
    const ReconstructionHistory& history,
    const AudioTimeline& timeline,
    const ConfidenceMap& confidence,
    const AudioData& audio) {
    AuditSummary summary;
    if (!audio.valid()) {
        summary.warnings.push_back("The audited audio object is invalid.");
        return summary;
    }
    summary.regionCount = history.regionIds().size();
    summary.reconstructedSampleCount = timeline.reconstructedSampleCount();
    summary.damagedSampleCount = timeline.damagedSampleCount();
    auditRegions(history, audio, summary);
    auditDerivedViews(timeline, confidence, audio, summary);

    if (!history.validate()) {
        addWarning(summary, "Reconstruction history invariants failed.");
    }
    if (!timeline.validate(audio.frameCount())) {
        addWarning(summary, "Timeline segments do not cover the audio contiguously.");
    }
    if (!confidence.validate(audio.frameCount())) {
        addWarning(summary, "Confidence bands do not cover the audio contiguously.");
    }
    if (summary.regionCount == 0) {
        addObservation(summary, "No damaged regions were detected.");
    }
    if (summary.hypothesisCount > 0) {
        addObservation(summary, "All retained candidates remain addressable by ID.");
    }
    if (summary.selectedCount == summary.regionCount &&
        summary.regionCount > 0) {
        addObservation(summary, "Every detected region has an active reconstruction.");
    }
    if (summary.revertedCount > 0) {
        addObservation(summary, "At least one region is intentionally reverted to damaged input.");
    }
    if (summary.algorithmCounts.size() >= 4U) {
        addObservation(summary, "All four built-in repair strategies are represented.");
    } else {
        addWarning(summary, "The history contains fewer than four repair strategies.");
    }
    summary.valid = summary.warnings.empty();
    return summary;
}

void ReconstructionAudit::auditRegions(
    const ReconstructionHistory& history,
    const AudioData& audio,
    AuditSummary& summary) {
    long double scoreTotal = 0.0;
    std::size_t scoredHypothesisCount = 0;
    long double confidenceTotal = 0.0;
    for (const RegionId regionId : history.regionIds()) {
        const auto* region = history.region(regionId);
        if (region == nullptr) {
            addWarning(summary, "A region ID points to missing history data.");
            continue;
        }
        if (region->region.startSample >= region->region.endSample ||
            region->region.endSample > audio.frameCount()) {
            addWarning(summary, "A region boundary is outside the audio frame range.");
        }
        if (region->hypotheses.empty()) {
            addWarning(summary, "A detected region has no retained hypotheses.");
        }
        if (region->reverted) {
            ++summary.revertedCount;
        }
        const auto* selected = history.selectedHypothesis(regionId);
        if (selected != nullptr) {
            ++summary.selectedCount;
            confidenceTotal += selected->confidence;
            if (selected->confidence < 0.55) {
                ++summary.lowConfidenceCount;
            }
        } else if (!region->reverted) {
            addWarning(summary, "A region has no selected hypothesis and is not reverted.");
        }
        for (const auto& hypothesis : region->hypotheses) {
            ++summary.hypothesisCount;
            addScoreObservation(hypothesis, summary);
            if (finiteAndBetween(hypothesis.qualityScore, 0.0, 100.0)) {
                scoreTotal += hypothesis.qualityScore;
                ++scoredHypothesisCount;
            }
            if (!hypothesis.validFor(audio)) {
                addWarning(summary, "A hypothesis contains samples with the wrong shape.");
            }
            if (!finiteAndBetween(hypothesis.qualityScore, 0.0, 100.0)) {
                addWarning(summary, "A hypothesis quality score is not finite or bounded.");
            }
            if (!finiteAndBetween(hypothesis.confidence, 0.0, 1.0)) {
                addWarning(summary, "A hypothesis confidence is not finite or bounded.");
            }
            if (hypothesis.algorithmName.empty()) {
                addWarning(summary, "A hypothesis is missing its algorithm name.");
            }
        }
    }
    if (scoredHypothesisCount > 0) {
        summary.meanHypothesisScore =
            static_cast<double>(scoreTotal /
                                static_cast<long double>(scoredHypothesisCount));
    }
    if (summary.selectedCount > 0) {
        summary.meanSelectedConfidence =
            static_cast<double>(confidenceTotal /
                                static_cast<long double>(summary.selectedCount));
    }
    if (summary.lowConfidenceCount > 0) {
        addWarning(summary, "One or more selected reconstructions are low confidence.");
    }
    if (summary.hypothesisCount > 0 &&
        summary.bestHypothesisScore - summary.lowestHypothesisScore > 50.0) {
        addObservation(summary, "Candidate quality varies substantially across regions.");
    }
}

void ReconstructionAudit::auditDerivedViews(
    const AudioTimeline& timeline,
    const ConfidenceMap& confidence,
    const AudioData& audio,
    AuditSummary& summary) {
    if (timeline.segments().empty()) {
        addWarning(summary, "The timeline has no segments.");
    }
    if (confidence.bands().empty()) {
        addWarning(summary, "The confidence map has no bands.");
    }
    if (confidence.averageConfidence() < 0.5) {
        addWarning(summary, "Weighted confidence is below 50%.");
    }
    for (const auto& segment : timeline.segments()) {
        if (segment.state == TimelineState::Reconstructed ||
            segment.state == TimelineState::LowConfidence) {
            if (segment.reconstructionId.empty()) {
                addWarning(summary, "A reconstructed timeline segment has no hypothesis ID.");
            }
        }
        if (!finiteAndBetween(segment.confidence, 0.0, 1.0)) {
            addWarning(summary, "A timeline confidence value is out of range.");
        }
    }
    for (const auto& band : confidence.bands()) {
        if (band.endSample > audio.frameCount()) {
            addWarning(summary, "A confidence band extends past the audio.");
        }
        if (band.evidence.untouched && band.confidence != 1.0) {
            addWarning(summary, "An untouched confidence band is not exactly 100%.");
        }
    }
}

void ReconstructionAudit::addScoreObservation(
    const ReconstructionHypothesis& hypothesis,
    AuditSummary& summary) {
    summary.algorithmCounts[hypothesis.algorithmName]++;
    summary.bestHypothesisScore =
        std::max(summary.bestHypothesisScore, hypothesis.qualityScore);
    summary.lowestHypothesisScore =
        std::min(summary.lowestHypothesisScore, hypothesis.qualityScore);
    if (!finiteAndBetween(hypothesis.qualityScore, 0.0, 100.0)) {
        return;
    }
}

std::string ReconstructionAudit::render(const AuditSummary& summary) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(1)
           << "RECONSTRUCTION AUDIT\n"
           << "----------------------------------------------\n"
           << "Status: " << (summary.valid ? "VALID" : "WARNINGS PRESENT") << "\n"
           << "Regions: " << summary.regionCount << "\n"
           << "Hypotheses retained: " << summary.hypothesisCount << "\n"
           << "Selected: " << summary.selectedCount << "\n"
           << "Reverted: " << summary.revertedCount << "\n"
           << "Low-confidence selections: " << summary.lowConfidenceCount << "\n"
           << "Mean candidate score: " << summary.meanHypothesisScore << "/100\n"
           << "Best candidate score: " << summary.bestHypothesisScore << "/100\n"
           << "Lowest candidate score: " << summary.lowestHypothesisScore << "/100\n"
           << "Mean selected confidence: "
           << summary.meanSelectedConfidence * 100.0 << "%\n"
           << "Damaged/reconstructed frames: " << summary.damagedSampleCount << "/"
           << summary.reconstructedSampleCount << "\n"
           << "Algorithm usage:\n";
    for (const auto& [algorithm, count] : summary.algorithmCounts) {
        output << "  - " << algorithm << ": " << count << "\n";
    }
    if (!summary.observations.empty()) {
        output << "Observations:\n";
        for (const auto& observation : summary.observations) {
            output << "  + " << observation << "\n";
        }
    }
    if (!summary.warnings.empty()) {
        output << "Warnings:\n";
        for (const auto& warning : summary.warnings) {
            output << "  ! " << warning << "\n";
        }
    }
    return output.str();
}

std::string ReconstructionAudit::toJson(const AuditSummary& summary) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(8)
           << "{\n"
           << "  \"valid\": " << (summary.valid ? "true" : "false") << ",\n"
           << "  \"regionCount\": " << summary.regionCount << ",\n"
           << "  \"hypothesisCount\": " << summary.hypothesisCount << ",\n"
           << "  \"selectedCount\": " << summary.selectedCount << ",\n"
           << "  \"revertedCount\": " << summary.revertedCount << ",\n"
           << "  \"lowConfidenceCount\": " << summary.lowConfidenceCount << ",\n"
           << "  \"reconstructedSampleCount\": "
           << summary.reconstructedSampleCount << ",\n"
           << "  \"damagedSampleCount\": " << summary.damagedSampleCount << ",\n"
           << "  \"meanHypothesisScore\": " << summary.meanHypothesisScore << ",\n"
           << "  \"bestHypothesisScore\": " << summary.bestHypothesisScore << ",\n"
           << "  \"lowestHypothesisScore\": " << summary.lowestHypothesisScore << ",\n"
           << "  \"meanSelectedConfidence\": "
           << summary.meanSelectedConfidence << ",\n"
           << "  \"algorithms\": {";
    std::size_t algorithmIndex = 0;
    for (const auto& [algorithm, count] : summary.algorithmCounts) {
        output << "\"" << escape(algorithm) << "\": " << count;
        if (++algorithmIndex < summary.algorithmCounts.size()) {
            output << ", ";
        }
    }
    output << "},\n  \"observations\": [";
    for (std::size_t index = 0; index < summary.observations.size(); ++index) {
        output << "\"" << escape(summary.observations[index]) << "\"";
        if (index + 1U < summary.observations.size()) {
            output << ", ";
        }
    }
    output << "],\n  \"warnings\": [";
    for (std::size_t index = 0; index < summary.warnings.size(); ++index) {
        output << "\"" << escape(summary.warnings[index]) << "\"";
        if (index + 1U < summary.warnings.size()) {
            output << ", ";
        }
    }
    output << "]\n}\n";
    return output.str();
}

std::string ReconstructionAudit::escape(const std::string& value) {
    std::ostringstream output;
    for (const char character : value) {
        if (character == '\\' || character == '"') {
            output << '\\';
        }
        output << character;
    }
    return output.str();
}

} // namespace walkman::time_machine