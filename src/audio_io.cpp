#include "walkman/audio_io.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace walkman {
namespace {

std::uint16_t readU16(const std::vector<std::byte>& data, std::size_t offset) {
    if (offset + 2 > data.size()) {
        throw std::runtime_error("Malformed WAV: truncated 16-bit field");
    }
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(data[offset]) |
        (static_cast<std::uint16_t>(data[offset + 1]) << 8U));
}

std::uint32_t readU32(const std::vector<std::byte>& data, std::size_t offset) {
    if (offset + 4 > data.size()) {
        throw std::runtime_error("Malformed WAV: truncated 32-bit field");
    }
    return static_cast<std::uint32_t>(
        static_cast<std::uint32_t>(data[offset]) |
        (static_cast<std::uint32_t>(data[offset + 1]) << 8U) |
        (static_cast<std::uint32_t>(data[offset + 2]) << 16U) |
        (static_cast<std::uint32_t>(data[offset + 3]) << 24U));
}

std::int32_t readSigned24(const std::vector<std::byte>& data, std::size_t offset) {
    const std::uint32_t raw = static_cast<std::uint32_t>(data[offset]) |
        (static_cast<std::uint32_t>(data[offset + 1]) << 8U) |
        (static_cast<std::uint32_t>(data[offset + 2]) << 16U);
    if ((raw & 0x00800000U) != 0U) {
        return static_cast<std::int32_t>(raw | 0xFF000000U);
    }
    return static_cast<std::int32_t>(raw);
}

void requireBytes(const std::vector<std::byte>& data, std::size_t offset,
                  std::size_t count, const std::string& message) {
    if (offset > data.size() || count > data.size() - offset) {
        throw std::runtime_error("Malformed WAV: " + message);
    }
}

double decodeSample(const std::vector<std::byte>& data, std::size_t offset,
                    std::uint16_t format, std::uint16_t bits) {
    if (format == 1U) {
        switch (bits) {
        case 8:
            return (static_cast<int>(data[offset]) - 128) / 128.0;
        case 16:
            return static_cast<double>(
                       static_cast<std::int16_t>(readU16(data, offset))) /
                   32768.0;
        case 24:
            return static_cast<double>(readSigned24(data, offset)) / 8388608.0;
        case 32:
            return static_cast<double>(
                       static_cast<std::int32_t>(readU32(data, offset))) /
                   2147483648.0;
        default:
            throw std::runtime_error("Unsupported PCM bit depth");
        }
    }
    if (format == 3U && bits == 32U) {
        const std::uint32_t raw = readU32(data, offset);
        return static_cast<double>(std::bit_cast<float>(raw));
    }
    if (format == 3U && bits == 64U) {
        requireBytes(data, offset, 8, "truncated 64-bit float sample");
        std::uint64_t raw = 0;
        for (std::size_t i = 0; i < 8; ++i) {
            raw |= static_cast<std::uint64_t>(data[offset + i]) << (8U * i);
        }
        return std::bit_cast<double>(raw);
    }
    throw std::runtime_error("Unsupported WAV format; expected PCM or IEEE float");
}

void writeU16(std::ofstream& stream, std::uint16_t value) {
    stream.put(static_cast<char>(value & 0xFFU));
    stream.put(static_cast<char>((value >> 8U) & 0xFFU));
}

void writeU32(std::ofstream& stream, std::uint32_t value) {
    stream.put(static_cast<char>(value & 0xFFU));
    stream.put(static_cast<char>((value >> 8U) & 0xFFU));
    stream.put(static_cast<char>((value >> 16U) & 0xFFU));
    stream.put(static_cast<char>((value >> 24U) & 0xFFU));
}

} // namespace

