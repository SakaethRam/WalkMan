#pragma once

#include "walkman/time_machine/confidence.hpp"
#include "walkman/time_machine/comparison.hpp"
#include "walkman/time_machine/audit.hpp"
#include "walkman/time_machine/timeline.hpp"

#include <filesystem>
#include <string>

namespace walkman::time_machine {

struct ReportOptions {
    bool includeAllMetrics{true};
    bool includeTimeline{true};
    bool includeConfidenceMap{true};
    bool includeHypothesisReasons{true};
};

std::string renderTimeMachineReport(
    const ReconstructionHistory& history,
    const AudioTimeline& timeline,
    const ConfidenceMap& confidence,
    const AudioData& damaged,
    const AudioData* cleanReference,
    const std::filesystem::path& inputPath,
    const std::filesystem::path& outputPath,
    ReportOptions options = {});

std::string renderTimelineReport(const AudioTimeline& timeline);
std::string renderConfidenceReport(const ConfidenceMap& confidence);
std::string renderComparisonReport(const ReconstructionHistory& history,
                                   RegionId regionId,
                                   std::uint32_t sampleRate = 1);

} // namespace walkman::time_machine