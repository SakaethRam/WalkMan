#pragma once

#include "walkman/types.hpp"

#include <filesystem>

namespace walkman {

// Standards-compliant RIFF/WAVE PCM reader. Supports PCM 8/16/24/32-bit and
// IEEE float 32/64-bit files, mono or interleaved stereo/multichannel audio.
AudioData loadWav(const std::filesystem::path& path);

// Writes normalized samples as 16-bit PCM while preserving rate and channels.
void saveWav(const std::filesystem::path& path, const AudioData& audio);

} // namespace walkman