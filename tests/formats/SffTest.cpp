#include "fixtures/FixtureFactory.h"
#include "formats/codecs/Pcx.h"
#include "formats/sff/SffDocument.h"
#include "formats/util/ByteReader.h"

#include <catch2/catch_test_macros.hpp>

using namespace OFS::Formats;
using namespace OFS::Formats::Test;

namespace {

SffDocument writeAndRead(const SffDocument& doc, const SffWriteOptions& options = {})
{
    auto bytes = doc.write(options);
    REQUIRE(bytes.ok());
    auto back = SffDocument::read(*bytes);
    REQUIRE(back.ok());
    return std::move(*back);
}

void requireSameSprites(const SffDocument& a, const SffDocument& b)
{
    REQUIRE(a.sprites.size() == b.sprites.size());
    for (std::size_t i = 0; i < a.sprites.size(); ++i) {
        INFO("sprite " << i);
        const SffSprite& x = a.sprites[i];
        const SffSprite& y = b.sprites[i];
        REQUIRE(x.group == y.group);
        REQUIRE(x.number == y.number);
        REQUIRE(x.axisX == y.axisX);
        REQUIRE(x.axisY == y.axisY);
        auto px = a.decodeSpriteRgba(i, nullptr, DecodeMode::Strict);
        auto py = b.decodeSpriteRgba(i, nullptr, DecodeMode::Strict);
        REQUIRE(px.ok());
        REQUIRE(py.ok());
        REQUIRE(*px == *py);
    }
}

std::uint32_t u32At(const Bytes& data, std::size_t offset)
{
    ByteReader r(data);
    r.seek(offset);
    return r.u32();
}

} // namespace

TEST_CASE("SFF version helpers", "[sff]")
{
    CHECK(sffVersionString(sffVersionBytes(SffVersion::V201)) == "2.0.1.0");
    CHECK(sffVersionString(sffVersionBytes(SffVersion::V2)) == "2.0.0.0");
    CHECK(sffVersionString(sffVersionBytes(SffVersion::V1)) == "1.0.1.0");
    CHECK(sffSupports(SffVersion::V201, SffCompression::Png8));
    CHECK_FALSE(sffSupports(SffVersion::V2, SffCompression::Png8));
    CHECK_FALSE(sffSupports(SffVersion::V1, SffCompression::Rle8));
    CHECK(sffSupports(SffVersion::V1, SffCompression::Pcx));
    CHECK(sffCompressionName(SffCompression::Lz5) == "lz5");
    CHECK_FALSE(sffIsIndexed(SffCompression::Png32));
}

TEST_CASE("SFF v2 write and read give back the same document", "[sff][v2]")
{
    for (SffVersion version : {SffVersion::V2, SffVersion::V201}) {
        INFO(sffVersionName(version));
        FixtureFactory f(version == SffVersion::V2 ? 21 : 22);
        const SffDocument doc = f.sff(version, 40);
        const SffDocument back = writeAndRead(doc);
        CHECK(back.version == version);
        CHECK(back.versionBytes == sffVersionBytes(version));
        REQUIRE(back.palettes.size() == doc.palettes.size());
        for (std::size_t i = 0; i < doc.palettes.size(); ++i) {
            CHECK(back.palettes[i].group == doc.palettes[i].group);
            CHECK(back.palettes[i].number == doc.palettes[i].number);
            CHECK(back.palettes[i].colors == doc.palettes[i].colors);
            CHECK(back.palettes[i].linkedIndex == doc.palettes[i].linkedIndex);
        }
        for (std::size_t i = 0; i < doc.sprites.size(); ++i) {
            INFO("sprite " << i);
            const SffSprite& s = doc.sprites[i];
            const SffSprite& t = back.sprites[i];
            CHECK(t.linkedIndex == s.linkedIndex);
            CHECK(t.paletteIndex == s.paletteIndex);
            CHECK(t.flags == s.flags);
            if (!s.isLinked()) {
                CHECK(t.compression == s.compression);
                CHECK(t.colorDepth == s.colorDepth);
                auto decoded = back.decodeSprite(i, DecodeMode::Strict);
                REQUIRE(decoded.ok());
                if (s.image->isIndexed())
                    CHECK(decoded->pixelBuffer() == s.image->pixelBuffer());
                else
                    CHECK(*decoded == *s.image);
            }
        }
        requireSameSprites(doc, back);
    }
}

