#pragma once

#include "formats/audio/SampleBuffer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace OFS::Formats::Test {

inline constexpr double kPi = 3.14159265358979323846;

// Channel c gets `frequency * (c + 1)` so channels can be told apart.
inline SampleBuffer sine(SampleType type, std::uint16_t channels, std::uint32_t sampleRate, std::size_t frames,
                         double frequency, double amplitude = 0.5)
{
    SampleBuffer buffer(type, channels, sampleRate, frames);
    for (std::size_t f = 0; f < frames; ++f) {
        for (std::uint16_t c = 0; c < channels; ++c) {
            const double t = static_cast<double>(f) / sampleRate;
            buffer.setSample(f, c, static_cast<float>(amplitude * std::sin(2.0 * kPi * frequency * (c + 1) * t)));
        }
    }
    return buffer;
}

// A 64-bit LCG, so the noise is the same everywhere.
inline SampleBuffer noise(SampleType type, std::uint16_t channels, std::uint32_t sampleRate, std::size_t frames,
                          std::uint64_t seed, double amplitude = 0.9)
{
    SampleBuffer buffer(type, channels, sampleRate, frames);
    std::uint64_t state = seed * 2862933555777941757ull + 3037000493ull;
    for (std::size_t f = 0; f < frames; ++f) {
        for (std::uint16_t c = 0; c < channels; ++c) {
            state = state * 6364136223846793005ull + 1442695040888963407ull;
            const double unit = static_cast<double>(state >> 11) / 9007199254740992.0;
            buffer.setSample(f, c, static_cast<float>(amplitude * (2.0 * unit - 1.0)));
        }
    }
    return buffer;
}

inline SampleBuffer int16Buffer(std::initializer_list<std::int16_t> samples, std::uint16_t channels = 1,
                                std::uint32_t sampleRate = 22050)
{
    return SampleBuffer::fromInt16(std::vector<std::int16_t>(samples), channels, sampleRate);
}

inline SampleBuffer floatBuffer(std::initializer_list<float> samples, std::uint16_t channels = 1,
                                std::uint32_t sampleRate = 22050)
{
    return SampleBuffer::fromFloat(std::vector<float>(samples), channels, sampleRate);
}

inline std::vector<std::int16_t> int16Values(const SampleBuffer& buffer)
{
    return std::vector<std::int16_t>(buffer.int16Samples().begin(), buffer.int16Samples().end());
}

inline std::vector<float> floatValues(const SampleBuffer& buffer)
{
    return std::vector<float>(buffer.floatSamples().begin(), buffer.floatSamples().end());
}

// Amplitude of one frequency from a single DFT bin (Goertzel).
inline double toneLevel(const SampleBuffer& buffer, std::uint16_t channel, double frequency, std::size_t first,
                        std::size_t count)
{
    double re = 0.0;
    double im = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double x = buffer.sample(first + i, channel);
        const double phase = 2.0 * kPi * frequency * static_cast<double>(first + i) / buffer.sampleRate();
        re += x * std::cos(phase);
        im += x * std::sin(phase);
    }
    return 2.0 * std::sqrt(re * re + im * im) / static_cast<double>(count);
}

inline double rms(const SampleBuffer& buffer, std::uint16_t channel, std::size_t first, std::size_t count)
{
    double sum = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double x = buffer.sample(first + i, channel);
        sum += x * x;
    }
    return count ? std::sqrt(sum / static_cast<double>(count)) : 0.0;
}

inline double maxDifference(const SampleBuffer& a, const SampleBuffer& b, std::size_t first, std::size_t count)
{
    double worst = 0.0;
    for (std::size_t f = first; f < first + count; ++f) {
        for (std::uint16_t c = 0; c < a.channels(); ++c)
            worst = std::max(worst, std::fabs(static_cast<double>(a.sample(f, c)) - b.sample(f, c)));
    }
    return worst;
}

} // namespace OFS::Formats::Test
