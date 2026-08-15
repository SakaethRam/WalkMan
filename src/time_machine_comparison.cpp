#include "walkman/time_machine/comparison.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace walkman::time_machine {

HypothesisComparison HypothesisComparator::compare(
    const ReconstructionHistory& history, RegionId regionId) {
    return history.compare(regionId);
}

std::string HypothesisComparator::render(
    const HypothesisComparison& comparison, bool includeMetrics,
    std::uint32_t sampleRate) {
    const double safeSampleRate = sampleRate == 0 ? 1.0 : sampleRate;
    std::ostringstream output;
    output << "REGION #" << comparison.regionId << "\n"
           << "-----------------------------------------\n"
           << "Time: " << formatTimestamp(
                               static_cast<double>(comparison.region.startSample) /
                               safeSampleRate)
           << " - " << formatTimestamp(
                            static_cast<double>(comparison.region.endSample) /
                            safeSampleRate)
           << "\n\n"
           << "Algorithm                 Score       Status\n"
           << "-----------------------------------------\n";
    output << std::fixed << std::setprecision(1);
    for (const auto& row : comparison.rows) {
        output << std::left << std::setw(27) << row.algorithmName
               << std::right << std::setw(6) << row.score << "%   "
               << (row.selected ? "SELECTED" : "") << "\n";
        if (includeMetrics) {
            output << "  id=" << row.id << " rank=" << row.rank
                   << " mse=" << std::setprecision(5) << row.metrics.mse
                   << " snr=" << std::setprecision(2) << row.metrics.snrDb
                   << "dB corr=" << row.metrics.correlation
                   << " rms=" << row.metrics.rmsError
                   << " spectral=" << row.metrics.spectralError << "\n";
            output << std::setprecision(1);
        }
    }
    output << "\nWhy the current winner?\n"
           << comparison.winnerReason << "\n";
    return output.str();
}

std::string HypothesisComparator::explainMetric(
    const ReconstructionHypothesis& hypothesis,
    const ReconstructionHypothesis& winner,
    const ReconstructionHypothesis& weakest) {
    std::ostringstream output;
    output << hypothesis.algorithmName << " scored "
           << std::fixed << std::setprecision(2) << hypothesis.qualityScore
           << "/100";
    if (hypothesis.id == winner.id) {
        output << " and ranked first";
    } else if (hypothesis.id == weakest.id) {
        output << " and ranked last";
    }
    if (hypothesis.metrics.correlation >= winner.metrics.correlation - 0.001) {
        output << "; strongest waveform correlation";
    }
    if (hypothesis.metrics.rmsError <= winner.metrics.rmsError + 0.001) {
        output << "; lowest local RMS error";
    }
    if (hypothesis.metrics.spectralError <= winner.metrics.spectralError + 0.001) {
        output << "; closest spectral profile";
    }
    if (hypothesis.metrics.snrDb >= winner.metrics.snrDb - 0.001) {
        output << "; highest measured SNR";
    }
    return output.str();
}

} // namespace walkman::time_machine