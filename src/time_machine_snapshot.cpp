#include "walkman/time_machine/snapshot.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace walkman::time_machine {

SnapshotId SnapshotStore::create(
    const std::string& label,
    const std::vector<SnapshotSelection>& selections) {
    std::ostringstream id;
    id << "S" << nextNumber_++;
    snapshots_.push_back(
        ReconstructionSnapshot{id.str(), label, "deterministic-run", selections});
    return snapshots_.back().id;
}

bool SnapshotStore::contains(const SnapshotId& id) const noexcept {
    return find(id) != nullptr;
}

const ReconstructionSnapshot* SnapshotStore::find(
    const SnapshotId& id) const noexcept {
    const auto iterator = std::find_if(
        snapshots_.begin(), snapshots_.end(),
        [&](const ReconstructionSnapshot& snapshot) { return snapshot.id == id; });
    return iterator == snapshots_.end() ? nullptr : &*iterator;
}

std::vector<ReconstructionSnapshot> SnapshotStore::list() const {
    return snapshots_;
}

void SnapshotStore::clear() noexcept {
    snapshots_.clear();
    nextNumber_ = 1;
}

} // namespace walkman::time_machine