#include "formats/codecs/Gif.h"
#include "fixtures/FixtureFactory.h"
#include "formats/codecs/Lzw.h"
#include "formats/util/ByteWriter.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace OFS::Formats;
using namespace OFS::Formats::Test;

namespace {

Bytes hexBytes(std::string_view hex)
{
    Bytes out;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2)
        out.push_back(std::byte{static_cast<std::uint8_t>(std::stoi(std::string(hex.substr(i, 2)), nullptr, 16))});
    return out;
}

// The 10x10 sample picture and its LZW code stream found in many descriptions of the format.
const std::vector<std::uint8_t> kSampleIndices = {
    1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 1, 0,
    0, 0, 0, 2, 2, 2, 1, 1, 1, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 0, 0, 0, 0, 1,
    1, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1};
const char* const kSampleCode = "8C2D99872A1CDC33A00275EC95FAA8DE608C04914C01";

std::vector<std::uint8_t> decodeAll(ByteSpan code, int minCodeSize, std::size_t count)
{
    std::vector<std::uint8_t> out(count, 0xEE);
    auto produced = decodeGifLzw(code, minCodeSize, out);
    REQUIRE(produced.ok());
    CHECK(*produced == count);
    return out;
}

Bytes literalStream(const std::vector<std::uint8_t>& pixels, int minCodeSize)
{
    Bytes out;
    std::uint32_t acc = 0;
    int bits = 0;
    auto put = [&](unsigned code, int width) {
        acc |= code << bits;
        bits += width;
        while (bits >= 8) {
            out.push_back(std::byte{static_cast<std::uint8_t>(acc & 0xFF)});
            acc >>= 8;
            bits -= 8;
        }
    };
    const unsigned clearCode = 1u << minCodeSize;
    int width = minCodeSize + 1;
    unsigned next = clearCode + 2;
    put(clearCode, width);
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        put(pixels[i], width);
        if (i > 0 && next < 4096) {
            ++next;
            if (next == (1u << width) && width < 12)
                ++width;
        }
    }
    put(clearCode + 1, width);
    if (bits > 0)
        out.push_back(std::byte{static_cast<std::uint8_t>(acc & 0xFF)});
    return out;
}

GifFrame frame(Image image, int left = 0, int top = 0)
{
    GifFrame f;
    f.image = std::move(image);
    f.left = left;
    f.top = top;
    return f;
}

void requireSameRgb(const Palette& expected, const Palette& actual)
{
    REQUIRE(actual.size() >= expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        INFO("palette index " << i);
        CHECK(sameRgb(expected[i], actual[i]));
    }
}

} // namespace

TEST_CASE("GIF LZW matches a known code stream", "[gif][lzw]")
{
    const Bytes code = hexBytes(kSampleCode);
    CHECK(decodeAll(code, 2, kSampleIndices.size()) == kSampleIndices);
    auto encoded = encodeGifLzw(kSampleIndices, 2);
    REQUIRE(encoded.ok());
    CHECK(*encoded == code);

    auto one = encodeGifLzw(std::vector<std::uint8_t>{0}, 2);
    REQUIRE(one.ok());
    CHECK(*one == hexBytes("4401"));
}

TEST_CASE("GIF LZW round-trips every code size and pattern", "[gif][lzw]")
{
    FixtureFactory f(0x612);
    for (int bits = 2; bits <= 8; ++bits) {
        for (std::size_t count : {std::size_t{0}, std::size_t{1}, std::size_t{2}, std::size_t{5000}}) {
            for (int round = 0; round < 6; ++round) {
                const std::vector<std::uint8_t> pixels = f.pixels(count, 1 << bits, f.randomPattern());
                INFO("bits " << bits << ", count " << count << ", round " << round);
                auto code = encodeGifLzw(pixels, bits);
                REQUIRE(code.ok());
                CHECK(decodeAll(*code, bits, count) == pixels);
            }
        }
    }
}

TEST_CASE("GIF LZW clears a full dictionary and keeps decoding", "[gif][lzw]")
{
    FixtureFactory f(0x4096);
    const std::vector<std::uint8_t> noise = f.pixels(300000, 256, PixelPattern::Noise);
    auto code = encodeGifLzw(noise, 8);
    REQUIRE(code.ok());
    CHECK(decodeAll(*code, 8, noise.size()) == noise);

    // Some encoders go on without a clear code once the dictionary is full ("deferred clear").
    std::vector<std::uint8_t> literals;
    for (int i = 0; i < 6000; ++i)
        literals.push_back(static_cast<std::uint8_t>((i * 7) & 3));
    CHECK(decodeAll(literalStream(literals, 2), 2, literals.size()) == literals);
}

