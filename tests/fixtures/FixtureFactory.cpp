#include "fixtures/FixtureFactory.h"

#include <algorithm>

namespace OFS::Formats::Test {

PixelPattern FixtureFactory::randomPattern()
{
    switch (m_rng.below(5)) {
    case 0:
        return PixelPattern::Noise;
    case 1:
        return PixelPattern::Runs;
    case 2:
        return PixelPattern::SingleColor;
    case 3:
        return PixelPattern::Repeats;
    default:
        return PixelPattern::Mixed;
    }
}

Palette FixtureFactory::palette(std::size_t count)
{
    Palette p(count);
    for (Rgba& c : p)
        c = rgb(m_rng.byte(), m_rng.byte(), m_rng.byte());
    return p;
}

std::vector<std::uint8_t> FixtureFactory::pixels(std::size_t count, int colors, PixelPattern pattern)
{
    colors = std::clamp(colors, 1, 256);
    const auto colorBound = static_cast<std::uint32_t>(colors);
    auto color = [&] {
        return static_cast<std::uint8_t>(m_rng.below(colorBound));
    };
    std::vector<std::uint8_t> out;
    out.reserve(count);

    switch (pattern) {
    case PixelPattern::Noise:
        while (out.size() < count)
            out.push_back(color());
        break;
    case PixelPattern::SingleColor: {
        const std::uint8_t c = color();
        out.assign(count, c);
        break;
    }
    case PixelPattern::Runs:
        while (out.size() < count) {
            std::size_t length = 0;
            switch (m_rng.below(4)) {
            case 0:
                length = 1 + m_rng.below(8);
                break;
            case 1:
                length = 1 + m_rng.below(70);
                break;
            case 2: {
                static constexpr std::size_t kEdges[] = {62, 63, 64, 65, 127, 128, 255, 256, 257, 262, 263, 264, 265};
                length = kEdges[m_rng.below(static_cast<std::uint32_t>(std::size(kEdges)))];
                break;
            }
            default:
                length = 1 + m_rng.below(1200);
                break;
            }
            const std::uint8_t c = color();
            for (std::size_t k = 0; k < length && out.size() < count; ++k)
                out.push_back(c);
        }
        break;
    case PixelPattern::Repeats: {
        std::vector<std::uint8_t> motif(1 + m_rng.below(m_rng.chance(50) ? 12 : 400));
        for (std::uint8_t& m : motif)
            m = color();
        std::size_t k = 0;
        while (out.size() < count) {
            out.push_back(motif[k % motif.size()]);
            ++k;
            if (m_rng.chance(2))
                motif[m_rng.below(static_cast<std::uint32_t>(motif.size()))] = color();
            if (m_rng.chance(1))
                k += m_rng.below(5);
        }
        break;
    }
    case PixelPattern::Mixed:
        while (out.size() < count) {
            const std::size_t segment = std::min<std::size_t>(count - out.size(), 1 + m_rng.below(600));
            const auto kind = static_cast<PixelPattern>(m_rng.below(4));
            const std::vector<std::uint8_t> part = pixels(segment, colors, kind);
            out.insert(out.end(), part.begin(), part.end());
        }
        break;
    }
    out.resize(count);
    return out;
}

Image FixtureFactory::indexedImage(int width, int height, int colors, PixelPattern pattern)
{
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    return Image::indexed(width, height, pixels(count, colors, pattern), palette(256));
}

Image FixtureFactory::randomIndexedImage(int maxSide, int colors)
{
    int w = 1 + static_cast<int>(m_rng.below(static_cast<std::uint32_t>(maxSide)));
    int h = 1 + static_cast<int>(m_rng.below(static_cast<std::uint32_t>(maxSide)));
    switch (m_rng.below(10)) {
    case 0:
        w = 1;
        h = 1;
        break;
    case 1:
        w = 1;
        break;
    case 2:
        h = 1;
        break;
    default:
        break;
    }
    return indexedImage(w, h, colors, randomPattern());
}

Image FixtureFactory::rgbaImage(int width, int height)
{
    Image image(width, height, PixelFormat::Rgba32);
    for (std::uint8_t& b : image.pixels())
        b = m_rng.byte();
    for (std::size_t i = 0; i < image.pixelCount(); ++i) {
        if (m_rng.chance(20))
            image.pixels()[i * 4 + 3] = 0;
        else if (m_rng.chance(50))
            image.pixels()[i * 4 + 3] = 255;
    }
    return image;
}

} // namespace OFS::Formats::Test
