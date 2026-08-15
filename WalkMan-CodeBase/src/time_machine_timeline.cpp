#include "walkman/time_machine/timeline.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace walkman::time_machine {
namespace {

TimelineState stateFor(const RegionHypothesisSet& region) {
    if (region.reverted) {
        return TimelineState::Reverted;
    }
    const auto* selected = region.hypotheses.empty()
                               ? nullptr
                               : [&]() -> const ReconstructionHypothesis* {
                                     if (region.selectedId.empty()) {
                                         return nullptr;
                                     }
                                     const auto iterator = std::find_if(
                                         region.hypotheses.begin(),
                                         region.hypotheses.end(),
                                         [&](const ReconstructionHypothesis& candidate) {
                                             return candidate.id == region.selectedId;
                                         });
                                     return iterator == region.hypotheses.end() ? nullptr : &*iterator;
                                 }();
    if (selected == nullptr) {
        return TimelineState::Damaged;
    }
    return selected->confidence < 0.55 ? TimelineState::LowConfidence
                                       : TimelineState::Reconstructed;
}

} // namespace

void AudioTimeline::rebuild(const AudioData& damaged,
                            const ReconstructionHistory& history) {
    segments_.clear();
    if (!damaged.valid()) {
        return;
    }
    std::size_t cursor = 0;
    const double sampleRate = static_cast<double>(damaged.sampleRate);
    for (const RegionId regionId : history.regionIds()) {
        const auto* region = history.region(regionId);
        if (region == nullptr) {
            continue;
        }
        if (region->region.startSample > cursor) {
            segments_.push_back(TimelineSegment{
                cursor,
                region->region.startSample,
                static_cast<double>(cursor) / sampleRate,
                static_cast<double>(region->region.startSample) / sampleRate,
                TimelineState::Clean,
                1.0,
                0,
                {},
                {},
            });
        }
        const auto* selected = history.selectedHypothesis(regionId);
        const TimelineState state = stateFor(*region);
        const double confidence = selected == nullptr
                                      ? 0.0
                                      : selected->confidence;
        segments_.push_back(TimelineSegment{
            region->region.startSample,
            region->region.endSample,
            static_cast<double>(region->region.startSample) / sampleRate,
            static_cast<double>(region->region.endSample) / sampleRate,
            state,
            confidence,
            regionId,
            selected == nullptr ? HypothesisId{} : selected->id,
            selected == nullptr ? std::string{} : selected->algorithmName,
        });
        cursor = std::max(cursor, region->region.endSample);
    }
    if (cursor < damaged.frameCount()) {
        segments_.push_back(TimelineSegment{
            cursor,
            damaged.frameCount(),
            static_cast<double>(cursor) / sampleRate,
            static_cast<double>(damaged.frameCount()) / sampleRate,
            TimelineState::Clean,
            1.0,
            0,
            {},
            {},
        });
    }
}

const std::vector<TimelineSegment>& AudioTimeline::segments() const noexcept {
    return segments_;
}

const TimelineSegment* AudioTimeline::segmentAt(
    std::size_t sample) const noexcept {
    const auto iterator = std::find_if(
        segments_.begin(), segments_.end(),
        [&](const TimelineSegment& segment) {
            return sample >= segment.startSample && sample < segment.endSample;
        });
    return iterator == segments_.end() ? nullptr : &*iterator;
}

std::size_t AudioTimeline::reconstructedSampleCount() const noexcept {
    std::size_t count = 0;
    for (const auto& segment : segments_) {
        if (segment.state == TimelineState::Reconstructed ||
            segment.state == TimelineState::LowConfidence) {
            count += segment.endSample - segment.startSample;
        }
    }
    return count;
}

std::size_t AudioTimeline::damagedSampleCount() const noexcept {
    std::size_t count = 0;
    for (const auto& segment : segments_) {
        if (segment.state != TimelineState::Clean) {
            count += segment.endSample - segment.startSample;
        }
    }
    return count;
}

std::string AudioTimeline::renderAscii(std::size_t width) const {
    if (segments_.empty() || width == 0) {
        return "Timeline: <empty>\n";
    }
    const std::size_t total = segments_.back().endSample;
    std::string bar(width, '-');
    for (std::size_t i = 0; i < width; ++i) {
        const std::size_t sample = (i * total) / width;
        const auto* segment = segmentAt(sample);
        if (segment == nullptr) {
            continue;
        }
        char marker = ' ';
        switch (segment->state) {
        case TimelineState::Clean:
            marker = '=';
            break;
        case TimelineState::Suspected:
            marker = '?';
            break;
        case TimelineState::Damaged:
            marker = '!';
            break;
        case TimelineState::Reconstructed:
            marker = '#';
            break;
        case TimelineState::LowConfidence:
            marker = '~';
            break;
        case TimelineState::Reverted:
            marker = 'R';
            break;
        }
        bar[i] = marker;
    }
    std::ostringstream output;
    output << "0:00 " << bar << " "
           << formatTimestamp(segments_.back().endSeconds) << "\n"
           << "Legend: = clean, # reconstructed, ~ low confidence, "
              "! damaged, R reverted\n";
    return output.str();
}

std::string AudioTimeline::toCsv() const {
    std::ostringstream output;
    output << "start_sample,end_sample,start_time,end_time,state,confidence,"
              "region_id,reconstruction_id,algorithm\n";
    output << std::fixed << std::setprecision(6);
    for (const auto& segment : segments_) {
        output << segment.startSample << "," << segment.endSample << ","
               << segment.startSeconds << "," << segment.endSeconds << ","
               << toString(segment.state) << "," << segment.confidence << ","
               << segment.regionId << "," << segment.reconstructionId << ","
               << segment.selectedAlgorithm << "\n";
    }
    return output.str();
}

bool AudioTimeline::validate(std::size_t totalSamples) const noexcept {
    std::size_t cursor = 0;
    for (const auto& segment : segments_) {
        if (segment.startSample != cursor ||
            segment.startSample >= segment.endSample ||
            segment.endSample > totalSamples ||
            segment.confidence < 0.0 || segment.confidence > 1.0) {
            return false;
        }
        cursor = segment.endSample;
    }
    return segments_.empty() ? totalSamples == 0 : cursor == totalSamples;
}

} // namespace walkman::time_machine