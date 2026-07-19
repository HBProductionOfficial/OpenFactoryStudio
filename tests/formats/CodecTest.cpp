#include "fixtures/FixtureFactory.h"
#include "formats/codecs/Lz5.h"
#include "formats/codecs/Pcx.h"
#include "formats/codecs/Png.h"
#include "formats/codecs/Rle5.h"
#include "formats/codecs/Rle8.h"
#include "formats/util/ByteWriter.h"
#include "formats/util/Crc.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>

using namespace OFS::Formats;
using namespace OFS::Formats::Test;

namespace {

constexpr int kRoundTrips = 1000;

Bytes bytes(std::initializer_list<int> values)
{
    Bytes out;
    for (int v : values)
        out.push_back(std::byte{static_cast<std::uint8_t>(v)});
    return out;
}

std::vector<std::uint8_t> repeat(std::uint8_t value, std::size_t count)
{
    return std::vector<std::uint8_t>(count, value);
}

std::vector<std::uint8_t> concat(std::initializer_list<std::vector<std::uint8_t>> parts)
{
    std::vector<std::uint8_t> out;
    for (const auto& p : parts)
        out.insert(out.end(), p.begin(), p.end());
    return out;
}

std::vector<std::vector<std::uint8_t>> edgeCases(int colors)
{
    const auto top = static_cast<std::uint8_t>(colors - 1);
    std::vector<std::vector<std::uint8_t>> cases = {
        {0},
        {top},
        repeat(0, 2),
        repeat(top, 7),
        repeat(1, 8),
        repeat(2, 63),
        repeat(3, 64),
        repeat(4, 65),
        repeat(5, 256),
        repeat(6, 257),
        repeat(7, 263),
        repeat(8, 264),
        repeat(9, 1030),
        repeat(0, 5000),
        concat({repeat(1, 63), repeat(2, 1), repeat(1, 64)}),
    };
    std::vector<std::uint8_t> alternating(301);
    std::vector<std::uint8_t> ramp(600);
    for (std::size_t i = 0; i < alternating.size(); ++i)
        alternating[i] = static_cast<std::uint8_t>(i % 2 ? top : 0);
    for (std::size_t i = 0; i < ramp.size(); ++i)
        ramp[i] = static_cast<std::uint8_t>(i % static_cast<std::size_t>(colors));
    cases.push_back(alternating);
    cases.push_back(ramp);
    return cases;
}

// --- LZ5 stream analysis -------------------------------------------------------------------------

struct Lz5Stats
{
    int shortRuns = 0;
    int longRuns = 0;
    int shortCopies = 0;
    int recycledCopies = 0; // fourth short copy of a group (distance from the collected bits)
    int longCopies = 0;
    int farCopies = 0;
    int overlappingCopies = 0;
    int controlBytes = 0;
};

Lz5Stats analyzeLz5(ByteSpan data, std::size_t pixelCount)
{
    Lz5Stats st;
    const std::span<const std::uint8_t> in = asU8(data);
    std::size_t pos = 0;
    std::size_t produced = 0;
    std::uint8_t control = 0;
    int bits = 0;
    int pieces = 0;
    std::uint8_t carried = 0;
    while (produced < pixelCount && pos < in.size()) {
        if (bits == 0) {
            control = in[pos++];
            bits = 8;
            ++st.controlBytes;
        }
        const bool copy = control & 1;
        control = static_cast<std::uint8_t>(control >> 1);
        --bits;
        const std::uint8_t b = in[pos++];
        if (copy) {
            std::size_t distance = 0;
            std::size_t length = 0;
            if ((b & 0x3F) == 0) {
                distance = ((std::size_t{static_cast<std::uint8_t>(b >> 6)} << 8) | in[pos]) + 1;
                length = std::size_t{in[pos + 1]} + 3;
                pos += 2;
                ++st.longCopies;
            } else {
                carried = static_cast<std::uint8_t>(carried | ((b & 0xC0) >> (pieces * 2)));
                ++pieces;
                length = std::size_t{static_cast<std::uint8_t>(b & 0x3F)} + 1;
                if (pieces < 4) {
                    distance = std::size_t{in[pos++]} + 1;
                } else {
                    distance = std::size_t{carried} + 1;
                    carried = 0;
                    pieces = 0;
                    ++st.recycledCopies;
                }
                ++st.shortCopies;
            }
            if (distance > 256)
                ++st.farCopies;
            if (distance < length)
                ++st.overlappingCopies;
            produced += length;
        } else if ((b & 0xE0) == 0) {
            produced += std::size_t{in[pos++]} + 8;
            ++st.longRuns;
        } else {
            produced += b >> 5;
            ++st.shortRuns;
        }
    }
    return st;
}

// --- hand-built PNG files (stored deflate blocks, so no compressor is involved) -----------------

void chunk(ByteWriter& out, std::string_view type, const Bytes& body)
{
    out.u32be(static_cast<std::uint32_t>(body.size()));
    Bytes crcInput = toBytes(type);
    crcInput.insert(crcInput.end(), body.begin(), body.end());
    out.text(type);
    out.bytes(body);
    out.u32be(crc32(crcInput));
}

Bytes zlibStored(const std::vector<std::uint8_t>& raw)
{
    ByteWriter out;
    out.u8(0x78);
    out.u8(0x01);
    std::size_t pos = 0;
    do {
        const std::size_t n = std::min<std::size_t>(raw.size() - pos, 65535);
        out.u8(pos + n == raw.size() ? 1 : 0);
        out.u16(static_cast<std::uint16_t>(n));
        out.u16(static_cast<std::uint16_t>(~n & 0xFFFF));
        out.bytes(std::span(raw).subspan(pos, n));
        pos += n;
    } while (pos < raw.size());
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (const std::uint8_t v : raw) {
        a = (a + v) % 65521;
        b = (b + a) % 65521;
    }
    out.u32be((b << 16) | a);
    return out.take();
}

Bytes palettePng(int width, int height, int depth, const std::vector<std::uint8_t>& indices, const Palette& palette,
                 std::optional<std::vector<std::uint8_t>> alpha = std::nullopt)
{
    std::vector<std::uint8_t> raw;
    const auto rowBytes = static_cast<std::size_t>((width * depth + 7) / 8);
    for (int y = 0; y < height; ++y) {
        raw.push_back(0); // filter: none
        std::vector<std::uint8_t> row(rowBytes, 0);
        for (int x = 0; x < width; ++x) {
            const std::size_t bit = static_cast<std::size_t>(x * depth);
            const std::uint8_t v = indices[static_cast<std::size_t>(y * width + x)];
            row[bit / 8] = static_cast<std::uint8_t>(row[bit / 8] | (v << (8 - depth - static_cast<int>(bit % 8))));
        }
        raw.insert(raw.end(), row.begin(), row.end());
    }
    ByteWriter out;
    out.bytes(bytes({0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A}));
    ByteWriter ihdr;
    ihdr.u32be(static_cast<std::uint32_t>(width));
    ihdr.u32be(static_cast<std::uint32_t>(height));
    ihdr.u8(static_cast<std::uint8_t>(depth));
    ihdr.u8(3); // palette
    ihdr.u8(0);
    ihdr.u8(0);
    ihdr.u8(0);
    chunk(out, "IHDR", ihdr.data());
    ByteWriter plte;
    for (const Rgba& c : palette) {
        plte.u8(c.r);
        plte.u8(c.g);
        plte.u8(c.b);
    }
    chunk(out, "PLTE", plte.data());
    if (alpha)
        chunk(out, "tRNS", toBytes(*alpha));
    chunk(out, "IDAT", zlibStored(raw));
    chunk(out, "IEND", {});
    return out.take();
}

} // namespace