TEST_CASE("SFF v1 write and read give back the same document", "[sff][v1]")
{
    FixtureFactory f(11);
    const SffDocument doc = f.sff(SffVersion::V1, 40);
    const SffDocument back = writeAndRead(doc);
    CHECK(back.version == SffVersion::V1);
    CHECK(back.sharedPalette == doc.sharedPalette);
    requireSameSprites(doc, back);
    std::size_t same = 0;
    for (std::size_t i = 0; i < back.sprites.size(); ++i) {
        if (back.sprites[i].samePalette && !back.sprites[i].isLinked()) {
            ++same;
            CHECK_FALSE(readPcxPalette(back.sprites[i].data).has_value());
        }
    }
    CHECK(same > 0);
    CHECK(back.palettes.size() < back.sprites.size());
}

TEST_CASE("an unmodified SFF is written back byte for byte", "[sff][lossless]")
{
    for (SffVersion version : {SffVersion::V1, SffVersion::V2, SffVersion::V201}) {
        INFO(sffVersionName(version));
        FixtureFactory f(31);
        SffDocument doc = f.sff(version, 30);
        const char comment[] = "made by a test";
        for (std::size_t i = 0; i < sizeof comment; ++i)
            doc.header[100 + i] = static_cast<std::uint8_t>(comment[i]);
        auto first = doc.write();
        REQUIRE(first.ok());
        auto reread = SffDocument::read(*first);
        REQUIRE(reread.ok());
        auto second = reread->write();
        REQUIRE(second.ok());
        CHECK(*second == *first);
        CHECK(toText(ByteSpan(*second).subspan(100, sizeof comment - 1)) == "made by a test");
    }
}

TEST_CASE("SFF v1 quirks are preserved and tolerated", "[sff][v1]")
{
    FixtureFactory f(41);
    SffDocument doc = f.sff(SffVersion::V1, 12);
    auto bytes = doc.write();
    REQUIRE(bytes.ok());
    Bytes data = *bytes;
    // Some old tools add 768 to the length of a sprite stored without a palette.
    std::size_t offset = 512;
    std::size_t patched = 0;
    for (std::size_t i = 0; i < doc.sprites.size(); ++i) {
        const std::uint32_t next = u32At(data, offset);
        const std::uint32_t length = u32At(data, offset + 4);
        if (length != 0 && static_cast<std::uint8_t>(data[offset + 18]) == 1 && patched == 0) {
            const std::uint32_t inflated = length + 768;
            for (int b = 0; b < 4; ++b)
                data[offset + 4 + static_cast<std::size_t>(b)] =
                    std::byte{static_cast<std::uint8_t>(inflated >> (8 * b))};
            patched = offset;
        }
        offset = next;
    }
    REQUIRE(patched != 0);
    // Some files have a wrong group count too.
    data[16] = std::byte{0};
    auto reread = SffDocument::read(data);
    REQUIRE(reread.ok());
    requireSameSprites(doc, *reread);
    auto again = reread->write();
    REQUIRE(again.ok());
    CHECK(*again == data);
}

