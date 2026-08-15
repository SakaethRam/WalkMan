#include "walkman/corruption.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <stdexcept>

namespace walkman {

AudioCorruptor::AudioCorruptor(std::uint32_t seed) : seed_(seed) {}

CorruptionResult AudioCorruptor::corrupt(const AudioData& clean) const {
    if (!clean.valid()) {
        throw std::invalid_argument("Cannot corrupt invalid audio");
    }
    CorruptionResult result{clean, {}};
    const std::size_t frames = clean.frameCount();
    const auto atSecond = [&](double seconds) {
        return std::min(frames - 1, static_cast<std::size_t>(seconds * clean.sampleRate));
    };
    const auto addEvent = [&](DamageType type, std::size_t start, std::size_t length) {
        const std::size_t end = std::min(frames, start + length);
        result.events.push_back({type, start, end});
        return std::pair{start, end};
    };

    const auto [zeroStart, zeroEnd] =
        addEvent(DamageType::ZeroDropout, atSecond(0.70),
                 std::max<std::size_t>(clean.sampleRate / 12U, 24U));
    for (std::size_t frame = zeroStart; frame < zeroEnd; ++frame) {
        for (std::size_t channel = 0; channel < clean.channels; ++channel) {
            result.damaged.sample(frame, channel) = 0.0;
        }
    }

    const auto [clipStart, clipEnd] =
        addEvent(DamageType::Clipping, atSecond(1.55),
                 std::max<std::size_t>(clean.sampleRate / 18U, 12U));
    for (std::size_t frame = clipStart; frame < clipEnd; ++frame) {
        for (std::size_t channel = 0; channel < clean.channels; ++channel) {
            const double value = result.damaged.sample(frame, channel);
            result.damaged.sample(frame, channel) = value < 0.0 ? -1.0 : 1.0;
        }
    }

    const auto [noiseStart, noiseEnd] =
        addEvent(DamageType::NoiseBurst, atSecond(2.30),
                 std::max<std::size_t>(clean.sampleRate / 16U, 16U));
    std::mt19937 generator(seed_);
    std::normal_distribution<double> noise(0.0, 0.48);
    for (std::size_t frame = noiseStart; frame < noiseEnd; ++frame) {
        for (std::size_t channel = 0; channel < clean.channels; ++channel) {
            result.damaged.sample(frame, channel) =
                std::clamp(result.damaged.sample(frame, channel) + noise(generator),
                           -1.0, 1.0);
        }
    }

    const auto [gapStart, gapEnd] =
        addEvent(DamageType::SampleGap, atSecond(2.95),
                 std::max<std::size_t>(clean.sampleRate / 20U, 10U));
    for (std::size_t frame = gapStart; frame < gapEnd; ++frame) {
        for (std::size_t channel = 0; channel < clean.channels; ++channel) {
            result.damaged.sample(frame, channel) = 0.0;
        }
    }

    const std::size_t jump = atSecond(3.45);
    if (jump + 1 < frames) {
        const auto [jumpStart, jumpEnd] = addEvent(DamageType::Discontinuity, jump, 1);
        (void)jumpEnd;
        for (std::size_t channel = 0; channel < clean.channels; ++channel) {
            result.damaged.sample(jumpStart, channel) =
                std::clamp(result.damaged.sample(jumpStart, channel) +
                               (channel % 2 == 0 ? 0.85 : -0.85),
                           -1.0, 1.0);
        }
    }
    return result;
}

AudioData makeSyntheticAudio(std::uint32_t sampleRate, std::uint16_t channels,
                             double durationSeconds) {
    if (sampleRate == 0 || channels == 0 || durationSeconds <= 0.0) {
        throw std::invalid_argument("Synthetic audio parameters must be positive");
    }
    AudioData audio;
    audio.sampleRate = sampleRate;
    audio.channels = channels;
    const auto frames = static_cast<std::size_t>(
        std::ceil(durationSeconds * static_cast<double>(sampleRate)));
    audio.samples.resize(frames * channels);

    for (std::size_t frame = 0; frame < frames; ++frame) {
        const double t = static_cast<double>(frame) / sampleRate;
        const double envelope = 0.72 + 0.18 * std::sin(2.0 * std::numbers::pi * 0.35 * t);
        for (std::size_t channel = 0; channel < channels; ++channel) {
            const double pan = channels == 1
                                   ? 1.0
                                   : (channel == 0 ? 0.94 : 0.86);
            const double fundamental =
                std::sin(2.0 * std::numbers::pi * (220.0 + 4.0 * std::sin(t)) * t);
            const double harmonic =
                0.24 * std::sin(2.0 * std::numbers::pi * 440.0 * t +
                                static_cast<double>(channel) * 0.11);
            audio.sample(frame, channel) = std::clamp(envelope * pan *
                                                            (0.62 * fundamental + harmonic),
                                                        -1.0, 1.0);
        }
    }
    return audio;
}

} // namespace walkman