// --- RLE8 ----------------------------------------------------------------------------------------

TEST_CASE("RLE8 decodes hand-built streams", "[codec][rle8]")
{
    // 0x45 = run of 5, 0x80/0xFF literals, 0x40 = empty run, 0x7F = run of 63.
    const Bytes stream = bytes({0x45, 0x07, 0x80, 0xFF, 0x40, 0x09, 0x02, 0x7F, 0x03, 0x41, 0x40});
    auto px = decodeRle8(stream, 5 + 2 + 1 + 63 + 1);
    REQUIRE(px.ok());
    const std::vector<std::uint8_t> expected = concat({repeat(7, 5), {0x80, 0xFF, 0x02}, repeat(3, 63), {0x40}});
    CHECK(*px == expected);
}

TEST_CASE("RLE8 encoder output matches the format rules", "[codec][rle8]")
{
    CHECK(encodeRle8(std::vector<std::uint8_t>{5, 5, 5}) == bytes({0x43, 5}));
    CHECK(encodeRle8(std::vector<std::uint8_t>{1, 2}) == bytes({1, 2}));
    CHECK(encodeRle8(std::vector<std::uint8_t>{0x40}) == bytes({0x41, 0x40})); // marker value
    CHECK(encodeRle8(std::vector<std::uint8_t>{0x7F, 0x80}) == bytes({0x41, 0x7F, 0x80}));
    CHECK(encodeRle8(repeat(9, 64)) == bytes({0x7F, 9, 9}));
    CHECK(encodeRle8(std::vector<std::uint8_t>{}).empty());
}

