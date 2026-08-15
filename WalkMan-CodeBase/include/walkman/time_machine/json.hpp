#pragma once

#include "walkman/time_machine/confidence.hpp"
#include "walkman/time_machine/timeline.hpp"

#include <string>

namespace walkman::time_machine {

class JsonExporter {
public:
    [[nodiscard]] static std::string historyToJson(
        const ReconstructionHistory& history,
        const AudioData& audio,
        const std::string& inputName);
    [[nodiscard]] static std::string timelineToJson(
        const AudioTimeline& timeline);
    [[nodiscard]] static std::string confidenceToJson(
        const ConfidenceMap& confidence);
    [[nodiscard]] static std::string completeToJson(
        const ReconstructionHistory& history,
        const AudioTimeline& timeline,
        const ConfidenceMap& confidence,
        const AudioData& audio,
        const std::string& inputName);
    static void writeFile(const std::string& path,
                          const std::string& content);

private:
    [[nodiscard]] static std::string escape(const std::string& value);
    [[nodiscard]] static std::string number(double value);
    [[nodiscard]] static std::string metricsToJson(
        const QualityMetrics& metrics);
    [[nodiscard]] static std::string regionToJson(
        const RegionHypothesisSet& region);
};

} // namespace walkman::time_machine