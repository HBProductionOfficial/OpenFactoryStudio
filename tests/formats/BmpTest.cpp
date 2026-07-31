#include "formats/codecs/Bmp.h"
#include "fixtures/FixtureFactory.h"
#include "formats/util/ByteWriter.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace OFS::Formats;
using namespace OFS::Formats::Test;

namespace {

// `tableEntries` is the number of colors the caller writes (`colors` by default).
ByteWriter infoHeader(int width, int height, int bpp, std::uint32_t compression, std::uint32_t colors,
                      std::uint32_t extraHeaderBytes = 0, std::uint32_t tableEntries = 0xFFFFFFFF)
{
    if (tableEntries == 0xFFFFFFFF)
        tableEntries = colors;
    ByteWriter w;
    w.text("BM");
    w.u32(0); // file size
    w.u32(0);
    w.u32(14 + 40 + extraHeaderBytes + tableEntries * 4);
    w.u32(40);
    w.s32(width);
    w.s32(height);
    w.u16(1);
    w.u16(static_cast<std::uint16_t>(bpp));
    w.u32(compression);
    w.u32(0);
    w.s32(2835);
    w.s32(2835);
    w.u32(colors);
    w.u32(0);
    return w;
}

void color(ByteWriter& w, std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    w.u8(b);
    w.u8(g);
    w.u8(r);
    w.u8(0);
}

} // namespace

TEST_CASE("8-bit BMP files keep palettes and indices exactly", "[bmp]")
{
    FixtureFactory f(0xB4F);
    for (int width = 1; width <= 9; ++width) {
        for (std::size_t colors : {std::size_t{2}, std::size_t{32}, std::size_t{256}}) {
            Image image = f.indexedImage(width, 3, static_cast<int>(colors), PixelPattern::Noise);
            image.setPalette(f.palette(colors));
            INFO("width " << width << ", colors " << colors);
            auto bytes = encodeBmp(image);
            REQUIRE(bytes.ok());
            auto info = readBmpInfo(*bytes);
            REQUIRE(info.ok());
            CHECK(info->bitsPerPixel == 8);
            CHECK_FALSE(info->topDown);
            auto back = decodeBmp(*bytes);
            REQUIRE(back.ok());
            REQUIRE(back->isIndexed());
            CHECK(back->pixelBuffer() == image.pixelBuffer());
            REQUIRE(back->palette().size() >= image.maxIndex() + std::size_t{1});
            for (std::size_t i = 0; i < back->palette().size(); ++i)
                CHECK(back->palette()[i] == (i < colors ? image.palette()[i] : rgb(0, 0, 0)));
        }
    }
    const Palette same = {rgb(5, 5, 5), rgb(5, 5, 5), rgb(5, 5, 5)};
    const Image dup = Image::indexed(3, 1, {2, 1, 0}, same);
    auto back = decodeBmp(*encodeBmp(dup));
    REQUIRE(back.ok());
    CHECK(back->pixelBuffer() == dup.pixelBuffer());
    CHECK(back->palette() == same);
}

TEST_CASE("24 and 32-bit BMP files round-trip", "[bmp]")
{
    FixtureFactory f(0xB24);
    const Image rgba = f.rgbaImage(7, 5);

    BmpEncodeOptions rgbOptions;
    rgbOptions.color = BmpColor::Rgb;
    auto rgbBytes = encodeBmp(rgba, rgbOptions);
    REQUIRE(rgbBytes.ok());
    CHECK(readBmpInfo(*rgbBytes)->bitsPerPixel == 24);
    auto rgbBack = decodeBmp(*rgbBytes);
    REQUIRE(rgbBack.ok());
    REQUIRE(rgbBack->isRgba());
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 7; ++x) {
            Rgba expected = rgba.colorAt(x, y);
            expected.a = 255;
            CHECK(rgbBack->colorAt(x, y) == expected);
        }
    }

    auto rgbaBytes = encodeBmp(rgba);
    REQUIRE(rgbaBytes.ok());
    auto info = readBmpInfo(*rgbaBytes);
    REQUIRE(info.ok());
    CHECK(info->bitsPerPixel == 32);
    CHECK(info->headerSize == 108);
    CHECK(info->compression == BmpCompression::Bitfields);
    CHECK(info->alphaMask == 0xFF000000u);
    auto rgbaBack = decodeBmp(*rgbaBytes);
    REQUIRE(rgbaBack.ok());
    CHECK(*rgbaBack == rgba);

    const Image indexed = Image::indexed(2, 1, {0, 1}, Palette{rgb(1, 2, 3), rgb(4, 5, 6)});
    BmpEncodeOptions alpha;
    alpha.color = BmpColor::Rgba;
    alpha.transparentIndex0 = true;
    auto converted = decodeBmp(*encodeBmp(indexed, alpha));
    REQUIRE(converted.ok());
    CHECK(converted->colorAt(0, 0) == rgb(1, 2, 3, 0));
    CHECK(converted->colorAt(1, 0) == rgb(4, 5, 6, 255));

    CHECK_FALSE(encodeBmp(Image()).ok());
}

