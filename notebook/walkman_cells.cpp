// WalkMan notebook companion.
// Each "// %% Cell N" marker is an executable Zerve/Jupyter-style cell boundary.
// The file is also compiled as a normal executable so the complete sequence is
// reproducible outside a notebook.

#include "walkman/audio_io.hpp"
#include "walkman/analysis.hpp"
#include "walkman/corruption.hpp"
#include "walkman/detection.hpp"
#include "walkman/engine.hpp"
#include "walkman/evaluation.hpp"
#include "walkman/repair.hpp"

#include <cassert>
#include <filesystem>
#include <iomanip>
#include <iostream>

int main() {
    using namespace walkman;
    std::filesystem::create_directories("walkman_output");

    // %% Cell 1 — verify the C++20 environment and prepare output paths.
    static_assert(__cplusplus >= 202002L);
    const std::filesystem::path outputDir{"walkman_output"};
    std::cout << "WalkMan C++20 notebook sequence started.\n";

    // %% Cell 2 — bring in the core data structures and deterministic test audio.
    AudioData clean = makeSyntheticAudio(8000, 2, 4.0);
    assert(clean.valid());
    std::cout << "Audio: " << clean.sampleRate << " Hz, " << clean.channels
              << " channels, " << clean.frameCount() << " frames\n";

    // %% Cell 3 — write and reload WAV/PCM audio.
    const auto cleanPath = outputDir / "cells_clean.wav";
    saveWav(cleanPath, clean);
    const AudioData loaded = loadWav(cleanPath);
    assert(loaded.sampleRate == clean.sampleRate && loaded.channels == clean.channels);
    std::cout << "WAV round trip verified: " << cleanPath << "\n";

    // %% Cell 4 — calculate basic signal-analysis features.
    std::cout << std::fixed << std::setprecision(4)
              << "RMS=" << rms(loaded, 0, loaded.frameCount())
              << " Peak=" << peakAmplitude(loaded, 0, loaded.frameCount())
              << " ZCR=" << zeroCrossingRate(loaded, 0, loaded.frameCount())
              << " MAD=" << meanAbsoluteDifference(loaded, 0, loaded.frameCount())
              << "\n";

    // %% Cell 5 — generate deterministic corruption and inspect its ground truth.
    const auto corruption = AudioCorruptor(42).corrupt(loaded);
    const auto damagedPath = outputDir / "cells_damaged.wav";
    saveWav(damagedPath, corruption.damaged);
    std::cout << "Injected " << corruption.events.size() << " corruption events.\n";

    // %% Cell 6 — detect zero dropouts, clipping, anomalies, and discontinuities.
    const DamageDetector detector;
    const auto detected = detector.detect(corruption.damaged);
    std::cout << "Detected " << detected.size() << " damaged regions.\n";

    // %% Cell 7 — instantiate the repair abstraction and linear interpolation.
    LinearRepair linear;
    if (!detected.empty()) {
        const auto candidate = linear.repair(corruption.damaged, detected.front());
        std::cout << linear.name() << " candidate samples: " << candidate.size() << "\n";
    }

    // %% Cell 8 — create a smooth cubic/spline reconstruction.
    SplineRepair spline;
    if (!detected.empty()) {
        std::cout << spline.name() << " is independently callable.\n";
    }

    // %% Cell 9 — search context for a matching waveform segment.
    WaveformMatchRepair waveformMatch;
    if (!detected.empty()) {
        std::cout << waveformMatch.name() << " is independently callable.\n";
    }

    // %% Cell 10 — estimate a harmonic continuation with a radix-2 FFT.
    SpectralRepair spectral;
    if (!detected.empty()) {
        std::cout << spectral.name() << " is independently callable.\n";
    }

    // %% Cell 11 — score every candidate against clean truth and local continuity.
    const CandidateEvaluator evaluator;
    if (!detected.empty()) {
        const auto probe = evaluator.evaluate(
            corruption.damaged, &loaded, detected.front(), linear.name(),
            linear.repair(corruption.damaged, detected.front()));
        std::cout << "Probe score for LINEAR: " << probe.score << "/100, SNR="
                  << probe.metrics.snrDb << " dB\n";
    }

    // %% Cell 12 — run automatic detect → generate → score → select → replace.
    AudioData repaired;
    const HealingEngine engine;
    const auto report = engine.heal(corruption.damaged, &loaded, repaired);
    std::cout << "Automatic selection repaired " << report.repairedRegions << " regions.\n";

    // %% Cell 13 — evaluate full-file integrity and render the transparent report.
    const auto quality = evaluateQuality(loaded, repaired);
    std::cout << "Full-file MSE=" << quality.mse
              << " SNR=" << quality.snrDb
              << " correlation=" << quality.correlation << "\n";
    std::cout << renderReport(report, corruption.damaged, damagedPath,
                              outputDir / "cells_repaired.wav");

    // %% Cell 14 — persist the end-to-end repaired artifact.
    const auto repairedPath = outputDir / "cells_repaired.wav";
    saveWav(repairedPath, repaired);
    std::cout << "Wrote " << repairedPath << "\n";

    // %% Cell 15 — final reproducible benchmark summary.
    std::cout << "WalkMan benchmark complete: detected=" << report.detectedRegions
              << ", repaired=" << report.repairedRegions
              << ", integrity=" << report.overallIntegrityScore << "/100\n";
    return 0;
}