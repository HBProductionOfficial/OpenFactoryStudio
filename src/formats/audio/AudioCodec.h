#pragma once

#include "formats/audio/AudioExport.h"
#include "formats/audio/SampleBuffer.h"
#include "formats/util/Bytes.h"
#include "formats/util/Result.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace OFS::Formats {

enum class AudioFileFormat : std::uint8_t
{
    Unknown,
    Wav,
    OggVorbis,
    Flac,
    Mp3,
};

OFS_AUDIO_EXPORT std::string_view audioFormatName(AudioFileFormat format) noexcept;
OFS_AUDIO_EXPORT std::string_view audioFormatExtension(AudioFileFormat format) noexcept;

// Looks at the data only. Ogg FLAC counts as Flac. MP3 needs a second frame header where the
// first frame ends, or the end of the data.
OFS_AUDIO_EXPORT AudioFileFormat detectAudioFormat(ByteSpan data) noexcept;

struct DecodeOptions
{
    // By default sources of 16 bits or less give Int16 and the rest Float32.
    std::optional<SampleType> sampleType;
};

struct DecodedAudio
{
    SampleBuffer buffer;
    AudioFileFormat format = AudioFileFormat::Unknown;
    std::uint16_t bitsPerSample = 0;
    std::string codec;
};

// Fails past 2^28 samples to guard against broken or hostile headers.
OFS_AUDIO_EXPORT Result<DecodedAudio> decodeAudio(ByteSpan data, const DecodeOptions& options = {});
OFS_AUDIO_EXPORT Result<DecodedAudio> loadAudio(const std::filesystem::path& path, const DecodeOptions& options = {});

struct EncodeOptions
{
    AudioFileFormat format = AudioFileFormat::Wav;
    // WAV: 8, 16, 24 or 32 for PCM, 32 or 64 for float. FLAC: 8, 16 or 24.
    std::uint16_t bitsPerSample = 16;
    bool floatingPoint = false;
    // -0.1 to 1.0. 0.4 is about 128 kbit/s for 44.1 kHz stereo.
    float vorbisQuality = 0.4f;
    unsigned flacCompressionLevel = 5;
};

OFS_AUDIO_EXPORT bool canEncode(AudioFileFormat format) noexcept;

OFS_AUDIO_EXPORT Result<Bytes> encodeAudio(const SampleBuffer& buffer, const EncodeOptions& options = {});
OFS_AUDIO_EXPORT Status saveAudio(const SampleBuffer& buffer, const std::filesystem::path& path,
                                  const EncodeOptions& options = {});

} // namespace OFS::Formats
