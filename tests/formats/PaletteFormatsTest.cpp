#include "formats/palette/PaletteFormats.h"
#include "fixtures/FixtureFactory.h"
#include "formats/codecs/Pcx.h"
#include "formats/codecs/Png.h"
#include "formats/util/ByteWriter.h"

#include <catch2/catch_test_macros.hpp>

using namespace OFS::Formats;
using namespace OFS::Formats::Test;

TEST_CASE("ACT files store M.U.G.E.N palettes in reverse", "[palette][act]")
{
    FixtureFactory f(0xAC7);
    const Palette palette = f.palette(256);

    const Bytes reversed = writeAct(palette, ActOrder::Reversed);
    REQUIRE(reversed.size() == 768);
    // The first triplet is index 255.
    CHECK(static_cast<std::uint8_t>(reversed[0]) == palette[255].r);
    CHECK(static_cast<std::uint8_t>(reversed[767]) == palette[0].b);
    auto back = readAct(reversed, ActOrder::Reversed);
    REQUIRE(back.ok());
    CHECK(*back == palette);

    const Bytes natural = writeAct(palette, ActOrder::Natural);
    CHECK(static_cast<std::uint8_t>(natural[0]) == palette[0].r);
    auto naturalBack = readAct(natural, ActOrder::Natural);
    REQUIRE(naturalBack.ok());
    CHECK(*naturalBack == palette);

    auto swapped = readAct(natural, ActOrder::Reversed);
    REQUIRE(swapped.ok());
    CHECK((*swapped)[255] == palette[0]);

    // Photoshop's 772-byte variant (count + transparent index).
    Bytes extended = natural;
    for (int b : {0x01, 0x00, 0xFF, 0xFF})
        extended.push_back(std::byte{static_cast<std::uint8_t>(b)});
    CHECK(readAct(extended, ActOrder::Natural).ok());
    CHECK_FALSE(readAct(Bytes(767), ActOrder::Natural).ok());

    const Bytes small = writeAct(Palette{rgb(1, 2, 3)}, ActOrder::Natural);
    CHECK(small.size() == 768);
    CHECK(static_cast<std::uint8_t>(small[3]) == 0);

    for (ActOrder order : {ActOrder::Reversed, ActOrder::Natural}) {
        const Bytes file = f.act(order);
        auto colors = readAct(file, order);
        REQUIRE(colors.ok());
        CHECK(writeAct(*colors, order) == file);
        CHECK(readPaletteFile(file, order).value() == *colors);
    }
}

TEST_CASE("RIFF PAL round-trips", "[palette][riff]")
{
    FixtureFactory f(0x9A1);
    for (std::size_t count : {1u, 16u, 256u}) {
        const Palette palette = f.palette(count);
        const Bytes file = writeRiffPal(palette);
        CHECK(toText(ByteSpan(file).first(4)) == "RIFF");
        CHECK(toText(ByteSpan(file).subspan(8, 8)) == "PAL data");
        auto back = readRiffPal(file);
        REQUIRE(back.ok());
        CHECK(*back == palette);
        auto detected = readPaletteFile(file);
        REQUIRE(detected.ok());
        CHECK(*detected == palette);
    }
    CHECK_FALSE(readRiffPal(toBytes("RIFF\x04\x00\x00\x00WAVE")).ok());
}

TEST_CASE("RIFF PAL skips unknown chunks", "[palette][riff]")
{
    ByteWriter w;
    w.text("RIFF");
    w.u32(4 + 10 + 16);
    w.text("PAL ");
    w.text("junk");
    w.u32(1);
    w.u8(0xEE);
    w.u8(0); // pad byte
    w.text("data");
    w.u32(8);
    w.u16(0x0300);
    w.u16(1);
    w.u8(10);
    w.u8(20);
    w.u8(30);
    w.u8(0);
    auto palette = readRiffPal(w.data());
    REQUIRE(palette.ok());
    CHECK(*palette == Palette{rgb(10, 20, 30)});
}