TEST_CASE("RLE8 strict and lenient decoding of damaged data", "[codec][rle8]")
{
    CHECK_FALSE(decodeRle8(bytes({0x43, 1}), 4).ok());
    CHECK_FALSE(decodeRle8(bytes({0x43}), 3).ok());
    CHECK_FALSE(decodeRle8(bytes({0x45, 1}), 3).ok());
    CHECK(decodeRle8(bytes({0x43, 1, 9}), 3).ok());
    auto lenient = decodeRle8(bytes({0x43, 1}), 5, DecodeMode::Lenient);
    REQUIRE(lenient.ok());
    CHECK(*lenient == std::vector<std::uint8_t>{1, 1, 1, 0, 0});
    auto clipped = decodeRle8(bytes({0x45, 1}), 3, DecodeMode::Lenient);
    REQUIRE(clipped.ok());
    CHECK(*clipped == repeat(1, 3));
    CHECK(decodeRle8({}, 0).ok());
}

TEST_CASE("RLE8 round-trips random and edge-case images", "[codec][rle8][roundtrip]")
{
    FixtureFactory f(0x8001);
    for (const auto& px : edgeCases(256)) {
        auto back = decodeRle8(encodeRle8(px), px.size());
        REQUIRE(back.ok());
        CHECK(*back == px);
    }
    for (int i = 0; i < kRoundTrips; ++i) {
        const Image img = f.randomIndexedImage(80, 256);
        auto back = decodeRle8(encodeRle8(img.pixels()), img.pixelCount());
        REQUIRE(back.ok());
        REQUIRE(*back == img.pixelBuffer());
    }
}

// --- RLE5 ----------------------------------------------------------------------------------------

TEST_CASE("RLE5 decodes hand-built streams", "[codec][rle5]")
{
    // Packet 1: first color 7 (explicit) x3, extra runs 1 x2 and 3 x8.
    // Packet 2: first color 0 (implicit) x5, no extra runs.
    // Packet 3: first color 0 x1, extra run 31 x1.
    const Bytes stream = bytes({0x02, 0x82, 0x07, 0x21, 0xE3, 0x04, 0x00, 0x00, 0x01, 0x1F});
    auto px = decodeRle5(stream, 3 + 2 + 8 + 5 + 2);
    REQUIRE(px.ok());
    CHECK(*px == concat({repeat(7, 3), repeat(1, 2), repeat(3, 8), repeat(0, 5), {0, 31}}));

    auto longRun = decodeRle5(bytes({0xFF, 0x80, 0x11}), 256);
    REQUIRE(longRun.ok());
    CHECK(*longRun == repeat(0x11, 256));
}

TEST_CASE("RLE5 strict and lenient decoding of damaged data", "[codec][rle5]")
{
    CHECK_FALSE(decodeRle5(bytes({0x02}), 3).ok());
    CHECK_FALSE(decodeRle5(bytes({0x02, 0x80}), 3).ok());
    CHECK_FALSE(decodeRle5(bytes({0x00, 0x02, 0x21}), 4).ok());
    CHECK_FALSE(decodeRle5(bytes({0x05, 0x00}), 3).ok());
    auto lenient = decodeRle5(bytes({0x01, 0x81, 0x04}), 4, DecodeMode::Lenient);
    REQUIRE(lenient.ok());
    CHECK(*lenient == std::vector<std::uint8_t>{4, 4, 0, 0});
}

TEST_CASE("RLE5 encoder rejects indices above 31", "[codec][rle5]")
{
    CHECK_FALSE(encodeRle5(std::vector<std::uint8_t>{1, 32}).ok());
    CHECK(encodeRle5(std::vector<std::uint8_t>{31}).ok());
}

