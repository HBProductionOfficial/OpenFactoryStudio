#pragma once

#include "formats/audio/AudioExport.h"
#include "formats/util/Result.h"
#include "formats/wav/WavFile.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace OFS::Formats {

enum class SampleType : std::uint8_t
{
    Int16,
    Float32,
};

// Functions that take a range clip it to the buffer, so a count past the end means "to the end".
struct SampleRange
{
    std::size_t start = 0;
    std::size_t count = 0;

    constexpr bool empty() const noexcept { return count == 0; }
    constexpr std::size_t end() const noexcept
    {
        return count > std::numeric_limits<std::size_t>::max() - start ? std::numeric_limits<std::size_t>::max()
                                                                       : start + count;
    }
    static constexpr SampleRange between(std::size_t a, std::size_t b) noexcept
    {
        return a <= b ? SampleRange{a, b - a} : SampleRange{b, a - b};
    }
    constexpr SampleRange clipped(std::size_t frames) const noexcept
    {
        const std::size_t first = start < frames ? start : frames;
        const std::size_t room = frames - first;
        return SampleRange{first, count < room ? count : room};
    }

    friend bool operator==(const SampleRange&, const SampleRange&) = default;
};

inline constexpr SampleRange kAllFrames{0, std::numeric_limits<std::size_t>::max()};

// Uses the WAV scale of 2^(bits - 1), so Int16 -> float -> Int16 is lossless.
inline float int16ToFloat(std::int16_t sample) noexcept
{
    return static_cast<float>(sample) * (1.0f / 32768.0f);
}

inline std::int16_t floatToInt16(float value) noexcept
{
    if (std::isnan(value))
        return 0;
    const float scaled = value * 32768.0f;
    if (scaled <= -32768.0f)
        return -32768;
    if (scaled >= 32767.0f)
        return 32767;
    return static_cast<std::int16_t>(std::lround(scaled));
}

struct WavSampleFormat
{
    std::uint16_t bitsPerSample = 16;
    bool floatingPoint = false;
};

// Interleaved PCM audio with at least one channel.
class OFS_AUDIO_EXPORT SampleBuffer
{
public:
    SampleBuffer() = default;
    SampleBuffer(SampleType type, std::uint16_t channels, std::uint32_t sampleRate, std::size_t frames = 0);

    static SampleBuffer fromInt16(std::vector<std::int16_t> samples, std::uint16_t channels, std::uint32_t sampleRate);
    static SampleBuffer fromFloat(std::vector<float> samples, std::uint16_t channels, std::uint32_t sampleRate);

    // Compressed WAV files go through decodeAudio() instead.
    static Result<SampleBuffer> fromWav(const WavFile& wav, std::optional<SampleType> type = std::nullopt);
    Result<WavFile> toWav(WavSampleFormat format = {}) const;

    SampleType type() const noexcept { return m_type; }
    std::uint16_t channels() const noexcept { return m_channels; }
    std::uint32_t sampleRate() const noexcept { return m_sampleRate; }
    // Does not convert the samples. Use resample() for that.
    void setSampleRate(std::uint32_t sampleRate) noexcept { m_sampleRate = sampleRate; }

    std::size_t frameCount() const noexcept;
    std::size_t sampleCount() const noexcept;
    bool empty() const noexcept { return frameCount() == 0; }
    double durationSeconds() const noexcept;
    std::size_t byteSize() const noexcept;

    std::span<std::int16_t> int16Samples() noexcept { return m_int16; }
    std::span<const std::int16_t> int16Samples() const noexcept { return m_int16; }
    std::span<float> floatSamples() noexcept { return m_float; }
    std::span<const float> floatSamples() const noexcept { return m_float; }

    float sample(std::size_t frame, std::uint16_t channel) const noexcept;
    void setSample(std::size_t frame, std::uint16_t channel, float value) noexcept;

    void resize(std::size_t frames);
    bool sameFormat(const SampleBuffer& other) const noexcept;

    friend bool operator==(const SampleBuffer&, const SampleBuffer&) = default;

private:
    SampleType m_type = SampleType::Int16;
    std::uint16_t m_channels = 1;
    std::uint32_t m_sampleRate = 22050;
    std::vector<std::int16_t> m_int16;
    std::vector<float> m_float;
};

OFS_AUDIO_EXPORT SampleBuffer convertSampleType(const SampleBuffer& buffer, SampleType type);

// Mono to N copies the channel, N to mono averages, anything else keeps the first channels.
OFS_AUDIO_EXPORT SampleBuffer convertChannels(const SampleBuffer& buffer, std::uint16_t channels);

inline SampleBuffer toMono(const SampleBuffer& buffer)
{
    return convertChannels(buffer, 1);
}

inline SampleBuffer toStereo(const SampleBuffer& buffer)
{
    return convertChannels(buffer, 2);
}

// Rounds samples to what an N-bit PCM file holds, so you hear what a save at that depth gives.
OFS_AUDIO_EXPORT Result<SampleBuffer> quantize(const SampleBuffer& buffer, std::uint16_t bitsPerSample);

} // namespace OFS::Formats