TEST_CASE("GIF LZW rejects bad input in strict mode", "[gif][lzw]")
{
    std::vector<std::uint8_t> out(4);
    CHECK_FALSE(encodeGifLzw(std::vector<std::uint8_t>{4}, 2).ok());
    CHECK_FALSE(encodeGifLzw(std::vector<std::uint8_t>{0}, 1).ok());
    CHECK_FALSE(encodeGifLzw(std::vector<std::uint8_t>{0}, 9).ok());
    CHECK_FALSE(decodeGifLzw(hexBytes("4401"), 9, out).ok());

    auto code = encodeGifLzw(std::vector<std::uint8_t>{1, 2, 3, 0}, 2);
    REQUIRE(code.ok());
    std::vector<std::uint8_t> small(2);
    CHECK_FALSE(decodeGifLzw(*code, 2, small).ok());
    auto lenient = decodeGifLzw(*code, 2, small, DecodeMode::Lenient);
    REQUIRE(lenient.ok());
    CHECK(*lenient == 2);
    CHECK(small == std::vector<std::uint8_t>{1, 2});
    std::vector<std::uint8_t> big(6, 9);
    CHECK_FALSE(decodeGifLzw(*code, 2, big).ok());
    auto partial = decodeGifLzw(*code, 2, big, DecodeMode::Lenient);
    REQUIRE(partial.ok());
    CHECK(*partial == 4);
    CHECK(big[5] == 9);
    // 0b111 right after a clear (4) with 3-bit codes is not in the dictionary yet.
    CHECK_FALSE(decodeGifLzw(hexBytes("3C"), 2, out).ok());
}

TEST_CASE("the smallest GIF decodes", "[gif]")
{
    // 1x1, two-color global table, graphic control with transparent index 0.
    const Bytes file = hexBytes("474946383961"
                                "0100"
                                "0100"
                                "80"
                                "00"
                                "00"
                                "000000"
                                "FFFFFF"
                                "21F904"
                                "01"
                                "0000"
                                "00"
                                "00"
                                "2C"
                                "0000"
                                "0000"
                                "0100"
                                "0100"
                                "00"
                                "02"
                                "024401"
                                "00"
                                "3B");
    auto gif = readGif(file);
    REQUIRE(gif.ok());
    CHECK(gif->width == 1);
    CHECK(gif->height == 1);
    REQUIRE(gif->globalPalette.size() == 2);
    CHECK(gif->globalPalette[1] == rgb(255, 255, 255));
    REQUIRE(gif->frames.size() == 1);
    CHECK(gif->frames[0].transparentIndex == 0);
    CHECK(gif->frames[0].image.pixelBuffer() == std::vector<std::uint8_t>{0});
    CHECK(gif->frames[0].image.palette()[0].a == 0);
    CHECK(gif->frames[0].image.palette()[1].a == 255);
    CHECK(gif->loopCount == -1);

    auto image = decodeGif(file);
    REQUIRE(image.ok());
    CHECK(image->width() == 1);
    CHECK(image->colorAt(0, 0).a == 0);
}

TEST_CASE("GIF files round-trip frames, palettes and timing", "[gif]")
{
    FixtureFactory f(0x61F);
    GifDocument gif;
    gif.width = 40;
    gif.height = 30;
    gif.globalPalette = f.palette(256);
    gif.backgroundIndex = 7;
    gif.loopCount = 0;

    Image full = f.indexedImage(40, 30, 256, PixelPattern::Mixed);
    full.setPalette(gif.globalPalette);
    gif.frames.push_back(frame(full));
    gif.frames.back().delay = 10;
    Image local = f.indexedImage(13, 7, 16, PixelPattern::Runs);
    local.setPalette(f.palette(16));
    gif.frames.push_back(frame(local, 5, 9));
    gif.frames.back().delay = 3;
    gif.frames.back().disposal = GifDisposal::Background;
    Image interlaced = f.indexedImage(17, 19, 256, PixelPattern::Noise);
    interlaced.setPalette(gif.globalPalette);
    gif.frames.push_back(frame(interlaced, 20, 1));
    gif.frames.back().interlaced = true;
    gif.frames.back().transparentIndex = 255;
    gif.frames.back().disposal = GifDisposal::Previous;
    Image dot = Image::indexed(1, 1, {1}, Palette{rgb(1, 2, 3), rgb(4, 5, 6)});
    gif.frames.push_back(frame(dot, 39, 29));
    gif.frames.back().disposal = GifDisposal::Keep;
    gif.frames.back().delay = 65535;

    auto bytes = writeGif(gif);
    REQUIRE(bytes.ok());
    CHECK(toText(ByteSpan(*bytes).first(6)) == "GIF89a");
    auto back = readGif(*bytes);
    REQUIRE(back.ok());
    CHECK(back->width == 40);
    CHECK(back->height == 30);
    CHECK(back->backgroundIndex == 7);
    CHECK(back->loopCount == 0);
    CHECK(back->globalPalette == gif.globalPalette);
    REQUIRE(back->frames.size() == gif.frames.size());
    for (std::size_t i = 0; i < gif.frames.size(); ++i) {
        INFO("frame " << i);
        const GifFrame& a = gif.frames[i];
        const GifFrame& b = back->frames[i];
        CHECK(b.left == a.left);
        CHECK(b.top == a.top);
        CHECK(b.delay == a.delay);
        CHECK(b.disposal == a.disposal);
        CHECK(b.transparentIndex == a.transparentIndex);
        CHECK(b.interlaced == a.interlaced);
        CHECK(b.image.width() == a.image.width());
        CHECK(b.image.height() == a.image.height());
        CHECK(b.image.pixelBuffer() == a.image.pixelBuffer());
        requireSameRgb(a.image.palette(), b.image.palette());
    }
    CHECK_FALSE(back->frames[0].localPalette);
    CHECK(back->frames[1].localPalette);
    CHECK(back->frames[1].image.palette().size() == 16);
    CHECK_FALSE(back->frames[2].localPalette);
    CHECK(back->frames[2].image.palette()[255].a == 0);
    CHECK(back->frames[3].localPalette);
    CHECK(back->frames[3].image.palette().size() == 2);

    auto again = writeGif(*back);
    REQUIRE(again.ok());
    CHECK(*again == *bytes);
}

