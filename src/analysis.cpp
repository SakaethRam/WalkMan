#include "walkman/analysis.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace walkman {
namespace {

void validateRange(const AudioData& audio, std::size_t begin, std::size_t end) {
    if (!audio.valid() || begin > end || end > audio.frameCount()) {
        throw std::out_of_range("Invalid audio analysis range");
    }
}

} // namespace

double rms(const AudioData& audio, std::size_t begin, std::size_t end) {
    validateRange(audio, begin, end);
    if (begin == end) {
        return 0.0;
    }
    long double sum = 0.0;
    const auto count = static_cast<long double>((end - begin) * audio.channels);
    for (std::size_t frame = begin; frame < end; ++frame) {
        for (std::size_t channel = 0; channel < audio.channels; ++channel) {
            const double value = audio.sample(frame, channel);
            sum += static_cast<long double>(value * value);
        }
    }
    return std::sqrt(static_cast<double>(sum / count));
}

double peakAmplitude(const AudioData& audio, std::size_t begin, std::size_t end) {
    validateRange(audio, begin, end);
    double peak = 0.0;
    for (std::size_t frame = begin; frame < end; ++frame) {
        for (std::size_t channel = 0; channel < audio.channels; ++channel) {
            peak = std::max(peak, std::abs(audio.sample(frame, channel)));
        }
    }
    return peak;
}

double zeroCrossingRate(const AudioData& audio, std::size_t begin, std::size_t end) {
    validateRange(audio, begin, end);
    if (end - begin < 2) {
        return 0.0;
    }
    std::size_t crossings = 0;
    std::size_t comparisons = 0;
    for (std::size_t frame = begin + 1; frame < end; ++frame) {
        for (std::size_t channel = 0; channel < audio.channels; ++channel) {
            const double previous = audio.sample(frame - 1, channel);
            const double current = audio.sample(frame, channel);
            if ((previous < 0.0 && current >= 0.0) ||
                (previous >= 0.0 && current < 0.0)) {
                ++crossings;
            }
            ++comparisons;
        }
    }
    return comparisons == 0
               ? 0.0
               : static_cast<double>(crossings) /
                     static_cast<double>(comparisons);
}

double meanAbsoluteDifference(const AudioData& audio, std::size_t begin,
                              std::size_t end) {
    validateRange(audio, begin, end);
    if (end - begin < 2) {
        return 0.0;
    }
    long double sum = 0.0;
    const auto count = static_cast<long double>((end - begin - 1) * audio.channels);
    for (std::size_t frame = begin + 1; frame < end; ++frame) {
        for (std::size_t channel = 0; channel < audio.channels; ++channel) {
            sum += std::abs(audio.sample(frame, channel) -
                            audio.sample(frame - 1, channel));
        }
    }
    return static_cast<double>(sum / count);
}

double correlation(const std::vector<double>& lhs,
                   const std::vector<double>& rhs) {
    if (lhs.size() != rhs.size() || lhs.empty()) {
        return 0.0;
    }
    const double lhsMean =
        std::accumulate(lhs.begin(), lhs.end(), 0.0) / static_cast<double>(lhs.size());
    const double rhsMean =
        std::accumulate(rhs.begin(), rhs.end(), 0.0) / static_cast<double>(rhs.size());
    long double numerator = 0.0;
    long double lhsEnergy = 0.0;
    long double rhsEnergy = 0.0;
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        const double a = lhs[i] - lhsMean;
        const double b = rhs[i] - rhsMean;
        numerator += static_cast<long double>(a * b);
        lhsEnergy += static_cast<long double>(a * a);
        rhsEnergy += static_cast<long double>(b * b);
    }
    const double denominator = std::sqrt(static_cast<double>(lhsEnergy * rhsEnergy));
    return denominator < 1e-12 ? 0.0 : std::clamp(static_cast<double>(numerator) / denominator, -1.0, 1.0);
}

double mse(const std::vector<double>& lhs, const std::vector<double>& rhs) {
    if (lhs.size() != rhs.size() || lhs.empty()) {
        return std::numeric_limits<double>::infinity();
    }
    long double error = 0.0;
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        const double diff = lhs[i] - rhs[i];
        error += static_cast<long double>(diff * diff);
    }
    return static_cast<double>(error / static_cast<long double>(lhs.size()));
}

double rmsError(const std::vector<double>& lhs, const std::vector<double>& rhs) {
    return std::sqrt(mse(lhs, rhs));
}

double snrDb(const std::vector<double>& reference,
             const std::vector<double>& candidate) {
    if (reference.size() != candidate.size() || reference.empty()) {
        return -std::numeric_limits<double>::infinity();
    }
    long double signal = 0.0;
    long double noise = 0.0;
    for (std::size_t i = 0; i < reference.size(); ++i) {
        signal += static_cast<long double>(reference[i] * reference[i]);
        const double diff = reference[i] - candidate[i];
        noise += static_cast<long double>(diff * diff);
    }
    if (noise < 1e-18L) {
        return 120.0;
    }
    return 10.0 * std::log10(static_cast<double>(signal / noise));
}

} // namespace walkman