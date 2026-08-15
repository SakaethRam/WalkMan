#pragma once

#include "walkman/detection.hpp"
#include "walkman/types.hpp"

#include <filesystem>

namespace walkman {

struct HealingConfig {
    DetectionConfig detection;
};

class HealingEngine {
public:
    explicit HealingEngine(HealingConfig config = {});

    [[nodiscard]] RepairReport heal(const AudioData& damaged,
                                    const AudioData* cleanReference,
                                    AudioData& repaired) const;

private:
    HealingConfig config_;
};

std::string renderReport(const RepairReport& report,
                         const AudioData& audio,
                         const std::filesystem::path& inputPath,
                         const std::filesystem::path& outputPath);

} // namespace walkman