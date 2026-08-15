#include "walkman/repair.hpp"

#include "walkman/analysis.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <numbers>
#include <numeric>

namespace walkman {
namespace {

std::size_t regionFrames(const AudioData& audio, const AudioRegion& region) {
    if (!audio.valid() || region.startSample >= region.endSample ||
        region.endSample > audio.frameCount()) {
        return 0;
    }
    return region.endSample - region.startSample;
}

std::vector<double> linearRepair(const AudioData& audio, const AudioRegion& region) {
    const std::size_t length = regionFrames(audio, region);
    std::vector<double> output(length * audio.channels, 0.0);
    if (length == 0) {
        return output;
    }
    const std::size_t leftFrame = region.startSample > 0
                                      ? region.startSample - 1
                                      : region.endSample;
    const std::size_t rightFrame = region.endSample < audio.frameCount()
                                       ? region.endSample
                                       : region.startSample > 0
                                             ? region.startSample - 1
                                             : region.startSample;
    for (std::size_t i = 0; i < length; ++i) {
        const double ratio = static_cast<double>(i + 1) /
                             static_cast<double>(length + 1);
        for (std::size_t channel = 0; channel < audio.channels; ++channel) {
            const double left = audio.sample(leftFrame, channel);
            const double right = audio.sample(rightFrame, channel);
            output[i * audio.channels + channel] = left + ratio * (right - left);
        }
    }
    return output;
}

double catmullRom(double p0, double p1, double p2, double p3, double t) {
    return 0.5 * ((2.0 * p1) + (-p0 + p2) * t +
                  (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t * t +
                  (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t * t * t);
}

std::size_t nextPowerOfTwo(std::size_t value) {
    std::size_t result = 1;
    while (result < value) {
        result *= 2;
    }
    return result;
}

void fft(std::vector<std::complex<double>>& values, bool inverse) {
    const std::size_t n = values.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1U;
        for (; (j & bit) != 0U; bit >>= 1U) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(values[i], values[j]);
        }
    }
    for (std::size_t len = 2; len <= n; len *= 2U) {
        const double angle = 2.0 * std::numbers::pi / static_cast<double>(len) *
                             (inverse ? 1.0 : -1.0);
        const std::complex<double> wLen(std::cos(angle), std::sin(angle));
        for (std::size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (std::size_t j = 0; j < len / 2U; ++j) {
                const auto even = values[i + j];
                const auto odd = values[i + j + len / 2U] * w;
                values[i + j] = even + odd;
                values[i + j + len / 2U] = even - odd;
                w *= wLen;
            }
        }
    }
    if (inverse) {
        for (auto& value : values) {
            value /= static_cast<double>(n);
        }
    }
}

} // namespace

std::string LinearRepair::name() const { return "LINEAR"; }

std::vector<double> LinearRepair::repair(const AudioData& audio,
                                         const AudioRegion& region) const {
    return linearRepair(audio, region);
}

std::string SplineRepair::name() const { return "SPLINE"; }

std::vector<double> SplineRepair::repair(const AudioData& audio,
                                         const AudioRegion& region) const {
    const std::size_t length = regionFrames(audio, region);
    if (length == 0 || region.startSample == 0 ||
        region.endSample >= audio.frameCount()) {
        return linearRepair(audio, region);
    }
    std::vector<double> output(length * audio.channels);
    const std::size_t p0Frame = region.startSample >= 2 ? region.startSample - 2 : 0;
    const std::size_t p1Frame = region.startSample - 1;
    const std::size_t p2Frame = region.endSample;
    const std::size_t p3Frame = std::min(audio.frameCount() - 1, region.endSample + 1);
    for (std::size_t i = 0; i < length; ++i) {
        const double t = static_cast<double>(i + 1) /
                         static_cast<double>(length + 1);
        for (std::size_t channel = 0; channel < audio.channels; ++channel) {
            output[i * audio.channels + channel] = std::clamp(
                catmullRom(audio.sample(p0Frame, channel), audio.sample(p1Frame, channel),
                           audio.sample(p2Frame, channel), audio.sample(p3Frame, channel), t),
                -1.0, 1.0);
        }
    }
    return output;
}

std::string WaveformMatchRepair::name() const { return "WAVEFORM_MATCH"; }

std::vector<double> WaveformMatchRepair::repair(const AudioData& audio,
                                                const AudioRegion& region) const {
    const std::size_t length = regionFrames(audio, region);
    const std::size_t context = std::min<std::size_t>(64, region.startSample);
    if (length == 0 || context < 8 || audio.frameCount() < length + context * 2U) {
        return linearRepair(audio, region);
    }

    const std::size_t targetContextStart = region.startSample - context;
    double bestScore = -std::numeric_limits<double>::infinity();
    std::size_t bestStart = 0;
    for (std::size_t candidateStart = context;
         candidateStart + length + context < audio.frameCount(); ++candidateStart) {
        const std::size_t candidateEnd = candidateStart + length;
        if (candidateEnd > region.startSample && candidateStart < region.endSample) {
            continue;
        }
        std::vector<double> target;
        std::vector<double> candidate;
        target.reserve(context * audio.channels);
        candidate.reserve(context * audio.channels);
        for (std::size_t i = 0; i < context; ++i) {
            for (std::size_t channel = 0; channel < audio.channels; ++channel) {
                target.push_back(audio.sample(targetContextStart + i, channel));
                candidate.push_back(audio.sample(candidateStart - context + i, channel));
            }
        }
        const double score = correlation(target, candidate);
        if (score > bestScore) {
            bestScore = score;
            bestStart = candidateStart;
        }
    }
    if (!std::isfinite(bestScore) || bestScore < 0.25) {
        return linearRepair(audio, region);
    }

    std::vector<double> output(length * audio.channels);
    for (std::size_t i = 0; i < length; ++i) {
        for (std::size_t channel = 0; channel < audio.channels; ++channel) {
            output[i * audio.channels + channel] =
                audio.sample(bestStart + i, channel);
        }
    }
    return output;
}

std::string SpectralRepair::name() const { return "SPECTRAL"; }

std::vector<double> SpectralRepair::repair(const AudioData& audio,
                                           const AudioRegion& region) const {
    const std::size_t length = regionFrames(audio, region);
    if (length < 8 || region.startSample < 32) {
        return linearRepair(audio, region);
    }
    const std::size_t context = std::min<std::size_t>(256, region.startSample);
    const std::size_t nfft = nextPowerOfTwo(context);
    std::vector<double> output(length * audio.channels, 0.0);
    const auto linear = linearRepair(audio, region);

    for (std::size_t channel = 0; channel < audio.channels; ++channel) {
        std::vector<std::complex<double>> spectrum(nfft, {0.0, 0.0});
        for (std::size_t i = 0; i < context; ++i) {
            const double window = 0.5 - 0.5 * std::cos(
                                            2.0 * std::numbers::pi *
                                            static_cast<double>(i) /
                                            static_cast<double>(context - 1));
            spectrum[i] = audio.sample(region.startSample - context + i, channel) * window;
        }
        fft(spectrum, false);

        std::vector<std::size_t> bins(nfft / 2U);
        std::iota(bins.begin(), bins.end(), 1U);
        std::sort(bins.begin(), bins.end(), [&](std::size_t lhs, std::size_t rhs) {
            return std::abs(spectrum[lhs]) > std::abs(spectrum[rhs]);
        });
        const std::size_t componentCount = std::min<std::size_t>(8, bins.size());
        const double targetRms = rms(audio, region.startSample - context, region.startSample);
        std::vector<double> spectral(length);
        for (std::size_t i = 0; i < length; ++i) {
            double value = 0.0;
            for (std::size_t component = 0; component < componentCount; ++component) {
                const std::size_t bin = bins[component];
                const double amplitude = 2.0 * std::abs(spectrum[bin]) /
                                         static_cast<double>(nfft);
                const double phase = std::arg(spectrum[bin]);
                const double phaseAtSample =
                    phase + 2.0 * std::numbers::pi * static_cast<double>(bin) *
                                static_cast<double>(context + i) /
                                static_cast<double>(nfft);
                value += amplitude * std::cos(phaseAtSample);
            }
            spectral[i] = value;
        }
        const double spectralRms = std::sqrt(std::accumulate(
            spectral.begin(), spectral.end(), 0.0,
            [](double sum, double value) { return sum + value * value; }) /
                                             static_cast<double>(spectral.size()));
        const double scale = spectralRms > 1e-8 ? targetRms / spectralRms : 1.0;
        const std::size_t fade = std::min<std::size_t>(32, std::max<std::size_t>(1, length / 3));
        for (std::size_t i = 0; i < length; ++i) {
            const double edge = static_cast<double>(
                std::min({i + 1, length - i, fade})) /
                                static_cast<double>(fade);
            const double blend = std::clamp(edge, 0.0, 1.0);
            output[i * audio.channels + channel] = std::clamp(
                (1.0 - blend) * linear[i * audio.channels + channel] +
                    blend * spectral[i] * scale,
                -1.0, 1.0);
        }
    }
    return output;
}

std::vector<std::unique_ptr<IRepairAlgorithm>> makeRepairAlgorithms() {
    std::vector<std::unique_ptr<IRepairAlgorithm>> algorithms;
    algorithms.push_back(std::make_unique<LinearRepair>());
    algorithms.push_back(std::make_unique<SplineRepair>());
    algorithms.push_back(std::make_unique<WaveformMatchRepair>());
    algorithms.push_back(std::make_unique<SpectralRepair>());
    return algorithms;
}

} // namespace walkman