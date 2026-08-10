// FLAC: decoding with dr_flac (native and Ogg FLAC), encoding with libFLAC.

#include "formats/audio/CodecInternal.h"

#include <FLAC/stream_encoder.h>
#include <dr_flac.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>

namespace OFS::Formats::Detail {

namespace {

// libFLAC seeks back at the end to fill in the stream info.
struct MemorySink
{
    Bytes bytes;
    std::size_t position = 0;
};

FLAC__StreamEncoderWriteStatus writeToSink(const FLAC__StreamEncoder*, const FLAC__byte buffer[], size_t bytes,
                                           uint32_t, uint32_t, void* client)
{
    auto* sink = static_cast<MemorySink*>(client);
    if (sink->bytes.size() < sink->position + bytes)
        sink->bytes.resize(sink->position + bytes);
    std::memcpy(sink->bytes.data() + sink->position, buffer, bytes);
    sink->position += bytes;
    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

FLAC__StreamEncoderSeekStatus seekSink(const FLAC__StreamEncoder*, FLAC__uint64 offset, void* client)
{
    auto* sink = static_cast<MemorySink*>(client);
    if (offset > sink->bytes.size())
        return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;
    sink->position = static_cast<std::size_t>(offset);
    return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
}

FLAC__StreamEncoderTellStatus tellSink(const FLAC__StreamEncoder*, FLAC__uint64* offset, void* client)
{
    *offset = static_cast<MemorySink*>(client)->position;
    return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
}

struct EncoderDeleter
{
    void operator()(FLAC__StreamEncoder* encoder) const { FLAC__stream_encoder_delete(encoder); }
};

struct DecoderCloser
{
    void operator()(drflac* decoder) const { drflac_close(decoder); }
};

template <typename T, typename Read>
Result<std::vector<T>> readAll(drflac* flac, Read read)
{
    constexpr std::size_t kChunkFrames = 4096;
    const std::size_t channels = flac->channels;
    std::vector<T> samples;
    if (flac->totalPCMFrameCount > 0 && flac->totalPCMFrameCount <= kMaxDecodedSamples / channels)
        samples.reserve(static_cast<std::size_t>(flac->totalPCMFrameCount) * channels);
    std::vector<T> chunk(kChunkFrames * channels);
    for (;;) {
        const drflac_uint64 frames = read(flac, kChunkFrames, chunk.data());
        if (frames == 0)
            break;
        const std::size_t count = static_cast<std::size_t>(frames) * channels;
        if (samples.size() + count > kMaxDecodedSamples)
            return tooLongError();
        samples.insert(samples.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(count));
    }
    return samples;
}

} // namespace

Result<DecodedAudio> decodeFlac(ByteSpan data, const DecodeOptions& options)
{
    // dr_flac does not skip ID3v2 tags, which some tools put in front of FLAC files.
    if (const std::size_t tag = id3v2Size(data); tag > 0 && tag < data.size())
        data = data.subspan(tag);
    std::unique_ptr<drflac, DecoderCloser> flac(drflac_open_memory(data.data(), data.size(), nullptr));
    if (!flac)
        return Error{"invalid FLAC data"};
    const std::uint16_t channels = flac->channels;
    const std::uint32_t sampleRate = flac->sampleRate;
    if (channels == 0 || sampleRate == 0)
        return Error{"invalid FLAC stream information"};

    DecodedAudio result;
    result.format = AudioFileFormat::Flac;
    result.bitsPerSample = flac->bitsPerSample;
    result.codec = "FLAC";
    if (flac->bitsPerSample <= 16) {
        auto samples = readAll<std::int16_t>(flac.get(), drflac_read_pcm_frames_s16);
        if (!samples)
            return samples.error();
        result.buffer = SampleBuffer::fromInt16(std::move(*samples), channels, sampleRate);
    } else {
        auto samples = readAll<float>(flac.get(), drflac_read_pcm_frames_f32);
        if (!samples)
            return samples.error();
        result.buffer = SampleBuffer::fromFloat(std::move(*samples), channels, sampleRate);
    }
    // dr_flac gives nothing for damaged streams and ones it can't decode, like 32-bit samples.
    if (result.buffer.empty() && flac->totalPCMFrameCount > 0)
        return Error{"unsupported or damaged FLAC stream (" + std::to_string(flac->bitsPerSample) + " bits)"};
    result.buffer = withRequestedType(std::move(result.buffer), options);
    return result;
}

Result<Bytes> encodeFlac(const SampleBuffer& buffer, const EncodeOptions& options)
{
    // No 32-bit, because few decoders read it (dr_flac doesn't).
    const std::uint16_t bits = options.bitsPerSample;
    if (bits != 8 && bits != 16 && bits != 24)
        return Error{"FLAC files can be written with 8, 16 or 24 bits per sample, not " + std::to_string(bits)};
    if (buffer.channels() > FLAC__MAX_CHANNELS)
        return Error{"FLAC files hold at most 8 channels"};
    if (buffer.sampleRate() == 0 || buffer.sampleRate() > FLAC__MAX_SAMPLE_RATE)
        return Error{"FLAC cannot store a sample rate of " + std::to_string(buffer.sampleRate()) + " Hz"};

    std::unique_ptr<FLAC__StreamEncoder, EncoderDeleter> encoder(FLAC__stream_encoder_new());
    if (!encoder)
        return Error{"out of memory"};
    FLAC__StreamEncoder* e = encoder.get();
    FLAC__stream_encoder_set_verify(e, false);
    FLAC__stream_encoder_set_compression_level(e, std::min(options.flacCompressionLevel, 8u));
    FLAC__stream_encoder_set_channels(e, buffer.channels());
    FLAC__stream_encoder_set_bits_per_sample(e, bits);
    FLAC__stream_encoder_set_sample_rate(e, buffer.sampleRate());
    FLAC__stream_encoder_set_total_samples_estimate(e, buffer.frameCount());

    MemorySink sink;
    const FLAC__StreamEncoderInitStatus init =
        FLAC__stream_encoder_init_stream(e, writeToSink, seekSink, tellSink, nullptr, &sink);
    if (init != FLAC__STREAM_ENCODER_INIT_STATUS_OK)
        return Error{std::string("FLAC encoder setup failed: ") + FLAC__StreamEncoderInitStatusString[init]};

    constexpr std::size_t kChunkFrames = 4096;
    const std::size_t channels = buffer.channels();
    const std::size_t frames = buffer.frameCount();
    std::vector<FLAC__int32> chunk(kChunkFrames * channels);
    for (std::size_t first = 0; first < frames; first += kChunkFrames) {
        const std::size_t count = std::min(kChunkFrames, frames - first);
        for (std::size_t i = 0; i < count * channels; ++i)
            chunk[i] = integerSample(buffer, first * channels + i, bits);
        if (!FLAC__stream_encoder_process_interleaved(e, chunk.data(), static_cast<uint32_t>(count)))
            return Error{std::string("FLAC encoding failed: ") +
                         FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(e)]};
    }
    if (!FLAC__stream_encoder_finish(e))
        return Error{std::string("FLAC encoding failed: ") +
                     FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(e)]};
    return std::move(sink.bytes);
}

} // namespace OFS::Formats::Detail
