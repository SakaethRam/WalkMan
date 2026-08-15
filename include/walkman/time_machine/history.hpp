#pragma once

#include "walkman/time_machine/snapshot.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace walkman::time_machine {

class ReconstructionHistory {
public:
    ReconstructionHistory() = default;

    RegionId addRegion(const AudioRegion& region);
    void addHypothesis(RegionId regionId, const HypothesisId& id,
                       const std::string& algorithmName,
                       std::shared_ptr<const std::vector<double>> samples,
                       double confidence, double qualityScore,
                       const QualityMetrics& metrics,
                       const ReconstructionMetadata& metadata,
                       const std::string& reasoning);

    [[nodiscard]] bool hasRegion(RegionId regionId) const noexcept;
    [[nodiscard]] const RegionHypothesisSet* region(
        RegionId regionId) const noexcept;
    [[nodiscard]] RegionHypothesisSet* region(RegionId regionId) noexcept;
    [[nodiscard]] std::vector<RegionId> regionIds() const;
    [[nodiscard]] std::vector<ReconstructionHypothesis> hypothesesForRegion(
        RegionId regionId) const;
    [[nodiscard]] const ReconstructionHypothesis* hypothesis(
        RegionId regionId, const HypothesisId& hypothesisId) const noexcept;
    [[nodiscard]] const ReconstructionHypothesis* selectedHypothesis(
        RegionId regionId) const noexcept;
    [[nodiscard]] std::shared_ptr<const std::vector<double>>
    selectedSamples(RegionId regionId) const noexcept;

    [[nodiscard]] HypothesisComparison compare(RegionId regionId) const;
    [[nodiscard]] bool selectHypothesis(RegionId regionId,
                                        const HypothesisId& hypothesisId);
    [[nodiscard]] bool revertRegion(RegionId regionId) noexcept;
    [[nodiscard]] bool restoreBest(RegionId regionId);
    [[nodiscard]] bool isReverted(RegionId regionId) const noexcept;

    [[nodiscard]] SnapshotId createSnapshot(const std::string& label);
    [[nodiscard]] bool restoreSnapshot(const SnapshotId& snapshotId);
    [[nodiscard]] std::vector<ReconstructionSnapshot> snapshots() const;
    [[nodiscard]] const SnapshotStore& snapshotStore() const noexcept;

    [[nodiscard]] std::string nextHypothesisId() noexcept;
    [[nodiscard]] std::string describeSelections() const;
    [[nodiscard]] bool validate() const noexcept;

private:
    RegionHypothesisSet* findMutableRegion(RegionId regionId) noexcept;
    const RegionHypothesisSet* findRegion(RegionId regionId) const noexcept;
    static void sortHypotheses(RegionHypothesisSet& region);
    static std::string winnerReason(const RegionHypothesisSet& region);

    std::vector<RegionHypothesisSet> regions_;
    SnapshotStore snapshotStore_;
    std::size_t nextHypothesisNumber_{1};
};

} // namespace walkman::time_machine