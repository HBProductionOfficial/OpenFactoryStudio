// Elecbyte's sprmake2.exe and sndmaker.exe must accept our exports (Windows, OFS_CORPUS_MUGEN).

#include "fixtures/FixtureFactory.h"
#include "formats/OracleTools.h"
#include "formats/maker/SndMakerScript.h"
#include "formats/maker/SprMakerDef.h"
#include "formats/sff/SffDocument.h"
#include "formats/snd/SndDocument.h"
#include "formats/util/FileIo.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>

using namespace OFS::Formats;
using namespace OFS::Formats::Test;

namespace {

namespace fs = std::filesystem;

void writeFiles(const fs::path& dir, const std::vector<MakerFile>& files)
{
    for (const MakerFile& file : files)
        REQUIRE(writeFile(dir / file.name, file.data).ok());
}

void checkSprMakerExport(const SffDocument& source, const std::string& name)
{
    const auto sprmake2 = mugenTool("sprmake2.exe");
    if (!sprmake2)
        SKIP("OFS_CORPUS_MUGEN is not set or has no sprmake2.exe (Windows only)");

    SprMakerExportOptions options;
    options.outputFile = "out.sff";
    auto exported = exportSprMaker(source, options);
    REQUIRE(exported.ok());
    const SprMakerDef& def = exported->def;
    auto text = def.write();
    REQUIRE(text.ok());
    const fs::path dir = freshOracleDir("sprmaker-export-" + name);
    writeFiles(dir, exported->files);
    REQUIRE(writeFile(dir / "sprites.def", asBytes(*text)).ok());

    const int exitCode = runTool(dir, *sprmake2, "sprites.def");
    INFO("sprmake2 log:\n" << readToolLog(dir));
    REQUIRE(exitCode == 0);
    auto bytes = readFile(dir / "out.sff"); // sprmake2 exits with 0 even when it refuses a .def
    REQUIRE(bytes.ok());
    auto built = SffDocument::read(*bytes);
    REQUIRE(built.ok());
    CHECK(built->version == SffVersion::V2);

    REQUIRE(built->palettes.size() == def.palettes.size());
    std::map<std::pair<int, int>, std::size_t> builtPalettes;
    std::size_t paletteLinks = 0;
    for (std::size_t i = 0; i < def.palettes.size(); ++i) {
        INFO("palette " << i);
        const SffPalette& p = built->palettes[i];
        CHECK(p.group == def.palettes[i].id.group);
        CHECK(p.number == def.palettes[i].id.number);
        const std::size_t size = static_cast<std::size_t>(def.palettes[i].lastColor) + 1;
        REQUIRE(p.colors.size() == size);
        Palette expected = source.palettes[i].colors;
        expected.resize(size, rgb(0, 0, 0));
        CHECK(sameRgb(p.colors, expected));
        const bool sharesFile =
            i > 0 && std::any_of(def.palettes.begin(), def.palettes.begin() + static_cast<std::ptrdiff_t>(i),
                                 [&](const SprMakerPalette& q) { return q.file == def.palettes[i].file; });
        CHECK((p.linkedIndex >= 0) == sharesFile);
        paletteLinks += p.linkedIndex >= 0 ? 1 : 0;
        builtPalettes[{p.group, p.number}] = i;
    }

    REQUIRE(built->sprites.size() == def.sprites.size());
    std::map<std::string, std::size_t> firstWithFile;
    std::set<SffCompression> formats;
    std::size_t spriteLinks = 0;
    for (std::size_t k = 0; k < def.sprites.size(); ++k) {
        const SprMakerSprite& entry = def.sprites[k];
        const SffSprite& s = built->sprites[k];
        INFO("sprite " << entry.id.group << "," << entry.id.number);
        CHECK(s.group == entry.id.group);
        CHECK(s.number == entry.id.number);
        CHECK(s.axisX == entry.axisX);
        CHECK(s.axisY == entry.axisY);
        CHECK(((s.flags & 1) != 0) == entry.options.decompressOnLoad);
        const auto original =
            source.findSprite(static_cast<std::uint16_t>(entry.id.group), static_cast<std::uint16_t>(entry.id.number));
        REQUIRE(original.has_value());
        auto expected = source.decodeSprite(*original, DecodeMode::Strict);
        REQUIRE(expected.ok());
        auto actual = built->decodeSprite(k, DecodeMode::Strict);
        REQUIRE(actual.ok());
        REQUIRE(actual->isIndexed());
        CHECK(actual->width() == expected->width());
        CHECK(actual->height() == expected->height());
        CHECK(actual->pixelBuffer() == expected->pixelBuffer());
        REQUIRE(entry.options.usePalette.has_value());
        const auto palette = builtPalettes.find({entry.options.usePalette->group, entry.options.usePalette->number});
        REQUIRE(palette != builtPalettes.end());
        CHECK(s.paletteIndex == static_cast<std::int32_t>(palette->second));
        const auto first = firstWithFile.find(entry.file);
        if (first == firstWithFile.end()) {
            CHECK_FALSE(s.isLinked());
            firstWithFile[entry.file] = k;
            formats.insert(s.compression);
        } else {
            REQUIRE(s.isLinked());
            CHECK(built->resolveLink(k) == first->second);
            ++spriteLinks;
        }
    }

    auto again = built->write();
    REQUIRE(again.ok());
    CHECK(*again == *bytes);

    std::ostringstream report;
    report << "sprmake2 rebuilt our SprMaker2 export of " << name << ": " << def.sprites.size() << " sprites ("
           << spriteLinks << " linked), " << def.palettes.size() << " palettes (" << paletteLinks
           << " linked), all pixels, palettes, axes and flags identical, formats:";
    for (SffCompression c : formats)
        report << ' ' << sffCompressionName(c);
    if (!exported->warnings.empty())
        report << "; " << exported->warnings.size() << " left out (true color or repeated ids)";
    std::cout << report.str() << std::endl;
    WARN(report.str());
}

void checkSndMakerExport(const SndDocument& source, const std::string& name)
{
    const auto sndmaker = mugenTool("sndmaker.exe");
    if (!sndmaker)
        SKIP("OFS_CORPUS_MUGEN is not set or has no sndmaker.exe (Windows only)");

    SndMakerExportOptions options;
    options.outputFile = "out.snd";
    auto exported = exportSndMaker(source, options);
    REQUIRE(exported.ok());
    auto text = exported->script.write();
    REQUIRE(text.ok());
    const fs::path dir = freshOracleDir("sndmaker-export-" + name);
    writeFiles(dir, exported->files);
    REQUIRE(writeFile(dir / "sounds.txt", asBytes(*text)).ok());

    const int exitCode = runTool(dir, *sndmaker, "< sounds.txt");
    INFO("sndmaker log:\n" << readToolLog(dir));
    REQUIRE(exitCode == 0);
    auto bytes = readFile(dir / "out.snd");
    REQUIRE(bytes.ok());
    auto built = SndDocument::read(*bytes);
    REQUIRE(built.ok());
    REQUIRE(built->sounds.size() == source.sounds.size());
    for (std::size_t i = 0; i < source.sounds.size(); ++i) {
        INFO("sound " << i);
        CHECK(built->sounds[i].group == source.sounds[i].group);
        CHECK(built->sounds[i].number == source.sounds[i].number);
        CHECK(built->sounds[i].data == source.sounds[i].data);
    }

    // SndMaker's layout is ours with version bytes 04 00 00 00 and an empty comment.
    SndDocument ours;
    ours.versionBytes = {4, 0, 0, 0};
    ours.sounds = source.sounds;
    CHECK(ours.write() == *bytes);
    CHECK(built->write() == *bytes);

    std::ostringstream report;
    report << "sndmaker rebuilt our SndMaker export of " << name << ": " << source.sounds.size()
           << " sounds identical, archive byte-identical to our writer";
    std::cout << report.str() << std::endl;
    WARN(report.str());
}

} // namespace

