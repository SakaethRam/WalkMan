#include "walkman/time_machine/types.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace walkman::time_machine {

std::string toString(TimelineState state) {
    switch (state) {
    case TimelineState::Clean:
        return "CLEAN";
    case TimelineState::Suspected:
        return "SUSPECTED";
    case TimelineState::Damaged:
        return "DAMAGED";
    case TimelineState::Reconstructed:
        return "RECONSTRUCTED";
    case TimelineState::LowConfidence:
        return "LOW_CONFIDENCE";
    case TimelineState::Reverted:
        return "REVERTED";
    }
    return "UNKNOWN";
}

std::size_t ReconstructionHypothesis::sampleCount() const noexcept {
    return reconstructedSamples == nullptr ? 0 : reconstructedSamples->size();
}

bool ReconstructionHypothesis::validFor(const AudioData& audio) const noexcept {
    return reconstructedSamples != nullptr && region.startSample < region.endSample &&
           region.endSample <= audio.frameCount() &&
           reconstructedSamples->size() ==
               (region.endSample - region.startSample) * audio.channels;
}

bool RegionHypothesisSet::hasSelection() const noexcept {
    return !reverted && !selectedId.empty();
}

std::size_t RegionHypothesisSet::hypothesisCount() const noexcept {
    return hypotheses.size();
}

std::string formatTimestamp(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0.0) {
        seconds = 0.0;
    }
    const auto wholeSeconds = static_cast<std::size_t>(seconds);
    const auto minutes = wholeSeconds / 60U;
    const auto remaining = wholeSeconds % 60U;
    const double milliseconds =
        (seconds - static_cast<double>(wholeSeconds)) * 1000.0;
    std::ostringstream output;
    output << std::setfill('0') << std::setw(2) << minutes << ":"
           << std::setw(2) << remaining << "." << std::setw(3)
           << static_cast<int>(std::clamp(milliseconds, 0.0, 999.0));
    return output.str();
}

} // namespace walkman::time_machine