TEST_CASE("BMP decoding handles the classic layouts", "[bmp]")
{
    SECTION("24-bit bottom-up and top-down")
    {
        for (int sign : {1, -1}) {
            ByteWriter w = infoHeader(2, 2 * sign, 24, 0, 0);
            // Rows are padded to 4 bytes.
            const std::uint8_t rows[2][8] = {{0, 0, 255, 0, 255, 0, 0, 0}, {255, 0, 0, 10, 20, 30, 0, 0}};
            for (const auto& row : rows)
                w.bytes(std::span<const std::uint8_t>(row));
            auto image = decodeBmp(w.data());
            REQUIRE(image.ok());
            const int first = sign > 0 ? 1 : 0;
            CHECK(image->colorAt(0, first) == rgb(255, 0, 0));
            CHECK(image->colorAt(1, first) == rgb(0, 255, 0));
            CHECK(image->colorAt(0, 1 - first) == rgb(0, 0, 255));
            CHECK(image->colorAt(1, 1 - first) == rgb(30, 20, 10));
            CHECK(readBmpInfo(w.data())->topDown == (sign < 0));
        }
    }

    SECTION("1 and 4-bit images")
    {
        ByteWriter w1 = infoHeader(10, 1, 1, 0, 2);
        color(w1, 0, 0, 0);
        color(w1, 255, 255, 255);
        w1.u8(0b10110000);
        w1.u8(0b01000000);
        w1.zeros(2);
        auto mono = decodeBmp(w1.data());
        REQUIRE(mono.ok());
        CHECK(mono->pixelBuffer() == std::vector<std::uint8_t>{1, 0, 1, 1, 0, 0, 0, 0, 0, 1});
        CHECK(mono->palette().size() == 2);

        ByteWriter w4 = infoHeader(3, 1, 4, 0, 0, 0, 16); // 0 colors used: the full 16-entry table
        for (int i = 0; i < 16; ++i)
            color(w4, static_cast<std::uint8_t>(i), 0, 0);
        w4.u8(0x1F);
        w4.u8(0xA0);
        w4.zeros(2);
        auto nibbles = decodeBmp(w4.data());
        REQUIRE(nibbles.ok());
        CHECK(nibbles->pixelBuffer() == std::vector<std::uint8_t>{1, 15, 10});
        CHECK(nibbles->palette().size() == 16);
    }

    SECTION("RLE8 with runs, absolute runs, deltas and line ends")
    {
        ByteWriter w = infoHeader(4, 3, 8, 1, 4);
        for (int i = 0; i < 4; ++i)
            color(w, static_cast<std::uint8_t>(i * 50), 0, 0);
        const std::uint8_t data[] = {
            3, 2,          // bottom row: 2 2 2
            0, 0,          // end of line
            0, 3, 1, 2, 3, // absolute run of 3 (odd: one padding byte follows)
            0, 0, 0,       // padding, end of line
            0, 2, 1, 0,    // delta: one pixel right
            2, 1,          // top row: . 1 1
            0, 1,          // end of bitmap
        };
        w.bytes(std::span<const std::uint8_t>(data));
        auto image = decodeBmp(w.data());
        REQUIRE(image.ok());
        CHECK(image->pixelBuffer() == std::vector<std::uint8_t>{0, 1, 1, 0, 1, 2, 3, 0, 2, 2, 2, 0});

        const Bytes cut(w.data().begin(), w.data().end() - 2);
        CHECK_FALSE(decodeBmp(cut).ok());
        auto lenient = decodeBmp(cut, DecodeMode::Lenient);
        REQUIRE(lenient.ok());
        CHECK(lenient->pixelBuffer() == image->pixelBuffer());
    }

    SECTION("RLE4")
    {
        ByteWriter w = infoHeader(7, 1, 4, 2, 16);
        for (int i = 0; i < 16; ++i)
            color(w, static_cast<std::uint8_t>(i), 0, 0);
        const std::uint8_t data[] = {
            3, 0x12,             // 1 2 1
            0, 3,    0x34, 0x50, // absolute run of 3 nibbles: 3 4 5 (two bytes, already aligned)
            1, 0x90,             // a run of one: 9
            0, 1,                // end of bitmap
        };
        w.bytes(std::span<const std::uint8_t>(data));
        auto image = decodeBmp(w.data());
        REQUIRE(image.ok());
        CHECK(image->pixelBuffer() == std::vector<std::uint8_t>{1, 2, 1, 3, 4, 5, 9});

        ByteWriter bad = infoHeader(2, 1, 4, 2, 16);
        for (int i = 0; i < 16; ++i)
            color(bad, 0, 0, 0);
        const std::uint8_t overlong[] = {5, 0x11, 0, 1};
        bad.bytes(std::span<const std::uint8_t>(overlong));
        CHECK_FALSE(decodeBmp(bad.data()).ok());
        CHECK(decodeBmp(bad.data(), DecodeMode::Lenient).ok());
    }

    SECTION("16-bit 5-5-5 and 5-6-5 bit fields")
    {
        ByteWriter w = infoHeader(2, 1, 16, 0, 0);
        w.u16(0x7C00); // red
        w.u16(0x001F); // blue
        auto image = decodeBmp(w.data());
        REQUIRE(image.ok());
        CHECK(image->colorAt(0, 0) == rgb(255, 0, 0));
        CHECK(image->colorAt(1, 0) == rgb(0, 0, 255));

        ByteWriter f = infoHeader(1, 1, 16, 3, 0, 12);
        f.u32(0xF800);
        f.u32(0x07E0);
        f.u32(0x001F);
        f.u16(0x07E0); // green
        f.zeros(2);
        auto fields = decodeBmp(f.data());
        REQUIRE(fields.ok());
        CHECK(fields->colorAt(0, 0) == rgb(0, 255, 0));
    }

    SECTION("32-bit alpha: used when present, ignored when all zero")
    {
        ByteWriter w = infoHeader(2, 1, 32, 0, 0);
        w.u32(0x80112233);
        w.u32(0x00445566);
        auto image = decodeBmp(w.data());
        REQUIRE(image.ok());
        CHECK(image->colorAt(0, 0) == rgb(0x11, 0x22, 0x33, 0x80));
        CHECK(image->colorAt(1, 0) == rgb(0x44, 0x55, 0x66, 0x00));

        ByteWriter opaque = infoHeader(1, 1, 32, 0, 0);
        opaque.u32(0x00ABCDEF);
        auto solid = decodeBmp(opaque.data());
        REQUIRE(solid.ok());
        CHECK(solid->colorAt(0, 0) == rgb(0xAB, 0xCD, 0xEF, 255));
    }

    SECTION("OS/2 core header with 3-byte colors")
    {
        ByteWriter w;
        w.text("BM");
        w.u32(0);
        w.u32(0);
        w.u32(14 + 12 + 2 * 3);
        w.u32(12);
        w.u16(2);
        w.u16(1);
        w.u16(1);
        w.u16(1);
        w.u8(0x10);
        w.u8(0x20);
        w.u8(0x30); // index 0: B G R
        w.u8(0x40);
        w.u8(0x50);
        w.u8(0x60);
        w.u8(0b01000000);
        w.zeros(3);
        auto image = decodeBmp(w.data());
        REQUIRE(image.ok());
        CHECK(image->pixelBuffer() == std::vector<std::uint8_t>{0, 1});
        CHECK(image->palette() == Palette{rgb(0x30, 0x20, 0x10), rgb(0x60, 0x50, 0x40)});
    }
}

