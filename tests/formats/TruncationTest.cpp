// Each prefix gets its own buffer of exact size, so out-of-bounds reads are not hidden.

#include "fixtures/FixtureFactory.h"
#include "formats/codecs/Bmp.h"
#include "formats/codecs/Gif.h"
#include "formats/codecs/Lz5.h"
#include "formats/codecs/Lzw.h"
#include "formats/codecs/Pcx.h"
#include "formats/codecs/Png.h"
#include "formats/codecs/Rle5.h"
#include "formats/codecs/Rle8.h"
#include "formats/fnt/FntDocument.h"
#include "formats/palette/PaletteFormats.h"
#include "formats/sff/SffDocument.h"
#include "formats/snd/SndDocument.h"
#include "formats/util/ByteWriter.h"
#include "formats/wav/WavFile.h"

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <string>

using namespace OFS::Formats;
using namespace OFS::Formats::Test;

namespace {

using Parser = std::function<bool(ByteSpan)>;

std::vector<std::size_t> acceptedPrefixes(const Bytes& full, const Parser& parse)
{
    std::vector<std::size_t> accepted;
    for (std::size_t n = 0; n < full.size(); ++n) {
        const Bytes prefix(full.begin(), full.begin() + static_cast<std::ptrdiff_t>(n));
        if (parse(prefix))
            accepted.push_back(n);
    }
    return accepted;
}

void requireTruncationsRejected(const Bytes& full, const Parser& parse)
{
    REQUIRE(parse(full));
    const std::vector<std::size_t> accepted = acceptedPrefixes(full, parse);
    INFO("file size " << full.size() << ", first accepted prefix "
                      << (accepted.empty() ? std::string("-") : std::to_string(accepted.front())));
    CHECK(accepted.empty());
}

bool parseSff(ByteSpan data)
{
    return SffDocument::read(data).ok();
}

void mutate(const Bytes& original, SplitMix64& rng, int rounds, const std::function<void(ByteSpan)>& exercise)
{
    for (int iteration = 0; iteration < rounds; ++iteration) {
        Bytes data = original;
        const int flips = 1 + static_cast<int>(rng.below(8));
        for (int k = 0; k < flips; ++k)
            data[rng.below(static_cast<std::uint32_t>(data.size()))] = std::byte{rng.byte()};
        if (rng.chance(20))
            data.resize(rng.below(static_cast<std::uint32_t>(data.size())));
        exercise(data);
    }
}

} // namespace

TEST_CASE("truncated SFF files are rejected", "[truncation][sff]")
{
    for (SffVersion version : {SffVersion::V1, SffVersion::V2, SffVersion::V201}) {
        for (std::uint64_t seed : {1u, 2u, 3u}) {
            INFO(sffVersionName(version) << " seed " << seed);
            FixtureFactory f(seed);
            auto bytes = f.sff(version, 9, 12).write();
            REQUIRE(bytes.ok());
            requireTruncationsRejected(*bytes, parseSff);
        }
    }
}

TEST_CASE("truncated SND, WAV and FNT files are rejected", "[truncation]")
{
    FixtureFactory f(4);
    requireTruncationsRejected(f.snd(5).write(), [](ByteSpan d) { return SndDocument::read(d).ok(); });
    requireTruncationsRejected(SndDocument{}.write(), [](ByteSpan d) { return SndDocument::read(d).ok(); });

    for (int bits : {8, 16}) {
        const Bytes wav = f.wav(2, 8000, static_cast<std::uint16_t>(bits), 37).write();
        requireTruncationsRejected(wav, [](ByteSpan d) { return WavFile::parse(d, WavParseOptions{true}).ok(); });
    }

    FntDocument font;
    font.pcx = *encodePcx(f.indexedImage(9, 5, 3, PixelPattern::Noise));
    font.text = "[Def]\nSize = 3,5\n[Map]\nA 0 3\nB 3 3\nC 6 3\n";
    requireTruncationsRejected(font.write(), [](ByteSpan d) { return FntDocument::read(d).ok(); });
}

TEST_CASE("truncated images and palettes are rejected", "[truncation]")
{
    FixtureFactory f(5);
    const Image img = f.indexedImage(13, 7, 256, PixelPattern::Mixed);

    requireTruncationsRejected(*encodePcx(img), [](ByteSpan d) { return decodePcx(d).ok(); });
    requireTruncationsRejected(*encodePng(img), [](ByteSpan d) { return decodePng(d).ok(); });
    requireTruncationsRejected(*encodePng(f.rgbaImage(6, 5)), [](ByteSpan d) { return decodePng(d).ok(); });

    const Palette palette = f.palette(256);
    requireTruncationsRejected(f.act(ActOrder::Reversed),
                               [](ByteSpan d) { return readAct(d, ActOrder::Reversed).ok(); });
    requireTruncationsRejected(writeRiffPal(palette), [](ByteSpan d) { return readRiffPal(d).ok(); });

    ByteWriter aco;
    aco.u16be(1);
    aco.u16be(3);
    for (int i = 0; i < 3; ++i) {
        aco.u16be(0);
        aco.u16be(static_cast<std::uint16_t>(i * 1000));
        aco.u16be(2);
        aco.u16be(3);
        aco.u16be(0);
    }
    requireTruncationsRejected(aco.data(), [](ByteSpan d) { return readAco(d).ok(); });
}

