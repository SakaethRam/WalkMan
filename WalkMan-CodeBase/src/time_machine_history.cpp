#include "walkman/time_machine/history.hpp"

#include <algorithm>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace walkman::time_machine {

RegionId ReconstructionHistory::addRegion(const AudioRegion& region) {
    const RegionId id = regions_.size() + 1U;
    regions_.push_back(RegionHypothesisSet{id, region, {}, {}, false});
    return id;
}

void ReconstructionHistory::addHypothesis(
    RegionId regionId, const HypothesisId& id,
    const std::string& algorithmName,
    std::shared_ptr<const std::vector<double>> samples,
    double confidence, double qualityScore,
    const QualityMetrics& metrics,
    const ReconstructionMetadata& metadata,
    const std::string& reasoning) {
    RegionHypothesisSet* target = findMutableRegion(regionId);
    if (target == nullptr) {
        throw std::out_of_range("Cannot add hypothesis to unknown region");
    }
    if (samples == nullptr) {
        throw std::invalid_argument("A reconstruction hypothesis needs sample data");
    }
    const bool duplicate = std::any_of(
        regions_.begin(), regions_.end(),
        [&](const RegionHypothesisSet& region) {
            return std::any_of(
                region.hypotheses.begin(), region.hypotheses.end(),
                [&](const ReconstructionHypothesis& candidate) {
                    return candidate.id == id;
                });
        });
    if (duplicate) {
        throw std::invalid_argument("Duplicate reconstruction hypothesis id: " + id);
    }
    target->hypotheses.push_back(ReconstructionHypothesis{
        id,
        regionId,
        target->region,
        algorithmName,
        std::move(samples),
        std::clamp(confidence, 0.0, 1.0),
        std::clamp(qualityScore, 0.0, 100.0),
        metrics,
        metadata,
        false,
        reasoning,
    });
    sortHypotheses(*target);
}

bool ReconstructionHistory::hasRegion(RegionId regionId) const noexcept {
    return findRegion(regionId) != nullptr;
}

const RegionHypothesisSet* ReconstructionHistory::region(
    RegionId regionId) const noexcept {
    return findRegion(regionId);
}

RegionHypothesisSet* ReconstructionHistory::region(RegionId regionId) noexcept {
    return findMutableRegion(regionId);
}

std::vector<RegionId> ReconstructionHistory::regionIds() const {
    std::vector<RegionId> ids;
    ids.reserve(regions_.size());
    for (const auto& region : regions_) {
        ids.push_back(region.regionId);
    }
    return ids;
}

std::vector<ReconstructionHypothesis>
ReconstructionHistory::hypothesesForRegion(RegionId regionId) const {
    const auto* target = findRegion(regionId);
    return target == nullptr ? std::vector<ReconstructionHypothesis>{}
                             : target->hypotheses;
}

const ReconstructionHypothesis* ReconstructionHistory::hypothesis(
    RegionId regionId, const HypothesisId& hypothesisId) const noexcept {
    const auto* target = findRegion(regionId);
    if (target == nullptr) {
        return nullptr;
    }
    const auto iterator = std::find_if(
        target->hypotheses.begin(), target->hypotheses.end(),
        [&](const ReconstructionHypothesis& candidate) {
            return candidate.id == hypothesisId;
        });
    return iterator == target->hypotheses.end() ? nullptr : &*iterator;
}

const ReconstructionHypothesis* ReconstructionHistory::selectedHypothesis(
    RegionId regionId) const noexcept {
    const auto* target = findRegion(regionId);
    if (target == nullptr || target->reverted || target->selectedId.empty()) {
        return nullptr;
    }
    return hypothesis(regionId, target->selectedId);
}

std::shared_ptr<const std::vector<double>>
ReconstructionHistory::selectedSamples(RegionId regionId) const noexcept {
    const auto* selected = selectedHypothesis(regionId);
    return selected == nullptr ? nullptr : selected->reconstructedSamples;
}

HypothesisComparison ReconstructionHistory::compare(RegionId regionId) const {
    const auto* target = findRegion(regionId);
    if (target == nullptr) {
        throw std::out_of_range("Cannot compare unknown reconstruction region");
    }
    HypothesisComparison comparison;
    comparison.regionId = target->regionId;
    comparison.region = target->region;
    comparison.selectedId = target->selectedId;
    comparison.winnerReason = winnerReason(*target);
    comparison.rows.reserve(target->hypotheses.size());
    for (std::size_t i = 0; i < target->hypotheses.size(); ++i) {
        const auto& candidate = target->hypotheses[i];
        comparison.rows.push_back(HypothesisComparisonRow{
            candidate.id,
            candidate.algorithmName,
            candidate.qualityScore,
            candidate.metrics,
            candidate.selected,
            i + 1U,
        });
    }
    return comparison;
}

bool ReconstructionHistory::selectHypothesis(
    RegionId regionId, const HypothesisId& hypothesisId) {
    auto* target = findMutableRegion(regionId);
    if (target == nullptr || hypothesis(regionId, hypothesisId) == nullptr) {
        return false;
    }
    target->reverted = false;
    target->selectedId = hypothesisId;
    for (auto& candidate : target->hypotheses) {
        candidate.selected = candidate.id == hypothesisId;
    }
    return true;
}

