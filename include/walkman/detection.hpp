#pragma once

#include "walkman/types.hpp"

#include <cstddef>
#include <vector>

namespace walkman {

struct DetectionConfig {
    double zeroThreshold{0.015};
    std::size_t minZeroRun{24};
    double clippingThreshold{0.985};
    std::size_t minClippingRun{3};
    double discontinuityThreshold{0.68};
    std::size_t anomalyWindow{32};
    double anomalyRmsMultiplier{2.8};
    double anomalyZcrMultiplier{1.8};
    std::size_t mergeGap{8};
};

class DamageDetector {
public:
    explicit DamageDetector(DetectionConfig config = {});
    [[nodiscard]] std::vector<AudioRegion> detect(const AudioData& audio) const;

private:
    DetectionConfig config_;
};

} // namespace walkman