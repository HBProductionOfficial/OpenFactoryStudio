#include "fixtures/FixtureFactory.h"
#include "formats/codecs/Pcx.h"
#include "formats/fnt/FntDocument.h"
#include "formats/snd/SndDocument.h"
#include "formats/util/ByteReader.h"
#include "formats/util/ByteWriter.h"
#include "formats/wav/WavFile.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace OFS::Formats;
using namespace OFS::Formats::Test;

// --- SND -------------------------------------------------------------------------------------

TEST_CASE("SND write and read give back the same sounds", "[snd]")
{
    FixtureFactory f(0x5D);
    SndDocument doc = f.snd(12);
    doc.versionBytes = {4, 0, 0, 0};
    doc.sounds[3].group = -1;
    doc.sounds[4].number = 70000;
    const Bytes bytes = doc.write();
    CHECK(toText(ByteSpan(bytes).first(12)) == std::string_view("ElecbyteSnd\0", 12));

    auto back = SndDocument::read(bytes);
    REQUIRE(back.ok());
    CHECK(back->versionBytes == doc.versionBytes);
    CHECK(back->comment == doc.comment);
    REQUIRE(back->sounds.size() == doc.sounds.size());
    for (std::size_t i = 0; i < doc.sounds.size(); ++i) {
        CHECK(back->sounds[i].group == doc.sounds[i].group);
        CHECK(back->sounds[i].number == doc.sounds[i].number);
        CHECK(back->sounds[i].data == doc.sounds[i].data);
    }
    CHECK(back->write() == bytes);
    CHECK(back->find(doc.sounds[3].group, doc.sounds[3].number).has_value());
    CHECK_FALSE(back->find(12345, 6789).has_value());

    // Layout: header 512 bytes, 16-byte subheaders, the last "next" offset is the file size.
    ByteReader r(bytes);
    r.seek(16);
    CHECK(r.u32() == 12);
    CHECK(r.u32() == 512);
}

TEST_CASE("SND keeps sound data verbatim, even when it is not a WAV file", "[snd]")
{
    SndDocument doc;
    SndSound s;
    s.group = 1;
    s.data = toBytes("not a wav at all");
    doc.sounds.push_back(s);
    doc.sounds.push_back(SndSound{});
    auto back = SndDocument::read(doc.write());
    REQUIRE(back.ok());
    CHECK(back->sounds[0].data == s.data);
    CHECK(back->sounds[1].data.empty());

    CHECK(SndDocument::read(SndDocument{}.write()).ok());
    CHECK_FALSE(SndDocument::read(toBytes("ElecbyteSpr")).ok());
}

// --- WAV -------------------------------------------------------------------------------------

TEST_CASE("WAV files round-trip in every supported sample format", "[wav]")
{
    FixtureFactory f(0x3A7);
    struct Case
    {
        std::uint16_t bits;
        bool floatingPoint;
        double tolerance;
    };
    for (const Case c : {Case{8, false, 0.02}, Case{16, false, 1e-4}, Case{24, false, 1e-6}, Case{32, false, 1e-9},
                         Case{32, true, 1e-7}, Case{64, true, 1e-15}}) {
        INFO("bits " << c.bits << (c.floatingPoint ? " float" : " pcm"));
        std::vector<double> samples(400);
        for (double& s : samples)
            s = static_cast<double>(f.rng().range(-1000, 1000)) / 1000.0;
        auto wav = WavFile::fromSamples(samples, 2, 44100, c.bits, c.floatingPoint);
        REQUIRE(wav.ok());
        CHECK(wav->isSupported());
        CHECK(wav->frameCount() == 200);
        CHECK(wav->durationSeconds() == Catch::Approx(200.0 / 44100.0));
        auto back = WavFile::parse(wav->write(), WavParseOptions{true});
        REQUIRE(back.ok());
        CHECK(back->format.bitsPerSample == c.bits);
        CHECK(back->format.channels == 2);
        CHECK(back->format.sampleRate == 44100);
        const std::vector<double> decoded = back->toDoubles();
        REQUIRE(decoded.size() == samples.size());
        for (std::size_t i = 0; i < samples.size(); ++i)
            REQUIRE(std::abs(decoded[i] - samples[i]) <= c.tolerance);
        CHECK(back->sample(0, 1) == decoded[1]);
        CHECK(back->sample(999, 0) == 0.0);
    }
    CHECK_FALSE(WavFile::fromSamples({0.0}, 1, 8000, 12).ok());
    CHECK_FALSE(WavFile::fromSamples({0.0}, 2, 8000, 16).ok());
}

TEST_CASE("WAV parsing preserves unknown chunks and their order", "[wav]")
{
    FixtureFactory f(0x3A8);
    const WavFile wav = f.wav(1, 22050, 16, 33);
    const Bytes bytes = wav.write();
    auto back = WavFile::parse(bytes, WavParseOptions{true});
    REQUIRE(back.ok());
    REQUIRE(back->chunks.size() == 3);
    CHECK(back->chunks[0].name() == "fmt ");
    CHECK(back->chunks[1].name() == "data");
    CHECK(back->chunks[2].name() == "LIST");
    CHECK(back->chunks[2].data == wav.chunks[2].data);
    CHECK(back->samples == wav.samples);
    CHECK(back->write() == bytes);
    // The odd-sized LIST chunk gets a pad byte and the RIFF size covers it.
    ByteReader r(bytes);
    r.seek(4);
    CHECK(r.u32() + 8 == bytes.size());
    CHECK(bytes.size() % 2 == 0);
}