AudioData loadWav(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Unable to open WAV input: " + path.string());
    }
    stream.seekg(0, std::ios::end);
    const auto size = stream.tellg();
    if (size < 12) {
        throw std::runtime_error("Malformed WAV: file is shorter than RIFF header");
    }
    stream.seekg(0, std::ios::beg);
    std::vector<std::byte> data(static_cast<std::size_t>(size));
    stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!stream) {
        throw std::runtime_error("Unable to read WAV input: " + path.string());
    }
    if (std::memcmp(data.data(), "RIFF", 4) != 0 ||
        std::memcmp(data.data() + 8, "WAVE", 4) != 0) {
        throw std::runtime_error("Malformed WAV: expected RIFF/WAVE container");
    }

    std::uint16_t format = 0;
    std::uint16_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint16_t bits = 0;
    std::size_t dataOffset = 0;
    std::size_t dataSize = 0;

    std::size_t offset = 12;
    while (offset + 8 <= data.size()) {
        const char* chunk = reinterpret_cast<const char*>(data.data() + offset);
        const std::uint32_t chunkSize = readU32(data, offset + 4);
        const std::size_t payload = offset + 8;
        requireBytes(data, payload, chunkSize, "chunk extends past end of file");
        if (std::memcmp(chunk, "fmt ", 4) == 0) {
            if (chunkSize < 16U) {
                throw std::runtime_error("Malformed WAV: fmt chunk is too short");
            }
            format = readU16(data, payload);
            channels = readU16(data, payload + 2);
            sampleRate = readU32(data, payload + 4);
            bits = readU16(data, payload + 14);
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            dataOffset = payload;
            dataSize = chunkSize;
        }
        offset = payload + chunkSize + (chunkSize % 2U);
    }

    if (format == 0 || channels == 0 || sampleRate == 0 || bits == 0 ||
        dataSize == 0) {
        throw std::runtime_error("Malformed WAV: missing fmt or data chunk");
    }
    const std::size_t bytesPerSample = (bits + 7U) / 8U;
    const std::size_t blockAlign = bytesPerSample * channels;
    if (blockAlign == 0 || dataSize % blockAlign != 0) {
        throw std::runtime_error("Malformed WAV: data is not aligned to complete frames");
    }

    AudioData audio;
    audio.sampleRate = sampleRate;
    audio.channels = channels;
    audio.samples.reserve(dataSize / bytesPerSample);
    for (std::size_t i = 0; i < dataSize; i += bytesPerSample) {
        double value = decodeSample(data, dataOffset + i, format, bits);
        if (!std::isfinite(value)) {
            throw std::runtime_error("WAV contains a non-finite sample");
        }
        audio.samples.push_back(std::clamp(value, -1.0, 1.0));
    }
    return audio;
}

void saveWav(const std::filesystem::path& path, const AudioData& audio) {
    if (!audio.valid()) {
        throw std::invalid_argument("Cannot write invalid audio");
    }
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }
    const std::size_t frameBytes = static_cast<std::size_t>(audio.channels) * 2U;
    const std::size_t dataBytes = audio.frameCount() * frameBytes;
    if (dataBytes > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("WAV output exceeds RIFF's 4 GiB limit");
    }
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Unable to open WAV output: " + path.string());
    }

    stream.write("RIFF", 4);
    writeU32(stream, static_cast<std::uint32_t>(36U + dataBytes));
    stream.write("WAVEfmt ", 8);
    writeU32(stream, 16);
    writeU16(stream, 1);
    writeU16(stream, audio.channels);
    writeU32(stream, audio.sampleRate);
    writeU32(stream, audio.sampleRate * static_cast<std::uint32_t>(frameBytes));
    writeU16(stream, static_cast<std::uint16_t>(frameBytes));
    writeU16(stream, 16);
    stream.write("data", 4);
    writeU32(stream, static_cast<std::uint32_t>(dataBytes));

    for (const double value : audio.samples) {
        const double clipped = std::clamp(value, -1.0, 1.0);
        const auto pcm = static_cast<std::int16_t>(
            std::lround(clipped * (clipped < 0.0 ? 32768.0 : 32767.0)));
        writeU16(stream, static_cast<std::uint16_t>(pcm));
    }
    if (!stream) {
        throw std::runtime_error("Failed while writing WAV output: " + path.string());
    }
}

} // namespace walkman