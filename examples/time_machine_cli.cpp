#include "walkman/audio_io.hpp"
#include "walkman/corruption.hpp"
#include "walkman/time_machine/engine.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

using walkman::AudioData;
using walkman::loadWav;
using walkman::saveWav;
using walkman::time_machine::AudioTimeMachine;
using walkman::time_machine::HypothesisId;
using walkman::time_machine::RegionId;
using walkman::time_machine::TimeMachineState;

struct Options {
    std::string command{"demo"};
    std::optional<std::filesystem::path> input;
    std::optional<std::filesystem::path> reference;
    std::filesystem::path output{"walkman_output/time_machine_repaired.wav"};
    std::filesystem::path json{"walkman_output/time_machine.json"};
    std::uint32_t seed{42};
    std::optional<RegionId> region;
    std::optional<HypothesisId> hypothesis;
    bool revert{false};
    bool writeJson{true};
    std::string label{"manual snapshot"};
};

struct InputSession {
    AudioData damaged;
    std::optional<AudioData> reference;
    std::filesystem::path inputPath;
    std::filesystem::path referencePath;
    bool generated{false};
};

void printUsage() {
    std::cout
        << "WalkMan Audio Time Machine\n\n"
        << "Usage:\n"
        << "  walkman_time_machine_cli demo [options]\n"
        << "  walkman_time_machine_cli analyze damaged.wav [options]\n"
        << "  walkman_time_machine_cli repair damaged.wav --auto [options]\n"
        << "  walkman_time_machine_cli timeline damaged.wav [options]\n"
        << "  walkman_time_machine_cli hypotheses damaged.wav [options]\n"
        << "  walkman_time_machine_cli compare damaged.wav --region N [options]\n"
        << "  walkman_time_machine_cli restore damaged.wav --region N [--hypothesis R3 | --revert]\n"
        << "  walkman_time_machine_cli snapshot damaged.wav [options]\n\n"
        << "Options:\n"
        << "  --input PATH        damaged WAV input; omitted for deterministic demo audio\n"
        << "  --reference PATH    clean WAV used for objective candidate scoring\n"
        << "  --output PATH       repaired WAV output\n"
        << "  --json PATH         machine-readable metadata output\n"
        << "  --no-json           skip JSON export\n"
        << "  --region N          one-based damaged region number\n"
        << "  --hypothesis ID     hypothesis ID such as R3\n"
        << "  --revert            restore a region to damaged input\n"
        << "  --label TEXT        snapshot label\n"
        << "  --seed N            deterministic synthetic corruption seed\n"
        << "  --help              show this help\n";
}

Options parseOptions(int argc, char** argv) {
    Options options;
    int index = 1;
    if (index < argc && std::string(argv[index]).rfind("--", 0) != 0) {
        options.command = argv[index++];
    }
    while (index < argc) {
        const std::string argument(argv[index++]);
        const auto take = [&](const std::string& name) -> std::string {
            if (index >= argc) {
                throw std::invalid_argument("Missing value after " + name);
            }
            return argv[index++];
        };
        if (argument == "--help") {
            printUsage();
            std::exit(0);
        } else if (argument == "--input") {
            options.input = take(argument);
        } else if (argument == "--reference") {
            options.reference = take(argument);
        } else if (argument == "--output") {
            options.output = take(argument);
        } else if (argument == "--json") {
            options.json = take(argument);
        } else if (argument == "--no-json") {
            options.writeJson = false;
        } else if (argument == "--region") {
            options.region = static_cast<RegionId>(std::stoull(take(argument)));
        } else if (argument == "--hypothesis") {
            options.hypothesis = take(argument);
        } else if (argument == "--revert") {
            options.revert = true;
        } else if (argument == "--label") {
            options.label = take(argument);
        } else if (argument == "--seed") {
            options.seed = static_cast<std::uint32_t>(std::stoul(take(argument)));
        } else {
            throw std::invalid_argument("Unknown option: " + argument);
        }
    }
    return options;
}

