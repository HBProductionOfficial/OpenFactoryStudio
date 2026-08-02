#include "formats/audio/AudioCodec.h"

#include "formats/audio/CodecInternal.h"
#include "formats/util/FileIo.h"

#include <cstring>
#include <string>

namespace OFS::Formats {

namespace Detail {

std::size_t id3v2Size(ByteSpan data) noexcept
{
    // "ID3", a two-byte version, flags, then a 28-bit size stored 7 bits per byte. The size leaves
    // out the 10-byte header and the 10-byte footer, which is there when flag 0x10 is set.
    const std::span<const std::uint8_t> b = asU8(data);
    if (b.size() < 10 || b[0] != 'I' || b[1] != 'D' || b[2] != '3' || b[3] == 0xFF || b[4] == 0xFF)
        return 0;
    if ((b[6] | b[7] | b[8] | b[9]) & 0x80)
        return 0;
    std::size_t size = (std::size_t{b[6]} << 21) | (std::size_t{b[7]} << 14) | (std::size_t{b[8]} << 7) | b[9];
    size += 10;
    if (b[5] & 0x10)
        size += 10;
    return size;
}

} // namespace Detail

namespace {

bool startsWith(std::span<const std::uint8_t> data, std::size_t offset, std::string_view text) noexcept
{
    return offset <= data.size() && data.size() - offset >= text.size() &&
           std::memcmp(data.data() + offset, text.data(), text.size()) == 0;
}

struct MpegFrame
{
    std::size_t length = 0; // 0: not a frame header
    int version = 0;
    int layer = 0;
    int rateIndex = 0;
};

// Free-format frames (bit rate index 0) have no length in the header, so they are not accepted.
MpegFrame mpegFrame(const std::uint8_t* h) noexcept
{
    static constexpr int kBitrates[2][3][15] = {
        {{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448},
         {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384},
         {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320}},
        {{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256},
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160},
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}},
    };
    static constexpr int kRates[3] = {44100, 48000, 32000};

    MpegFrame frame;
    if (h[0] != 0xFF || (h[1] & 0xE0) != 0xE0)
        return frame;
    const int version = (h[1] >> 3) & 3;   // 0: MPEG-2.5, 1: reserved, 2: MPEG-2, 3: MPEG-1
    const int layerBits = (h[1] >> 1) & 3; // 1: layer III, 2: layer II, 3: layer I, 0: reserved
    const int bitrateIndex = (h[2] >> 4) & 15;
    const int rateIndex = (h[2] >> 2) & 3;
    const int padding = (h[2] >> 1) & 1;
    if (version == 1 || layerBits == 0 || bitrateIndex == 0 || bitrateIndex == 15 || rateIndex == 3)
        return frame;
    const bool mpeg1 = version == 3;
    const int layer = 4 - layerBits;
    const int sampleRate = kRates[rateIndex] >> (mpeg1 ? 0 : (version == 2 ? 1 : 2));
    const int bitrate = kBitrates[mpeg1 ? 0 : 1][layer - 1][bitrateIndex] * 1000;
    if (layer == 1) {
        frame.length = static_cast<std::size_t>((12 * bitrate / sampleRate + padding) * 4);
    } else {
        // Layer II and III frames hold 1152 samples (576 for layer III in MPEG-2/2.5), so the
        // length is samples / 8 * bit rate / sample rate bytes.
        const int factor = (layer == 3 && !mpeg1) ? 72 : 144;
        frame.length = static_cast<std::size_t>(factor * bitrate / sampleRate + padding);
    }
    frame.version = version;
    frame.layer = layer;
    frame.rateIndex = rateIndex;
    return frame;
}

bool looksLikeMpegAudio(std::span<const std::uint8_t> b) noexcept
{
    const std::size_t pos = Detail::id3v2Size(asBytes(b));
    if (pos > b.size() || b.size() - pos < 4)
        return false;
    const MpegFrame first = mpegFrame(b.data() + pos);
    if (first.length == 0)
        return false;
    const std::size_t next = pos + first.length;
    if (next >= b.size() || b.size() - next < 4)
        return true;
    const MpegFrame second = mpegFrame(b.data() + next);
    return second.length != 0 && second.version == first.version && second.layer == first.layer &&
           second.rateIndex == first.rateIndex;
}

} // namespace

