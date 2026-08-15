#pragma once

#include "walkman/types.hpp"

#include <cstdint>
#include <vector>

namespace walkman {

struct CorruptionEvent {
    DamageType type{DamageType::Unknown};
    std::size_t startSample{0};
    std::size_t endSample{0};
};

struct CorruptionResult {
    AudioData damaged;
    std::vector<CorruptionEvent> events;
};

class AudioCorruptor {
public:
    explicit AudioCorruptor(std::uint32_t seed = 42);
    [[nodiscard]] CorruptionResult corrupt(const AudioData& clean) const;

private:
    std::uint32_t seed_;
};

AudioData makeSyntheticAudio(std::uint32_t sampleRate = 8000,
                             std::uint16_t channels = 2,
                             double durationSeconds = 4.0);

} // namespace walkman