TEST_CASE("JASC-PAL round-trips and tolerates LF line ends", "[palette][jasc]")
{
    FixtureFactory f(0x7A5C);
    const Palette palette = f.palette(256);
    const std::string text = writeJascPal(palette);
    CHECK(text.rfind("JASC-PAL\r\n0100\r\n256\r\n", 0) == 0);
    auto back = readJascPal(text);
    REQUIRE(back.ok());
    CHECK(*back == palette);

    auto lf = readJascPal("JASC-PAL\n0100\n2\n1 2 3\n255 254 253\n");
    REQUIRE(lf.ok());
    CHECK(*lf == Palette{rgb(1, 2, 3), rgb(255, 254, 253)});

    CHECK_FALSE(readJascPal("JASC-PAL\n0100\n3\n1 2 3\n").ok());
    CHECK_FALSE(readJascPal("JASC-PAL\n0100\n1\n1 2 300\n").ok());
    CHECK_FALSE(readJascPal("JASC-PAL\n0100\n1\n1 two 3\n").ok());
    CHECK_FALSE(readJascPal("GIMP Palette\n").ok());
}

TEST_CASE("ACO swatches are read in every supported color space", "[palette][aco]")
{
    ByteWriter w;
    // Version 1 section.
    w.u16be(1);
    w.u16be(5);
    auto color = [&w](int space, int a, int b, int c, int d) {
        w.u16be(static_cast<std::uint16_t>(space));
        w.u16be(static_cast<std::uint16_t>(a));
        w.u16be(static_cast<std::uint16_t>(b));
        w.u16be(static_cast<std::uint16_t>(c));
        w.u16be(static_cast<std::uint16_t>(d));
    };
    color(0, 0xFF00, 0x8000, 0x0000, 0); // RGB
    color(1, 0, 65535, 65535, 0);        // HSB: pure red
    color(2, 65535, 65535, 65535, 0);    // CMYK: no ink but full black -> black
    color(8, 0, 0, 0, 0);                // grayscale: no ink -> white
    color(7, 10000, 0, 0, 0);            // Lab: L=100 -> white
    auto v1 = readAco(w.data());
    REQUIRE(v1.ok());
    REQUIRE(v1->size() == 5);
    CHECK((*v1)[0] == rgb(255, 128, 0));
    CHECK((*v1)[1] == rgb(255, 0, 0));
    CHECK((*v1)[2] == rgb(0, 0, 0));
    CHECK((*v1)[3] == rgb(255, 255, 255));
    CHECK((*v1)[4].r >= 254);

    // Version 2 section with names.
    w.u16be(2);
    w.u16be(1);
    color(0, 0x1200, 0x3400, 0x5600, 0);
    w.u32be(3);
    w.u16be('R');
    w.u16be('e');
    w.u16be(0);
    auto v2 = readAco(w.data());
    REQUIRE(v2.ok());
    CHECK(*v2 == Palette{rgb(0x12, 0x34, 0x56)});

    Bytes cut = w.data();
    cut.resize(cut.size() - 3);
    CHECK_FALSE(readAco(cut).ok());
}

TEST_CASE("palettes are extracted from PCX and PNG images", "[palette]")
{
    FixtureFactory f(0xE7);
    const Image img = f.indexedImage(5, 3, 256, PixelPattern::Noise);
    auto pcx = encodePcx(img);
    REQUIRE(pcx.ok());
    auto fromPcx = paletteFromPcx(*pcx);
    REQUIRE(fromPcx.ok());
    CHECK(*fromPcx == img.palette());
    CHECK(readPaletteFile(*pcx).value() == img.palette());

    PcxEncodeOptions noPalette;
    noPalette.includePalette = false;
    CHECK_FALSE(paletteFromPcx(*encodePcx(img, noPalette)).ok());

    auto png = encodePng(img);
    REQUIRE(png.ok());
    auto fromPng = paletteFromPng(*png);
    REQUIRE(fromPng.ok());
    REQUIRE(fromPng->size() == 256);
    CHECK(sameRgb(*fromPng, img.palette()));
    CHECK(readPaletteFile(*png).ok());
    CHECK_FALSE(paletteFromPng(*encodePng(f.rgbaImage(2, 2))).ok());
}
