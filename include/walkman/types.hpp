#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace walkman {

enum class DamageType {
    ZeroDropout,
    Clipping,
    NoiseBurst,
    Discontinuity,
    SampleGap,
    Unknown,
};

std::string toString(DamageType type);

struct AudioData {
    std::uint32_t sampleRate{0};
    std::uint16_t channels{0};
    std::vector<double> samples; // Interleaved, normalized to [-1, 1].

    [[nodiscard]] std::size_t frameCount() const noexcept;
    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] double sample(std::size_t frame, std::size_t channel) const;
    [[nodiscard]] double& sample(std::size_t frame, std::size_t channel);
};

struct AudioRegion {
    std::size_t startSample{0}; // Frame index, inclusive.
    std::size_t endSample{0};   // Frame index, exclusive.
    double confidence{0.0};
    DamageType damageType{DamageType::Unknown};
    std::vector<DamageType> evidence;

    [[nodiscard]] std::size_t length() const noexcept;
};

struct QualityMetrics {
    double mse{0.0};
    double snrDb{0.0};
    double correlation{0.0};
    double rmsError{0.0};
    double spectralError{0.0};
};

struct RepairCandidate {
    std::string algorithmName;
    std::size_t startSample{0};
    std::size_t endSample{0};
    std::vector<double> repairedSamples; // Region-only, interleaved.
    double score{0.0};
    QualityMetrics metrics;
};

struct RepairReport {
    std::string inputName;
    std::size_t detectedRegions{0};
    std::size_t repairedRegions{0};
    std::size_t samplesReconstructed{0};
    std::vector<AudioRegion> regions;
    std::vector<std::string> selectedAlgorithms;
    std::vector<double> selectedScores;
    QualityMetrics beforeQuality;
    QualityMetrics afterQuality;
    double overallIntegrityScore{0.0};
};

} // namespace walkman