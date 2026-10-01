#pragma once

// Generated test data for the formats library. Nothing here reads files.

#include "formats/image/Image.h"
#include "formats/palette/PaletteFormats.h"
#include "formats/sff/SffDocument.h"
#include "formats/snd/SndDocument.h"
#include "formats/util/Bytes.h"
#include "formats/wav/WavFile.h"

#include <cstdint>
#include <vector>

namespace OFS::Formats::Test {

// SplitMix64 gives the same output on every platform. The std distributions differ between
// standard libraries.
class SplitMix64
{
public:
    explicit SplitMix64(std::uint64_t seed)
        : m_state(seed)
    {}

    std::uint64_t next()
    {
        std::uint64_t z = (m_state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    std::uint32_t below(std::uint32_t bound)
    {
        if (bound == 0)
            return 0;
        return static_cast<std::uint32_t>(((next() >> 32) * bound) >> 32);
    }

    int range(int lo, int hi) { return lo + static_cast<int>(below(static_cast<std::uint32_t>(hi - lo + 1))); }

    bool chance(std::uint32_t percent) { return below(100) < percent; }
    std::uint8_t byte() { return static_cast<std::uint8_t>(next() >> 56); }

private:
    std::uint64_t m_state;
};

enum class PixelPattern
{
    Noise,
    Runs,
    SingleColor,
    Repeats,
    Mixed,
};

class FixtureFactory
{
public:
    explicit FixtureFactory(std::uint64_t seed = 0x0F5F0F5ull)
        : m_rng(seed)
    {}

    SplitMix64& rng() { return m_rng; }

    PixelPattern randomPattern();

    Palette palette(std::size_t count = 256);

    std::vector<std::uint8_t> pixels(std::size_t count, int colors, PixelPattern pattern);

    Image indexedImage(int width, int height, int colors, PixelPattern pattern);
    Image randomIndexedImage(int maxSide, int colors);
    Image rgbaImage(int width, int height);

    SffDocument sff(SffVersion version, int spriteCount, int maxSide = 48);

    SndDocument snd(int soundCount);
    WavFile wav(std::uint16_t channels, std::uint32_t sampleRate, std::uint16_t bitsPerSample, std::size_t frames,
                bool floatingPoint = false);

    Bytes act(ActOrder order = ActOrder::Reversed);

private:
    SplitMix64 m_rng;
};

} // namespace OFS::Formats::Test
