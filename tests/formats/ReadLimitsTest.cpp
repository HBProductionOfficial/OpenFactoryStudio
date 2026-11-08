#include "formats/util/ReadLimits.h"
#include "fixtures/FixtureFactory.h"
#include "formats/codecs/Lz5.h"
#include "formats/codecs/Rle5.h"
#include "formats/codecs/Rle8.h"
#include "formats/sff/SffDocument.h"
#include "formats/snd/SndDocument.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <initializer_list>

using namespace OFS::Formats;
using namespace OFS::Formats::Test;

// Small files that ask for far more memory or time than their size, the way a crafted file would.

namespace {

constexpr std::uint64_t kSlack = std::uint64_t{64} << 20;
constexpr std::size_t kSpriteNode = 28;
constexpr std::size_t kPaletteNode = 16;
const std::string kMoreProblems = "There are more problems than the ones listed here.";

std::uint32_t u32At(const Bytes& data, std::size_t offset)
{
    std::uint32_t v = 0;
    std::memcpy(&v, data.data() + offset, 4);
    return v;
}

void put16(Bytes& data, std::size_t at, std::uint16_t v)
{
    std::memcpy(data.data() + at, &v, 2);
}

void put32(Bytes& data, std::size_t at, std::uint32_t v)
{
    std::memcpy(data.data() + at, &v, 4);
}

Bytes bytes(std::initializer_list<int> values)
{
    Bytes out;
    for (int v : values)
        out.push_back(static_cast<std::byte>(v));
    return out;
}

// A v2 file with one sprite, written with the given size and compression.
Bytes oneSprite(FixtureFactory& f, int width, int height, SffCompression compression)
{
    SffDocument doc(SffVersion::V2);
    SffPalette p;
    p.colors = f.palette(256);
    doc.palettes.push_back(p);
    doc.sprites.emplace_back();
    doc.setSpriteImage(0, f.indexedImage(width, height, 256, PixelPattern::Noise));
    doc.sprites[0].compression = compression;
    auto written = doc.write();
    REQUIRE(written.ok());
    return *written;
}

// Puts a new sprite table at the end of the file, made from copies of the first node.
std::size_t newSpriteTable(Bytes& data, std::size_t count)
{
    const std::size_t original = u32At(data, 0x24);
    const Bytes node(data.begin() + static_cast<std::ptrdiff_t>(original),
                     data.begin() + static_cast<std::ptrdiff_t>(original + kSpriteNode));
    const std::size_t table = data.size();
    for (std::size_t i = 0; i < count; ++i)
        data.insert(data.end(), node.begin(), node.end());
    put32(data, 0x24, static_cast<std::uint32_t>(table));
    put32(data, 0x28, static_cast<std::uint32_t>(count));
    return table;
}

bool endsWith(const std::string& text, const std::string& end)
{
    return text.size() >= end.size() && text.compare(text.size() - end.size(), end.size(), end) == 0;
}

} // namespace

TEST_CASE("SFF v2 sprites that point to the same data are copied only up to a limit", "[sff][limits]")
{
    FixtureFactory f(301);
    Bytes data = oneSprite(f, 1024, 1024, SffCompression::Raw);
    newSpriteTable(data, 300);
    auto back = SffDocument::read(data);
    REQUIRE(back.ok());
    REQUIRE(back->sprites.size() == 300);
    std::uint64_t copied = 0;
    for (const SffSprite& s : back->sprites)
        copied += s.data.size();
    CHECK(copied <= 2 * std::uint64_t{data.size()} + kSlack);
    CHECK(back->decodeSprite(0).ok());
    REQUIRE(back->readProblems.size() == kMaxReadProblems + 1);
    CHECK(endsWith(back->readProblems.front(), " uses data that too many other sprites use, so it has no image."));
    CHECK(back->readProblems.back() == kMoreProblems);
}

