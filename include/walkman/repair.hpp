#pragma once

#include "walkman/types.hpp"

#include <memory>
#include <string>
#include <vector>

namespace walkman {

class IRepairAlgorithm {
public:
    virtual ~IRepairAlgorithm() = default;
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual std::vector<double> repair(
        const AudioData& audio, const AudioRegion& region) const = 0;
};

class LinearRepair final : public IRepairAlgorithm {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::vector<double> repair(
        const AudioData& audio, const AudioRegion& region) const override;
};

class SplineRepair final : public IRepairAlgorithm {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::vector<double> repair(
        const AudioData& audio, const AudioRegion& region) const override;
};

class WaveformMatchRepair final : public IRepairAlgorithm {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::vector<double> repair(
        const AudioData& audio, const AudioRegion& region) const override;
};

class SpectralRepair final : public IRepairAlgorithm {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::vector<double> repair(
        const AudioData& audio, const AudioRegion& region) const override;
};

[[nodiscard]] std::vector<std::unique_ptr<IRepairAlgorithm>>
makeRepairAlgorithms();

} // namespace walkman