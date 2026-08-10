#include "formats/audio/WaveformPeaks.h"

#include <algorithm>
#include <cmath>

namespace OFS::Formats {

namespace {

// Empty (min > max) until a value is added. NaN samples compare false and are skipped.
struct Extremes
{
    float min = std::numeric_limits<float>::infinity();
    float max = -std::numeric_limits<float>::infinity();

    void add(float value) noexcept
    {
        if (value < min)
            min = value;
        if (value > max)
            max = value;
    }
    void add(const PeakPair& pair) noexcept
    {
        if (pair.min < min)
            min = pair.min;
        if (pair.max > max)
            max = pair.max;
    }
    PeakPair result() const noexcept { return min > max ? PeakPair{} : PeakPair{min, max}; }
};

void scan(const SampleBuffer& buffer, std::uint16_t channel, std::size_t first, std::size_t last, Extremes& extremes)
{
    if (first >= last)
        return;
    const std::size_t channels = buffer.channels();
    if (buffer.type() == SampleType::Int16) {
        const std::int16_t* s = buffer.int16Samples().data() + first * channels + channel;
        std::int16_t lo = *s;
        std::int16_t hi = *s;
        for (std::size_t f = first; f < last; ++f, s += channels) {
            lo = std::min(lo, *s);
            hi = std::max(hi, *s);
        }
        extremes.add(int16ToFloat(lo));
        extremes.add(int16ToFloat(hi));
        return;
    }
    const float* s = buffer.floatSamples().data() + first * channels + channel;
    for (std::size_t f = first; f < last; ++f, s += channels)
        extremes.add(*s);
}

void addBlocks(const std::vector<std::vector<PeakPair>>& levels, std::size_t channels, std::size_t level,
               std::size_t first, std::size_t last, std::uint16_t channel, Extremes& extremes)
{
    constexpr std::size_t kFactor = WaveformPeaks::kLevelFactor;
    for (;;) {
        const std::vector<PeakPair>& entries = levels[level];
        if (level + 1 < levels.size()) {
            const std::size_t parentFirst = (first + kFactor - 1) / kFactor;
            const std::size_t parentLast = std::min(last / kFactor, levels[level + 1].size() / channels);
            if (parentFirst < parentLast) {
                for (std::size_t k = first; k < parentFirst * kFactor; ++k)
                    extremes.add(entries[k * channels + channel]);
                for (std::size_t k = parentLast * kFactor; k < last; ++k)
                    extremes.add(entries[k * channels + channel]);
                ++level;
                first = parentFirst;
                last = parentLast;
                continue;
            }
        }
        for (std::size_t k = first; k < last; ++k)
            extremes.add(entries[k * channels + channel]);
        return;
    }
}

} // namespace

void WaveformPeaks::clear()
{
    m_channels = 0;
    m_levels.clear();
}

void WaveformPeaks::build(const SampleBuffer& buffer)
{
    clear();
    extend(buffer);
}

std::size_t WaveformPeaks::extend(const SampleBuffer& buffer, std::size_t maxFrames)
{
    if (buffer.channels() != m_channels) {
        clear();
        m_channels = buffer.channels();
    }
    if (m_levels.empty())
        m_levels.emplace_back();
    const std::size_t available = buffer.frameCount() / kBlockFrames;
    if (blocksAt(0) > available)
        invalidateFrom(available * kBlockFrames); // the buffer got shorter
    const std::size_t have = blocksAt(0);
    const std::size_t wanted = maxFrames / kBlockFrames + (maxFrames % kBlockFrames != 0 ? 1 : 0);
    const std::size_t target = have + std::min(available - have, wanted);
    if (target == have)
        return 0;

    std::vector<PeakPair>& level0 = m_levels[0];
    level0.resize(target * m_channels);
    for (std::size_t block = have; block < target; ++block)
        computeBlock(buffer, 0, block, level0.data() + block * m_channels);
    extendUpperLevels(buffer);
    return (target - have) * kBlockFrames;
}

void WaveformPeaks::invalidateFrom(std::size_t frame)
{
    // A block of level n covers kLevelFactor^n blocks of level 0 and stays valid when all of them
    // lie before `frame`.
    std::size_t keep = frame / kBlockFrames;
    for (std::vector<PeakPair>& entries : m_levels) {
        entries.resize(std::min(entries.size(), keep * m_channels));
        keep /= kLevelFactor;
    }
    while (m_levels.size() > 1 && m_levels.back().empty())
        m_levels.pop_back();
}

void WaveformPeaks::update(const SampleBuffer& buffer, SampleRange changed)
{
    if (buffer.channels() != m_channels || buffer.frameCount() / kBlockFrames < blocksAt(0)) {
        build(buffer);
        return;
    }
    const SampleRange r = changed.clipped(buffer.frameCount());
    if (r.empty())
        return;
    std::size_t first = r.start / kBlockFrames;
    std::size_t last = (r.end() - 1) / kBlockFrames;
    for (std::size_t level = 0; level < m_levels.size(); ++level) {
        const std::size_t blocks = blocksAt(level);
        if (first >= blocks)
            break;
        last = std::min(last, blocks - 1);
        for (std::size_t block = first; block <= last; ++block)
            computeBlock(buffer, level, block, m_levels[level].data() + block * m_channels);
        first /= kLevelFactor;
        last /= kLevelFactor;
    }
}

std::size_t WaveformPeaks::summarizedFrames() const noexcept
{
    return blocksAt(0) * kBlockFrames;
}

bool WaveformPeaks::isComplete(const SampleBuffer& buffer) const noexcept
{
    return m_channels == buffer.channels() && blocksAt(0) == buffer.frameCount() / kBlockFrames;
}

std::size_t WaveformPeaks::byteSize() const noexcept
{
    std::size_t bytes = 0;
    for (const std::vector<PeakPair>& entries : m_levels)
        bytes += entries.size() * sizeof(PeakPair);
    return bytes;
}

PeakPair WaveformPeaks::peak(const SampleBuffer& buffer, std::uint16_t channel, SampleRange range) const
{
    const std::size_t frames = buffer.frameCount();
    const SampleRange r = range.clipped(frames);
    if (r.empty() || channel >= buffer.channels())
        return {};
    Extremes extremes;
    const std::size_t summarized =
        m_channels == buffer.channels() ? std::min(blocksAt(0), frames / kBlockFrames) * kBlockFrames : 0;
    const std::size_t firstBlock = (r.start + kBlockFrames - 1) / kBlockFrames;
    const std::size_t endBlock = std::min(r.end(), summarized) / kBlockFrames;
    if (firstBlock < endBlock) {
        scan(buffer, channel, r.start, firstBlock * kBlockFrames, extremes);
        addBlocks(m_levels, m_channels, 0, firstBlock, endBlock, channel, extremes);
        scan(buffer, channel, endBlock * kBlockFrames, r.end(), extremes);
    } else {
        scan(buffer, channel, r.start, r.end(), extremes);
    }
    return extremes.result();
}

void WaveformPeaks::columns(const SampleBuffer& buffer, std::uint16_t channel, double firstFrame,
                            double framesPerColumn, std::span<PeakPair> out) const
{
    const auto frames = static_cast<double>(buffer.frameCount());
    for (std::size_t i = 0; i < out.size(); ++i) {
        const double x = firstFrame + static_cast<double>(i) * framesPerColumn;
        const double next = x + framesPerColumn;
        if (!(framesPerColumn > 0.0) || !(next > 0.0) || !(x < frames)) {
            out[i] = PeakPair{};
            continue;
        }
        const auto start = static_cast<std::size_t>(std::floor(std::max(x, 0.0)));
        const auto end = std::max(start + 1, static_cast<std::size_t>(std::floor(std::min(next, frames))));
        out[i] = peak(buffer, channel, SampleRange{start, end - start});
    }
}

std::size_t WaveformPeaks::blocksAt(std::size_t level) const noexcept
{
    return m_channels != 0 && level < m_levels.size() ? m_levels[level].size() / m_channels : 0;
}

void WaveformPeaks::computeBlock(const SampleBuffer& buffer, std::size_t level, std::size_t index, PeakPair* out) const
{
    for (std::uint16_t c = 0; c < m_channels; ++c) {
        Extremes extremes;
        if (level == 0) {
            scan(buffer, c, index * kBlockFrames, (index + 1) * kBlockFrames, extremes);
        } else {
            const std::vector<PeakPair>& below = m_levels[level - 1];
            for (std::size_t k = index * kLevelFactor; k < (index + 1) * kLevelFactor; ++k)
                extremes.add(below[k * m_channels + c]);
        }
        out[c] = extremes.result();
    }
}

void WaveformPeaks::extendUpperLevels(const SampleBuffer& buffer)
{
    for (std::size_t level = 1;; ++level) {
        const std::size_t complete = blocksAt(level - 1) / kLevelFactor;
        if (complete == 0)
            break;
        if (m_levels.size() <= level)
            m_levels.emplace_back();
        const std::size_t have = blocksAt(level);
        m_levels[level].resize(complete * m_channels);
        for (std::size_t block = have; block < complete; ++block)
            computeBlock(buffer, level, block, m_levels[level].data() + block * m_channels);
    }
}

} // namespace OFS::Formats
