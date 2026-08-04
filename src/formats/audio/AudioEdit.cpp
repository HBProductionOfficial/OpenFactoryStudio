#include "formats/audio/AudioEdit.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <type_traits>

namespace OFS::Formats {

namespace {

template <typename T>
std::span<const T> samplesOf(const SampleBuffer& buffer)
{
    if constexpr (std::is_same_v<T, float>)
        return buffer.floatSamples();
    else
        return buffer.int16Samples();
}

template <typename T>
std::span<T> samplesOf(SampleBuffer& buffer)
{
    if constexpr (std::is_same_v<T, float>)
        return buffer.floatSamples();
    else
        return buffer.int16Samples();
}

template <typename Fn>
decltype(auto) visitType(const SampleBuffer& buffer, Fn&& fn)
{
    if (buffer.type() == SampleType::Float32)
        return fn(float{});
    return fn(std::int16_t{});
}

// A null `source` means `range.count` frames of silence.
struct Piece
{
    const SampleBuffer* source = nullptr;
    SampleRange range;
};

SampleBuffer splice(const SampleBuffer& format, std::initializer_list<Piece> pieces)
{
    std::size_t total = 0;
    for (const Piece& piece : pieces)
        total += piece.range.count;
    SampleBuffer out(format.type(), format.channels(), format.sampleRate(), total);
    const std::size_t channels = format.channels();
    visitType(format, [&](auto tag) {
        using T = decltype(tag);
        T* dst = samplesOf<T>(out).data();
        for (const Piece& piece : pieces) {
            const std::size_t count = piece.range.count * channels;
            if (piece.source)
                std::copy_n(samplesOf<T>(*piece.source).data() + piece.range.start * channels, count, dst);
            dst += count; // silence: the new buffer is already zero
        }
    });
    return out;
}

template <typename Gain>
SampleBuffer applyGain(const SampleBuffer& buffer, SampleRange range, Gain gain)
{
    SampleBuffer out = buffer;
    const SampleRange r = range.clipped(buffer.frameCount());
    const std::size_t channels = buffer.channels();
    visitType(out, [&](auto tag) {
        using T = decltype(tag);
        T* s = samplesOf<T>(out).data() + r.start * channels;
        for (std::size_t i = 0; i < r.count; ++i) {
            const float g = gain(i, r.count);
            for (std::size_t c = 0; c < channels; ++c, ++s) {
                if constexpr (std::is_same_v<T, float>)
                    *s *= g;
                else
                    *s = floatToInt16(int16ToFloat(*s) * g);
            }
        }
    });
    return out;
}

} // namespace

SampleBuffer copyRange(const SampleBuffer& buffer, SampleRange range)
{
    return splice(buffer, {{&buffer, range.clipped(buffer.frameCount())}});
}

SampleBuffer removeRange(const SampleBuffer& buffer, SampleRange range)
{
    const std::size_t frames = buffer.frameCount();
    const SampleRange r = range.clipped(frames);
    return splice(buffer, {{&buffer, {0, r.start}}, {&buffer, {r.end(), frames - r.end()}}});
}

Result<SampleBuffer> conformTo(const SampleBuffer& clip, const SampleBuffer& like, ResampleQuality quality)
{
    SampleBuffer out = clip;
    if (out.sampleRate() != like.sampleRate()) {
        // Resampling works in floats, so convert first to keep full precision.
        out = convertSampleType(out, SampleType::Float32);
        out = convertChannels(out, like.channels());
        auto resampled = resample(out, like.sampleRate(), quality);
        if (!resampled)
            return resampled.error();
        out = std::move(*resampled);
    }
    out = convertChannels(out, like.channels());
    return convertSampleType(out, like.type());
}

Result<SampleBuffer> insertBuffer(const SampleBuffer& buffer, std::size_t position, const SampleBuffer& clip)
{
    return replaceRange(buffer, SampleRange{position, 0}, clip);
}

Result<SampleBuffer> replaceRange(const SampleBuffer& buffer, SampleRange range, const SampleBuffer& clip)
{
    auto conformed = conformTo(clip, buffer);
    if (!conformed)
        return conformed.error();
    const std::size_t frames = buffer.frameCount();
    const SampleRange r = range.clipped(frames);
    return splice(
        buffer,
        {{&buffer, {0, r.start}}, {&*conformed, {0, conformed->frameCount()}}, {&buffer, {r.end(), frames - r.end()}}});
}

SampleBuffer insertSilence(const SampleBuffer& buffer, std::size_t position, std::size_t frames)
{
    const std::size_t total = buffer.frameCount();
    const std::size_t at = std::min(position, total);
    return splice(buffer, {{&buffer, {0, at}}, {nullptr, {0, frames}}, {&buffer, {at, total - at}}});
}

SampleBuffer fillSilence(const SampleBuffer& buffer, SampleRange range)
{
    SampleBuffer out = buffer;
    const SampleRange r = range.clipped(buffer.frameCount());
    const std::size_t channels = buffer.channels();
    visitType(out, [&](auto tag) {
        using T = decltype(tag);
        std::fill_n(samplesOf<T>(out).data() + r.start * channels, r.count * channels, T{});
    });
    return out;
}

SampleBuffer fadeIn(const SampleBuffer& buffer, SampleRange range)
{
    return applyGain(buffer, range, [](std::size_t i, std::size_t n) {
        return n <= 1 ? 1.0f : static_cast<float>(static_cast<double>(i) / static_cast<double>(n - 1));
    });
}

SampleBuffer fadeOut(const SampleBuffer& buffer, SampleRange range)
{
    return applyGain(buffer, range, [](std::size_t i, std::size_t n) {
        return n <= 1 ? 0.0f : static_cast<float>(static_cast<double>(n - 1 - i) / static_cast<double>(n - 1));
    });
}

SampleBuffer changeVolume(const SampleBuffer& buffer, SampleRange range, float gain)
{
    return applyGain(buffer, range, [gain](std::size_t, std::size_t) { return gain; });
}

SampleBuffer normalize(const SampleBuffer& buffer, SampleRange range, float peak)
{
    const float current = peakLevel(buffer, range);
    if (!(current > 0.0f))
        return buffer;
    return changeVolume(buffer, range, peak / current);
}

SampleBuffer reverse(const SampleBuffer& buffer, SampleRange range)
{
    SampleBuffer out = buffer;
    const SampleRange r = range.clipped(buffer.frameCount());
    const std::size_t channels = buffer.channels();
    visitType(out, [&](auto tag) {
        using T = decltype(tag);
        T* s = samplesOf<T>(out).data();
        for (std::size_t i = 0; i < r.count / 2; ++i)
            std::swap_ranges(s + (r.start + i) * channels, s + (r.start + i + 1) * channels,
                             s + (r.end() - 1 - i) * channels);
    });
    return out;
}

SampleBuffer moveRange(const SampleBuffer& buffer, SampleRange range, std::size_t position)
{
    const SampleRange r = range.clipped(buffer.frameCount());
    const SampleBuffer moved = copyRange(buffer, r);
    const SampleBuffer rest = removeRange(buffer, r);
    const std::size_t at = std::min(position, rest.frameCount());
    return splice(buffer, {{&rest, {0, at}}, {&moved, {0, r.count}}, {&rest, {at, rest.frameCount() - at}}});
}

Result<SampleBuffer> stretchRange(const SampleBuffer& buffer, SampleRange range, std::size_t frames,
                                  ResampleQuality quality)
{
    const SampleRange r = range.clipped(buffer.frameCount());
    auto stretched = resampleToLength(copyRange(buffer, r), frames, quality);
    if (!stretched)
        return stretched.error();
    return replaceRange(buffer, r, *stretched);
}

float peakLevel(const SampleBuffer& buffer, SampleRange range)
{
    const SampleRange r = range.clipped(buffer.frameCount());
    const std::size_t channels = buffer.channels();
    return visitType(buffer, [&](auto tag) {
        using T = decltype(tag);
        const auto samples = samplesOf<T>(buffer).subspan(r.start * channels, r.count * channels);
        if constexpr (std::is_same_v<T, float>) {
            float peak = 0.0f;
            for (const float x : samples)
                peak = std::max(peak, std::fabs(x));
            return peak;
        } else {
            int peak = 0;
            for (const std::int16_t s : samples)
                peak = std::max(peak, std::abs(static_cast<int>(s)));
            return static_cast<float>(peak) / 32768.0f;
        }
    });
}

SampleRange nonSilentRange(const SampleBuffer& buffer, float threshold)
{
    const std::size_t frames = buffer.frameCount();
    const std::size_t channels = buffer.channels();
    auto loud = [&](std::size_t frame) {
        for (std::uint16_t c = 0; c < channels; ++c) {
            if (std::fabs(buffer.sample(frame, c)) > threshold)
                return true;
        }
        return false;
    };
    std::size_t first = 0;
    while (first < frames && !loud(first))
        ++first;
    if (first == frames)
        return SampleRange{0, 0};
    std::size_t last = frames - 1;
    while (last > first && !loud(last))
        --last;
    return SampleRange{first, last - first + 1};
}

SampleBuffer trimSilence(const SampleBuffer& buffer, float threshold)
{
    return copyRange(buffer, nonSilentRange(buffer, threshold));
}

float decibelsToGain(float decibels) noexcept
{
    return std::pow(10.0f, decibels / 20.0f);
}

float gainToDecibels(float gain) noexcept
{
    if (!(gain > 0.0f))
        return -std::numeric_limits<float>::infinity();
    return 20.0f * std::log10(gain);
}

} // namespace OFS::Formats