std::string_view audioFormatName(AudioFileFormat format) noexcept
{
    switch (format) {
    case AudioFileFormat::Wav:
        return "WAV";
    case AudioFileFormat::OggVorbis:
        return "Ogg Vorbis";
    case AudioFileFormat::Flac:
        return "FLAC";
    case AudioFileFormat::Mp3:
        return "MP3";
    case AudioFileFormat::Unknown:
        break;
    }
    return "unknown";
}

std::string_view audioFormatExtension(AudioFileFormat format) noexcept
{
    switch (format) {
    case AudioFileFormat::Wav:
        return "wav";
    case AudioFileFormat::OggVorbis:
        return "ogg";
    case AudioFileFormat::Flac:
        return "flac";
    case AudioFileFormat::Mp3:
        return "mp3";
    case AudioFileFormat::Unknown:
        break;
    }
    return {};
}

AudioFileFormat detectAudioFormat(ByteSpan data) noexcept
{
    const std::span<const std::uint8_t> b = asU8(data);
    if (startsWith(b, 0, "RIFF") && startsWith(b, 8, "WAVE"))
        return AudioFileFormat::Wav;
    if (startsWith(b, 0, "OggS") && b.size() >= 27) {
        // The first page holds the codec's identification packet, after the segment table.
        const std::size_t packet = 27 + std::size_t{b[26]};
        if (b.size() > packet && b[packet] == 0x01 && startsWith(b, packet + 1, "vorbis"))
            return AudioFileFormat::OggVorbis;
        if (b.size() > packet && b[packet] == 0x7F && startsWith(b, packet + 1, "FLAC"))
            return AudioFileFormat::Flac;
        return AudioFileFormat::Unknown;
    }
    if (startsWith(b, Detail::id3v2Size(data), "fLaC"))
        return AudioFileFormat::Flac;
    if (looksLikeMpegAudio(b))
        return AudioFileFormat::Mp3;
    return AudioFileFormat::Unknown;
}

Result<DecodedAudio> decodeAudio(ByteSpan data, const DecodeOptions& options)
{
    switch (detectAudioFormat(data)) {
    case AudioFileFormat::Wav:
        return Detail::decodeWav(data, options);
    case AudioFileFormat::OggVorbis:
        return Detail::decodeVorbis(data, options);
    case AudioFileFormat::Flac:
        return Detail::decodeFlac(data, options);
    case AudioFileFormat::Mp3:
        return Detail::decodeMp3(data, options);
    case AudioFileFormat::Unknown:
        break;
    }
    if (startsWith(asU8(data), 0, "OggS"))
        return Error{"unsupported Ogg stream (only Vorbis and FLAC are supported)"};
    return Error{"not a WAV, Ogg Vorbis, FLAC or MP3 file"};
}

Result<DecodedAudio> loadAudio(const std::filesystem::path& path, const DecodeOptions& options)
{
    auto data = readFile(path);
    if (!data)
        return data.error();
    return decodeAudio(*data, options);
}

bool canEncode(AudioFileFormat format) noexcept
{
    return format == AudioFileFormat::Wav || format == AudioFileFormat::OggVorbis || format == AudioFileFormat::Flac;
}

Result<Bytes> encodeAudio(const SampleBuffer& buffer, const EncodeOptions& options)
{
    switch (options.format) {
    case AudioFileFormat::Wav: {
        auto wav = buffer.toWav(WavSampleFormat{options.bitsPerSample, options.floatingPoint});
        if (!wav)
            return wav.error();
        return wav->write();
    }
    case AudioFileFormat::OggVorbis:
        return Detail::encodeVorbis(buffer, options);
    case AudioFileFormat::Flac:
        return Detail::encodeFlac(buffer, options);
    case AudioFileFormat::Mp3:
    case AudioFileFormat::Unknown:
        break;
    }
    return Error{"cannot encode " + std::string(audioFormatName(options.format)) + " files"};
}

Status saveAudio(const SampleBuffer& buffer, const std::filesystem::path& path, const EncodeOptions& options)
{
    auto bytes = encodeAudio(buffer, options);
    if (!bytes)
        return bytes.error();
    return writeFile(path, *bytes);
}

} // namespace OFS::Formats