TEST_CASE("truncated GIF, BMP and swatch files are rejected", "[truncation]")
{
    FixtureFactory f(8);
    GifDocument gif;
    gif.globalPalette = f.palette(32);
    gif.loopCount = 0;
    Image first = f.indexedImage(11, 6, 32, PixelPattern::Mixed);
    first.setPalette(gif.globalPalette);
    gif.frames.push_back(GifFrame{first});
    gif.frames.back().delay = 5;
    Image second = f.indexedImage(5, 9, 8, PixelPattern::Noise);
    second.setPalette(f.palette(8));
    gif.frames.push_back(GifFrame{second, 2, 1});
    gif.frames.back().interlaced = true;
    gif.frames.back().transparentIndex = 3;
    auto gifBytes = writeGif(gif);
    REQUIRE(gifBytes.ok());
    requireTruncationsRejected(*gifBytes, [](ByteSpan d) { return readGif(d).ok(); });

    const Image indexed = f.indexedImage(7, 5, 200, PixelPattern::Mixed);
    requireTruncationsRejected(*encodeBmp(indexed), [](ByteSpan d) { return decodeBmp(d).ok(); });
    for (BmpColor color : {BmpColor::Rgb, BmpColor::Rgba}) {
        BmpEncodeOptions options;
        options.color = color;
        requireTruncationsRejected(*encodeBmp(f.rgbaImage(5, 3), options),
                                   [](ByteSpan d) { return decodeBmp(d).ok(); });
    }

    const Palette palette = f.palette(20);
    requireTruncationsRejected(writeAse(palette), [](ByteSpan d) { return readAse(d).ok(); });
    // An ACO file may end after its version 1 section.
    const Bytes aco = writeAco(palette);
    const std::vector<std::size_t> accepted = acceptedPrefixes(aco, [](ByteSpan d) { return readAco(d).ok(); });
    CHECK(accepted == std::vector<std::size_t>{4 + palette.size() * 10});

    for (int bits : {2, 5, 8}) {
        const std::vector<std::uint8_t> pixels = f.pixels(2000, 1 << bits, PixelPattern::Mixed);
        auto code = encodeGifLzw(pixels, bits);
        REQUIRE(code.ok());
        requireTruncationsRejected(*code, [&](ByteSpan d) {
            std::vector<std::uint8_t> out(pixels.size());
            return decodeGifLzw(d, bits, out).ok();
        });
    }
}