TEST_CASE("RLE5 round-trips random and edge-case images", "[codec][rle5][roundtrip]")
{
    FixtureFactory f(0x5005);
    for (const auto& px : edgeCases(32)) {
        auto encoded = encodeRle5(px);
        REQUIRE(encoded.ok());
        auto back = decodeRle5(*encoded, px.size());
        REQUIRE(back.ok());
        CHECK(*back == px);
    }
    for (int i = 0; i < kRoundTrips; ++i) {
        const Image img = f.randomIndexedImage(80, 32);
        auto encoded = encodeRle5(img.pixels());
        REQUIRE(encoded.ok());
        auto back = decodeRle5(*encoded, img.pixelCount());
        REQUIRE(back.ok());
        REQUIRE(*back == img.pixelBuffer());
    }
}

// --- LZ5 -----------------------------------------------------------------------------------------

TEST_CASE("LZ5 decodes hand-built streams", "[codec][lz5]")
{
    SECTION("short run then overlapping short copy")
    {
        auto px = decodeLz5(bytes({0x02, 0x64, 0x02, 0x02}), 6);
        REQUIRE(px.ok());
        CHECK(*px == repeat(4, 6));
    }
    SECTION("long run")
    {
        auto px = decodeLz5(bytes({0x00, 0x05, 0x02}), 10);
        REQUIRE(px.ok());
        CHECK(*px == repeat(5, 10));
    }
    SECTION("long copy")
    {
        auto px = decodeLz5(bytes({0x08, 0x21, 0x22, 0x23, 0x00, 0x02, 0x06}), 12);
        REQUIRE(px.ok());
        CHECK(*px == std::vector<std::uint8_t>{1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3});
    }
    SECTION("long copy from more than 256 pixels back")
    {
        // 263 x 1 and 37 x 2 as long runs, then copy 3 pixels from distance 300 (0x12B).
        auto px = decodeLz5(bytes({0x04, 0x01, 0xFF, 0x02, 0x1D, 0x40, 0x2B, 0x00}), 303);
        REQUIRE(px.ok());
        CHECK(*px == concat({repeat(1, 263), repeat(2, 37), repeat(1, 3)}));
    }
    SECTION("recycled distance bits")
    {
        // Four short runs 1,2,3,4 then four short copies. Copies A-C have explicit distances
        // (4, 4, 2). Their top bits 00, 00, 01 plus copy D's 01 form 0b00000101, so D copies
        // from distance 6.
        const Bytes stream = bytes({0xF0, 0x21, 0x22, 0x23, 0x24, 0x01, 0x03, 0x01, 0x03, 0x41, 0x01, 0x41});
        auto px = decodeLz5(stream, 12);
        REQUIRE(px.ok());
        CHECK(*px == std::vector<std::uint8_t>{1, 2, 3, 4, 1, 2, 3, 4, 3, 4, 1, 2});
    }
    SECTION("second control byte and a copy at distance 1")
    {
        const Bytes stream = bytes({0x80, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x09, 0x00, 0x00, 0x43});
        auto px = decodeLz5(stream, 19);
        REQUIRE(px.ok());
        CHECK(*px == concat({repeat(1, 17), repeat(3, 2)}));
    }
}

TEST_CASE("LZ5 strict and lenient decoding of damaged data", "[codec][lz5]")
{
    CHECK_FALSE(decodeLz5({}, 1).ok());
    CHECK_FALSE(decodeLz5(bytes({0x00}), 1).ok());
    CHECK_FALSE(decodeLz5(bytes({0x00, 0x05}), 9).ok());
    CHECK_FALSE(decodeLz5(bytes({0x01, 0x00, 0x00}), 4).ok());
    CHECK_FALSE(decodeLz5(bytes({0x02, 0x21, 0x01, 0x05}), 3).ok());
    CHECK_FALSE(decodeLz5(bytes({0x00, 0x64}), 2).ok());
    auto lenient = decodeLz5(bytes({0x02, 0x21, 0x01, 0x05}), 3, DecodeMode::Lenient);
    REQUIRE(lenient.ok());
    CHECK(*lenient == std::vector<std::uint8_t>{1, 0, 0});
    auto eight = decodeLz5(bytes({0x00, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21, 0x21}), 8);
    REQUIRE(eight.ok());
    CHECK(*eight == repeat(1, 8));
}

