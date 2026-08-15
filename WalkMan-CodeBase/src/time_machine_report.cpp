#include "walkman/time_machine/report.hpp"

#include "walkman/evaluation.hpp"

#include <iomanip>
#include <sstream>

namespace walkman::time_machine {

std::string renderTimeMachineReport(
    const ReconstructionHistory& history,
    const AudioTimeline& timeline,
    const ConfidenceMap& confidence,
    const AudioData& damaged,
    const AudioData* cleanReference,
    const std::filesystem::path& inputPath,
    const std::filesystem::path& outputPath,
    ReportOptions options) {
    std::ostringstream output;
    const double duration =
        static_cast<double>(damaged.frameCount()) /
        static_cast<double>(damaged.sampleRate);
    std::size_t totalHypotheses = 0;
    std::size_t selected = 0;
    for (const RegionId regionId : history.regionIds()) {
        const auto* region = history.region(regionId);
        if (region == nullptr) {
            continue;
        }
        totalHypotheses += region->hypotheses.size();
        selected += region->hasSelection() ? 1U : 0U;
    }

    std::size_t untouchedSamples = 0;
    for (const auto& segment : timeline.segments()) {
        if (segment.state == TimelineState::Clean) {
            untouchedSamples += segment.endSample - segment.startSample;
        }
    }
    const double untouchedShare =
        damaged.frameCount() == 0
            ? 0.0
            : static_cast<double>(untouchedSamples) /
                  static_cast<double>(damaged.frameCount());
    const auto audit = ReconstructionAudit::run(
        history, timeline, confidence, damaged);
    output << std::fixed << std::setprecision(1)
           << "\n==================================================\n"
           << "             WALKMAN AUDIO TIME MACHINE\n"
           << "==================================================\n\n"
           << "FILE\n"
           << "----------------------------------------------\n"
           << "Input: " << inputPath.string() << "\n"
           << "Output: " << outputPath.string() << "\n"
           << "Duration: " << formatTimestamp(duration) << "\n"
           << "Sample Rate: " << damaged.sampleRate << " Hz\n"
           << "Channels: " << damaged.channels << "\n\n"
           << "RECONSTRUCTION SUMMARY\n"
           << "----------------------------------------------\n"
           << "Damaged Regions:              " << history.regionIds().size() << "\n"
           << "Reconstructed Regions:        " << selected << "\n"
           << "Total Hypotheses:             " << totalHypotheses << "\n"
           << "Selected Hypotheses:          " << selected << "\n\n"
           << "GLOBAL CONFIDENCE\n"
           << "----------------------------------------------\n"
           << "Original / untouched audio:   " << untouchedShare * 100.0 << "%\n"
           << "Reconstructed coverage:       "
           << ((1.0 - untouchedShare) * 100.0) << "%\n"
           << "Weighted reconstruction confidence: "
           << (confidence.averageConfidence() * 100.0) << "%\n"
           << "Note: 100% means untouched/original, not guaranteed correct.\n";

    for (const RegionId regionId : history.regionIds()) {
        const auto* region = history.region(regionId);
        if (region == nullptr) {
            continue;
        }
        output << "\nREGION #" << regionId << "\n"
               << "----------------------------------------------\n"
               << "Time: "
               << formatTimestamp(static_cast<double>(region->region.startSample) /
                                  static_cast<double>(damaged.sampleRate))
               << " - "
               << formatTimestamp(static_cast<double>(region->region.endSample) /
                                  static_cast<double>(damaged.sampleRate))
               << "\n"
               << "Damage: " << walkman::toString(region->region.damageType) << "\n"
               << "Candidates retained: " << region->hypotheses.size() << "\n";
        if (options.includeAllMetrics) {
            output << HypothesisComparator::render(
                history.compare(regionId), true, damaged.sampleRate);
        }
        if (options.includeHypothesisReasons) {
            for (const auto& candidate : region->hypotheses) {
                output << "Reason " << candidate.id << ": "
                       << candidate.reasoning << "\n";
            }
        }
    }

    if (options.includeTimeline) {
        output << "\n" << renderTimelineReport(timeline);
    }
    if (options.includeConfidenceMap) {
        output << "\n" << renderConfidenceReport(confidence);
    }
    if (cleanReference != nullptr) {
        // The report uses the same evaluator as the original WalkMan engine.
        // This is objective quality, while confidence remains a plausibility estimate.
        output << "\nREFERENCE QUALITY MODE\n"
               << "----------------------------------------------\n"
               << "Damaged SNR: "
               << evaluateQuality(*cleanReference, damaged).snrDb << " dB\n";
    } else {
        output << "\nREFERENCE QUALITY MODE\n"
               << "----------------------------------------------\n"
               << "No clean reference supplied; metrics are self-consistency based.\n";
    }
    output << "\n" << ReconstructionAudit::render(audit);
    return output.str();
}

std::string renderTimelineReport(const AudioTimeline& timeline) {
    std::ostringstream output;
    output << "RECONSTRUCTION TIMELINE\n"
           << "----------------------------------------------\n"
           << timeline.renderAscii(80)
           << "Segments:\n"
           << "Start              End                State           Confidence  Algorithm\n";
    output << std::fixed << std::setprecision(1);
    for (const auto& segment : timeline.segments()) {
        output << formatTimestamp(segment.startSeconds) << "       "
               << formatTimestamp(segment.endSeconds) << "       "
               << std::left << std::setw(16) << toString(segment.state)
               << std::right << std::setw(7) << segment.confidence * 100.0
               << "%       " << segment.selectedAlgorithm << "\n";
    }
    return output.str();
}

std::string renderConfidenceReport(const ConfidenceMap& confidence) {
    std::ostringstream output;
    output << "CONFIDENCE MAP\n"
           << "----------------------------------------------\n"
           << confidence.renderAscii(80)
           << confidence.renderTable();
    return output.str();
}

std::string renderComparisonReport(const ReconstructionHistory& history,
                                   RegionId regionId,
                                   std::uint32_t sampleRate) {
    return HypothesisComparator::render(history.compare(regionId), true, sampleRate);
}

} // namespace walkman::time_machine