#pragma once

#include "walkman/time_machine/history.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace walkman::time_machine {

class AudioTimeline {
public:
    AudioTimeline() = default;

    void rebuild(const AudioData& damaged,
                 const ReconstructionHistory& history);
    [[nodiscard]] const std::vector<TimelineSegment>& segments() const noexcept;
    [[nodiscard]] const TimelineSegment* segmentAt(
        std::size_t sample) const noexcept;
    [[nodiscard]] std::size_t reconstructedSampleCount() const noexcept;
    [[nodiscard]] std::size_t damagedSampleCount() const noexcept;
    [[nodiscard]] std::string renderAscii(
        std::size_t width = 72) const;
    [[nodiscard]] std::string toCsv() const;
    [[nodiscard]] bool validate(std::size_t totalSamples) const noexcept;

private:
    std::vector<TimelineSegment> segments_;
};

} // namespace walkman::time_machine