TEST_CASE("SFF v1 subheaders that overlap are copied only up to a limit", "[sff][limits]")
{
    // A subheader every 8 bytes, each one taking the rest of the file as its data.
    constexpr std::size_t header = 512;
    constexpr std::size_t count = 4000;
    Bytes data = *SffDocument(SffVersion::V1).write();
    REQUIRE(data.size() >= header);
    data.resize(header + (std::size_t{1} << 20), std::byte{0x11});
    put32(data, 20, count);
    put32(data, 24, header);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t at = header + i * 8;
        put32(data, at, static_cast<std::uint32_t>(at + 8));
        put32(data, at + 4, static_cast<std::uint32_t>(data.size() - at - 32));
    }
    auto back = SffDocument::read(data);
    REQUIRE(back.ok());
    std::uint64_t copied = 0;
    for (const SffSprite& s : back->sprites)
        copied += s.data.size();
    CHECK(copied <= 2 * std::uint64_t{data.size()} + kSlack);
    CHECK(back->readProblems.size() == kMaxReadProblems + 1);
    bool limited = false;
    for (const std::string& problem : back->readProblems)
        limited = limited || endsWith(problem, " uses data that too many other sprites use, so it has no image.");
    CHECK(limited);
}

TEST_CASE("a long chain of SFF links reads at once and links straight to the pixels", "[sff][limits]")
{
    FixtureFactory f(302);
    Bytes data = oneSprite(f, 6, 5, SffCompression::Rle8);
    const auto first = SffDocument::read(data);
    REQUIRE(first.ok());
    // Each sprite links to the next one, and only the last one has pixels.
    constexpr std::size_t count = 50000;
    const std::size_t table = newSpriteTable(data, count);
    for (std::size_t i = 0; i + 1 < count; ++i) {
        put16(data, table + i * kSpriteNode + 2, static_cast<std::uint16_t>(i));
        put16(data, table + i * kSpriteNode + 12, static_cast<std::uint16_t>(i + 1));
        put32(data, table + i * kSpriteNode + 20, 0);
    }
    auto back = SffDocument::read(data);
    REQUIRE(back.ok());
    CHECK(back->readProblems.empty());
    CHECK_FALSE(back->sprites[0].isLinked());
    CHECK(back->sprites[0].data == first->sprites[0].data);
    bool straight = true;
    for (std::size_t i = 1; i < count; ++i)
        straight = straight && back->sprites[i].linkedIndex == 0;
    CHECK(straight);
    auto last = back->decodeSprite(count - 1);
    REQUIRE(last.ok());
    CHECK(last->pixelBuffer() == first->decodeSprite(0)->pixelBuffer());
}

TEST_CASE("SFF v1 links to a link go straight to the pixels", "[sff][limits]")
{
    FixtureFactory f(303);
    SffDocument doc = f.sff(SffVersion::V1, 3);
    doc.sprites[1].image.reset();
    doc.sprites[1].data.clear();
    doc.sprites[1].linkedIndex = 0;
    doc.sprites[2].image.reset();
    doc.sprites[2].data.clear();
    doc.sprites[2].linkedIndex = 1;
    auto written = doc.write();
    REQUIRE(written.ok());
    auto back = SffDocument::read(*written);
    REQUIRE(back.ok());
    CHECK(back->sprites[1].linkedIndex == 0);
    CHECK(back->sprites[2].linkedIndex == 0);
    CHECK(back->resolveLink(2) == 0);
}

