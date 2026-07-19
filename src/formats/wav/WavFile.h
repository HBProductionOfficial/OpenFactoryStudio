#pragma once

#include "formats/FormatsExport.h"
#include "formats/util/Bytes.h"
#include "formats/util/Result.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

namespace OFS::Formats {

inline constexpr std::uint16_t kWavFormatPcm = 1;
inline constexpr std::uint16_t kWavFormatFloat = 3;
inline constexpr std::uint16_t kWavFormatExtensible = 0xFFFE;

struct OFS_FORMATS_EXPORT WavFormat
{
    std::uint16_t formatTag = kWavFormatPcm;
    std::uint16_t channels = 1;
    std::uint32_t sampleRate = 22050;
    std::uint32_t byteRate = 44100;
    std::uint16_t blockAlign = 2;
    std::uint16_t bitsPerSample = 16;
    Bytes extension;

    // formatTag, or the sub-format of WAVE_FORMAT_EXTENSIBLE.
    std::uint16_t sampleFormat() const noexcept;
};

struct WavChunk
{
    std::array<char, 4> id{};
    Bytes data;

    std::string_view name() const noexcept { return std::string_view(id.data(), id.size()); }
};

struct WavParseOptions
{
    // Off by default because game sound files often have a wrong RIFF size or broken chunks after
    // the sample data.
    bool strict = false;
};

class OFS_FORMATS_EXPORT WavFile
{
public:
    WavFormat format;
    Bytes samples;
    // In file order. The "fmt " and "data" entries are placeholders filled on write.
    std::vector<WavChunk> chunks;

    static Result<WavFile> parse(ByteSpan data, const WavParseOptions& options = {});
    static Result<WavFile> load(const std::filesystem::path& path, const WavParseOptions& options = {});
    Bytes write() const;
    Status save(const std::filesystem::path& path) const;

    static Result<WavFile> fromSamples(const std::vector<double>& interleaved, std::uint16_t channels,
                                       std::uint32_t sampleRate, std::uint16_t bitsPerSample,
                                       bool floatingPoint = false);

    bool isSupported() const noexcept;
    std::size_t frameCount() const noexcept;
    double sample(std::size_t frame, std::uint16_t channel) const noexcept;
    std::vector<double> toDoubles() const;
    double durationSeconds() const noexcept;
};

} // namespace OFS::Formats
