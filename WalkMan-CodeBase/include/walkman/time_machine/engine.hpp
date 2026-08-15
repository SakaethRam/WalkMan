#pragma once

#include "walkman/engine.hpp"
#include "walkman/time_machine/audit.hpp"
#include "walkman/time_machine/confidence.hpp"
#include "walkman/time_machine/json.hpp"
#include "walkman/time_machine/report.hpp"
#include "walkman/time_machine/timeline.hpp"

#include <memory>
#include <optional>
#include <string>

namespace walkman::time_machine {

struct TimeMachineConfig {
    HealingConfig healing;
    std::string engineVersion{"WalkMan Audio Time Machine 1.0"};
    std::string creationMetadata{"deterministic-run"};
};

struct TimeMachineState {
    std::shared_ptr<const AudioData> damaged;
    std::shared_ptr<const AudioData> cleanReference;
    AudioData rebuilt;
    ReconstructionHistory history;
    AudioTimeline timeline;
    ConfidenceMap confidenceMap;
    QualityMetrics currentQuality;
    std::string inputName{"damaged.wav"};

    [[nodiscard]] bool hasReference() const noexcept;
    [[nodiscard]] bool valid() const noexcept;
};

class AudioTimeMachine {
public:
    explicit AudioTimeMachine(TimeMachineConfig config = {});

    [[nodiscard]] TimeMachineState analyze(
        std::shared_ptr<const AudioData> damaged,
        std::shared_ptr<const AudioData> cleanReference = nullptr,
        std::string inputName = "damaged.wav") const;
    [[nodiscard]] TimeMachineState analyze(
        const AudioData& damaged,
        const AudioData* cleanReference = nullptr,
        std::string inputName = "damaged.wav") const;

    void rebuild(TimeMachineState& state) const;
    [[nodiscard]] bool selectHypothesis(TimeMachineState& state,
                                         RegionId regionId,
                                         const HypothesisId& hypothesisId) const;
    [[nodiscard]] bool revertRegion(TimeMachineState& state,
                                    RegionId regionId) const;
    [[nodiscard]] bool restoreBest(TimeMachineState& state,
                                   RegionId regionId) const;
    [[nodiscard]] SnapshotId createSnapshot(TimeMachineState& state,
                                             const std::string& label) const;
    [[nodiscard]] bool restoreSnapshot(TimeMachineState& state,
                                       const SnapshotId& snapshotId) const;
    [[nodiscard]] bool validateState(const TimeMachineState& state) const noexcept;

private:
    void rebuildDerivedState(TimeMachineState& state) const;
    [[nodiscard]] std::string makeReason(
        const RegionHypothesisSet& region,
        const RepairCandidate& candidate,
        const std::vector<RepairCandidate>& allCandidates) const;

    TimeMachineConfig config_;
};

} // namespace walkman::time_machine