InputSession loadSession(const Options& options) {
    InputSession session;
    if (options.input.has_value()) {
        session.inputPath = *options.input;
        session.damaged = loadWav(session.inputPath);
        if (options.reference.has_value()) {
            session.referencePath = *options.reference;
            session.reference = loadWav(session.referencePath);
        }
        return session;
    }

    session.generated = true;
    session.inputPath = "walkman_output/time_machine_damaged.wav";
    session.referencePath = "walkman_output/time_machine_clean.wav";
    const auto clean = walkman::makeSyntheticAudio(8000, 2, 4.0);
    const auto corruption = walkman::AudioCorruptor(options.seed).corrupt(clean);
    std::filesystem::create_directories("walkman_output");
    saveWav(session.referencePath, clean);
    saveWav(session.inputPath, corruption.damaged);
    session.damaged = corruption.damaged;
    session.reference = clean;
    std::cout << "Generated deterministic demo input with "
              << corruption.events.size() << " injected events.\n";
    return session;
}

TimeMachineState analyzeSession(const Options& options,
                                const InputSession& session,
                                const AudioTimeMachine& machine) {
    return machine.analyze(
        session.damaged,
        session.reference.has_value() ? &session.reference.value() : nullptr,
        session.inputPath.string());
}

void writeStateOutputs(const Options& options, const InputSession& session,
                       const AudioTimeMachine& machine,
                       TimeMachineState& state) {
    machine.rebuild(state);
    saveWav(options.output, state.rebuilt);
    if (options.writeJson) {
        const auto json = walkman::time_machine::JsonExporter::completeToJson(
            state.history, state.timeline, state.confidenceMap, *state.damaged,
            session.inputPath.string());
        walkman::time_machine::JsonExporter::writeFile(options.json, json);
    }
}

void printHypotheses(const TimeMachineState& state,
                     std::optional<RegionId> onlyRegion = std::nullopt) {
    for (const RegionId regionId : state.history.regionIds()) {
        if (onlyRegion.has_value() && *onlyRegion != regionId) {
            continue;
        }
        std::cout << walkman::time_machine::renderComparisonReport(
            state.history, regionId, state.damaged->sampleRate);
    }
}

std::optional<RegionId> firstSwitchableRegion(const TimeMachineState& state) {
    for (const RegionId regionId : state.history.regionIds()) {
        const auto* region = state.history.region(regionId);
        if (region != nullptr && region->hypotheses.size() > 1U) {
            return regionId;
        }
    }
    return std::nullopt;
}

void runDemo(const Options& options, const InputSession& session,
             const AudioTimeMachine& machine) {
    auto state = analyzeSession(options, session, machine);
    const auto bestSnapshot = machine.createSnapshot(state, "automatic best repair");
    std::cout << walkman::time_machine::renderTimeMachineReport(
        state.history, state.timeline, state.confidenceMap, *state.damaged,
        state.cleanReference.get(), session.inputPath, options.output);
    std::cout << "\nHYPOTHESIS COMPARISONS\n"
              << "==============================================\n";
    printHypotheses(state);

    writeStateOutputs(options, session, machine, state);
    std::cout << "\nAutomatic reconstruction written to " << options.output << "\n"
              << "Metadata written to " << options.json << "\n";

    const auto switchRegion = firstSwitchableRegion(state);
    if (switchRegion.has_value()) {
        const auto* region = state.history.region(*switchRegion);
        if (region != nullptr && region->hypotheses.size() > 1U) {
            const auto alternate = region->hypotheses[1].id;
            const double bestSnr = state.currentQuality.snrDb;
            if (machine.selectHypothesis(state, *switchRegion, alternate)) {
                const auto alternateOutput =
                    options.output.parent_path() / "time_machine_alternate.wav";
                saveWav(alternateOutput, state.rebuilt);
                std::cout << "\nTIME MACHINE SWITCH\n"
                          << "Region " << *switchRegion << " switched to "
                          << alternate << ".\n"
                          << "New output: " << alternateOutput << "\n";
                if (state.hasReference()) {
                    std::cout << "SNR delta: "
                              << state.currentQuality.snrDb - bestSnr << " dB\n";
                }
                std::cout << state.timeline.renderAscii(80)
                          << state.confidenceMap.renderAscii(80);
            }
        }
    }

    if (machine.restoreSnapshot(state, bestSnapshot)) {
        const auto restoredOutput =
            options.output.parent_path() / "time_machine_restored.wav";
        saveWav(restoredOutput, state.rebuilt);
        std::cout << "\nRESTORED SNAPSHOT " << bestSnapshot << "\n"
                  << state.history.describeSelections()
                  << "Restored output: " << restoredOutput << "\n";
    }
}

