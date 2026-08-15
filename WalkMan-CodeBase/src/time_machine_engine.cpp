#include "walkman/time_machine/engine.hpp"

#include "walkman/evaluation.hpp"
#include "walkman/repair.hpp"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <memory>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace walkman::time_machine {
namespace {

double confidenceFromCandidate(const AudioRegion& region,
                               const RepairCandidate& candidate) {
    return std::clamp(region.confidence * (candidate.score / 100.0), 0.0, 1.0);
}

std::string evaluationMode(const AudioData* reference) {
    return reference == nullptr ? "local-continuity" : "clean-reference";
}

} // namespace

bool TimeMachineState::hasReference() const noexcept {
    return cleanReference != nullptr;
}

bool TimeMachineState::valid() const noexcept {
    return damaged != nullptr && damaged->valid() && rebuilt.valid() &&
           history.validate() &&
           timeline.validate(damaged->frameCount()) &&
           confidenceMap.validate(damaged->frameCount());
}

AudioTimeMachine::AudioTimeMachine(TimeMachineConfig config)
    : config_(std::move(config)) {}

TimeMachineState AudioTimeMachine::analyze(
    std::shared_ptr<const AudioData> damaged,
    std::shared_ptr<const AudioData> cleanReference,
    std::string inputName) const {
    if (damaged == nullptr || !damaged->valid()) {
        throw std::invalid_argument("Audio Time Machine requires valid damaged audio");
    }
    if (cleanReference != nullptr &&
        (cleanReference->sampleRate != damaged->sampleRate ||
         cleanReference->channels != damaged->channels ||
         cleanReference->samples.size() != damaged->samples.size())) {
        throw std::invalid_argument(
            "Audio Time Machine reference must match damaged audio shape");
    }

    TimeMachineState state;
    state.damaged = std::move(damaged);
    state.cleanReference = std::move(cleanReference);
    state.rebuilt = *state.damaged;
    state.inputName = std::move(inputName);

    DamageDetector detector(config_.healing.detection);
    const auto regions = detector.detect(*state.damaged);
    const auto algorithms = makeRepairAlgorithms();
    CandidateEvaluator evaluator;

    for (const auto& detectedRegion : regions) {
        const RegionId regionId = state.history.addRegion(detectedRegion);
        std::vector<RepairCandidate> candidates;
        candidates.reserve(algorithms.size());
        for (const auto& algorithm : algorithms) {
            auto samples = algorithm->repair(*state.damaged, detectedRegion);
            candidates.push_back(evaluator.evaluate(
                *state.damaged,
                state.cleanReference.get(),
                detectedRegion,
                algorithm->name(),
                std::move(samples)));
        }
        std::sort(candidates.begin(), candidates.end(),
                  [](const RepairCandidate& lhs, const RepairCandidate& rhs) {
                      return lhs.score > rhs.score;
                  });
        const std::string mode = evaluationMode(state.cleanReference.get());
        for (std::size_t rank = 0; rank < candidates.size(); ++rank) {
            const auto& candidate = candidates[rank];
            auto samples = std::make_shared<const std::vector<double>>(
                candidate.repairedSamples);
            ReconstructionMetadata metadata;
            metadata.createdAt = config_.creationMetadata;
            metadata.engineVersion = config_.engineVersion;
            metadata.evaluationMode = mode;
            metadata.candidateRank = rank + 1U;
            metadata.sourceRegionLabel = toString(detectedRegion.damageType);
            state.history.addHypothesis(
                regionId,
                state.history.nextHypothesisId(),
                candidate.algorithmName,
                std::move(samples),
                confidenceFromCandidate(detectedRegion, candidate),
                candidate.score,
                candidate.metrics,
                metadata,
                makeReason(*state.history.region(regionId), candidate, candidates));
        }
        if (!state.history.restoreBest(regionId)) {
            throw std::runtime_error(
                "Audio Time Machine could not select the best hypothesis");
        }
    }
    rebuildDerivedState(state);
    if (!state.valid()) {
        throw std::runtime_error("Audio Time Machine produced invalid analysis state");
    }
    return state;
}

TimeMachineState AudioTimeMachine::analyze(
    const AudioData& damaged, const AudioData* cleanReference,
    std::string inputName) const {
    auto damagedShared = std::make_shared<const AudioData>(damaged);
    std::shared_ptr<const AudioData> referenceShared;
    if (cleanReference != nullptr) {
        referenceShared = std::make_shared<const AudioData>(*cleanReference);
    }
    return analyze(std::move(damagedShared), std::move(referenceShared),
                   std::move(inputName));
}

