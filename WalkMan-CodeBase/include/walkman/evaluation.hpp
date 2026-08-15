#pragma once

#include "walkman/types.hpp"

#include <vector>

namespace walkman {

QualityMetrics evaluateQuality(const AudioData& reference,
                               const AudioData& candidate);

class CandidateEvaluator {
public:
    [[nodiscard]] RepairCandidate evaluate(
        const AudioData& damaged,
        const AudioData* cleanReference,
        const AudioRegion& region,
        std::string algorithmName,
        std::vector<double> repairedSamples) const;
};

} // namespace walkman