TEST_CASE("WAV parsing tolerates common damage unless strict", "[wav]")
{
    FixtureFactory f(0x3A9);
    const Bytes good = f.wav(1, 11025, 8, 101).write();

    Bytes wrongSize = good;
    wrongSize[4] = std::byte{0xFF};
    wrongSize[5] = std::byte{0xFF};
    CHECK(WavFile::parse(wrongSize).ok());
    CHECK_FALSE(WavFile::parse(wrongSize, WavParseOptions{true}).ok());

    Bytes trailing = good;
    for (int i = 0; i < 11; ++i)
        trailing.push_back(std::byte{0x41});
    CHECK(WavFile::parse(trailing).ok());

    ByteWriter w;
    w.text("RIFF");
    w.u32(100);
    w.text("WAVEfmt ");
    w.u32(16);
    w.u16(1);
    w.u16(1);
    w.u32(8000);
    w.u32(8000);
    w.u16(1);
    w.u16(8);
    w.text("data");
    w.u32(50);
    w.fill(10, 0x80);
    auto cut = WavFile::parse(w.data());
    REQUIRE(cut.ok());
    CHECK(cut->samples.size() == 10);

    CHECK_FALSE(WavFile::parse(toBytes("RIFF\x10\x00\x00\x00WAVE")).ok());
    CHECK_FALSE(WavFile::parse(toBytes("RIFX")).ok());
}

TEST_CASE("WAV extensible format reports its sub-format", "[wav]")
{
    WavFormat format;
    format.formatTag = kWavFormatExtensible;
    ByteWriter ext;
    ext.u16(22);
    ext.u16(24);
    ext.u32(3);
    ext.u16(kWavFormatFloat);
    ext.zeros(14);
    format.extension = ext.take();
    CHECK(format.sampleFormat() == kWavFormatFloat);
    format.extension.resize(4);
    CHECK(format.sampleFormat() == kWavFormatExtensible);
}

// --- FNT -------------------------------------------------------------------------------------

namespace {

const char* const kFontText = "; comment line\r\n"
                              "[Def]\r\n"
                              "Size = 8, 9 ; width, height\r\n"
                              "Spacing = 1,2\r\n"
                              "Colors = 3\r\n"
                              "Offset = -1,4\r\n"
                              "Type = Variable\r\n"
                              "\r\n"
                              "[Map]\r\n"
                              "A 0 7\r\n"
                              "B 7 6 ; comment\r\n"
                              "; 9 9 9\r\n"
                              "0x3B 13 5\r\n"
                              "= 18 4\r\n";

FntDocument makeFont(std::uint64_t seed)
{
    FixtureFactory f(seed);
    FntDocument doc;
    doc.pcx = *encodePcx(f.indexedImage(22, 9, 4, PixelPattern::Noise));
    doc.text = kFontText;
    return doc;
}

} // namespace

TEST_CASE("FNT write and read keep the image and text verbatim", "[fnt]")
{
    FntDocument doc = makeFont(1);
    const Bytes bytes = doc.write();
    auto back = FntDocument::read(bytes);
    REQUIRE(back.ok());
    CHECK(back->pcx == doc.pcx);
    CHECK(back->text == doc.text);
    CHECK(back->versionBytes == doc.versionBytes);
    CHECK(back->write() == bytes);
    auto image = back->image();
    REQUIRE(image.ok());
    CHECK(image->width() == 22);

    doc.headerPadding = toBytes("12345678");
    auto padded = FntDocument::read(doc.write());
    REQUIRE(padded.ok());
    CHECK(padded->headerPadding == doc.headerPadding);
    CHECK(padded->write() == doc.write());
}

TEST_CASE("FNT text is parsed into font metrics and glyphs", "[fnt]")
{
    const FntInfo info = parseFntText(kFontText);
    CHECK(info.width == 8);
    CHECK(info.height == 9);
    CHECK(info.spacingX == 1);
    CHECK(info.spacingY == 2);
    CHECK(info.colors == 3);
    CHECK(info.offsetX == -1);
    CHECK(info.offsetY == 4);
    CHECK(info.type == FntInfo::Type::Variable);
    REQUIRE(info.glyphs.size() == 5);
    CHECK(info.glyphs[0].code == 'A');
    CHECK(info.glyphs[1].x == 7);
    CHECK(info.glyphs[1].width == 6);
    CHECK(info.glyphs[2].code == ';');
    CHECK(info.glyphs[2].x == 9);
    CHECK(info.glyphs[3].code == 0x3B);
    CHECK(info.glyphs[3].x == 13);
    CHECK(info.glyphs[4].code == '=');

    const FntInfo fixed = parseFntText("[Def]\nSize=6,8\nType=Fixed\n[Map]\na\nb\nc 10\nd\n");
    CHECK(fixed.type == FntInfo::Type::Fixed);
    REQUIRE(fixed.glyphs.size() == 4);
    CHECK(fixed.glyphs[1].x == 1);
    CHECK(fixed.glyphs[2].x == 10);
    CHECK(fixed.glyphs[3].x == 11);
    CHECK(fixed.glyphs[3].width == 6);
}

TEST_CASE("FNT reader rejects malformed files", "[fnt]")
{
    CHECK_FALSE(FntDocument::read(toBytes("ElecbyteFnt")).ok());
    Bytes bytes = makeFont(2).write();
    bytes[23] = std::byte{0x7F}; // PCX size far too large
    CHECK_FALSE(FntDocument::read(bytes).ok());
}