TEST_CASE("GIF writing fills in sizes, tables and loop counts", "[gif]")
{
    GifDocument gif;
    gif.frames.push_back(frame(Image::indexed(3, 2, {0, 1, 2, 3, 4, 5}, grayscalePalette(6)), 2, 1));
    auto bytes = writeGif(gif);
    REQUIRE(bytes.ok());
    CHECK(toText(*bytes).find("NETSCAPE2.0") == std::string::npos);
    CHECK(toText(*bytes).find("\x21\xF9") == std::string::npos);
    auto back = readGif(*bytes);
    REQUIRE(back.ok());
    CHECK(back->width == 5);
    CHECK(back->height == 3);
    CHECK(back->globalPalette.empty());
    REQUIRE(back->frames.size() == 1);
    CHECK(back->frames[0].localPalette);
    CHECK(back->frames[0].image.palette().size() == 8);
    CHECK(back->frames[0].image.pixelBuffer() == gif.frames[0].image.pixelBuffer());

    GifDocument wide;
    wide.frames.push_back(frame(Image::indexed(2, 1, {0, 200}, Palette{rgb(9, 9, 9)})));
    wide.loopCount = 3;
    auto wideBytes = writeGif(wide);
    REQUIRE(wideBytes.ok());
    auto wideBack = readGif(*wideBytes);
    REQUIRE(wideBack.ok());
    CHECK(wideBack->loopCount == 3);
    CHECK(wideBack->frames[0].image.palette().size() == 256);
    CHECK(wideBack->frames[0].image.pixelBuffer() == std::vector<std::uint8_t>{0, 200});

    GifDocument differs;
    differs.globalPalette = {rgb(0, 0, 0), rgb(255, 255, 255)};
    differs.frames.push_back(frame(Image::indexed(1, 1, {1}, Palette{rgb(0, 0, 0), rgb(1, 1, 1)})));
    auto differsBack = readGif(*writeGif(differs));
    REQUIRE(differsBack.ok());
    CHECK(differsBack->frames[0].localPalette);
    CHECK(differsBack->frames[0].image.palette()[1] == rgb(1, 1, 1));
}

TEST_CASE("GIF writing rejects what the format cannot store", "[gif]")
{
    GifDocument empty;
    CHECK_FALSE(writeGif(empty).ok());

    GifDocument rgbaFrame;
    rgbaFrame.frames.push_back(frame(Image(2, 2, PixelFormat::Rgba32)));
    CHECK_FALSE(writeGif(rgbaFrame).ok());

    GifDocument nullFrame;
    nullFrame.frames.push_back(frame(Image(0, 2, PixelFormat::Indexed8)));
    CHECK_FALSE(writeGif(nullFrame).ok());

    GifDocument bigPalette;
    bigPalette.frames.push_back(frame(Image::indexed(1, 1, {0}, Palette(257))));
    CHECK_FALSE(writeGif(bigPalette).ok());

    GifDocument negative;
    negative.frames.push_back(frame(Image::indexed(1, 1, {0}), -1, 0));
    CHECK_FALSE(writeGif(negative).ok());
}

