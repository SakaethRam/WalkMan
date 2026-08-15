#pragma once

#include "walkman/time_machine/history.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace walkman::time_machine {

class ConfidenceMap {
public:
    ConfidenceMap() = default;

    void rebuild(const AudioData& damaged,
                 const ReconstructionHistory& history);
    [[nodiscard]] const std::vector<ConfidenceBand>& bands() const noexcept;
    [[nodiscard]] double averageConfidence() const noexcept;
    [[nodiscard]] double confidenceAt(std::size_t sample) const noexcept;
    [[nodiscard]] std::string renderTable() const;
    [[nodiscard]] std::string renderAscii(std::size_t width = 72) const;
    [[nodiscard]] bool validate(std::size_t totalSamples) const noexcept;

    static ConfidenceEvidence deriveEvidence(
        const RegionHypothesisSet& region,
        const ReconstructionHypothesis& selected);
    static double calculateConfidence(const ConfidenceEvidence& evidence);

private:
    std::vector<ConfidenceBand> bands_;
};

} // namespace walkman::time_machine