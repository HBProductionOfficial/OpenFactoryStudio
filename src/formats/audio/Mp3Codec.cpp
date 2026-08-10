// MPEG audio decoding (layers I to III) with dr_mp3.

#include "formats/audio/CodecInternal.h"

#include <dr_mp3.h>

#include <memory>

namespace OFS::Formats::Detail {

Result<SampleBuffer> decodeMpegData(ByteSpan data)
{
    // The decoder state holds a whole frame of samples, so it goes on the heap.
    auto mp3 = std::make_unique<drmp3>();
    if (!drmp3_init_memory(mp3.get(), data.data(), data.size(), nullptr))
        return Error{"no MPEG audio frames found"};
    struct Closer
    {
        drmp3* decoder;
        ~Closer() { drmp3_uninit(decoder); }
    } closer{mp3.get()};

    const std::uint32_t channels = mp3->channels;
    const std::uint32_t sampleRate = mp3->sampleRate;
    if (channels == 0 || channels > 2 || sampleRate == 0)
        return Error{"invalid MPEG audio stream"};

    constexpr std::size_t kChunkFrames = 4096;
    std::vector<std::int16_t> chunk(kChunkFrames * channels);
    std::vector<std::int16_t> samples;
    for (;;) {
        const drmp3_uint64 frames = drmp3_read_pcm_frames_s16(mp3.get(), kChunkFrames, chunk.data());
        if (frames == 0)
            break;
        const std::size_t count = static_cast<std::size_t>(frames) * channels;
        if (samples.size() + count > kMaxDecodedSamples)
            return tooLongError();
        samples.insert(samples.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(count));
    }
    if (samples.empty())
        return Error{"no MPEG audio frames found"};
    return SampleBuffer::fromInt16(std::move(samples), static_cast<std::uint16_t>(channels), sampleRate);
}

Result<DecodedAudio> decodeMp3(ByteSpan data, const DecodeOptions& options)
{
    auto buffer = decodeMpegData(data);
    if (!buffer)
        return withContext("MP3", buffer.error());
    DecodedAudio result;
    result.buffer = withRequestedType(std::move(*buffer), options);
    result.format = AudioFileFormat::Mp3;
    result.codec = "MPEG audio";
    return result;
}

} // namespace OFS::Formats::Detail
