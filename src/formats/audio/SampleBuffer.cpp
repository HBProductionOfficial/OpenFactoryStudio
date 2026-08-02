#include "formats/audio/SampleBuffer.h"

#include "formats/audio/CodecInternal.h"

#include <algorithm>
#include <bit>
#include <string>
#include <type_traits>

namespace OFS::Formats {

namespace {

std::uint32_t readLe(const std::byte* p, std::size_t n) noexcept
{
    std::uint32_t v = 0;
    for (std::size_t i = 0; i < n; ++i)
        v |= std::uint32_t{static_cast<std::uint8_t>(p[i])} << (8 * i);
    return v;
}

void writeLe(std::byte* p, std::uint64_t v, std::size_t n) noexcept
{
    for (std::size_t i = 0; i < n; ++i)
        p[i] = static_cast<std::byte>((v >> (8 * i)) & 0xFF);
}

// Rounds halves away from zero. Doubles make this exact up to 32 bits.
std::int64_t toInteger(double value, int bits) noexcept
{
    if (std::isnan(value))
        return 0;
    const double scale = std::ldexp(1.0, bits - 1);
    const double scaled = value * scale;
    if (scaled <= -scale)
        return -static_cast<std::int64_t>(scale);
    if (scaled >= scale - 1.0)
        return static_cast<std::int64_t>(scale) - 1;
    return std::llround(scaled);
}

std::int16_t narrowToInt16(std::int64_t sample, int bits) noexcept
{
    const double value = std::ldexp(static_cast<double>(sample), -(bits - 1));
    return static_cast<std::int16_t>(toInteger(value, 16));
}

template <typename T, typename Convert>
void decodeSamples(const std::byte* p, std::size_t count, std::size_t step, T* out, Convert convert)
{
    for (std::size_t i = 0; i < count; ++i, p += step)
        out[i] = convert(p);
}

std::int32_t pcm24(const std::byte* p) noexcept
{
    std::uint32_t u = readLe(p, 3);
    if (u & 0x800000u)
        u |= 0xFF000000u;
    return static_cast<std::int32_t>(u);
}

template <typename T>
void remix(const T* in, std::size_t frames, std::size_t inChannels, T* out, std::size_t outChannels)
{
    if (inChannels == 1) {
        for (std::size_t f = 0; f < frames; ++f)
            std::fill_n(out + f * outChannels, outChannels, in[f]);
        return;
    }
    if (outChannels == 1) {
        for (std::size_t f = 0; f < frames; ++f) {
            const T* frame = in + f * inChannels;
            if constexpr (std::is_same_v<T, float>) {
                float sum = 0.0f;
                for (std::size_t c = 0; c < inChannels; ++c)
                    sum += frame[c];
                out[f] = sum / static_cast<float>(inChannels);
            } else {
                std::int64_t sum = 0;
                for (std::size_t c = 0; c < inChannels; ++c)
                    sum += frame[c];
                out[f] = static_cast<T>(std::llround(static_cast<double>(sum) / static_cast<double>(inChannels)));
            }
        }
        return;
    }
    const std::size_t common = std::min(inChannels, outChannels);
    for (std::size_t f = 0; f < frames; ++f)
        std::copy_n(in + f * inChannels, common, out + f * outChannels);
}

} // namespace

namespace Detail {

std::int32_t integerSample(const SampleBuffer& buffer, std::size_t index, int bits) noexcept
{
    if (buffer.type() == SampleType::Float32)
        return static_cast<std::int32_t>(toInteger(buffer.floatSamples()[index], bits));
    const std::int16_t s = buffer.int16Samples()[index];
    if (bits >= 16)
        return static_cast<std::int32_t>(std::int64_t{s} * (std::int64_t{1} << (bits - 16)));
    return static_cast<std::int32_t>(toInteger(s / 32768.0, bits));
}

} // namespace Detail

SampleBuffer::SampleBuffer(SampleType type, std::uint16_t channels, std::uint32_t sampleRate, std::size_t frames)
    : m_type(type)
    , m_channels(std::max<std::uint16_t>(channels, 1))
    , m_sampleRate(sampleRate)
{
    resize(frames);
}

SampleBuffer SampleBuffer::fromInt16(std::vector<std::int16_t> samples, std::uint16_t channels,
                                     std::uint32_t sampleRate)
{
    SampleBuffer buffer(SampleType::Int16, channels, sampleRate);
    samples.resize(samples.size() - samples.size() % buffer.m_channels);
    buffer.m_int16 = std::move(samples);
    return buffer;
}

SampleBuffer SampleBuffer::fromFloat(std::vector<float> samples, std::uint16_t channels, std::uint32_t sampleRate)
{
    SampleBuffer buffer(SampleType::Float32, channels, sampleRate);
    samples.resize(samples.size() - samples.size() % buffer.m_channels);
    buffer.m_float = std::move(samples);
    return buffer;
}

Result<SampleBuffer> SampleBuffer::fromWav(const WavFile& wav, std::optional<SampleType> type)
{
    const WavFormat& format = wav.format;
    const std::uint16_t encoding = format.sampleFormat();
    const std::uint16_t bits = format.bitsPerSample;
    if (!wav.isSupported())
        return Error{"unsupported WAV sample format (format tag " + std::to_string(encoding) + ", " +
                     std::to_string(bits) + " bits, " + std::to_string(format.channels) + " channels)"};
    if (format.sampleRate == 0)
        return Error{"WAV file has a sample rate of 0"};

    const bool isFloat = encoding == kWavFormatFloat;
    const SampleType target = type.value_or(!isFloat && bits <= 16 ? SampleType::Int16 : SampleType::Float32);
    const std::size_t frames = wav.frameCount();
    SampleBuffer out(target, format.channels, format.sampleRate, frames);
    const std::size_t count = frames * format.channels;
    const std::size_t step = bits / 8u;
    const std::byte* in = wav.samples.data();

    if (target == SampleType::Int16) {
        std::int16_t* dst = out.m_int16.data();
        if (isFloat && bits == 32) {
            decodeSamples(in, count, step, dst,
                          [](const std::byte* p) { return floatToInt16(std::bit_cast<float>(readLe(p, 4))); });
        } else if (isFloat) {
            decodeSamples(in, count, step, dst, [](const std::byte* p) {
                const std::uint64_t raw = readLe(p, 4) | (std::uint64_t{readLe(p + 4, 4)} << 32);
                return static_cast<std::int16_t>(toInteger(std::bit_cast<double>(raw), 16));
            });
        } else if (bits == 8) {
            decodeSamples(in, count, step, dst, [](const std::byte* p) {
                return static_cast<std::int16_t>((static_cast<int>(static_cast<std::uint8_t>(p[0])) - 128) * 256);
            });
        } else if (bits == 16) {
            decodeSamples(in, count, step, dst,
                          [](const std::byte* p) { return static_cast<std::int16_t>(readLe(p, 2)); });
        } else if (bits == 24) {
            decodeSamples(in, count, step, dst, [](const std::byte* p) { return narrowToInt16(pcm24(p), 24); });
        } else {
            decodeSamples(in, count, step, dst, [](const std::byte* p) {
                return narrowToInt16(static_cast<std::int32_t>(readLe(p, 4)), 32);
            });
        }
        return out;
    }

    float* dst = out.m_float.data();
    if (isFloat && bits == 32) {
        decodeSamples(in, count, step, dst, [](const std::byte* p) { return std::bit_cast<float>(readLe(p, 4)); });
    } else if (isFloat) {
        decodeSamples(in, count, step, dst, [](const std::byte* p) {
            const std::uint64_t raw = readLe(p, 4) | (std::uint64_t{readLe(p + 4, 4)} << 32);
            return static_cast<float>(std::bit_cast<double>(raw));
        });
    } else if (bits == 8) {
        decodeSamples(in, count, step, dst, [](const std::byte* p) {
            return static_cast<float>(static_cast<int>(static_cast<std::uint8_t>(p[0])) - 128) * (1.0f / 128.0f);
        });
    } else if (bits == 16) {
        decodeSamples(in, count, step, dst,
                      [](const std::byte* p) { return int16ToFloat(static_cast<std::int16_t>(readLe(p, 2))); });
    } else if (bits == 24) {
        decodeSamples(in, count, step, dst,
                      [](const std::byte* p) { return static_cast<float>(pcm24(p)) * (1.0f / 8388608.0f); });
    } else {
        decodeSamples(in, count, step, dst, [](const std::byte* p) {
            return static_cast<float>(static_cast<double>(static_cast<std::int32_t>(readLe(p, 4))) / 2147483648.0);
        });
    }
    return out;
}

Result<WavFile> SampleBuffer::toWav(WavSampleFormat format) const
{
    const std::uint16_t bits = format.bitsPerSample;
    if (format.floatingPoint ? (bits != 32 && bits != 64) : (bits != 8 && bits != 16 && bits != 24 && bits != 32))
        return Error{"unsupported WAV sample size " + std::to_string(bits) + (format.floatingPoint ? " (float)" : "")};
    if (static_cast<std::uint64_t>(m_channels) * (bits / 8u) > 0xFFFF)
        return Error{"too many channels for a WAV file"};
    const std::size_t step = bits / 8u;
    if (sampleCount() > (std::size_t{0xFFFFFFFF} - 64) / step)
        return Error{"too many samples for a WAV file (the data chunk would exceed 4 GiB)"};

    WavFile wav;
    wav.format.formatTag = format.floatingPoint ? kWavFormatFloat : kWavFormatPcm;
    wav.format.channels = m_channels;
    wav.format.sampleRate = m_sampleRate;
    wav.format.bitsPerSample = bits;
    wav.format.blockAlign = static_cast<std::uint16_t>(m_channels * step);
    wav.format.byteRate = m_sampleRate * wav.format.blockAlign;

    const std::size_t count = sampleCount();
    wav.samples.resize(count * step);
    std::byte* p = wav.samples.data();
    for (std::size_t i = 0; i < count; ++i, p += step) {
        if (format.floatingPoint) {
            const double value = m_type == SampleType::Int16 ? m_int16[i] / 32768.0 : static_cast<double>(m_float[i]);
            if (bits == 32)
                writeLe(p, std::bit_cast<std::uint32_t>(static_cast<float>(value)), 4);
            else
                writeLe(p, std::bit_cast<std::uint64_t>(value), 8);
            continue;
        }
        std::int64_t v = Detail::integerSample(*this, i, bits);
        if (bits == 8)
            v += 128; // 8-bit WAV samples are unsigned
        writeLe(p, static_cast<std::uint64_t>(v), step);
    }
    return wav;
}

std::size_t SampleBuffer::frameCount() const noexcept
{
    return sampleCount() / m_channels;
}

std::size_t SampleBuffer::sampleCount() const noexcept
{
    return m_type == SampleType::Int16 ? m_int16.size() : m_float.size();
}

double SampleBuffer::durationSeconds() const noexcept
{
    return m_sampleRate ? static_cast<double>(frameCount()) / m_sampleRate : 0.0;
}

std::size_t SampleBuffer::byteSize() const noexcept
{
    return m_int16.size() * sizeof(std::int16_t) + m_float.size() * sizeof(float);
}

float SampleBuffer::sample(std::size_t frame, std::uint16_t channel) const noexcept
{
    if (channel >= m_channels || frame >= frameCount())
        return 0.0f;
    const std::size_t i = frame * m_channels + channel;
    return m_type == SampleType::Int16 ? int16ToFloat(m_int16[i]) : m_float[i];
}

void SampleBuffer::setSample(std::size_t frame, std::uint16_t channel, float value) noexcept
{
    if (channel >= m_channels || frame >= frameCount())
        return;
    const std::size_t i = frame * m_channels + channel;
    if (m_type == SampleType::Int16)
        m_int16[i] = floatToInt16(value);
    else
        m_float[i] = value;
}

void SampleBuffer::resize(std::size_t frames)
{
    if (m_type == SampleType::Int16)
        m_int16.resize(frames * m_channels);
    else
        m_float.resize(frames * m_channels);
}

bool SampleBuffer::sameFormat(const SampleBuffer& other) const noexcept
{
    return m_type == other.m_type && m_channels == other.m_channels && m_sampleRate == other.m_sampleRate;
}

SampleBuffer convertSampleType(const SampleBuffer& buffer, SampleType type)
{
    if (buffer.type() == type)
        return buffer;
    SampleBuffer out(type, buffer.channels(), buffer.sampleRate(), buffer.frameCount());
    if (type == SampleType::Float32)
        std::transform(buffer.int16Samples().begin(), buffer.int16Samples().end(), out.floatSamples().begin(),
                       int16ToFloat);
    else
        std::transform(buffer.floatSamples().begin(), buffer.floatSamples().end(), out.int16Samples().begin(),
                       floatToInt16);
    return out;
}

SampleBuffer convertChannels(const SampleBuffer& buffer, std::uint16_t channels)
{
    channels = std::max<std::uint16_t>(channels, 1);
    if (channels == buffer.channels())
        return buffer;
    const std::size_t frames = buffer.frameCount();
    SampleBuffer out(buffer.type(), channels, buffer.sampleRate(), frames);
    if (buffer.type() == SampleType::Int16)
        remix(buffer.int16Samples().data(), frames, buffer.channels(), out.int16Samples().data(), channels);
    else
        remix(buffer.floatSamples().data(), frames, buffer.channels(), out.floatSamples().data(), channels);
    return out;
}

Result<SampleBuffer> quantize(const SampleBuffer& buffer, std::uint16_t bitsPerSample)
{
    if (bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32)
        return Error{"unsupported bit depth " + std::to_string(bitsPerSample)};
    SampleBuffer out = buffer;
    if (buffer.type() == SampleType::Int16) {
        if (bitsPerSample >= 16)
            return out;
        for (std::int16_t& s : out.int16Samples())
            s = static_cast<std::int16_t>(toInteger(s / 32768.0, 8) * 256);
        return out;
    }
    const double scale = std::ldexp(1.0, -(bitsPerSample - 1));
    for (float& x : out.floatSamples())
        x = static_cast<float>(static_cast<double>(toInteger(x, bitsPerSample)) * scale);
    return out;
}

} // namespace OFS::Formats