TEST_CASE("damaged GIF, BMP and swatch files never crash the parsers", "[truncation][fuzz]")
{
    FixtureFactory f(9);
    SplitMix64 rng(0x6B);

    GifDocument gif;
    gif.globalPalette = f.palette(64);
    for (int i = 0; i < 3; ++i) {
        Image frame = f.indexedImage(9 + i, 7, 64, f.randomPattern());
        frame.setPalette(i == 1 ? f.palette(64) : gif.globalPalette);
        gif.frames.push_back(GifFrame{frame, i, i});
        gif.frames.back().transparentIndex = i == 2 ? 5 : -1;
        gif.frames.back().disposal = static_cast<GifDisposal>(i + 1);
        gif.frames.back().interlaced = i == 0;
    }
    auto gifBytes = writeGif(gif);
    REQUIRE(gifBytes.ok());
    mutate(*gifBytes, rng, 400, [](ByteSpan d) {
        for (DecodeMode mode : {DecodeMode::Strict, DecodeMode::Lenient}) {
            auto doc = readGif(d, mode);
            if (!doc)
                continue;
            // Damaged headers can announce huge screens, which are slow to compose.
            if (std::uint64_t(doc->width) * std::uint64_t(doc->height) > 100000)
                continue;
            (void)renderGifFrames(*doc);
            (void)gifFrameOnScreen(*doc, 0);
            (void)writeGif(*doc);
        }
        (void)decodeGif(d);
        (void)paletteFromGif(d);
    });

    for (BmpColor color : {BmpColor::Automatic, BmpColor::Rgb, BmpColor::Rgba}) {
        BmpEncodeOptions options;
        options.color = color;
        auto bmp = encodeBmp(f.indexedImage(6, 4, 16, PixelPattern::Noise), options);
        REQUIRE(bmp.ok());
        mutate(*bmp, rng, 300, [](ByteSpan d) {
            (void)decodeBmp(d);
            (void)paletteFromBmp(d);
            // Lenient decoding allocates the announced size, which is slow for huge damaged headers.
            auto info = readBmpInfo(d);
            if (info && std::uint64_t(info->width) * std::uint64_t(info->height) <= 100000)
                (void)decodeBmp(d, DecodeMode::Lenient);
        });
    }

    mutate(writeAse(f.palette(12)), rng, 300, [](ByteSpan d) { (void)readAse(d); });
    mutate(writeAco(f.palette(12)), rng, 300, [](ByteSpan d) { (void)readAco(d); });
    auto code = encodeGifLzw(f.pixels(500, 16, PixelPattern::Mixed), 4);
    REQUIRE(code.ok());
    mutate(*code, rng, 300, [](ByteSpan d) {
        std::vector<std::uint8_t> out(500);
        (void)decodeGifLzw(d, 4, out);
        (void)decodeGifLzw(d, 4, out, DecodeMode::Lenient);
    });
    for (const char* head : {"GIF89a", "BM", "ASEF", "\x89PNG", "RIFF", "JASC-PAL"}) {
        for (int round = 0; round < 100; ++round) {
            Bytes data = toBytes(head);
            for (std::uint32_t k = 0, n = rng.below(64); k < n; ++k)
                data.push_back(std::byte{rng.byte()});
            (void)readPaletteFile(data);
        }
    }
    SUCCEED("no crashes");
}

TEST_CASE("truncated sprite streams are rejected", "[truncation][codec]")
{
    FixtureFactory f(6);
    for (int i = 0; i < 20; ++i) {
        const auto px256 = f.pixels(300, 256, f.randomPattern());
        const auto px32 = f.pixels(300, 32, f.randomPattern());
        const std::size_t n = px256.size();
        requireTruncationsRejected(encodeRle8(px256), [n](ByteSpan d) { return decodeRle8(d, n).ok(); });
        requireTruncationsRejected(*encodeRle5(px32), [n](ByteSpan d) { return decodeRle5(d, n).ok(); });
        requireTruncationsRejected(*encodeLz5(px32), [n](ByteSpan d) { return decodeLz5(d, n).ok(); });
    }
}

TEST_CASE("damaged files never crash the parsers", "[truncation][fuzz]")
{
    FixtureFactory f(7);
    SplitMix64 rng(99);

    for (SffVersion version : {SffVersion::V1, SffVersion::V2, SffVersion::V201}) {
        auto bytes = f.sff(version, 10, 16).write();
        REQUIRE(bytes.ok());
        mutate(*bytes, rng, 300, [](ByteSpan d) {
            auto doc = SffDocument::read(d);
            if (!doc)
                return;
            for (std::size_t i = 0; i < doc->sprites.size(); ++i) {
                (void)doc->decodeSprite(i, DecodeMode::Lenient);
                (void)doc->decodeSprite(i, DecodeMode::Strict);
            }
            (void)doc->write();
        });
    }

    mutate(f.snd(4).write(), rng, 300, [](ByteSpan d) { (void)SndDocument::read(d); });
    mutate(f.wav(1, 8000, 16, 50).write(), rng, 300, [](ByteSpan d) {
        auto wav = WavFile::parse(d);
        if (wav)
            (void)wav->toDoubles();
    });

    const Image img = f.indexedImage(17, 9, 32, PixelPattern::Mixed);
    mutate(*encodePcx(img), rng, 300, [](ByteSpan d) {
        (void)decodePcx(d, PcxPalette::Optional, DecodeMode::Lenient);
        (void)decodePcx(d);
    });
    mutate(*encodePng(img), rng, 300, [](ByteSpan d) { (void)decodePng(d); });
    const std::size_t n = img.pixelCount();
    mutate(encodeRle8(img.pixels()), rng, 300, [n](ByteSpan d) {
        (void)decodeRle8(d, n, DecodeMode::Lenient);
        (void)decodeRle8(d, n);
    });
    mutate(*encodeRle5(img.pixels()), rng, 300, [n](ByteSpan d) {
        (void)decodeRle5(d, n, DecodeMode::Lenient);
        (void)decodeRle5(d, n);
    });
    mutate(*encodeLz5(img.pixels()), rng, 300, [n](ByteSpan d) {
        (void)decodeLz5(d, n, DecodeMode::Lenient);
        (void)decodeLz5(d, n);
    });
    SUCCEED("no crashes");
}
