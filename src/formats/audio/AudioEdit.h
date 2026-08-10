#pragma once

// Edits for the Sounds editor's waveform view. Each returns a new buffer so the editor can keep
// the old one for undo.

#include "formats/audio/AudioExport.h"
#include "formats/audio/Resampler.h"
#include "formats/audio/SampleBuffer.h"
#include "formats/util/Result.h"

#include <cstddef>

namespace OFS::Formats {

OFS_AUDIO_EXPORT SampleBuffer copyRange(const SampleBuffer& buffer, SampleRange range);

inline SampleBuffer trim(const SampleBuffer& buffer, SampleRange range)
{
    return copyRange(buffer, range);
}

OFS_AUDIO_EXPORT SampleBuffer removeRange(const SampleBuffer& buffer, SampleRange range);

OFS_AUDIO_EXPORT Result<SampleBuffer> conformTo(const SampleBuffer& clip, const SampleBuffer& like,
                                                ResampleQuality quality = ResampleQuality::Best);

OFS_AUDIO_EXPORT Result<SampleBuffer> insertBuffer(const SampleBuffer& buffer, std::size_t position,
                                                   const SampleBuffer& clip);

OFS_AUDIO_EXPORT Result<SampleBuffer> replaceRange(const SampleBuffer& buffer, SampleRange range,
                                                   const SampleBuffer& clip);

OFS_AUDIO_EXPORT SampleBuffer insertSilence(const SampleBuffer& buffer, std::size_t position, std::size_t frames);

OFS_AUDIO_EXPORT SampleBuffer fillSilence(const SampleBuffer& buffer, SampleRange range);

OFS_AUDIO_EXPORT SampleBuffer fadeIn(const SampleBuffer& buffer, SampleRange range);
OFS_AUDIO_EXPORT SampleBuffer fadeOut(const SampleBuffer& buffer, SampleRange range);

OFS_AUDIO_EXPORT SampleBuffer changeVolume(const SampleBuffer& buffer, SampleRange range, float gain);

OFS_AUDIO_EXPORT SampleBuffer normalize(const SampleBuffer& buffer, SampleRange range, float peak = 1.0f);

OFS_AUDIO_EXPORT SampleBuffer reverse(const SampleBuffer& buffer, SampleRange range);

OFS_AUDIO_EXPORT SampleBuffer moveRange(const SampleBuffer& buffer, SampleRange range, std::size_t position);

OFS_AUDIO_EXPORT Result<SampleBuffer> stretchRange(const SampleBuffer& buffer, SampleRange range, std::size_t frames,
                                                   ResampleQuality quality = ResampleQuality::Best);

OFS_AUDIO_EXPORT float peakLevel(const SampleBuffer& buffer, SampleRange range = kAllFrames);

OFS_AUDIO_EXPORT SampleRange nonSilentRange(const SampleBuffer& buffer, float threshold = 0.0f);

OFS_AUDIO_EXPORT SampleBuffer trimSilence(const SampleBuffer& buffer, float threshold = 0.0f);

OFS_AUDIO_EXPORT float decibelsToGain(float decibels) noexcept;
OFS_AUDIO_EXPORT float gainToDecibels(float gain) noexcept;

} // namespace OFS::Formats