TEST_CASE("an SFF palette that links to a palette linking forward gets its colors", "[sff][limits]")
{
    FixtureFactory f(304);
    SffDocument doc(SffVersion::V2);
    for (int i = 0; i < 3; ++i) {
        SffPalette p;
        p.group = 1;
        p.number = static_cast<std::uint16_t>(i + 1);
        p.colors = f.palette(256);
        doc.palettes.push_back(p);
    }
    doc.sprites.emplace_back();
    doc.setSpriteImage(0, f.indexedImage(4, 4, 256, PixelPattern::Noise));
    auto written = doc.write();
    REQUIRE(written.ok());
    Bytes data = *written;
    // Palette 0 links forward to palette 2, and palette 1 links back to palette 0.
    const std::size_t table = u32At(data, 0x2C);
    put16(data, table + 6, 2);
    put32(data, table + 12, 0);
    put16(data, table + kPaletteNode + 6, 0);
    put32(data, table + kPaletteNode + 12, 0);
    auto back = SffDocument::read(data);
    REQUIRE(back.ok());
    CHECK(back->readProblems.empty());
    CHECK(back->palettes[0].colors == doc.palettes[2].colors);
    CHECK(back->palettes[1].colors == doc.palettes[2].colors);
    CHECK(back->palettes[0].linkedIndex == -1);
    CHECK(back->palettes[1].linkedIndex == 0);

    SECTION("and a loop of palette links is gray")
    {
        put16(data, table + 2 * kPaletteNode + 6, 1);
        put32(data, table + 2 * kPaletteNode + 12, 0);
        auto loop = SffDocument::read(data);
        REQUIRE(loop.ok());
        CHECK(loop->readProblems.size() == 3);
        for (const SffPalette& p : loop->palettes) {
            CHECK(p.linkedIndex == -1);
            CHECK(p.colors.size() == 256);
        }
    }
}

TEST_CASE("SND sounds that run over the next ones are copied only up to a limit", "[snd][limits]")
{
    // Every sound starts right after the one before it and runs to the end of the file.
    constexpr std::size_t count = 300;
    constexpr std::size_t header = 512;
    constexpr std::size_t node = 16;
    Bytes data = SndDocument{}.write();
    REQUIRE(data.size() == header);
    data.resize(header + count * node + (std::size_t{1} << 20), std::byte{0x11});
    put32(data, 16, count);
    put32(data, 20, header);
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t at = header + i * node;
        put32(data, at, static_cast<std::uint32_t>(at + node));
        put32(data, at + 4, static_cast<std::uint32_t>(data.size() - at - node));
        put32(data, at + 8, 1);
        put32(data, at + 12, static_cast<std::uint32_t>(i));
    }
    auto back = SndDocument::read(data);
    REQUIRE(back.ok());
    REQUIRE(back->sounds.size() == count);
    std::uint64_t copied = 0;
    for (const SndSound& s : back->sounds)
        copied += s.data.size();
    CHECK(copied <= 2 * std::uint64_t{data.size()} + kSlack);
    CHECK_FALSE(back->sounds[0].data.empty());
    REQUIRE(back->readProblems.size() == kMaxReadProblems + 1);
    CHECK(endsWith(back->readProblems.front(), " uses data that too many other sounds use, so it is silent."));
    CHECK(back->readProblems.back() == kMoreProblems);
}

TEST_CASE("lenient decoding pads small damaged sprites but not huge ones", "[codec][limits]")
{
    constexpr std::size_t small = 64 * 64;
    constexpr std::size_t huge = 4096 * 4096;
    CHECK(decodeRle8(bytes({0x43, 1}), small, DecodeMode::Lenient).ok());
    CHECK_FALSE(decodeRle8(bytes({0x43, 1}), huge, DecodeMode::Lenient).ok());
    CHECK(decodeRle5(bytes({0x01, 0x81, 0x04}), small, DecodeMode::Lenient).ok());
    CHECK_FALSE(decodeRle5(bytes({0x01, 0x81, 0x04}), huge, DecodeMode::Lenient).ok());
    CHECK(decodeLz5(bytes({0x02, 0x21, 0x01, 0x05}), small, DecodeMode::Lenient).ok());
    CHECK_FALSE(decodeLz5(bytes({0x02, 0x21, 0x01, 0x05}), huge, DecodeMode::Lenient).ok());

    SffSprite s;
    s.compression = SffCompression::Raw;
    s.colorDepth = 8;
    s.data = Bytes(100, std::byte{1});
    s.width = 64;
    s.height = 64;
    CHECK(SffDocument::decodePayload(s, nullptr, DecodeMode::Lenient).ok());
    s.width = 4096;
    s.height = 4096;
    CHECK_FALSE(SffDocument::decodePayload(s, nullptr, DecodeMode::Lenient).ok());
}