TEST_CASE("SFF v2 linked sprites and palettes", "[sff][v2][links]")
{
    FixtureFactory f(51);
    SffDocument doc(SffVersion::V201);
    for (int i = 0; i < 2; ++i) {
        SffPalette p;
        p.group = 1;
        p.number = static_cast<std::uint16_t>(i + 1);
        p.colors = f.palette(256);
        doc.palettes.push_back(p);
    }
    SffPalette copy = doc.palettes[0];
    copy.number = 3;
    doc.palettes.push_back(copy);

    const Image img = f.indexedImage(10, 8, 256, PixelPattern::Runs);
    for (int i = 0; i < 4; ++i) {
        SffSprite s;
        s.number = static_cast<std::uint16_t>(i);
        s.paletteIndex = i % 2;
        doc.sprites.push_back(s);
        doc.setSpriteImage(static_cast<std::size_t>(i), img);
        doc.sprites.back().compression = SffCompression::Rle8;
    }
    doc.sprites[3].image = f.indexedImage(10, 8, 256, PixelPattern::Noise);

    SECTION("duplicates are linked when asked")
    {
        SffWriteOptions options;
        options.linkDuplicateSprites = true;
        options.linkDuplicatePalettes = true;
        const SffDocument back = writeAndRead(doc, options);
        CHECK(back.sprites[0].linkedIndex == -1);
        CHECK(back.sprites[1].linkedIndex == 0);
        CHECK(back.sprites[2].linkedIndex == 0);
        CHECK(back.sprites[3].linkedIndex == -1);
        CHECK(back.sprites[1].paletteIndex == 1);
        CHECK(back.palettes[2].linkedIndex == 0);
        CHECK(back.palettes[2].colors == doc.palettes[0].colors);
        requireSameSprites(doc, back);
        CHECK(doc.write(options)->size() < doc.write()->size());
    }

    SECTION("links survive a round trip and decode through the target")
    {
        doc.sprites[2].image.reset();
        doc.sprites[2].linkedIndex = 1;
        doc.palettes[2].linkedIndex = 0;
        const SffDocument back = writeAndRead(doc);
        CHECK(back.sprites[2].linkedIndex == 1);
        CHECK(back.resolveLink(2) == 1);
        CHECK(back.palettes[2].linkedIndex == 0);
        auto linked = back.decodeSprite(2);
        REQUIRE(linked.ok());
        CHECK(linked->pixelBuffer() == img.pixelBuffer());
        CHECK(linked->palette() == doc.palettes[0].colors);
    }

    SECTION("a palette link is dropped when the colors no longer match")
    {
        doc.palettes[2].linkedIndex = 0;
        doc.palettes[2].colors[5] = rgb(1, 2, 3);
        const SffDocument back = writeAndRead(doc);
        CHECK(back.palettes[2].linkedIndex == -1);
        CHECK(back.palettes[2].colors[5] == rgb(1, 2, 3));
    }

    SECTION("linked offsets follow the document's convention")
    {
        doc.sprites[2].image.reset();
        doc.sprites[2].linkedIndex = 1;
        doc.linkOffsets = SffLinkOffsets::TargetOffset;
        const SffDocument target = writeAndRead(doc);
        CHECK(target.linkOffsets == SffLinkOffsets::TargetOffset);
        doc.linkOffsets = SffLinkOffsets::RunningOffset;
        const SffDocument running = writeAndRead(doc);
        CHECK(running.linkOffsets == SffLinkOffsets::RunningOffset);
    }
}

TEST_CASE("SFF compression policy", "[sff][compression]")
{
    FixtureFactory f(61);
    SffDocument doc(SffVersion::V201);
    SffPalette p;
    p.colors = f.palette(256);
    doc.palettes.push_back(p);
    SffSprite s;
    doc.sprites.push_back(s);
    doc.sprites.push_back(s);
    doc.sprites.push_back(s);
    doc.setSpriteImage(0, f.indexedImage(30, 30, 256, PixelPattern::Runs));
    doc.setSpriteImage(1, f.indexedImage(30, 30, 32, PixelPattern::Repeats));
    doc.setSpriteImage(2, f.rgbaImage(8, 8));

    SECTION("defaults: RLE8, LZ5 for 5-bit sprites, PNG32 for true color")
    {
        SffWriteOptions options;
        options.compression.keepExisting = false;
        const SffDocument back = writeAndRead(doc, options);
        CHECK(back.sprites[0].compression == SffCompression::Rle8);
        CHECK(back.sprites[0].colorDepth == 8);
        CHECK(back.sprites[1].compression == SffCompression::Lz5);
        CHECK(back.sprites[1].colorDepth == 5);
        CHECK(back.sprites[2].compression == SffCompression::Png32);
        CHECK(back.sprites[2].colorDepth == 32);
        requireSameSprites(doc, back);
    }

    SECTION("every compression can be forced")
    {
        for (SffCompression c : {SffCompression::Raw, SffCompression::Rle8, SffCompression::Rle5, SffCompression::Lz5,
                                 SffCompression::Png8}) {
            SffWriteOptions options;
            options.compression.keepExisting = false;
            options.compression.indexed5 = c;
            options.compression.indexed8 = c;
            options.compression.trueColor = SffCompression::Png24;
            const SffDocument back = writeAndRead(doc, options);
            CHECK(back.sprites[1].compression == c);
            const bool fiveBitOnly = c == SffCompression::Rle5 || c == SffCompression::Lz5;
            CHECK(back.sprites[0].compression == (fiveBitOnly ? SffCompression::Rle8 : c));
            CHECK(back.sprites[2].compression == SffCompression::Png24);
            auto px = back.decodeSprite(1, DecodeMode::Strict);
            REQUIRE(px.ok());
            CHECK(px->pixelBuffer() == doc.sprites[1].image->pixelBuffer());
        }
    }

    SECTION("smallest picks the shortest encoding")
    {
        SffWriteOptions options;
        options.compression.smallest = true;
        auto smallest = doc.write(options);
        REQUIRE(smallest.ok());
        for (SffCompression c : {SffCompression::Raw, SffCompression::Rle8, SffCompression::Png8}) {
            SffWriteOptions forced;
            forced.compression.keepExisting = false;
            forced.compression.indexed5 = c;
            forced.compression.indexed8 = c;
            auto other = doc.write(forced);
            REQUIRE(other.ok());
            CHECK(smallest->size() <= other->size());
        }
        requireSameSprites(doc, *SffDocument::read(*smallest));
    }

    SECTION("SFF v2.0 has no PNG and no true color")
    {
        doc.sprites[0].compression = SffCompression::Png8;
        doc.version = SffVersion::V2;
        doc.versionBytes = sffVersionBytes(SffVersion::V2);
        CHECK_FALSE(doc.write().ok());
        doc.sprites.pop_back();
        const SffDocument back = writeAndRead(doc);
        CHECK(back.sprites[0].compression == SffCompression::Rle8);
        requireSameSprites(doc, back);
    }
}

