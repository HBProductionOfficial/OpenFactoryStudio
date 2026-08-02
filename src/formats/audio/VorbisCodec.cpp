// Ogg Vorbis: decoding with stb_vorbis, encoding with libvorbis (vorbisenc) and libogg.

#include "formats/audio/CodecInternal.h"

#include <ogg/ogg.h>
#include <vorbis/vorbisenc.h>

#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>

#include <algorithm>
#include <climits>
#include <memory>
#include <string>

namespace OFS::Formats::Detail {

namespace {

// Fixed so the same input always gives the same file.
constexpr int kStreamSerial = 0x4F465331;

struct VorbisCloser
{
    void operator()(stb_vorbis* decoder) const { stb_vorbis_close(decoder); }
};

void appendPage(Bytes& out, const ogg_page& page)
{
    const auto* header = reinterpret_cast<const std::byte*>(page.header);
    const auto* body = reinterpret_cast<const std::byte*>(page.body);
    out.insert(out.end(), header, header + page.header_len);
    out.insert(out.end(), body, body + page.body_len);
}

class VorbisEncoder
{
public:
    VorbisEncoder() { vorbis_info_init(&info); }
    ~VorbisEncoder()
    {
        if (m_stream)
            ogg_stream_clear(&stream);
        if (m_analysis) {
            vorbis_block_clear(&block);
            vorbis_dsp_clear(&dsp);
        }
        if (m_comment)
            vorbis_comment_clear(&comment);
        vorbis_info_clear(&info);
    }
    VorbisEncoder(const VorbisEncoder&) = delete;
    VorbisEncoder& operator=(const VorbisEncoder&) = delete;

    bool setUp(int channels, long sampleRate, float quality)
    {
        if (vorbis_encode_init_vbr(&info, channels, sampleRate, quality) != 0)
            return false;
        vorbis_comment_init(&comment);
        m_comment = true;
        vorbis_comment_add_tag(&comment, "ENCODER", "Open Factory Studio");
        if (vorbis_analysis_init(&dsp, &info) != 0)
            return false;
        vorbis_block_init(&dsp, &block);
        m_analysis = true;
        ogg_stream_init(&stream, kStreamSerial);
        m_stream = true;
        return true;
    }

    vorbis_info info{};
    vorbis_comment comment{};
    vorbis_dsp_state dsp{};
    vorbis_block block{};
    ogg_stream_state stream{};

private:
    bool m_comment = false;
    bool m_analysis = false;
    bool m_stream = false;
};

} // namespace

Result<DecodedAudio> decodeVorbis(ByteSpan data, const DecodeOptions& options)
{
    if (data.size() > static_cast<std::size_t>(INT_MAX))
        return Error{"Ogg Vorbis file is too large"};
    int error = 0;
    std::unique_ptr<stb_vorbis, VorbisCloser> vorbis(stb_vorbis_open_memory(
        reinterpret_cast<const unsigned char*>(data.data()), static_cast<int>(data.size()), &error, nullptr));
    if (!vorbis)
        return Error{"invalid Ogg Vorbis data (decoder error " + std::to_string(error) + ")"};
    const stb_vorbis_info info = stb_vorbis_get_info(vorbis.get());
    if (info.channels <= 0 || info.channels > 0xFFFF || info.sample_rate == 0)
        return Error{"invalid Ogg Vorbis stream information"};

    const auto channels = static_cast<std::size_t>(info.channels);
    constexpr std::size_t kChunkFrames = 4096;
    std::vector<float> chunk(kChunkFrames * channels);
    std::vector<float> samples;
    for (;;) {
        const int frames = stb_vorbis_get_samples_float_interleaved(vorbis.get(), info.channels, chunk.data(),
                                                                    static_cast<int>(chunk.size()));
        if (frames <= 0)
            break;
        const std::size_t count = static_cast<std::size_t>(frames) * channels;
        if (samples.size() + count > kMaxDecodedSamples)
            return tooLongError();
        samples.insert(samples.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(count));
    }

    DecodedAudio result;
    result.buffer = withRequestedType(
        SampleBuffer::fromFloat(std::move(samples), static_cast<std::uint16_t>(channels), info.sample_rate), options);
    result.format = AudioFileFormat::OggVorbis;
    result.codec = "Vorbis";
    return result;
}

Result<Bytes> encodeVorbis(const SampleBuffer& buffer, const EncodeOptions& options)
{
    const float quality = std::clamp(options.vorbisQuality, -0.1f, 1.0f);
    if (buffer.channels() > 255)
        return Error{"Ogg Vorbis files hold at most 255 channels"};
    if (buffer.sampleRate() == 0 || buffer.sampleRate() > static_cast<std::uint32_t>(INT_MAX))
        return Error{"invalid sample rate"};
    VorbisEncoder enc;
    if (!enc.setUp(buffer.channels(), static_cast<long>(buffer.sampleRate()), quality))
        return Error{"Vorbis cannot encode " + std::to_string(buffer.channels()) + " channel(s) at " +
                     std::to_string(buffer.sampleRate()) + " Hz with quality " + std::to_string(quality)};

    Bytes out;
    ogg_page page;
    // The three header packets go on pages of their own so the audio starts on a fresh page.
    ogg_packet identification;
    ogg_packet comments;
    ogg_packet codebooks;
    vorbis_analysis_headerout(&enc.dsp, &enc.comment, &identification, &comments, &codebooks);
    ogg_stream_packetin(&enc.stream, &identification);
    ogg_stream_packetin(&enc.stream, &comments);
    ogg_stream_packetin(&enc.stream, &codebooks);
    while (ogg_stream_flush(&enc.stream, &page) != 0)
        appendPage(out, page);

    constexpr std::size_t kChunkFrames = 1024;
    const std::size_t frames = buffer.frameCount();
    const std::uint16_t channels = buffer.channels();
    std::size_t position = 0;
    bool submittedEnd = false;
    bool endOfStream = false;
    while (!endOfStream) {
        if (position < frames) {
            const std::size_t count = std::min(kChunkFrames, frames - position);
            float** planes = vorbis_analysis_buffer(&enc.dsp, static_cast<int>(count));
            for (std::uint16_t c = 0; c < channels; ++c) {
                for (std::size_t f = 0; f < count; ++f)
                    planes[c][f] = buffer.sample(position + f, c);
            }
            vorbis_analysis_wrote(&enc.dsp, static_cast<int>(count));
            position += count;
        } else if (!submittedEnd) {
            vorbis_analysis_wrote(&enc.dsp, 0);
            submittedEnd = true;
        } else {
            while (ogg_stream_flush(&enc.stream, &page) != 0)
                appendPage(out, page);
            break;
        }
        while (vorbis_analysis_blockout(&enc.dsp, &enc.block) == 1) {
            vorbis_analysis(&enc.block, nullptr);
            vorbis_bitrate_addblock(&enc.block);
            ogg_packet packet;
            while (vorbis_bitrate_flushpacket(&enc.dsp, &packet) == 1) {
                ogg_stream_packetin(&enc.stream, &packet);
                while (ogg_stream_pageout(&enc.stream, &page) != 0) {
                    appendPage(out, page);
                    if (ogg_page_eos(&page))
                        endOfStream = true;
                }
            }
        }
    }
    return out;
}

} // namespace OFS::Formats::Detail