void AudioTimeMachine::rebuild(TimeMachineState& state) const {
    if (state.damaged == nullptr || !state.damaged->valid()) {
        throw std::invalid_argument("Cannot rebuild an invalid time-machine state");
    }
    state.rebuilt = *state.damaged;
    for (const RegionId regionId : state.history.regionIds()) {
        const auto* region = state.history.region(regionId);
        const auto selected = state.history.selectedHypothesis(regionId);
        if (region == nullptr || selected == nullptr ||
            !selected->validFor(*state.damaged)) {
            continue;
        }
        for (std::size_t frame = region->region.startSample;
             frame < region->region.endSample; ++frame) {
            const std::size_t offset =
                (frame - region->region.startSample) * state.rebuilt.channels;
            for (std::size_t channel = 0; channel < state.rebuilt.channels; ++channel) {
                state.rebuilt.sample(frame, channel) =
                    (*selected->reconstructedSamples)[offset + channel];
            }
        }
    }
    rebuildDerivedState(state);
}

bool AudioTimeMachine::selectHypothesis(
    TimeMachineState& state, RegionId regionId,
    const HypothesisId& hypothesisId) const {
    if (!state.history.selectHypothesis(regionId, hypothesisId)) {
        return false;
    }
    rebuild(state);
    return true;
}

bool AudioTimeMachine::revertRegion(TimeMachineState& state,
                                    RegionId regionId) const {
    if (!state.history.revertRegion(regionId)) {
        return false;
    }
    rebuild(state);
    return true;
}

bool AudioTimeMachine::restoreBest(TimeMachineState& state,
                                   RegionId regionId) const {
    if (!state.history.restoreBest(regionId)) {
        return false;
    }
    rebuild(state);
    return true;
}

SnapshotId AudioTimeMachine::createSnapshot(
    TimeMachineState& state, const std::string& label) const {
    return state.history.createSnapshot(label);
}

bool AudioTimeMachine::restoreSnapshot(
    TimeMachineState& state, const SnapshotId& snapshotId) const {
    if (!state.history.restoreSnapshot(snapshotId)) {
        return false;
    }
    rebuild(state);
    return true;
}

bool AudioTimeMachine::validateState(
    const TimeMachineState& state) const noexcept {
    return state.valid();
}

void AudioTimeMachine::rebuildDerivedState(TimeMachineState& state) const {
    state.timeline.rebuild(*state.damaged, state.history);
    state.confidenceMap.rebuild(*state.damaged, state.history);
    if (state.cleanReference != nullptr) {
        state.currentQuality =
            evaluateQuality(*state.cleanReference, state.rebuilt);
    } else {
        state.currentQuality = QualityMetrics{};
    }
}

std::string AudioTimeMachine::makeReason(
    const RegionHypothesisSet& region,
    const RepairCandidate& candidate,
    const std::vector<RepairCandidate>& allCandidates) const {
    double strongestCorrelation = -2.0;
    double lowestRmsError = std::numeric_limits<double>::infinity();
    double lowestSpectralError = std::numeric_limits<double>::infinity();
    double strongestSnr = -std::numeric_limits<double>::infinity();
    for (const auto& item : allCandidates) {
        strongestCorrelation =
            std::max(strongestCorrelation, item.metrics.correlation);
        lowestRmsError = std::min(lowestRmsError, item.metrics.rmsError);
        lowestSpectralError =
            std::min(lowestSpectralError, item.metrics.spectralError);
        strongestSnr = std::max(strongestSnr, item.metrics.snrDb);
    }
    std::ostringstream reason;
    reason << std::fixed << std::setprecision(2)
           << "Score " << candidate.score << "/100 from "
           << (region.region.damageType == DamageType::Unknown
                   ? "unknown"
                   : toString(region.region.damageType))
           << " evidence.";
    if (candidate.metrics.correlation >= strongestCorrelation - 1e-9) {
        reason << " Highest local waveform correlation.";
    }
    if (candidate.metrics.rmsError <= lowestRmsError + 1e-9) {
        reason << " Lowest RMS reconstruction error.";
    }
    if (candidate.metrics.spectralError <= lowestSpectralError + 1e-9) {
        reason << " Strongest spectral continuity.";
    }
    if (candidate.metrics.snrDb >= strongestSnr - 1e-9) {
        reason << " Highest measured SNR.";
    }
    if (candidate.score == allCandidates.front().score) {
        reason << " Ranked first among " << allCandidates.size()
               << " retained hypotheses.";
    }
    return reason.str();
}

} // namespace walkman::time_machine