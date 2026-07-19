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

SffDocument FixtureFactory::sff(SffVersion version, int spriteCount, int maxSide)
{
    SffDocument doc(version);
    if (version == SffVersion::V1) {
        doc.sharedPalette = m_rng.chance(50);
    } else {
        const std::uint16_t ids[][2] = {{1, 1}, {1, 2}, {9000, 0}};
        for (const auto& id : ids) {
            SffPalette p;
            p.group = id[0];
            p.number = id[1];
            p.colors = palette(id[0] == 9000 ? 32 : 256);
            // SFF v2 keeps the fourth byte of each color as alpha.
            for (Rgba& c : p.colors)
                c.a = m_rng.chance(50) ? 255 : 0;
            doc.palettes.push_back(std::move(p));
        }
        SffPalette linked = doc.palettes[0];
        linked.number = 3;
        linked.linkedIndex = 0;
        doc.palettes.push_back(std::move(linked));
    }

    std::vector<std::size_t> owners;
    for (int i = 0; i < spriteCount; ++i) {
        SffSprite s;
        s.group = static_cast<std::uint16_t>((i / 4) * 10);
        s.number = static_cast<std::uint16_t>(i % 4);
        s.axisX = static_cast<std::int16_t>(m_rng.range(-200, 200));
        s.axisY = static_cast<std::int16_t>(m_rng.range(-200, 200));

        if (i % 7 == 6 && !owners.empty()) {
            const std::size_t target = owners[m_rng.below(static_cast<std::uint32_t>(owners.size()))];
            const SffSprite& t = doc.sprites[target];
            s.linkedIndex = static_cast<std::int32_t>(target);
            s.width = t.width;
            s.height = t.height;
            s.compression = t.compression;
            s.colorDepth = t.colorDepth;
            const bool trueColor = t.image && t.image->isRgba();
            if (version == SffVersion::V1 || trueColor) {
                s.paletteIndex = t.paletteIndex;
            } else {
                // A v2 link may use another palette of the size its color depth needs. Elecbyte's tools ignore it
                // otherwise.
                static constexpr std::int32_t kFullPalettes[] = {0, 1, 3};
                s.paletteIndex = t.colorDepth == 5
                                     ? 2
                                     : kFullPalettes[m_rng.below(static_cast<std::uint32_t>(std::size(kFullPalettes)))];
            }
            doc.sprites.push_back(std::move(s));
            continue;
        }

        const int w = 1 + static_cast<int>(m_rng.below(static_cast<std::uint32_t>(maxSide)));
        const int h = 1 + static_cast<int>(m_rng.below(static_cast<std::uint32_t>(maxSide)));
        if (version == SffVersion::V1) {
            if (doc.palettes.empty() || i % 3 == 0) {
                SffPalette p;
                p.group = s.group;
                p.number = s.number;
                p.colors = palette(256);
                doc.palettes.push_back(std::move(p));
            }
            s.paletteIndex = static_cast<std::int32_t>(doc.palettes.size() - 1);
            s.compression = SffCompression::Pcx;
            s.image = indexedImage(w, h, 256, randomPattern());
        } else {
            static constexpr SffCompression kCycle[] = {SffCompression::Rle8, SffCompression::Lz5,
                                                        SffCompression::Rle5, SffCompression::Raw,
                                                        SffCompression::Png8, SffCompression::Png32};
            SffCompression c = kCycle[static_cast<std::size_t>(i) % std::size(kCycle)];
            if (version == SffVersion::V2 && (c == SffCompression::Png8 || c == SffCompression::Png32))
                c = SffCompression::Rle8;
            s.compression = c;
            if (c == SffCompression::Png32) {
                s.colorDepth = 32;
                s.paletteIndex = 0;
                s.image = rgbaImage(w, h);
            } else {
                const bool fiveBit = c == SffCompression::Lz5 || c == SffCompression::Rle5;
                s.colorDepth = fiveBit ? 5 : 8;
                s.paletteIndex = fiveBit ? 2 : static_cast<std::int32_t>(m_rng.below(2));
                s.image = indexedImage(w, h, fiveBit ? 32 : 256, randomPattern());
            }
            if (c != SffCompression::Raw && m_rng.chance(10))
                s.flags = 1; // data in the translated block (compressed sprites only)
        }
        s.width = static_cast<std::uint16_t>(w);
        s.height = static_cast<std::uint16_t>(h);
        owners.push_back(doc.sprites.size());
        doc.sprites.push_back(std::move(s));
    }
    return doc;
}

SndDocument FixtureFactory::snd(int soundCount)
{
    SndDocument doc;
    for (std::size_t i = 0; i < doc.comment.size(); ++i)
        doc.comment[i] = i < 20 ? static_cast<std::uint8_t>('a' + i) : 0;
    for (int i = 0; i < soundCount; ++i) {
        SndSound s;
        s.group = static_cast<std::int32_t>(m_rng.below(100));
        s.number = static_cast<std::int32_t>(m_rng.below(10));
        const auto bits = static_cast<std::uint16_t>(m_rng.chance(50) ? 8 : 16);
        s.data = wav(static_cast<std::uint16_t>(1 + m_rng.below(2)), 11025, bits, 1 + m_rng.below(300)).write();
        doc.sounds.push_back(std::move(s));
    }
    return doc;
}

WavFile FixtureFactory::wav(std::uint16_t channels, std::uint32_t sampleRate, std::uint16_t bitsPerSample,
                            std::size_t frames, bool floatingPoint)
{
    std::vector<double> samples(frames * channels);
    for (double& v : samples)
        v = static_cast<double>(m_rng.range(-1000, 1000)) / 1000.0;
    auto wav = WavFile::fromSamples(samples, channels, sampleRate, bitsPerSample, floatingPoint);
    WavFile result = wav ? std::move(*wav) : WavFile{};
    WavChunk list;
    list.id = {'L', 'I', 'S', 'T'};
    list.data = toBytes(std::string_view("INFOISFT\x05\x00\x00\x00test\x00", 17)); // odd size: needs a pad byte
    result.chunks = {WavChunk{{'f', 'm', 't', ' '}, {}}, WavChunk{{'d', 'a', 't', 'a'}, {}}, list};
    return result;
}

} // namespace OFS::Formats::Test