TEST_CASE("sprmake2 rebuilds an SFF v2.0 from our SprMaker2 export", "[oracle][sprmake2][export]")
{
    FixtureFactory f(0xE4F);
    SffDocument sff = f.sff(SffVersion::V2, 48, 40);
    auto bytes = sff.write();
    REQUIRE(bytes.ok());
    auto source = SffDocument::read(*bytes);
    REQUIRE(source.ok());
    checkSprMakerExport(*source, "fixture-v2");
}

TEST_CASE("sprmake2 rebuilds the indexed sprites of an SFF v2.01 from our export", "[oracle][sprmake2][export]")
{
    FixtureFactory f(0xE50);
    SffDocument sff = f.sff(SffVersion::V201, 36, 30);
    auto bytes = sff.write();
    REQUIRE(bytes.ok());
    auto source = SffDocument::read(*bytes);
    REQUIRE(source.ok());
    checkSprMakerExport(*source, "fixture-v201");
}

TEST_CASE("sprmake2 rebuilds an SFF v1 from our export", "[oracle][sprmake2][export]")
{
    FixtureFactory f(0xE51);
    SffDocument sff = f.sff(SffVersion::V1, 20, 30);
    auto bytes = sff.write();
    REQUIRE(bytes.ok());
    auto source = SffDocument::read(*bytes);
    REQUIRE(source.ok());
    checkSprMakerExport(*source, "fixture-v1");
}

TEST_CASE("sprmake2 rebuilds Kung Fu Man from our SprMaker2 export", "[oracle][sprmake2][export][kfm]")
{
    const auto kfm = corpusDir("OFS_CORPUS_KFM");
    if (!kfm)
        SKIP("OFS_CORPUS_KFM is not set");
    auto source = SffDocument::load(*kfm / "kfm.sff");
    REQUIRE(source.ok());
    checkSprMakerExport(*source, "kfm");
}

TEST_CASE("sndmaker rebuilds an SND from our SndMaker export", "[oracle][sndmaker][export]")
{
    FixtureFactory f(0x5D0);
    SndDocument snd = f.snd(12);
    snd.sounds[3].group = -7;
    snd.sounds[5].number = 123456;
    checkSndMakerExport(snd, "fixture");
}

TEST_CASE("sndmaker rebuilds Kung Fu Man's sounds from our SndMaker export", "[oracle][sndmaker][export][kfm]")
{
    const auto kfm = corpusDir("OFS_CORPUS_KFM");
    if (!kfm)
        SKIP("OFS_CORPUS_KFM is not set");
    auto source = SndDocument::load(*kfm / "kfm.snd");
    REQUIRE(source.ok());
    checkSndMakerExport(*source, "kfm");
}
