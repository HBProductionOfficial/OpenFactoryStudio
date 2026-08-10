// WAV decoding, including the compressed encodings.

#include "formats/audio/CodecInternal.h"
#include "formats/util/ByteReader.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

namespace OFS::Formats::Detail {

namespace {

constexpr std::uint16_t kFormatMsAdpcm = 0x0002;
constexpr std::uint16_t kFormatALaw = 0x0006;
constexpr std::uint16_t kFormatMuLaw = 0x0007;
constexpr std::uint16_t kFormatImaAdpcm = 0x0011;
constexpr std::uint16_t kFormatMpeg = 0x0050;
constexpr std::uint16_t kFormatMpegLayer3 = 0x0055;

std::int16_t clamp16(int value) noexcept
{
    return static_cast<std::int16_t>(std::clamp(value, -32768, 32767));
}

std::int16_t readS16(const std::uint8_t* p) noexcept
{
    return static_cast<std::int16_t>(p[0] | (p[1] << 8));
}

// ITU-T G.711.
std::int16_t expandALaw(std::uint8_t code) noexcept
{
    code = static_cast<std::uint8_t>(code ^ 0x55);
    const int segment = (code >> 4) & 7;
    int magnitude = ((code & 0x0F) << 4) + 8;
    if (segment > 0)
        magnitude = (magnitude + 0x100) << (segment - 1);
    return static_cast<std::int16_t>((code & 0x80) ? magnitude : -magnitude);
}

std::int16_t expandMuLaw(std::uint8_t code) noexcept
{
    code = static_cast<std::uint8_t>(~code);
    constexpr int kBias = 0x84;
    const int magnitude = ((((code & 0x0F) << 3) + kBias) << ((code >> 4) & 7)) - kBias;
    return static_cast<std::int16_t>((code & 0x80) ? -magnitude : magnitude);
}

std::optional<std::size_t> factSampleCount(const WavFile& wav)
{
    for (const WavChunk& chunk : wav.chunks) {
        if (chunk.name() == "fact" && chunk.data.size() >= 4) {
            ByteReader r(chunk.data);
            return std::size_t{r.u32()};
        }
    }
    return std::nullopt;
}

std::size_t extensionSamplesPerBlock(const WavFormat& format)
{
    if (format.extension.size() < 4)
        return 0;
    ByteReader r(format.extension);
    r.seek(2);
    return r.u16();
}

// IMA ADPCM block: per channel the first sample (16 bits), the step index and a spare byte, then
// 4-byte groups of eight codes, low nibble first, channels taking turns group by group.
class ImaDecoder
{
public:
    static constexpr std::array<int, 89> kSteps = {
        7,    8,     9,     10,    11,    12,    13,    14,    16,    17,    19,    21,    23,    25,   28,
        31,   34,    37,    41,    45,    50,    55,    60,    66,    73,    80,    88,    97,    107,  118,
        130,  143,   157,   173,   190,   209,   230,   253,   279,   307,   337,   371,   408,   449,  494,
        544,  598,   658,   724,   796,   876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878, 2066,
        2272, 2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845, 8630,
        9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

    void reset(int predictor, int index) noexcept
    {
        m_predictor = predictor;
        m_index = std::clamp(index, 0, 88);
    }

    std::int16_t decode(int code) noexcept
    {
        static constexpr int kIndexSteps[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
        const int step = kSteps[static_cast<std::size_t>(m_index)];
        int diff = step >> 3;
        if (code & 1)
            diff += step >> 2;
        if (code & 2)
            diff += step >> 1;
        if (code & 4)
            diff += step;
        m_predictor = std::clamp(m_predictor + ((code & 8) ? -diff : diff), -32768, 32767);
        m_index = std::clamp(m_index + kIndexSteps[code & 7], 0, 88);
        return static_cast<std::int16_t>(m_predictor);
    }

private:
    int m_predictor = 0;
    int m_index = 0;
};

Result<std::vector<std::int16_t>> decodeImaAdpcm(const WavFile& wav)
{
    const std::size_t channels = wav.format.channels;
    const std::size_t blockAlign = wav.format.blockAlign;
    if (channels == 0 || blockAlign < 4 * channels)
        return Error{"invalid IMA ADPCM block size"};
    const std::size_t dataBytes = blockAlign - 4 * channels;
    if (dataBytes % (4 * channels) != 0)
        return Error{"invalid IMA ADPCM block size"};
    std::size_t perBlock = dataBytes * 2 / channels + 1;
    if (const std::size_t declared = extensionSamplesPerBlock(wav.format); declared > 0 && declared < perBlock)
        perBlock = declared;

    const std::span<const std::uint8_t> data = asU8(wav.samples);
    std::vector<std::int16_t> out;
    std::vector<ImaDecoder> decoders(channels);
    for (std::size_t offset = 0; data.size() - offset >= 4 * channels; offset += blockAlign) {
        const std::size_t size = std::min(blockAlign, data.size() - offset);
        const std::uint8_t* block = data.data() + offset;
        const std::size_t groups = (size - 4 * channels) / (4 * channels);
        const std::size_t frames = std::min(perBlock, 1 + groups * 8);
        if ((out.size() / channels + frames) * channels > kMaxDecodedSamples)
            return tooLongError();
        const std::size_t first = out.size();
        out.resize(first + frames * channels);
        for (std::size_t c = 0; c < channels; ++c) {
            const std::uint8_t* header = block + 4 * c;
            decoders[c].reset(readS16(header), header[2]);
            out[first + c] = readS16(header);
        }
        for (std::size_t g = 0; g < groups; ++g) {
            for (std::size_t c = 0; c < channels; ++c) {
                const std::uint8_t* codes = block + 4 * channels + (g * channels + c) * 4;
                for (std::size_t k = 0; k < 8; ++k) {
                    const std::size_t frame = 1 + g * 8 + k;
                    const int code = (codes[k / 2] >> ((k % 2) * 4)) & 0x0F;
                    const std::int16_t value = decoders[c].decode(code);
                    if (frame < frames)
                        out[first + frame * channels + c] = value;
                }
            }
        }
        if (size < blockAlign)
            break;
    }
    return out;
}

// Microsoft ADPCM block: predictor index, delta, second sample and first sample, each for all
// channels in turn. Then 4-bit codes, high nibble first, channels taking turns code by code.
Result<std::vector<std::int16_t>> decodeMsAdpcm(const WavFile& wav)
{
    static constexpr int kAdapt[16] = {230, 230, 230, 230, 307, 409, 512, 614, 768, 614, 512, 409, 307, 230, 230, 230};
    std::vector<std::array<int, 2>> coefficients = {{256, 0}, {512, -256}, {0, 0},     {192, 64},
                                                    {240, 0}, {460, -208}, {392, -232}};
    const std::size_t channels = wav.format.channels;
    const std::size_t blockAlign = wav.format.blockAlign;
    if (channels == 0 || channels > 2 || blockAlign < 7 * channels)
        return Error{"invalid Microsoft ADPCM block size"};
    std::size_t perBlock = (blockAlign - 7 * channels) * 2 / channels + 2;
    if (wav.format.extension.size() >= 6) {
        ByteReader r(wav.format.extension);
        r.seek(2);
        const std::size_t declared = r.u16();
        const std::size_t count = r.u16();
        if (declared > 0 && declared < perBlock)
            perBlock = declared;
        if (count > 0 && wav.format.extension.size() >= 6 + count * 4) {
            coefficients.assign(count, {});
            for (auto& pair : coefficients) {
                pair[0] = r.s16();
                pair[1] = r.s16();
            }
        }
    }

    const std::span<const std::uint8_t> data = asU8(wav.samples);
    std::vector<std::int16_t> out;
    for (std::size_t offset = 0; data.size() - offset >= 7 * channels; offset += blockAlign) {
        const std::size_t size = std::min(blockAlign, data.size() - offset);
        const std::uint8_t* block = data.data() + offset;
        int coef1[2] = {};
        int coef2[2] = {};
        int delta[2] = {};
        int sample1[2] = {};
        int sample2[2] = {};
        for (std::size_t c = 0; c < channels; ++c) {
            const std::size_t predictor = block[c];
            if (predictor >= coefficients.size())
                return Error{"invalid Microsoft ADPCM predictor index"};
            coef1[c] = coefficients[predictor][0];
            coef2[c] = coefficients[predictor][1];
            delta[c] = readS16(block + channels + 2 * c);
            sample1[c] = readS16(block + 3 * channels + 2 * c);
            sample2[c] = readS16(block + 5 * channels + 2 * c);
        }
        const std::size_t codes = (size - 7 * channels) * 2;
        const std::size_t frames = std::min(perBlock, 2 + codes / channels);
        if ((out.size() / channels + frames) * channels > kMaxDecodedSamples)
            return tooLongError();
        const std::size_t first = out.size();
        out.resize(first + frames * channels);
        for (std::size_t c = 0; c < channels; ++c) {
            out[first + c] = static_cast<std::int16_t>(sample2[c]);
            if (frames > 1)
                out[first + channels + c] = static_cast<std::int16_t>(sample1[c]);
        }
        const std::uint8_t* body = block + 7 * channels;
        for (std::size_t i = 0; i + 2 * channels < frames * channels; ++i) {
            const std::size_t c = i % channels;
            const int code = (body[i / 2] >> ((i % 2) ? 0 : 4)) & 0x0F;
            const int signedCode = (code & 8) ? code - 16 : code;
            const int predicted = (sample1[c] * coef1[c] + sample2[c] * coef2[c]) / 256;
            const std::int16_t value = clamp16(predicted + signedCode * delta[c]);
            sample2[c] = sample1[c];
            sample1[c] = value;
            delta[c] = std::max(16, kAdapt[code] * delta[c] / 256);
            out[first + 2 * channels + i] = value;
        }
        if (size < blockAlign)
            break;
    }
    return out;
}

std::string formatTagText(std::uint16_t tag)
{
    char text[8];
    std::snprintf(text, sizeof(text), "0x%04X", static_cast<unsigned>(tag));
    return text;
}

} // namespace

Result<DecodedAudio> decodeWav(ByteSpan data, const DecodeOptions& options)
{
    auto wav = WavFile::parse(data);
    if (!wav)
        return wav.error();
    const WavFormat& format = wav->format;
    const std::uint16_t encoding = format.sampleFormat();
    if (format.channels == 0)
        return Error{"WAV file has no channels"};
    if (format.sampleRate == 0)
        return Error{"WAV file has a sample rate of 0"};

    DecodedAudio result;
    result.format = AudioFileFormat::Wav;
    if (encoding == kWavFormatPcm || encoding == kWavFormatFloat) {
        if (wav->samples.size() > kMaxDecodedSamples * (format.bitsPerSample / 8u))
            return tooLongError();
        auto buffer = SampleBuffer::fromWav(*wav, options.sampleType);
        if (!buffer)
            return buffer.error();
        result.buffer = std::move(*buffer);
        result.bitsPerSample = format.bitsPerSample;
        result.codec = encoding == kWavFormatPcm ? "PCM" : "IEEE float";
        return result;
    }

    std::vector<std::int16_t> samples;
    switch (encoding) {
    case kFormatALaw:
    case kFormatMuLaw: {
        if (format.bitsPerSample != 8 || format.blockAlign != format.channels)
            return Error{"unsupported " + std::string(encoding == kFormatALaw ? "A-law" : "mu-law") + " layout"};
        if (wav->samples.size() > kMaxDecodedSamples)
            return tooLongError();
        const std::span<const std::uint8_t> codes = asU8(wav->samples);
        const std::size_t count = codes.size() - codes.size() % format.channels;
        samples.resize(count);
        for (std::size_t i = 0; i < count; ++i)
            samples[i] = encoding == kFormatALaw ? expandALaw(codes[i]) : expandMuLaw(codes[i]);
        result.bitsPerSample = 8;
        result.codec = encoding == kFormatALaw ? "A-law" : "mu-law";
        break;
    }
    case kFormatImaAdpcm: {
        auto decoded = decodeImaAdpcm(*wav);
        if (!decoded)
            return decoded.error();
        samples = std::move(*decoded);
        result.bitsPerSample = 4;
        result.codec = "IMA ADPCM";
        break;
    }
    case kFormatMsAdpcm: {
        auto decoded = decodeMsAdpcm(*wav);
        if (!decoded)
            return decoded.error();
        samples = std::move(*decoded);
        result.bitsPerSample = 4;
        result.codec = "Microsoft ADPCM";
        break;
    }
    case kFormatMpeg:
    case kFormatMpegLayer3: {
        auto decoded = decodeMpegData(wav->samples);
        if (!decoded)
            return withContext("MPEG audio in WAV", decoded.error());
        result.buffer = withRequestedType(std::move(*decoded), options);
        result.codec = encoding == kFormatMpegLayer3 ? "MPEG Layer III" : "MPEG audio";
        return result;
    }
    default:
        return Error{"unsupported WAV encoding (format tag " + formatTagText(encoding) + ")"};
    }

    // The last block may be padded, so the fact chunk gives the real length.
    if (const auto declared = factSampleCount(*wav); declared && *declared * format.channels < samples.size())
        samples.resize(*declared * format.channels);
    result.buffer =
        withRequestedType(SampleBuffer::fromInt16(std::move(samples), format.channels, format.sampleRate), options);
    return result;
}

} // namespace OFS::Formats::Detail
