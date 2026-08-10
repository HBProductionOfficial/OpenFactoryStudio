#pragma once

#include "formats/audio/AudioExport.h"
#include "formats/audio/SampleBuffer.h"
#include "formats/util/Result.h"

#include <cstddef>
#include <cstdint>

namespace OFS::Formats {

// The three sinc converters come from libsamplerate. Linear and ZeroOrderHold have no
// anti-aliasing filter.
enum class ResampleQuality : std::uint8_t
{
    Best,
    Medium,
    Fastest,
    Linear,
    ZeroOrderHold,
};

// Gives exactly round(frames * targetRate / sampleRate) frames. The rate ratio must be between
// 1/256 and 256.
OFS_AUDIO_EXPORT Result<SampleBuffer> resample(const SampleBuffer& buffer, std::uint32_t targetRate,
                                               ResampleQuality quality = ResampleQuality::Best);

// Keeps the sample rate, so the pitch changes with the length.
OFS_AUDIO_EXPORT Result<SampleBuffer> resampleToLength(const SampleBuffer& buffer, std::size_t frames,
                                                       ResampleQuality quality = ResampleQuality::Best);

} // namespace OFS::Formats
