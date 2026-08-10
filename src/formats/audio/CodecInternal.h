#pragma once

// Shared by AudioCodec.cpp and the per-format codec files. Not installed or exported.

#include "formats/audio/AudioCodec.h"

#include <cstddef>
#include <cstdint>

namespace OFS::Formats::Detail {

inline constexpr std::size_t kMaxDecodedSamples = std::size_t{1} << 28;

inline Error tooLongError()
{
    return Error{"the decoded audio would be too long (more than 2^28 samples)"};
}

Result<DecodedAudio> decodeWav(ByteSpan data, const DecodeOptions& options);
Result<DecodedAudio> decodeVorbis(ByteSpan data, const DecodeOptions& options);
Result<DecodedAudio> decodeFlac(ByteSpan data, const DecodeOptions& options);
Result<DecodedAudio> decodeMp3(ByteSpan data, const DecodeOptions& options);
// MPEG audio inside a WAV data chunk.
Result<SampleBuffer> decodeMpegData(ByteSpan data);

Result<Bytes> encodeVorbis(const SampleBuffer& buffer, const EncodeOptions& options);
Result<Bytes> encodeFlac(const SampleBuffer& buffer, const EncodeOptions& options);

inline SampleBuffer withRequestedType(SampleBuffer buffer, const DecodeOptions& options)
{
    if (options.sampleType && *options.sampleType != buffer.type())
        return convertSampleType(buffer, *options.sampleType);
    return buffer;
}

std::size_t id3v2Size(ByteSpan data) noexcept;

std::int32_t integerSample(const SampleBuffer& buffer, std::size_t index, int bits) noexcept;

} // namespace OFS::Formats::Detail