bool ReconstructionHistory::revertRegion(RegionId regionId) noexcept {
    auto* target = findMutableRegion(regionId);
    if (target == nullptr) {
        return false;
    }
    target->reverted = true;
    target->selectedId.clear();
    for (auto& candidate : target->hypotheses) {
        candidate.selected = false;
    }
    return true;
}

bool ReconstructionHistory::restoreBest(RegionId regionId) {
    auto* target = findMutableRegion(regionId);
    if (target == nullptr || target->hypotheses.empty()) {
        return false;
    }
    return selectHypothesis(regionId, target->hypotheses.front().id);
}

bool ReconstructionHistory::isReverted(RegionId regionId) const noexcept {
    const auto* target = findRegion(regionId);
    return target != nullptr && target->reverted;
}

SnapshotId ReconstructionHistory::createSnapshot(const std::string& label) {
    std::vector<SnapshotSelection> selections;
    selections.reserve(regions_.size());
    for (const auto& region : regions_) {
        selections.push_back(
            SnapshotSelection{region.regionId, region.selectedId, region.reverted});
    }
    return snapshotStore_.create(label, selections);
}

bool ReconstructionHistory::restoreSnapshot(const SnapshotId& snapshotId) {
    const auto* snapshot = snapshotStore_.find(snapshotId);
    if (snapshot == nullptr) {
        return false;
    }
    for (const auto& selection : snapshot->selections) {
        auto* target = findMutableRegion(selection.regionId);
        if (target == nullptr) {
            return false;
        }
        if (selection.reverted) {
            (void)revertRegion(selection.regionId);
        } else if (!selectHypothesis(selection.regionId, selection.hypothesisId)) {
            return false;
        }
    }
    return true;
}

std::vector<ReconstructionSnapshot> ReconstructionHistory::snapshots() const {
    return snapshotStore_.list();
}

const SnapshotStore& ReconstructionHistory::snapshotStore() const noexcept {
    return snapshotStore_;
}

std::string ReconstructionHistory::nextHypothesisId() noexcept {
    std::ostringstream id;
    id << "R" << nextHypothesisNumber_++;
    return id.str();
}

std::string ReconstructionHistory::describeSelections() const {
    std::ostringstream output;
    for (const auto& region : regions_) {
        output << "Region " << region.regionId << ": ";
        if (region.reverted) {
            output << "ORIGINAL_DAMAGED_AUDIO";
        } else if (region.selectedId.empty()) {
            output << "UNSELECTED";
        } else {
            const auto* selected = selectedHypothesis(region.regionId);
            output << region.selectedId << " ("
                   << (selected == nullptr ? "unknown" : selected->algorithmName)
                   << ")";
        }
        output << "\n";
    }
    return output.str();
}

bool ReconstructionHistory::validate() const noexcept {
    for (const auto& region : regions_) {
        if (region.regionId == 0 || region.region.startSample >= region.region.endSample) {
            return false;
        }
        std::size_t selectedCount = 0;
        for (const auto& candidate : region.hypotheses) {
            if (candidate.regionId != region.regionId || candidate.id.empty() ||
                candidate.reconstructedSamples == nullptr) {
                return false;
            }
            if (candidate.selected) {
                ++selectedCount;
                if (region.reverted || candidate.id != region.selectedId) {
                    return false;
                }
            }
        }
        if (region.reverted && (selectedCount != 0 || !region.selectedId.empty())) {
            return false;
        }
        if (!region.reverted &&
            (!region.selectedId.empty() ? selectedCount == 1 : selectedCount == 0)) {
            continue;
        }
        if (!region.reverted) {
            return false;
        }
    }
    return true;
}

RegionHypothesisSet* ReconstructionHistory::findMutableRegion(
    RegionId regionId) noexcept {
    const auto iterator = std::find_if(
        regions_.begin(), regions_.end(),
        [&](const RegionHypothesisSet& region) { return region.regionId == regionId; });
    return iterator == regions_.end() ? nullptr : &*iterator;
}

const RegionHypothesisSet* ReconstructionHistory::findRegion(
    RegionId regionId) const noexcept {
    const auto iterator = std::find_if(
        regions_.begin(), regions_.end(),
        [&](const RegionHypothesisSet& region) { return region.regionId == regionId; });
    return iterator == regions_.end() ? nullptr : &*iterator;
}

void ReconstructionHistory::sortHypotheses(RegionHypothesisSet& region) {
    std::sort(region.hypotheses.begin(), region.hypotheses.end(),
              [](const ReconstructionHypothesis& lhs,
                 const ReconstructionHypothesis& rhs) {
                  if (lhs.qualityScore != rhs.qualityScore) {
                      return lhs.qualityScore > rhs.qualityScore;
                  }
                  return lhs.id < rhs.id;
              });
}

std::string ReconstructionHistory::winnerReason(
    const RegionHypothesisSet& region) {
    if (region.hypotheses.empty()) {
        return "No reconstruction candidates were generated.";
    }
    const auto& winner = region.hypotheses.front();
    std::ostringstream reason;
    reason << winner.algorithmName << " ranked first with score "
           << std::fixed << std::setprecision(2) << winner.qualityScore
           << "/100";
    if (winner.metrics.correlation >= 0.85) {
        reason << ", strong local waveform correlation";
    }
    if (winner.metrics.rmsError < 0.1) {
        reason << ", low RMS error";
    }
    if (winner.metrics.spectralError < 0.1) {
        reason << ", close spectral profile";
    }
    return reason.str();
}

} // namespace walkman::time_machine