TEST_CASE("newly encoded raw sprites never go to the translated block", "[sff][compression]")
{
    FixtureFactory f(62);
    SffDocument doc(SffVersion::V2);
    SffPalette p;
    p.colors = f.palette(256);
    doc.palettes.push_back(p);
    doc.sprites.resize(2);
    doc.setSpriteImage(0, f.indexedImage(5, 5, 256, PixelPattern::Noise));
    doc.setSpriteImage(1, f.indexedImage(5, 5, 256, PixelPattern::Noise));
    doc.sprites[0].flags = 1;
    doc.sprites[1].flags = 1;
    doc.sprites[0].compression = SffCompression::Raw;
    const SffDocument back = writeAndRead(doc);
    CHECK(back.sprites[0].compression == SffCompression::Raw);
    CHECK(back.sprites[0].flags == 0); // Elecbyte's tools refuse raw data there
    CHECK(back.sprites[1].flags == 1);
    requireSameSprites(doc, back);
}

TEST_CASE("SFF files claiming 2.0.0.0 with PNG sprites are kept as they are", "[sff][v2]")
{
    // Seen in the wild: PNG sprites in a file whose version bytes say 2.0.0.0.
    FixtureFactory f(63);
    auto bytes = f.sff(SffVersion::V201, 12).write();
    REQUIRE(bytes.ok());
    Bytes data = *bytes;
    data[13] = std::byte{0}; // version 2.0.1.0 -> 2.0.0.0
    data[25] = std::byte{0};
    auto doc = SffDocument::read(data);
    REQUIRE(doc.ok());
    CHECK(doc->version == SffVersion::V201);
    CHECK(sffVersionString(doc->versionBytes) == "2.0.0.0");
    auto again = doc->write();
    REQUIRE(again.ok());
    CHECK(*again == data);
}

TEST_CASE("SFF conversion between v1 and v2", "[sff][convert]")
{
    FixtureFactory f(71);
    const SffDocument v1 = f.sff(SffVersion::V1, 30);

    SffDocument v2 = writeAndRead(v1);
    REQUIRE(v2.convertTo(SffVersion::V201).ok());
    CHECK(v2.version == SffVersion::V201);
    CHECK(v2.header[16] == 0);
    const SffDocument v2back = writeAndRead(v2);
    CHECK(v2back.version == SffVersion::V201);
    CHECK(v2back.palettes.size() <= v1.palettes.size());
    REQUIRE(v2back.findPalette(1, 1).has_value());
    CHECK(v2back.sprites[0].paletteIndex == static_cast<std::int32_t>(*v2back.findPalette(1, 1)));
    requireSameSprites(v1, v2back);

    SffDocument v1again = v2back;
    REQUIRE(v1again.convertTo(SffVersion::V1).ok());
    const SffDocument v1back = writeAndRead(v1again);
    CHECK(v1back.version == SffVersion::V1);
    requireSameSprites(v1, v1back);
    for (const SffSprite& s : v1back.sprites)
        CHECK(s.compression == SffCompression::Pcx);
}

