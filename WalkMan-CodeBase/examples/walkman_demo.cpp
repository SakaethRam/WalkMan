#include "walkman/audio_io.hpp"
#include "walkman/corruption.hpp"
#include "walkman/engine.hpp"

#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

struct Options {
    std::optional<std::filesystem::path> input;
    std::optional<std::filesystem::path> reference;
    std::filesystem::path output{"walkman_output/repaired.wav"};
    std::uint32_t seed{42};
};

Options parseOptions(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string argument(argv[i]);
        const auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                throw std::invalid_argument("Missing value after " + argument);
            }
            return argv[++i];
        };
        if (argument == "--input") {
            options.input = next();
        } else if (argument == "--reference") {
            options.reference = next();
        } else if (argument == "--output") {
            options.output = next();
        } else if (argument == "--seed") {
            options.seed = static_cast<std::uint32_t>(std::stoul(next()));
        } else if (argument == "--help") {
            std::cout << "Usage: walkman_demo [--input damaged.wav] "
                         "[--reference clean.wav] [--output repaired.wav] [--seed 42]\n";
            std::exit(0);
        } else {
            throw std::invalid_argument("Unknown option: " + argument);
        }
    }
    return options;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parseOptions(argc, argv);
        walkman::AudioData damaged;
        std::optional<walkman::AudioData> reference;
        std::filesystem::path inputPath;

        if (options.input.has_value()) {
            inputPath = *options.input;
            damaged = walkman::loadWav(inputPath);
            if (options.reference.has_value()) {
                reference = walkman::loadWav(*options.reference);
            }
        } else {
            inputPath = "walkman_output/damaged.wav";
            const auto clean = walkman::makeSyntheticAudio(8000, 2, 4.0);
            const auto corruption = walkman::AudioCorruptor(options.seed).corrupt(clean);
            std::filesystem::create_directories("walkman_output");
            walkman::saveWav("walkman_output/clean.wav", clean);
            walkman::saveWav(inputPath, corruption.damaged);
            damaged = corruption.damaged;
            reference = clean;
            std::cout << "No input supplied; generated deterministic synthetic test audio.\n";
            std::cout << "Injected corruption events: " << corruption.events.size() << "\n";
        }

        walkman::AudioData repaired;
        const walkman::HealingEngine engine;
        const auto report = engine.heal(
            damaged, reference.has_value() ? &reference.value() : nullptr, repaired);
        walkman::saveWav(options.output, repaired);
        std::cout << walkman::renderReport(report, damaged, inputPath, options.output);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "WalkMan error: " << error.what() << "\n";
        return 1;
    }
}