TEST_CASE("GIF frames are composed with their disposal methods", "[gif]")
{
    const Palette palette = {rgb(0, 0, 0), rgb(255, 0, 0), rgb(0, 255, 0), rgb(0, 0, 255)};
    GifDocument gif;
    gif.width = 3;
    gif.height = 1;
    gif.globalPalette = palette;
    gif.frames.push_back(frame(Image::indexed(3, 1, {1, 1, 1}, palette)));
    gif.frames.back().disposal = GifDisposal::Keep;
    gif.frames.push_back(frame(Image::indexed(1, 1, {2}, palette), 1, 0));
    gif.frames.back().disposal = GifDisposal::Background;
    gif.frames.push_back(frame(Image::indexed(2, 1, {0, 3}, palette), 1, 0));
    gif.frames.back().transparentIndex = 0;
    gif.frames.back().disposal = GifDisposal::Previous;
    gif.frames.push_back(frame(Image::indexed(1, 1, {0}, palette), 0, 0));
    gif.frames.back().transparentIndex = 0;

    auto back = readGif(*writeGif(gif));
    REQUIRE(back.ok());
    const std::vector<Image> shown = renderGifFrames(*back);
    REQUIRE(shown.size() == 4);
    const Rgba red = rgb(255, 0, 0);
    const Rgba clear = rgb(0, 0, 0, 0);
    CHECK(shown[0].colorAt(0, 0) == red);
    CHECK(shown[0].colorAt(2, 0) == red);
    CHECK(shown[1].colorAt(1, 0) == rgb(0, 255, 0));
    CHECK(shown[2].colorAt(1, 0) == clear);
    CHECK(shown[2].colorAt(2, 0) == rgb(0, 0, 255));
    CHECK(shown[3].colorAt(0, 0) == red);
    CHECK(shown[3].colorAt(1, 0) == clear);
    CHECK(shown[3].colorAt(2, 0) == red);

    const GifFrame placed = gifFrameOnScreen(*back, 2);
    CHECK(placed.image.width() == 3);
    CHECK(placed.image.pixelBuffer() == std::vector<std::uint8_t>{0, 0, 3});
    CHECK(gifFrameOnScreen(*back, 1).image.pixelBuffer() == std::vector<std::uint8_t>{0, 2, 0});
    CHECK(gifFrameOnScreen(*back, 9).image.isNull());
}

TEST_CASE("lenient GIF reading keeps what a damaged file holds", "[gif]")
{
    FixtureFactory f(0xBAD);
    GifDocument gif;
    gif.globalPalette = f.palette(64);
    Image first = f.indexedImage(30, 20, 64, PixelPattern::Noise);
    first.setPalette(gif.globalPalette);
    gif.frames.push_back(frame(first));
    gif.frames.push_back(frame(first));
    auto bytes = writeGif(gif);
    REQUIRE(bytes.ok());

    const Bytes cut(bytes->begin(), bytes->end() - 200);
    CHECK_FALSE(readGif(cut).ok());
    auto lenient = readGif(cut, DecodeMode::Lenient);
    REQUIRE(lenient.ok());
    REQUIRE(lenient->frames.size() == 2);
    CHECK(lenient->frames[0].image.pixelBuffer() == first.pixelBuffer());
    CHECK(lenient->frames[1].image.width() == 30);

    const Bytes noTrailer(bytes->begin(), bytes->end() - 1);
    CHECK_FALSE(readGif(noTrailer).ok());
    CHECK(readGif(noTrailer, DecodeMode::Lenient).ok());
    CHECK(decodeGif(cut).ok());

    CHECK_FALSE(readGif(toBytes("GIF87a")).ok());
    CHECK_FALSE(readGif(hexBytes("474946383961"
                                 "0100"
                                 "0100"
                                 "000000"
                                 "3B"))
                    .ok());
    CHECK_FALSE(readGif(toBytes("PNG")).ok());
}

TEST_CASE("GIF screens of size 0 take the size of their frames", "[gif]")
{
    ByteWriter w;
    w.text("GIF87a");
    w.u16(0);
    w.u16(0);
    w.u8(0x80); // global table with 2 entries
    w.u8(0);
    w.u8(0);
    w.zeros(6);
    w.u8(0x2C);
    w.u16(2);
    w.u16(3);
    w.u16(1);
    w.u16(1);
    w.u8(0);
    w.u8(2);
    w.u8(2);
    w.bytes(hexBytes("4401"));
    w.u8(0);
    w.u8(0x3B);
    auto gif = readGif(w.data());
    REQUIRE(gif.ok());
    CHECK(gif->width == 3);
    CHECK(gif->height == 4);
    CHECK(gif->frames[0].left == 2);
}