TEST_CASE("SFF v2 to v1 turns links with their own palette into copies", "[sff][convert]")
{
    FixtureFactory f(81);
    SffDocument doc = f.sff(SffVersion::V2, 30);
    bool found = false;
    for (std::size_t i = 0; i < doc.sprites.size() && !found; ++i) {
        SffSprite& s = doc.sprites[i];
        if (s.isLinked()) {
            const SffSprite& t = doc.sprites[static_cast<std::size_t>(s.linkedIndex)];
            if (t.colorDepth == 8) {
                s.paletteIndex = t.paletteIndex == 0 ? 1 : 0;
                found = true;
            }
        }
    }
    REQUIRE(found);
    const SffDocument original = writeAndRead(doc);
    SffDocument converted = original;
    REQUIRE(converted.convertTo(SffVersion::V1).ok());
    requireSameSprites(original, writeAndRead(converted));
}

TEST_CASE("SFF conversion refuses true color for v1 and v2.0", "[sff][convert]")
{
    FixtureFactory f(91);
    SffDocument doc = f.sff(SffVersion::V201, 12);
    SffDocument copy = doc;
    CHECK_FALSE(copy.convertTo(SffVersion::V1).ok());
    CHECK(copy.version == SffVersion::V201);
    CHECK_FALSE(copy.convertTo(SffVersion::V2).ok());
}

TEST_CASE("editing an SFF v1 palette updates the stored PCX", "[sff][v1]")
{
    FixtureFactory f(101);
    const SffDocument original = writeAndRead(f.sff(SffVersion::V1, 10));
    SffDocument doc = original;
    doc.palettes[0].colors[7] = rgb(1, 2, 3);
    const SffDocument back = writeAndRead(doc);
    CHECK(back.palettes[0].colors[7] == rgb(1, 2, 3));
    for (std::size_t i = 0; i < back.sprites.size(); ++i) {
        if (original.sprites[i].paletteIndex == 0) {
            const Palette* p = back.spritePalette(i);
            REQUIRE(p);
            CHECK((*p)[7] == rgb(1, 2, 3));
        }
    }
}

TEST_CASE("SFF sprite lookup and replacement", "[sff]")
{
    FixtureFactory f(111);
    SffDocument doc = writeAndRead(f.sff(SffVersion::V201, 10));
    REQUIRE(doc.findSprite(10, 1).has_value());
    CHECK_FALSE(doc.findSprite(999, 9).has_value());
    CHECK(doc.findPalette(9000, 0).has_value());
    CHECK_FALSE(doc.decodeSprite(1000).ok());

    const Image replacement = f.indexedImage(3, 2, 256, PixelPattern::Noise);
    doc.setSpriteImage(0, replacement);
    CHECK(doc.sprites[0].width == 3);
    CHECK(doc.sprites[0].data.empty());
    const SffDocument back = writeAndRead(doc);
    auto px = back.decodeSprite(0);
    REQUIRE(px.ok());
    CHECK(px->pixelBuffer() == replacement.pixelBuffer());

    const Palette red(256, rgb(255, 0, 0));
    auto rgbaImage = back.decodeSpriteRgba(0, &red);
    REQUIRE(rgbaImage.ok());
    CHECK(rgbaImage->colorAt(0, 0).r == 255);
}

TEST_CASE("SFF reader rejects malformed files", "[sff]")
{
    CHECK_FALSE(SffDocument::read(Bytes(100)).ok());
    Bytes junk(600, std::byte{0});
    CHECK_FALSE(SffDocument::read(junk).ok());

    FixtureFactory f(121);
    auto bytes = f.sff(SffVersion::V201, 6).write();
    REQUIRE(bytes.ok());
    Bytes data = *bytes;
    data[15] = std::byte{3}; // unknown major version
    CHECK_FALSE(SffDocument::read(data).ok());

    SffDocument doc(SffVersion::V201);
    SffSprite s;
    s.linkedIndex = 0;
    doc.sprites.push_back(s);
    CHECK_FALSE(doc.write().ok());
}