TEST_CASE("LZ5 encoder rejects indices above 31", "[codec][lz5]")
{
    CHECK_FALSE(encodeLz5(std::vector<std::uint8_t>{40}).ok());
}

TEST_CASE("LZ5 encoder uses every packet type", "[codec][lz5]")
{
    FixtureFactory f(0x1235);
    Lz5Stats total;
    std::size_t rawSize = 0;
    std::size_t packedSize = 0;
    for (int i = 0; i < 200; ++i) {
        const auto px = f.pixels(4000, 32, i % 2 ? PixelPattern::Repeats : PixelPattern::Mixed);
        auto encoded = encodeLz5(px);
        REQUIRE(encoded.ok());
        const Lz5Stats st = analyzeLz5(*encoded, px.size());
        total.shortRuns += st.shortRuns;
        total.longRuns += st.longRuns;
        total.shortCopies += st.shortCopies;
        total.recycledCopies += st.recycledCopies;
        total.longCopies += st.longCopies;
        total.farCopies += st.farCopies;
        total.overlappingCopies += st.overlappingCopies;
        rawSize += px.size();
        packedSize += encoded->size();
    }
    CHECK(total.shortRuns > 0);
    CHECK(total.longRuns > 0);
    CHECK(total.shortCopies > 0);
    CHECK(total.recycledCopies > 0);
    CHECK(total.longCopies > 0);
    CHECK(total.farCopies > 0);
    CHECK(total.overlappingCopies > 0);
    CHECK(packedSize < rawSize / 3);
}

TEST_CASE("LZ5 round-trips random and edge-case images", "[codec][lz5][roundtrip]")
{
    FixtureFactory f(0x1255);
    for (const auto& px : edgeCases(32)) {
        auto encoded = encodeLz5(px);
        REQUIRE(encoded.ok());
        auto back = decodeLz5(*encoded, px.size());
        REQUIRE(back.ok());
        CHECK(*back == px);
    }
    for (int i = 0; i < kRoundTrips; ++i) {
        const Image img = f.randomIndexedImage(80, 32);
        auto encoded = encodeLz5(img.pixels());
        REQUIRE(encoded.ok());
        auto back = decodeLz5(*encoded, img.pixelCount());
        REQUIRE(back.ok());
        REQUIRE(*back == img.pixelBuffer());
    }
    for (int i = 0; i < 50; ++i) {
        const auto px = f.pixels(20000 + f.rng().below(5000), 32, PixelPattern::Repeats);
        auto encoded = encodeLz5(px);
        REQUIRE(encoded.ok());
        auto back = decodeLz5(*encoded, px.size());
        REQUIRE(back.ok());
        REQUIRE(*back == px);
    }
}

// --- PCX -----------------------------------------------------------------------------------------

namespace {

Bytes pcxHeader(int width, int height, int bytesPerLine, int planes = 1)
{
    ByteWriter w;
    w.u8(0x0A);
    w.u8(5);
    w.u8(1);
    w.u8(8);
    w.u16(0);
    w.u16(0);
    w.u16(static_cast<std::uint16_t>(width - 1));
    w.u16(static_cast<std::uint16_t>(height - 1));
    w.u16(72);
    w.u16(72);
    w.zeros(48);
    w.u8(0);
    w.u8(static_cast<std::uint8_t>(planes));
    w.u16(static_cast<std::uint16_t>(bytesPerLine));
    w.u16(1);
    w.padTo(128);
    return w.take();
}

void appendPalette(Bytes& out, const Palette& palette)
{
    out.push_back(std::byte{0x0C});
    for (std::size_t i = 0; i < 256; ++i) {
        const Rgba c = i < palette.size() ? palette[i] : rgb(0, 0, 0);
        out.push_back(std::byte{c.r});
        out.push_back(std::byte{c.g});
        out.push_back(std::byte{c.b});
    }
}

} // namespace