void runSingleCommand(const Options& options, const InputSession& session,
                      const AudioTimeMachine& machine) {
    auto state = analyzeSession(options, session, machine);
    if (options.command == "analyze") {
        std::cout << walkman::time_machine::renderTimeMachineReport(
            state.history, state.timeline, state.confidenceMap, *state.damaged,
            state.cleanReference.get(), session.inputPath, options.output);
        return;
    }
    if (options.command == "timeline") {
        std::cout << walkman::time_machine::renderTimelineReport(state.timeline)
                  << walkman::time_machine::renderConfidenceReport(state.confidenceMap);
        return;
    }
    if (options.command == "hypotheses") {
        printHypotheses(state);
        return;
    }
    if (options.command == "compare") {
        if (!options.region.has_value()) {
            throw std::invalid_argument("compare requires --region N");
        }
        std::cout << walkman::time_machine::renderComparisonReport(
            state.history, *options.region, state.damaged->sampleRate);
        return;
    }
    if (options.command == "repair") {
        writeStateOutputs(options, session, machine, state);
        std::cout << walkman::time_machine::renderTimeMachineReport(
            state.history, state.timeline, state.confidenceMap, *state.damaged,
            state.cleanReference.get(), session.inputPath, options.output);
        return;
    }
    if (options.command == "restore") {
        if (!options.region.has_value()) {
            throw std::invalid_argument("restore requires --region N");
        }
        const RegionId region = *options.region;
        bool changed = false;
        if (options.revert) {
            changed = machine.revertRegion(state, region);
        } else if (options.hypothesis.has_value()) {
            changed = machine.selectHypothesis(state, region, *options.hypothesis);
        } else {
            changed = machine.restoreBest(state, region);
        }
        if (!changed) {
            throw std::runtime_error("The requested restore selection was not found");
        }
        writeStateOutputs(options, session, machine, state);
        std::cout << walkman::time_machine::renderTimeMachineReport(
            state.history, state.timeline, state.confidenceMap, *state.damaged,
            state.cleanReference.get(), session.inputPath, options.output);
        return;
    }
    if (options.command == "snapshot") {
        const auto first = machine.createSnapshot(state, options.label);
        const auto region = firstSwitchableRegion(state);
        if (region.has_value()) {
            const auto* target = state.history.region(*region);
            if (target != nullptr && target->hypotheses.size() > 1U) {
                if (!machine.selectHypothesis(
                        state, *region, target->hypotheses[1].id)) {
                    throw std::runtime_error("Unable to create manual snapshot switch");
                }
            }
        }
        const auto second = machine.createSnapshot(state, "after manual switch");
        if (!machine.restoreSnapshot(state, first)) {
            throw std::runtime_error("Unable to restore first snapshot");
        }
        std::cout << "Snapshots created: " << first << ", " << second << "\n"
                  << "Restored " << first << ":\n"
                  << state.history.describeSelections();
        return;
    }
    throw std::invalid_argument("Unknown command: " + options.command);
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parseOptions(argc, argv);
        if (options.command != "demo" && options.command != "analyze" &&
            options.command != "repair" && options.command != "timeline" &&
            options.command != "hypotheses" && options.command != "compare" &&
            options.command != "restore" && options.command != "snapshot") {
            printUsage();
            return 2;
        }
        const auto session = loadSession(options);
        const AudioTimeMachine machine;
        if (options.command == "demo") {
            runDemo(options, session, machine);
        } else {
            runSingleCommand(options, session, machine);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "WalkMan Audio Time Machine error: " << error.what() << "\n";
        return 1;
    }
}