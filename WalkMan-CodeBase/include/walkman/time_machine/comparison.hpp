#pragma once

#include "walkman/time_machine/history.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace walkman::time_machine {

class HypothesisComparator {
public:
    [[nodiscard]] static HypothesisComparison compare(
        const ReconstructionHistory& history, RegionId regionId);
    [[nodiscard]] static std::string render(
        const HypothesisComparison& comparison,
        bool includeMetrics = true,
        std::uint32_t sampleRate = 1);
    [[nodiscard]] static std::string explainMetric(
        const ReconstructionHypothesis& hypothesis,
        const ReconstructionHypothesis& winner,
        const ReconstructionHypothesis& weakest);
};

} // namespace walkman::time_machine