TEST_CASE("PCX decodes a hand-built 8-bit image", "[codec][pcx]")
{
    // 3x2 pixels, 4 bytes per line (one padding byte per line).
    Bytes file = pcxHeader(3, 2, 4);
    const Bytes body = bytes({0xC2, 0x05, 0xC1, 0xC7, 0x00, 0x01, 0x02, 0x03, 0x00});
    file.insert(file.end(), body.begin(), body.end());
    FixtureFactory f(7);
    const Palette palette = f.palette(256);
    appendPalette(file, palette);

    auto img = decodePcx(file);
    REQUIRE(img.ok());
    CHECK(img->width() == 3);
    CHECK(img->height() == 2);
    CHECK(img->pixelBuffer() == std::vector<std::uint8_t>{5, 5, 0xC7, 1, 2, 3});
    CHECK(img->palette() == palette);
    CHECK(readPcxPalette(file) == palette);

    auto header = readPcxHeader(file);
    REQUIRE(header.ok());
    CHECK(header->bytesPerLine == 4);
}

TEST_CASE("PCX runs may cross scanlines", "[codec][pcx]")
{
    Bytes file = pcxHeader(2, 2, 2);
    const Bytes body = bytes({0xC4, 0x09});
    file.insert(file.end(), body.begin(), body.end());
    auto img = decodePcx(file, PcxPalette::Optional);
    REQUIRE(img.ok());
    CHECK(img->pixelBuffer() == repeat(9, 4));
    CHECK(img->palette().empty());
    CHECK_FALSE(decodePcx(file, PcxPalette::Required).ok());
}

TEST_CASE("PCX decodes 24-bit images", "[codec][pcx]")
{
    // 2x1, three planes of two bytes each.
    Bytes file = pcxHeader(2, 1, 2, 3);
    const Bytes body = bytes({10, 20, 30, 40, 0xC2, 50});
    file.insert(file.end(), body.begin(), body.end());
    auto img = decodePcx(file);
    REQUIRE(img.ok());
    REQUIRE(img->isRgba());
    CHECK(img->colorAt(0, 0) == rgb(10, 30, 50));
    CHECK(img->colorAt(1, 0) == rgb(20, 40, 50));
}

TEST_CASE("PCX rejects malformed headers", "[codec][pcx]")
{
    Bytes file = pcxHeader(4, 4, 2); // bytes per line smaller than the width
    CHECK_FALSE(decodePcx(file).ok());
    Bytes notPcx(200, std::byte{0});
    CHECK_FALSE(decodePcx(notPcx).ok());
    CHECK_FALSE(readPcxHeader(bytes({0x0A, 5, 1, 8})).ok());
}

TEST_CASE("PCX round-trips random and edge-case images", "[codec][pcx][roundtrip]")
{
    FixtureFactory f(0xBCC);
    for (const auto& px : edgeCases(256)) {
        const Image img = Image::indexed(static_cast<int>(px.size()), 1, px, f.palette());
        auto encoded = encodePcx(img);
        REQUIRE(encoded.ok());
        auto back = decodePcx(*encoded);
        REQUIRE(back.ok());
        CHECK(*back == img);
    }
    for (int i = 0; i < kRoundTrips; ++i) {
        const Image img = f.randomIndexedImage(64, 256);
        PcxEncodeOptions options;
        options.includePalette = i % 5 != 0;
        auto encoded = encodePcx(img, options);
        REQUIRE(encoded.ok());
        auto back = decodePcx(*encoded, options.includePalette ? PcxPalette::Required : PcxPalette::Optional);
        REQUIRE(back.ok());
        REQUIRE(back->pixelBuffer() == img.pixelBuffer());
        REQUIRE(back->width() == img.width());
        if (options.includePalette)
            REQUIRE(back->palette() == img.palette());
        else
            REQUIRE(back->palette().empty());
    }
    CHECK_FALSE(encodePcx(Image(2, 2, PixelFormat::Rgba32)).ok());
}

// --- PNG -----------------------------------------------------------------------------------------

