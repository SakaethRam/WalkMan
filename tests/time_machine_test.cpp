#include "walkman/audio_io.hpp"
#include "walkman/corruption.hpp"
#include "walkman/time_machine/engine.hpp"
#include "walkman/time_machine/json.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace {

using namespace walkman;
using namespace walkman::time_machine;

TimeMachineState makeState(AudioData& clean, AudioData& damaged) {
    AudioTimeMachine machine;
    return machine.analyze(damaged, &clean, "test-damaged.wav");
}

void testHypothesisCreationAndRanking() {
    auto clean = makeSyntheticAudio(4000, 1, 2.0);
    auto damaged = AudioCorruptor(11).corrupt(clean).damaged;
    const auto state = makeState(clean, damaged);
    assert(!state.history.regionIds().empty());
    for (const auto regionId : state.history.regionIds()) {
        const auto candidates = state.history.hypothesesForRegion(regionId);
        assert(candidates.size() == 4);
        assert(candidates.front().qualityScore >= candidates.back().qualityScore);
        assert(state.history.selectedHypothesis(regionId) != nullptr);
    }
    assert(state.history.validate());
}

void testTimelineAndConfidence() {
    auto clean = makeSyntheticAudio(4000, 2, 2.0);
    auto damaged = AudioCorruptor(12).corrupt(clean).damaged;
    const auto state = makeState(clean, damaged);
    assert(state.timeline.validate(damaged.frameCount()));
    assert(state.confidenceMap.validate(damaged.frameCount()));
    assert(!state.timeline.segments().empty());
    assert(!state.confidenceMap.bands().empty());
    assert(state.confidenceMap.averageConfidence() > 0.0);
    assert(state.timeline.segmentAt(0) != nullptr);
    const auto audit = ReconstructionAudit::run(
        state.history, state.timeline, state.confidenceMap, *state.damaged);
    assert(audit.valid);
    assert(audit.hypothesisCount == state.history.regionIds().size() * 4U);
    assert(audit.algorithmUseCount("WAVEFORM_MATCH") > 0);
}

void testSwitchAndRebuild() {
    auto clean = makeSyntheticAudio(4000, 1, 2.0);
    auto damaged = AudioCorruptor(13).corrupt(clean).damaged;
    AudioTimeMachine machine;
    auto state = machine.analyze(damaged, &clean, "switch.wav");
    const auto regionId = state.history.regionIds().front();
    const auto candidates = state.history.hypothesesForRegion(regionId);
    const auto original = state.history.selectedHypothesis(regionId)->id;
    assert(machine.selectHypothesis(state, regionId, candidates.back().id));
    assert(state.history.selectedHypothesis(regionId)->id == candidates.back().id);
    assert(state.rebuilt.samples.size() == damaged.samples.size());
    assert(state.history.hypothesesForRegion(regionId).size() == candidates.size());
    assert(machine.restoreBest(state, regionId));
    assert(state.history.selectedHypothesis(regionId)->id == original);
}

void testRevertAndSnapshotRestore() {
    auto clean = makeSyntheticAudio(4000, 1, 2.0);
    auto damaged = AudioCorruptor(14).corrupt(clean).damaged;
    AudioTimeMachine machine;
    auto state = machine.analyze(damaged, &clean, "snapshot.wav");
    const auto regionId = state.history.regionIds().front();
    const auto snapshot = machine.createSnapshot(state, "best");
    const auto candidates = state.history.hypothesesForRegion(regionId);
    assert(machine.selectHypothesis(state, regionId, candidates.back().id));
    assert(machine.revertRegion(state, regionId));
    assert(state.history.isReverted(regionId));
    assert(machine.restoreSnapshot(state, snapshot));
    assert(!state.history.isReverted(regionId));
    assert(state.history.selectedHypothesis(regionId) != nullptr);
    assert(state.history.snapshotStore().contains(snapshot));
}

void testJsonExport() {
    auto clean = makeSyntheticAudio(4000, 2, 2.0);
    auto damaged = AudioCorruptor(15).corrupt(clean).damaged;
    const auto state = makeState(clean, damaged);
    const auto json = JsonExporter::completeToJson(
        state.history, state.timeline, state.confidenceMap, *state.damaged,
        state.inputName);
    assert(json.find("\"regions\"") != std::string::npos);
    assert(json.find("\"hypotheses\"") != std::string::npos);
    assert(json.find("\"confidenceMap\"") != std::string::npos);
    const auto audit = ReconstructionAudit::run(
        state.history, state.timeline, state.confidenceMap, *state.damaged);
    assert(ReconstructionAudit::toJson(audit).find("\"valid\": true") !=
           std::string::npos);
    const auto path = std::filesystem::path("walkman_output/time_machine_test.json");
    JsonExporter::writeFile(path.string(), json);
    std::ifstream input(path);
    assert(input.good());
}

void testNoReferenceMode() {
    auto clean = makeSyntheticAudio(4000, 1, 2.0);
    auto damaged = AudioCorruptor(16).corrupt(clean).damaged;
    AudioTimeMachine machine;
    auto state = machine.analyze(damaged, nullptr, "no-reference.wav");
    assert(!state.hasReference());
    assert(state.valid());
    const auto& first = state.history.hypothesesForRegion(
        state.history.regionIds().front()).front();
    assert(first.metadata.evaluationMode == "local-continuity");
}

} // namespace

int main() {
    std::filesystem::create_directories("walkman_output");
    testHypothesisCreationAndRanking();
    testTimelineAndConfidence();
    testSwitchAndRebuild();
    testRevertAndSnapshotRestore();
    testJsonExport();
    testNoReferenceMode();
    return 0;
}