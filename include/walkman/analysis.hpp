#pragma once

#include "walkman/types.hpp"

#include <cstddef>
#include <vector>

namespace walkman {

double rms(const AudioData& audio, std::size_t begin, std::size_t end);
double peakAmplitude(const AudioData& audio, std::size_t begin, std::size_t end);
double zeroCrossingRate(const AudioData& audio, std::size_t begin, std::size_t end);
double meanAbsoluteDifference(const AudioData& audio, std::size_t begin, std::size_t end);
double correlation(const std::vector<double>& lhs, const std::vector<double>& rhs);
double rmsError(const std::vector<double>& lhs, const std::vector<double>& rhs);
double mse(const std::vector<double>& lhs, const std::vector<double>& rhs);
double snrDb(const std::vector<double>& reference, const std::vector<double>& candidate);

} // namespace walkman