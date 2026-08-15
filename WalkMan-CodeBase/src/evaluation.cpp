#include "walkman/evaluation.hpp"

#include "walkman/analysis.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace walkman {
namespace {

std::vector<double> flatten(const AudioData& audio, std::size_t begin,
                            std::size_t end) {
    std::vector<double> values;
    if (!audio.valid() || begin > end || end > audio.frameCount()) {
        return values;
    }
    values.reserve((end - begin) * audio.channels);
    for (std::size_t frame = begin; frame < end; ++frame) {
        for (std::size_t channel = 0; channel < audio.channels; ++channel) {
            values.push_back(audio.sample(frame, channel));
        }
    }
    return values;
}

double spectralDistance(const std::vector<double>& lhs,
                        const std::vector<double>& rhs) {
    if (lhs.size() != rhs.size() || lhs.empty()) {
        return 1.0;
    }
    const std::size_t bins = std::min<std::size_t>(32, lhs.size() / 2U);
    if (bins == 0) {
        return 0.0;
    }
    long double total = 0.0;
    for (std::size_t bin = 1; bin < bins; ++bin) {
        std::complex<double> left(0.0, 0.0);
        std::complex<double> right(0.0, 0.0);
        for (std::size_t i = 0; i < lhs.size(); ++i) {
            const double angle = -2.0 * std::numbers::pi *
                                 static_cast<double>(bin * i) /
                                 static_cast<double>(lhs.size());
            const std::complex<double> rotation(std::cos(angle), std::sin(angle));
            left += lhs[i] * rotation;
            right += rhs[i] * rotation;
        }
        total += std::abs(std::abs(left) - std::abs(right));
    }
    return static_cast<double>(total / static_cast<long double>(bins - 1U));
}

double qualityScore(const QualityMetrics& metrics) {
    const double snrScore = std::clamp((metrics.snrDb + 5.0) / 35.0, 0.0, 1.0);
    const double corrScore = std::clamp((metrics.correlation + 1.0) / 2.0, 0.0, 1.0);
    const double rmsScore = std::clamp(1.0 - metrics.rmsError, 0.0, 1.0);
    const double spectralScore = std::clamp(1.0 - metrics.spectralError, 0.0, 1.0);
    return 100.0 * (0.42 * snrScore + 0.28 * corrScore +
                    0.18 * rmsScore + 0.12 * spectralScore);
}

} // namespace

QualityMetrics evaluateQuality(const AudioData& reference,
                               const AudioData& candidate) {
    if (!reference.valid() || !candidate.valid() ||
        reference.sampleRate != candidate.sampleRate ||
        reference.channels != candidate.channels ||
        reference.samples.size() != candidate.samples.size()) {
        throw std::invalid_argument("Quality comparison requires matching audio shapes");
    }
    QualityMetrics metrics;
    metrics.mse = mse(reference.samples, candidate.samples);
    metrics.snrDb = snrDb(reference.samples, candidate.samples);
    metrics.correlation = correlation(reference.samples, candidate.samples);
    metrics.rmsError = rmsError(reference.samples, candidate.samples);
    metrics.spectralError = spectralDistance(reference.samples, candidate.samples);
    return metrics;
}

RepairCandidate CandidateEvaluator::evaluate(
    const AudioData& damaged, const AudioData* cleanReference,
    const AudioRegion& region, std::string algorithmName,
    std::vector<double> repairedSamples) const {
    RepairCandidate candidate{std::move(algorithmName), region.startSample,
                              region.endSample, std::move(repairedSamples), 0.0, {}};
    const std::size_t length = region.length();
    const std::size_t expected = length * damaged.channels;
    if (length == 0 || candidate.repairedSamples.size() != expected) {
        candidate.score = 0.0;
        return candidate;
    }

    std::vector<double> referenceSamples;
    std::vector<double> baseline;
    if (cleanReference != nullptr) {
        referenceSamples = flatten(*cleanReference, region.startSample, region.endSample);
        candidate.metrics.mse = mse(referenceSamples, candidate.repairedSamples);
        candidate.metrics.snrDb = snrDb(referenceSamples, candidate.repairedSamples);
        candidate.metrics.correlation =
            correlation(referenceSamples, candidate.repairedSamples);
        candidate.metrics.rmsError =
            rmsError(referenceSamples, candidate.repairedSamples);
        candidate.metrics.spectralError =
            spectralDistance(referenceSamples, candidate.repairedSamples);
        candidate.score = qualityScore(candidate.metrics);
        return candidate;
    }

    const std::size_t leftStart = region.startSample > length
                                      ? region.startSample - length
                                      : 0;
    const std::size_t rightEnd =
        std::min(damaged.frameCount(), region.endSample + length);
    const auto left = flatten(damaged, leftStart, region.startSample);
    const auto right = flatten(damaged, region.endSample, rightEnd);
    const double leftRms = left.empty() ? 0.0 : std::sqrt(mse(left, std::vector<double>(left.size(), 0.0)));
    const double rightRms = right.empty() ? leftRms : std::sqrt(mse(right, std::vector<double>(right.size(), 0.0)));

    baseline.resize(candidate.repairedSamples.size());
    for (std::size_t i = 0; i < length; ++i) {
        const double t = static_cast<double>(i + 1) / static_cast<double>(length + 1);
        for (std::size_t channel = 0; channel < damaged.channels; ++channel) {
            const double leftValue = region.startSample > 0
                                         ? damaged.sample(region.startSample - 1, channel)
                                         : damaged.sample(region.endSample, channel);
            const double rightValue = region.endSample < damaged.frameCount()
                                          ? damaged.sample(region.endSample, channel)
                                          : leftValue;
            baseline[i * damaged.channels + channel] =
                leftValue + t * (rightValue - leftValue);
        }
    }
    candidate.metrics.mse = mse(baseline, candidate.repairedSamples);
    candidate.metrics.snrDb = 10.0 * std::log10(
        std::max(1e-12, (leftRms * leftRms + rightRms * rightRms) /
                             std::max(1e-12, candidate.metrics.mse)));
    candidate.metrics.correlation = correlation(baseline, candidate.repairedSamples);
    candidate.metrics.rmsError = std::abs(
        std::sqrt(mse(candidate.repairedSamples,
                       std::vector<double>(candidate.repairedSamples.size(), 0.0))) -
        (leftRms + rightRms) / 2.0);
    candidate.metrics.spectralError = spectralDistance(baseline, candidate.repairedSamples);
    candidate.score = qualityScore(candidate.metrics);
    return candidate;
}

} // namespace walkman