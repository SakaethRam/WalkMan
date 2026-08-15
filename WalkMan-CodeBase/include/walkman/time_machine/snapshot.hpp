#pragma once

#include "walkman/time_machine/types.hpp"

#include <string>
#include <vector>

namespace walkman::time_machine {

struct SnapshotSelection {
    RegionId regionId{0};
    HypothesisId hypothesisId;
    bool reverted{false};
};

struct ReconstructionSnapshot {
    SnapshotId id;
    std::string label;
    std::string createdAt{"deterministic-run"};
    std::vector<SnapshotSelection> selections;
};

class SnapshotStore {
public:
    SnapshotStore() = default;

    [[nodiscard]] SnapshotId create(
        const std::string& label,
        const std::vector<SnapshotSelection>& selections);
    [[nodiscard]] bool contains(const SnapshotId& id) const noexcept;
    [[nodiscard]] const ReconstructionSnapshot* find(
        const SnapshotId& id) const noexcept;
    [[nodiscard]] std::vector<ReconstructionSnapshot> list() const;
    void clear() noexcept;

private:
    std::vector<ReconstructionSnapshot> snapshots_;
    std::size_t nextNumber_{1};
};

} // namespace walkman::time_machine