TEST_CASE("PNG decodes hand-built palette images with exact indices", "[codec][png]")
{
    // Duplicate palette colors: a color-matching decoder would merge indices 1 and 2.
    const Palette palette = {rgb(0, 0, 0), rgb(9, 9, 9), rgb(9, 9, 9), rgb(200, 1, 2)};
    for (int depth : {2, 4, 8}) {
        const std::vector<std::uint8_t> indices = {0, 1, 2, 3, 3, 2, 1, 0, 2};
        auto img = decodePng(palettePng(3, 3, depth, indices, palette, std::vector<std::uint8_t>{0, 128}));
        REQUIRE(img.ok());
        REQUIRE(img->isIndexed());
        CHECK(img->pixelBuffer() == indices);
        REQUIRE(img->palette().size() == 4);
        CHECK(img->palette()[0] == rgb(0, 0, 0, 0));
        CHECK(img->palette()[1] == rgb(9, 9, 9, 128));
        CHECK(img->palette()[3] == rgb(200, 1, 2, 255));
    }
    const std::vector<std::uint8_t> bits = {1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0};
    auto mono = decodePng(palettePng(9, 2, 1, bits, Palette{rgb(0, 0, 0), rgb(255, 255, 255)}));
    REQUIRE(mono.ok());
    CHECK(mono->pixelBuffer() == bits);

    auto info = readPngInfo(palettePng(3, 3, 4, std::vector<std::uint8_t>(9, 1), palette));
    REQUIRE(info.ok());
    CHECK(info->colorType == 3);
    CHECK(info->bitDepth == 4);
    CHECK(info->paletteSize == 4);
}

TEST_CASE("PNG rejects damaged files", "[codec][png]")
{
    Bytes file = palettePng(2, 2, 8, {0, 1, 1, 0}, Palette{rgb(0, 0, 0), rgb(1, 1, 1)});
    file[file.size() - 20] ^= std::byte{0x55};
    CHECK_FALSE(decodePng(file).ok());
    CHECK_FALSE(decodePng(bytes({1, 2, 3})).ok());
    CHECK_FALSE(encodePng(Image()).ok());
}

TEST_CASE("PNG round-trips palette and true-color images", "[codec][png][roundtrip]")
{
    FixtureFactory f(0x9A9);
    for (int i = 0; i < kRoundTrips; ++i) {
        Image img = f.randomIndexedImage(48, 256);
        img.palette()[1] = img.palette()[2];
        auto encoded = encodePng(img);
        REQUIRE(encoded.ok());
        auto back = decodePng(*encoded);
        REQUIRE(back.ok());
        REQUIRE(back->isIndexed());
        REQUIRE(back->pixelBuffer() == img.pixelBuffer());
        REQUIRE(back->width() == img.width());
        for (std::size_t c = 0; c <= img.maxIndex(); ++c)
            REQUIRE(sameRgb(back->palette()[c], img.palette()[c]));
    }
    for (int i = 0; i < kRoundTrips; ++i) {
        const int w = 1 + static_cast<int>(f.rng().below(40));
        const int h = 1 + static_cast<int>(f.rng().below(40));
        const Image img = f.rgbaImage(w, h);
        auto encoded = encodePng(img);
        REQUIRE(encoded.ok());
        auto back = decodePng(*encoded);
        REQUIRE(back.ok());
        REQUIRE(*back == img);

        PngEncodeOptions rgbOnly;
        rgbOnly.color = PngColor::Rgb;
        auto encoded24 = encodePng(img, rgbOnly);
        REQUIRE(encoded24.ok());
        auto info = readPngInfo(*encoded24);
        REQUIRE(info.ok());
        REQUIRE(info->colorType == 2);
        auto back24 = decodePng(*encoded24);
        REQUIRE(back24.ok());
        for (std::size_t p = 0; p < img.pixelCount(); ++p) {
            REQUIRE(back24->pixels()[p * 4] == img.pixels()[p * 4]);
            REQUIRE(back24->pixels()[p * 4 + 3] == 255);
        }
    }
}

TEST_CASE("PNG can mark index 0 transparent", "[codec][png]")
{
    const Image img = Image::indexed(2, 1, {0, 1}, Palette{rgb(5, 5, 5), rgb(6, 6, 6)});
    PngEncodeOptions options;
    options.transparentIndex0 = true;
    auto encoded = encodePng(img, options);
    REQUIRE(encoded.ok());
    auto back = decodePng(*encoded);
    REQUIRE(back.ok());
    CHECK(back->palette()[0].a == 0);
    CHECK(back->palette()[1].a == 255);
    auto padded = decodePng(*encodePng(Image::indexed(1, 1, {9}, Palette{rgb(1, 1, 1)})));
    REQUIRE(padded.ok());
    CHECK(padded->palette().size() == 10);
    CHECK(padded->pixelBuffer() == std::vector<std::uint8_t>{9});
}