TEST_CASE("BMP decoding rejects what it cannot read", "[bmp]")
{
    CHECK_FALSE(decodeBmp(toBytes("BM")).ok());
    CHECK_FALSE(decodeBmp(toBytes("PNG image")).ok());

    ByteWriter jpeg = infoHeader(1, 1, 24, 4, 0);
    jpeg.zeros(4);
    CHECK_FALSE(decodeBmp(jpeg.data()).ok());

    ByteWriter depth = infoHeader(1, 1, 7, 0, 0);
    depth.zeros(4);
    CHECK_FALSE(decodeBmp(depth.data()).ok());

    ByteWriter rleTopDown = infoHeader(1, -1, 8, 1, 1);
    color(rleTopDown, 0, 0, 0);
    rleTopDown.zeros(4);
    CHECK_FALSE(decodeBmp(rleTopDown.data()).ok());

    ByteWriter zero = infoHeader(0, 1, 24, 0, 0);
    CHECK_FALSE(decodeBmp(zero.data()).ok());

    ByteWriter shortData = infoHeader(1, 2, 24, 0, 0);
    shortData.u8(1);
    shortData.u8(2);
    shortData.u8(3);
    shortData.u8(0);
    CHECK_FALSE(decodeBmp(shortData.data()).ok());
    auto lenient = decodeBmp(shortData.data(), DecodeMode::Lenient);
    REQUIRE(lenient.ok());
    CHECK(lenient->colorAt(0, 1) == rgb(3, 2, 1));

    const Bytes padded = *encodeBmp(Image::indexed(3, 1, {7, 8, 9}, grayscalePalette(10)));
    const Bytes unpadded(padded.begin(), padded.end() - 1);
    CHECK_FALSE(decodeBmp(unpadded).ok());
    auto partial = decodeBmp(unpadded, DecodeMode::Lenient);
    REQUIRE(partial.ok());
    CHECK(partial->pixelBuffer() == std::vector<std::uint8_t>{7, 8, 9});
}
