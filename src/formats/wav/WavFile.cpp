#include "formats/wav/WavFile.h"

#include "formats/util/ByteReader.h"
#include "formats/util/ByteWriter.h"
#include "formats/util/FileIo.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <string>

namespace OFS::Formats {

namespace {

std::array<char, 4> chunkId(std::string_view s)
{
    std::array<char, 4> id{};
    for (std::size_t i = 0; i < 4 && i < s.size(); ++i)
        id[i] = s[i];
    return id;
}

std::uint32_t readLe(const std::byte* p, std::size_t n)
{
    std::uint32_t v = 0;
    for (std::size_t i = 0; i < n; ++i)
        v |= std::uint32_t{static_cast<std::uint8_t>(p[i])} << (8 * i);
    return v;
}

} // namespace

std::uint16_t WavFormat::sampleFormat() const noexcept
{
    // WAVE_FORMAT_EXTENSIBLE: cbSize, valid bits, channel mask, then the sub-format GUID whose
    // first two bytes are the actual format tag.
    if (formatTag == kWavFormatExtensible && extension.size() >= 2 + 2 + 4 + 16)
        return static_cast<std::uint16_t>(readLe(extension.data() + 8, 2));
    return formatTag;
}

Result<WavFile> WavFile::parse(ByteSpan data, const WavParseOptions& options)
{
    ByteReader r(data);
    if (data.size() < 12)
        return Error{"WAV header is truncated"};
    if (toText(r.bytes(4)) != "RIFF")
        return Error{"not a RIFF file"};
    const std::uint32_t riffSize = r.u32();
    if (toText(r.bytes(4)) != "WAVE")
        return Error{"not a WAVE file"};

    std::size_t end = data.size();
    if (options.strict) {
        if (riffSize < 4 || riffSize > data.size() - 8)
            return Error{"WAV file is truncated (RIFF size is larger than the file)"};
        end = std::size_t{riffSize} + 8;
    }

    WavFile wav;
    bool haveFormat = false;
    bool haveData = false;
    std::size_t pos = 12;
    while (end - pos >= 8) {
        r.seek(pos);
        const std::string id = toText(r.bytes(4));
        std::size_t size = r.u32();
        const std::size_t body = pos + 8;
        if (size > end - body) {
            if (options.strict)
                return Error{"WAV chunk '" + id + "' is truncated"};
            if (id == "data" && !haveData)
                size = end - body; // truncated sample data: keep what is there
            else if (haveFormat && haveData)
                break; // damaged trailing chunk
            else
                return Error{"WAV chunk '" + id + "' is truncated"};
        }
        const ByteSpan bytes = data.subspan(body, size);
        if (id == "fmt ") {
            if (size < 16)
                return Error{"WAV format chunk is too small"};
            ByteReader f(bytes);
            wav.format.formatTag = f.u16();
            wav.format.channels = f.u16();
            wav.format.sampleRate = f.u32();
            wav.format.byteRate = f.u32();
            wav.format.blockAlign = f.u16();
            wav.format.bitsPerSample = f.u16();
            const ByteSpan extension = f.rest();
            wav.format.extension.assign(extension.begin(), extension.end());
            haveFormat = true;
            wav.chunks.push_back(WavChunk{chunkId(id), {}});
        } else if (id == "data") {
            wav.samples.assign(bytes.begin(), bytes.end());
            haveData = true;
            wav.chunks.push_back(WavChunk{chunkId(id), {}});
        } else {
            wav.chunks.push_back(WavChunk{chunkId(id), Bytes(bytes.begin(), bytes.end())});
        }
        pos = body + size + (size & 1);
        if (pos > end)
            break; // missing pad byte after the last chunk
    }
    if (!haveFormat)
        return Error{"WAV file has no format chunk"};
    if (!haveData)
        return Error{"WAV file has no data chunk"};
    return wav;
}

Result<WavFile> WavFile::load(const std::filesystem::path& path, const WavParseOptions& options)
{
    auto data = readFile(path);
    if (!data)
        return data.error();
    return parse(*data, options);
}

Bytes WavFile::write() const
{
    std::vector<WavChunk> order = chunks;
    auto has = [&order](std::string_view id) {
        return std::any_of(order.begin(), order.end(), [id](const WavChunk& c) { return c.name() == id; });
    };
    if (!has("data"))
        order.insert(order.begin(), WavChunk{chunkId("data"), {}});
    if (!has("fmt "))
        order.insert(order.begin(), WavChunk{chunkId("fmt "), {}});

    ByteWriter fmt;
    fmt.u16(format.formatTag);
    fmt.u16(format.channels);
    fmt.u32(format.sampleRate);
    fmt.u32(format.byteRate);
    fmt.u16(format.blockAlign);
    fmt.u16(format.bitsPerSample);
    fmt.bytes(format.extension);

    ByteWriter out;
    out.text("RIFF");
    out.u32(0);
    out.text("WAVE");
    bool wroteFormat = false;
    bool wroteData = false;
    for (const WavChunk& chunk : order) {
        const Bytes* body = &chunk.data;
        if (chunk.name() == "fmt ") {
            if (wroteFormat)
                continue;
            body = &fmt.data();
            wroteFormat = true;
        } else if (chunk.name() == "data") {
            if (wroteData)
                continue;
            body = &samples;
            wroteData = true;
        }
        out.bytes(asBytes(chunk.name()));
        out.u32(static_cast<std::uint32_t>(body->size()));
        out.bytes(*body);
        if (body->size() & 1)
            out.u8(0);
    }
    out.patchU32(4, static_cast<std::uint32_t>(out.size() - 8));
    return out.take();
}

Status WavFile::save(const std::filesystem::path& path) const
{
    return writeFile(path, write());
}

Result<WavFile> WavFile::fromSamples(const std::vector<double>& interleaved, std::uint16_t channels,
                                     std::uint32_t sampleRate, std::uint16_t bitsPerSample, bool floatingPoint)
{
    if (channels == 0)
        return Error{"a WAV file needs at least one channel"};
    if (floatingPoint ? (bitsPerSample != 32 && bitsPerSample != 64)
                      : (bitsPerSample != 8 && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32))
        return Error{"unsupported WAV sample size " + std::to_string(bitsPerSample)};
    if (interleaved.size() % channels != 0)
        return Error{"sample count is not a multiple of the channel count"};

    WavFile wav;
    wav.format.formatTag = floatingPoint ? kWavFormatFloat : kWavFormatPcm;
    wav.format.channels = channels;
    wav.format.sampleRate = sampleRate;
    wav.format.bitsPerSample = bitsPerSample;
    wav.format.blockAlign = static_cast<std::uint16_t>(channels * (bitsPerSample / 8));
    wav.format.byteRate = sampleRate * wav.format.blockAlign;

    ByteWriter out(interleaved.size() * (bitsPerSample / 8u));
    for (const double raw : interleaved) {
        const double v = std::clamp(raw, -1.0, 1.0);
        if (floatingPoint) {
            if (bitsPerSample == 32) {
                out.u32(std::bit_cast<std::uint32_t>(static_cast<float>(v)));
            } else {
                const auto bits = std::bit_cast<std::uint64_t>(v);
                out.u32(static_cast<std::uint32_t>(bits & 0xFFFFFFFFu));
                out.u32(static_cast<std::uint32_t>(bits >> 32));
            }
            continue;
        }
        switch (bitsPerSample) {
        case 8:
            out.u8(static_cast<std::uint8_t>(std::lround(v * 127.0) + 128));
            break;
        case 16:
            out.s16(static_cast<std::int16_t>(std::lround(v * 32767.0)));
            break;
        case 24: {
            const auto s = static_cast<std::int32_t>(std::lround(v * 8388607.0));
            const auto u = static_cast<std::uint32_t>(s);
            out.u8(static_cast<std::uint8_t>(u & 0xFF));
            out.u8(static_cast<std::uint8_t>((u >> 8) & 0xFF));
            out.u8(static_cast<std::uint8_t>((u >> 16) & 0xFF));
            break;
        }
        default:
            out.s32(static_cast<std::int32_t>(std::llround(v * 2147483647.0)));
            break;
        }
    }
    wav.samples = out.take();
    return wav;
}

bool WavFile::isSupported() const noexcept
{
    const std::uint16_t f = format.sampleFormat();
    const std::uint16_t bits = format.bitsPerSample;
    const bool known = (f == kWavFormatPcm && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) ||
                       (f == kWavFormatFloat && (bits == 32 || bits == 64));
    return known && format.channels > 0 && format.blockAlign == format.channels * (bits / 8);
}

std::size_t WavFile::frameCount() const noexcept
{
    return format.blockAlign ? samples.size() / format.blockAlign : 0;
}

double WavFile::sample(std::size_t frame, std::uint16_t channel) const noexcept
{
    if (!isSupported() || channel >= format.channels || frame >= frameCount())
        return 0.0;
    const std::size_t bytes = format.bitsPerSample / 8u;
    const std::byte* p = samples.data() + frame * format.blockAlign + channel * bytes;
    if (format.sampleFormat() == kWavFormatFloat) {
        if (bytes == 4)
            return static_cast<double>(std::bit_cast<float>(readLe(p, 4)));
        const std::uint64_t bits = readLe(p, 4) | (std::uint64_t{readLe(p + 4, 4)} << 32);
        return std::bit_cast<double>(bits);
    }
    switch (bytes) {
    case 1:
        return (static_cast<double>(static_cast<std::uint8_t>(p[0])) - 128.0) / 128.0;
    case 2:
        return static_cast<double>(static_cast<std::int16_t>(readLe(p, 2))) / 32768.0;
    case 3: {
        std::uint32_t u = readLe(p, 3);
        if (u & 0x800000u)
            u |= 0xFF000000u;
        return static_cast<double>(static_cast<std::int32_t>(u)) / 8388608.0;
    }
    default:
        return static_cast<double>(static_cast<std::int32_t>(readLe(p, 4))) / 2147483648.0;
    }
}

std::vector<double> WavFile::toDoubles() const
{
    std::vector<double> out;
    if (!isSupported())
        return out;
    const std::size_t frames = frameCount();
    out.reserve(frames * format.channels);
    for (std::size_t f = 0; f < frames; ++f) {
        for (std::uint16_t c = 0; c < format.channels; ++c)
            out.push_back(sample(f, c));
    }
    return out;
}

double WavFile::durationSeconds() const noexcept
{
    return format.sampleRate ? static_cast<double>(frameCount()) / format.sampleRate : 0.0;
}

} // namespace OFS::Formats
