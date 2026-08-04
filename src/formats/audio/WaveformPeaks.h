#pragma once

#include "formats/audio/AudioExport.h"
#include "formats/audio/SampleBuffer.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace OFS::Formats {

struct PeakPair
{
    float min = 0.0f;
    float max = 0.0f;

    friend bool operator==(const PeakPair&, const PeakPair&) = default;
};

// Min/max levels of a SampleBuffer for drawing waveforms at any zoom. Frames not summarized yet
// are read from the buffer, which must be the one the summary was built from. Not thread-safe.
class OFS_AUDIO_EXPORT WaveformPeaks
{
public:
    static constexpr std::size_t kBlockFrames = 256;
    static constexpr std::size_t kLevelFactor = 4;

    void clear();
    void build(const SampleBuffer& buffer);
    std::size_t extend(const SampleBuffer& buffer, std::size_t maxFrames = std::numeric_limits<std::size_t>::max());
    // For edits that move every frame from `frame` on, like an insert or delete.
    void invalidateFrom(std::size_t frame);
    // For edits that keep the buffer length.
    void update(const SampleBuffer& buffer, SampleRange changed);

    std::uint16_t channels() const noexcept { return m_channels; }
    std::size_t summarizedFrames() const noexcept;
    bool isComplete(const SampleBuffer& buffer) const noexcept;
    std::size_t levelCount() const noexcept { return m_levels.size(); }
    std::size_t byteSize() const noexcept;

    PeakPair peak(const SampleBuffer& buffer, std::uint16_t channel, SampleRange range) const;

    // Column i covers [floor(x), floor(x + framesPerColumn)) with x = firstFrame + i *
    // framesPerColumn. Both can be fractional for smooth scrolling and zooming.
    void columns(const SampleBuffer& buffer, std::uint16_t channel, double firstFrame, double framesPerColumn,
                 std::span<PeakPair> out) const;

private:
    std::size_t blocksAt(std::size_t level) const noexcept;
    void computeBlock(const SampleBuffer& buffer, std::size_t level, std::size_t index, PeakPair* out) const;
    void extendUpperLevels(const SampleBuffer& buffer);

    std::uint16_t m_channels = 0;
    // m_levels[level][block * channels + channel]
    std::vector<std::vector<PeakPair>> m_levels;
};

} // namespace OFS::Formats
