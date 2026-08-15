// Audio Time Machine notebook companion.
// Split on "// %% Cell N" when importing into a C++ notebook. The same source
// remains a normal executable for deterministic CI and local verification.

#include "walkman/audio_io.hpp"
#include "walkman/corruption.hpp"
#include "walkman/time_machine/engine.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <memory>

int main() {
    using namespace walkman;
    using namespace walkman::time_machine;

    std::filesystem::create_directories("walkman_output");
    const std::filesystem::path outputDir{"walkman_output"};

    // %% Cell 1 — verify the extension is compiled against C++20.
    static_assert(__cplusplus >= 202002L);
    std::cout << "Audio Time Machine notebook sequence started.\n";

    // %% Cell 2 — create clean and deterministic damaged WAV input.
    const auto clean = makeSyntheticAudio(8000, 2, 4.0);
    const auto corruption = AudioCorruptor(42).corrupt(clean);
    saveWav(outputDir / "time_machine_cells_clean.wav", clean);
    saveWav(outputDir / "time_machine_cells_damaged.wav", corruption.damaged);
    std::cout << "Injected events: " << corruption.events.size() << "\n";

    // %% Cell 3 — share input ownership without duplicating large sample buffers.
    auto damagedShared = std::make_shared<const AudioData>(corruption.damaged);
    auto referenceShared = std::make_shared<const AudioData>(clean);
    std::cout << "Shared damaged frames: " << damagedShared->frameCount() << "\n";

    // %% Cell 4 — analyze regions and generate every repair hypothesis.
    AudioTimeMachine machine;
    auto state = machine.analyze(
        damagedShared, referenceShared, "time_machine_cells_damaged.wav");
    assert(state.history.regionIds().size() == corruption.events.size());
    std::size_t totalHypotheses = 0;
    for (const auto regionId : state.history.regionIds()) {
        totalHypotheses += state.history.region(regionId)->hypotheses.size();
    }
    std::cout << "Regions: " << state.history.regionIds().size()
              << ", hypotheses: " << totalHypotheses << "\n";

    // %% Cell 5 — inspect one region's retained candidate set.
    const RegionId firstRegion = state.history.regionIds().front();
    std::cout << renderComparisonReport(
        state.history, firstRegion, state.damaged->sampleRate);

    // %% Cell 6 — confirm the automatic winner is selected but all candidates remain.
    const auto* initialWinner = state.history.selectedHypothesis(firstRegion);
    assert(initialWinner != nullptr);
    std::cout << "Initial winner: " << initialWinner->id << " / "
              << initialWinner->algorithmName << "\n";

    // %% Cell 7 — build the temporal reconstruction timeline.
    std::cout << renderTimelineReport(state.timeline);
    assert(state.timeline.validate(state.damaged->frameCount()));

    // %% Cell 8 — calculate the explainable confidence map.
    std::cout << renderConfidenceReport(state.confidenceMap);
    assert(state.confidenceMap.validate(state.damaged->frameCount()));
    const auto audit = ReconstructionAudit::run(
        state.history, state.timeline, state.confidenceMap, *state.damaged);
    assert(audit.valid);
    std::cout << ReconstructionAudit::render(audit);

    // %% Cell 9 — create a lightweight snapshot of selected hypotheses.
    const auto bestSnapshot =
        machine.createSnapshot(state, "automatic best hypothesis set");
    assert(state.history.snapshotStore().contains(bestSnapshot));
    std::cout << "Created snapshot: " << bestSnapshot << "\n";

    // %% Cell 10 — switch one damaged region to its next-ranked hypothesis.
    const auto beforeSwitch = state.currentQuality.snrDb;
    const auto candidates = state.history.hypothesesForRegion(firstRegion);
    assert(candidates.size() >= 2);
    const auto alternate = candidates[1].id;
    assert(machine.selectHypothesis(state, firstRegion, alternate));
    std::cout << "Switched region " << firstRegion << " to " << alternate
              << ", SNR delta=" << state.currentQuality.snrDb - beforeSwitch
              << " dB\n";

    // %% Cell 11 — rebuild audio while preserving the complete history.
    const auto switchedAudio = state.rebuilt;
    assert(switchedAudio.samples.size() == clean.samples.size());
    assert(state.history.hypothesesForRegion(firstRegion).size() == candidates.size());

    // %% Cell 12 — revert the region to damaged audio without deleting candidates.
    assert(machine.revertRegion(state, firstRegion));
    assert(state.history.selectedHypothesis(firstRegion) == nullptr);
    assert(state.history.hypothesesForRegion(firstRegion).size() == candidates.size());
    std::cout << "Region reverted; history retained "
              << candidates.size() << " hypotheses.\n";

    // %% Cell 13 — restore the best automatic hypothesis.
    assert(machine.restoreBest(state, firstRegion));
    assert(state.history.selectedHypothesis(firstRegion) != nullptr);
    std::cout << "Best hypothesis restored: "
              << state.history.selectedHypothesis(firstRegion)->id << "\n";

    // %% Cell 14 — restore the complete prior snapshot and export JSON metadata.
    assert(machine.restoreSnapshot(state, bestSnapshot));
    const auto json = JsonExporter::completeToJson(
        state.history, state.timeline, state.confidenceMap, *state.damaged,
        state.inputName);
    JsonExporter::writeFile(
        (outputDir / "time_machine_cells.json").string(), json);
    std::cout << "Exported reconstruction history, timeline, and confidence map.\n";

    // %% Cell 15 — write the final audio and validate the entire state.
    machine.rebuild(state);
    saveWav(outputDir / "time_machine_cells_repaired.wav", state.rebuilt);
    assert(machine.validateState(state));
    std::cout << "Time Machine cells complete. Weighted confidence="
              << state.confidenceMap.averageConfidence() * 100.0 << "%\n";
    return 0;
}