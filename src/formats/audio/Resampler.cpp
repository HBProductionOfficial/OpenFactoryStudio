#include "formats/audio/Resampler.h"

#include "formats/audio/CodecInternal.h"

#include <samplerate.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

namespace OFS::Formats {

namespace {

int sincConverter(ResampleQuality quality) noexcept
{
    switch (quality) {
    case ResampleQuality::Medium:
        return SRC_SINC_MEDIUM_QUALITY;
    case ResampleQuality::Fastest:
        return SRC_SINC_FASTEST;
    default:
        return SRC_SINC_BEST_QUALITY;
    }
}

// Done by hand because libsamplerate's linear and zero-order-hold converters lag one input frame
// behind the sinc ones.
SampleBuffer interpolate(const SampleBuffer& buffer, double ratio, std::size_t outFrames, std::uint32_t outRate,
                         bool hold)
{
    const std::uint16_t channels = buffer.channels();
    const std::size_t last = buffer.frameCount() - 1;
    std::vector<float> out(outFrames * channels);
    for (std::size_t j = 0; j < outFrames; ++j) {
        const double position = static_cast<double>(j) / ratio;
        const std::size_t index = std::min(static_cast<std::size_t>(position), last);
        const std::size_t next = std::min(index + 1, last);
        const float fraction = hold ? 0.0f : static_cast<float>(position - static_cast<double>(index));
        for (std::uint16_t c = 0; c < channels; ++c) {
            const float a = buffer.sample(index, c);
            const float b = buffer.sample(next, c);
            out[j * channels + c] = a + fraction * (b - a);
        }
    }
    SampleBuffer result = SampleBuffer::fromFloat(std::move(out), channels, outRate);
    return buffer.type() == SampleType::Float32 ? result : convertSampleType(result, SampleType::Int16);
}

struct StateDeleter
{
    void operator()(SRC_STATE* state) const { src_delete(state); }
};

// libsamplerate's output can be a frame or two off at the end, so it is cut or padded to
// `outFrames`.
Result<SampleBuffer> convert(const SampleBuffer& buffer, double ratio, std::size_t outFrames, std::uint32_t outRate,
                             ResampleQuality quality)
{
    const std::size_t channels = buffer.channels();
    const std::size_t frames = buffer.frameCount();
    if (frames == 0 || outFrames == 0)
        return SampleBuffer(buffer.type(), buffer.channels(), outRate, outFrames);
    if (src_is_valid_ratio(ratio) == 0)
        return Error{"the resampling ratio must be between 1/256 and 256"};
    if (outFrames > Detail::kMaxDecodedSamples / channels)
        return Detail::tooLongError();
    if (quality == ResampleQuality::Linear || quality == ResampleQuality::ZeroOrderHold)
        return interpolate(buffer, ratio, outFrames, outRate, quality == ResampleQuality::ZeroOrderHold);

    int error = 0;
    std::unique_ptr<SRC_STATE, StateDeleter> state(src_new(sincConverter(quality), static_cast<int>(channels), &error));
    if (!state)
        return Error{std::string("cannot set up the resampler: ") + src_strerror(error)};

    constexpr std::size_t kChunkFrames = 4096;
    const auto outCapacity = static_cast<std::size_t>(std::ceil(static_cast<double>(kChunkFrames) * ratio)) + 64;
    std::vector<float> input;
    std::vector<float> chunk(outCapacity * channels);
    std::vector<float> result;
    result.reserve(outFrames * channels);
    const bool isFloat = buffer.type() == SampleType::Float32;
    if (!isFloat)
        input.resize(kChunkFrames * channels);

    std::size_t position = 0;
    for (;;) {
        const std::size_t count = std::min(kChunkFrames, frames - position);
        const float* in = nullptr;
        if (isFloat) {
            in = buffer.floatSamples().data() + position * channels;
        } else {
            const auto source = buffer.int16Samples().subspan(position * channels, count * channels);
            std::transform(source.begin(), source.end(), input.begin(), int16ToFloat);
            in = input.data();
        }
        SRC_DATA data{};
        data.data_in = in;
        data.input_frames = static_cast<long>(count);
        data.data_out = chunk.data();
        data.output_frames = static_cast<long>(outCapacity);
        data.end_of_input = position + count >= frames ? 1 : 0;
        data.src_ratio = ratio;
        if (const int status = src_process(state.get(), &data); status != 0)
            return Error{std::string("resampling failed: ") + src_strerror(status)};
        const auto produced = static_cast<std::size_t>(data.output_frames_gen);
        const std::size_t room = outFrames * channels - std::min(result.size(), outFrames * channels);
        const std::size_t keep = std::min(produced * channels, room);
        result.insert(result.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(keep));
        position += static_cast<std::size_t>(data.input_frames_used);
        if (data.end_of_input && produced == 0)
            break;
        if (!data.end_of_input && data.input_frames_used == 0 && produced == 0)
            return Error{"the resampler made no progress"};
    }
    result.resize(outFrames * channels, 0.0f);

    SampleBuffer out = SampleBuffer::fromFloat(std::move(result), buffer.channels(), outRate);
    if (!isFloat)
        out = convertSampleType(out, SampleType::Int16);
    return out;
}

} // namespace

Result<SampleBuffer> resample(const SampleBuffer& buffer, std::uint32_t targetRate, ResampleQuality quality)
{
    const std::uint32_t rate = buffer.sampleRate();
    if (rate == 0 || targetRate == 0)
        return Error{"sample rates must be greater than 0"};
    if (rate == targetRate)
        return buffer;
    const std::size_t frames = buffer.frameCount();
    if (frames > (std::uint64_t{1} << 32))
        return Detail::tooLongError();
    const std::uint64_t outFrames = (std::uint64_t{frames} * targetRate + rate / 2) / rate;
    return convert(buffer, static_cast<double>(targetRate) / rate, static_cast<std::size_t>(outFrames), targetRate,
                   quality);
}

Result<SampleBuffer> resampleToLength(const SampleBuffer& buffer, std::size_t frames, ResampleQuality quality)
{
    const std::size_t current = buffer.frameCount();
    if (frames == current)
        return buffer;
    if (current == 0)
        return SampleBuffer(buffer.type(), buffer.channels(), buffer.sampleRate(), frames);
    return convert(buffer, static_cast<double>(frames) / static_cast<double>(current), frames, buffer.sampleRate(),
                   quality);
}

} // namespace OFS::Formats
