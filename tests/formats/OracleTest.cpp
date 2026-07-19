// Elecbyte's sff2png.exe and sprmake2.exe check our SFF files and decoders (Windows, OFS_CORPUS_MUGEN).

#include "fixtures/FixtureFactory.h"
#include "formats/TestEnv.h"
#include "formats/codecs/Png.h"
#include "formats/sff/SffDocument.h"
#include "formats/util/FileIo.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace OFS::Formats;
using namespace OFS::Formats::Test;

namespace {

namespace fs = std::filesystem;

std::string native(const fs::path& path)
{
    return path.string();
}

std::optional<fs::path> tool(const char* name)
{
#if defined(_WIN32)
    const auto mugen = corpusDir("OFS_CORPUS_MUGEN");
    if (!mugen)
        return std::nullopt;
    const fs::path exe = *mugen / name;
    std::error_code ec;
    if (!fs::is_regular_file(exe, ec))
        return std::nullopt;
    return exe;
#else
    (void)name;
    return std::nullopt;
#endif
}

fs::path freshDir(const std::string& name)
{
    const fs::path dir = fs::temp_directory_path() / "ofs-oracle" / name;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir);
    return dir;
}

int run(const fs::path& dir, const fs::path& exe, const std::string& args)
{
    const std::string command = "\"cd /d \"" + native(dir) + "\" && \"" + native(exe) + "\" " + args + " > \"" +
                                native(dir / "log.txt") + "\" 2>&1\"";
    return std::system(command.c_str());
}

std::string readLog(const fs::path& dir)
{
    auto log = readFile(dir / "log.txt");
    return log ? toText(*log) : std::string();
}

std::string trim(const std::string& s)
{
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return {};
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

struct DefSprite
{
    int group = 0;
    int number = 0;
    std::string file;
    int axisX = 0;
    int axisY = 0;
};

// The [Sprite] lines: group, number, file, axisx, axisy.
std::vector<DefSprite> readDefSprites(const fs::path& def)
{
    std::vector<DefSprite> sprites;
    std::ifstream in(def);
    std::string line;
    bool inSprites = false;
    while (std::getline(in, line)) {
        if (const auto semicolon = line.find(';'); semicolon != std::string::npos)
            line.resize(semicolon);
        line = trim(line);
        if (line.empty())
            continue;
        if (line.front() == '[') {
            inSprites = line == "[Sprite]";
            continue;
        }
        if (!inSprites)
            continue;
        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;
        while (std::getline(ss, field, ','))
            fields.push_back(trim(field));
        if (fields.size() < 5)
            continue;
        DefSprite s;
        s.group = std::stoi(fields[0]);
        s.number = std::stoi(fields[1]);
        s.file = fields[2];
        s.axisX = std::stoi(fields[3]);
        s.axisY = std::stoi(fields[4]);
        sprites.push_back(s);
    }
    return sprites;
}

void requireSamePicture(const Image& expected, const Image& actual)
{
    REQUIRE(actual.isIndexed());
    REQUIRE(expected.isIndexed());
    REQUIRE(actual.width() == expected.width());
    REQUIRE(actual.height() == expected.height());
    REQUIRE(actual.pixelBuffer() == expected.pixelBuffer());
    std::set<std::uint8_t> used(expected.pixelBuffer().begin(), expected.pixelBuffer().end());
    for (const std::uint8_t c : used) {
        INFO("palette index " << int{c});
        REQUIRE(c < actual.palette().size());
        REQUIRE(c < expected.palette().size());
        REQUIRE(sameRgb(actual.palette()[c], expected.palette()[c]));
    }
}

SffDocument oracleDocument(SffVersion version, std::uint64_t seed)
{
    FixtureFactory f(seed);
    SffDocument doc = f.sff(version, 36, 40);
    struct Extra
    {
        int w, h, colors;
        PixelPattern pattern;
        SffCompression compression;
    };
    const Extra extras[] = {{1, 1, 256, PixelPattern::Noise, SffCompression::Rle8},
                            {1, 90, 32, PixelPattern::Runs, SffCompression::Lz5},
                            {700, 1, 32, PixelPattern::Runs, SffCompression::Rle5},
                            {300, 4, 256, PixelPattern::Runs, SffCompression::Rle8},
                            {64, 64, 32, PixelPattern::Repeats, SffCompression::Lz5}};
    std::uint16_t number = 0;
    for (const Extra& e : extras) {
        SffSprite s;
        s.group = 777;
        s.number = number++;
        s.axisX = static_cast<std::int16_t>(e.w / 2);
        s.axisY = static_cast<std::int16_t>(-e.h);
        doc.sprites.push_back(s);
        const std::size_t index = doc.sprites.size() - 1;
        if (version == SffVersion::V1) {
            doc.sprites[index].paletteIndex = 0;
            doc.sprites[index].compression = SffCompression::Pcx;
        } else {
            doc.sprites[index].compression = e.compression;
            doc.sprites[index].paletteIndex = e.colors == 32 ? 2 : 0;
        }
        doc.setSpriteImage(index, f.indexedImage(e.w, e.h, e.colors, e.pattern));
    }
    return doc;
}

void checkWithSff2png(SffVersion version, std::uint64_t seed)
{
    const auto sff2png = tool("sff2png.exe");
    if (!sff2png)
        SKIP("OFS_CORPUS_MUGEN is not set or has no sff2png.exe (Windows only)");

    const fs::path dir = freshDir(std::string("sff2png-") + std::string(sffVersionName(version)));
    const SffDocument doc = oracleDocument(version, seed);
    REQUIRE(doc.save(dir / "ours.sff").ok());
    auto ours = SffDocument::load(dir / "ours.sff");
    REQUIRE(ours.ok());
    fs::create_directories(dir / "out");

    const int exit = run(dir, *sff2png, "ours.sff out/x");
    INFO("sff2png log:\n" << readLog(dir));
    REQUIRE(exit == 0);
    const std::vector<DefSprite> exported = readDefSprites(dir / "out" / "x-sff.def");
    REQUIRE(exported.size() == ours->sprites.size());

    std::set<SffCompression> formats;
    for (std::size_t i = 0; i < exported.size(); ++i) {
        INFO("sprite " << i << " exported as " << exported[i].file);
        const SffSprite& s = ours->sprites[i];
        CHECK(exported[i].group == s.group);
        CHECK(exported[i].number == s.number);
        CHECK(exported[i].axisX == s.axisX);
        CHECK(exported[i].axisY == s.axisY);
        auto png = readFile(dir / fs::path(exported[i].file));
        REQUIRE(png.ok());
        auto theirs = decodePng(*png);
        REQUIRE(theirs.ok());
        auto mine = ours->decodeSprite(i, DecodeMode::Strict);
        REQUIRE(mine.ok());
        // sff2png draws a link with its own palette, like decodeSprite(), unless that palette's
        // size differs from the target's. Then it falls back to the target's palette.
        const std::size_t root = ours->resolveLink(i);
        const Palette* own = ours->spritePalette(i);
        const Palette* targets = ours->spritePalette(root);
        if (root != i && own && targets && own->size() != targets->size())
            mine->setPalette(*targets);
        requireSamePicture(*mine, *theirs);
        if (!s.isLinked())
            formats.insert(s.compression);
    }
    std::ostringstream report;
    report << "sff2png matched all " << exported.size() << " sprites of our SFF " << sffVersionName(version)
           << " (formats:";
    for (SffCompression c : formats)
        report << ' ' << sffCompressionName(c);
    report << ")";
    std::cout << report.str() << std::endl;
    WARN(report.str());
}

std::set<SffCompression> checkWithSprmake2(const SffDocument& source, const std::string& name,
                                           const std::string& compress5)
{
    const auto sprmake2 = tool("sprmake2.exe");
    if (!sprmake2)
        SKIP("OFS_CORPUS_MUGEN is not set or has no sprmake2.exe (Windows only)");

    const fs::path dir = freshDir("sprmake2-" + name + "-" + compress5);
    std::ostringstream def;
    def << "[Output]\nfilename = out.sff\n\n"
        << "[Option]\n"
        << "sprite.compress.5 = " << compress5 << "\n"
        << "sprite.compress.8 = rle8\n"
        << "sprite.decompressonload = 0\n"
        << "sprite.detectduplicates = 0\n"
        << "sprite.autocrop = 0\n"
        << "pal.detectduplicates = 0\n"
        << "pal.discardduplicates = 0\n"
        << "pal.reverseact = 0\n"
        << "pal.reversepng = 0\n\n";

    struct Expected
    {
        std::size_t index;
        Image image;
        std::string line;
        std::int32_t sharedPalette; // -1: the sprite brings its own palette
    };
    std::vector<Expected> expected;
    std::set<std::pair<int, int>> ids;
    std::set<std::int32_t> palettes32;
    for (std::size_t i = 0; i < source.sprites.size(); ++i) {
        const SffSprite& s = source.sprites[i];
        if (!ids.insert({s.group, s.number}).second)
            continue; // SprMaker2 needs unique ids
        auto image = source.decodeSprite(i, DecodeMode::Strict);
        REQUIRE(image.ok());
        if (!image->isIndexed())
            continue;
        auto png = encodePng(*image);
        REQUIRE(png.ok());
        const std::string file = "s" + std::to_string(i) + ".png";
        REQUIRE(writeFile(dir / file, *png).ok());
        // SprMaker2 makes 5-bit sprites from sprites that use a 32-color palette from [Pal].
        const Palette* palette = source.spritePalette(i);
        const bool fiveBit = image->maxIndex() < 32 && palette && palette->size() <= 32;
        const std::int32_t shared = fiveBit ? s.paletteIndex : -1;
        if (fiveBit)
            palettes32.insert(shared);
        std::ostringstream line;
        line << s.group << ", " << s.number << ", " << file << ", " << s.axisX << ", " << s.axisY << "\n";
        expected.push_back(Expected{i, std::move(*image), line.str(), shared});
    }

    def << "[Pal]\n";
    for (const std::int32_t p : palettes32) {
        Image holder = Image::indexed(1, 1, {0}, source.palettes[static_cast<std::size_t>(p)].colors);
        const std::string file = "pal" + std::to_string(p) + ".png";
        REQUIRE(writeFile(dir / file, *encodePng(holder)).ok());
        def << "4000, " << p << ", " << file << ", 0, 31\n";
    }
    def << "\n[Option]\nsprite.usepal = -1\nsprite.removecolors = -1\n\n[Sprite]\n";
    for (const Expected& e : expected) {
        if (e.sharedPalette < 0)
            def << e.line;
    }
    for (const std::int32_t p : palettes32) {
        def << "\n[Option]\nsprite.usepal = 4000, " << p << "\nsprite.removecolors = 32, 255\n\n[Sprite]\n";
        for (const Expected& e : expected) {
            if (e.sharedPalette == p)
                def << e.line;
        }
    }
    {
        std::ofstream out(dir / "sprites.def");
        out << def.str();
    }

    const int exit = run(dir, *sprmake2, "sprites.def");
    INFO("sprmake2 log:\n" << readLog(dir));
    REQUIRE(exit == 0);
    auto bytes = readFile(dir / "out.sff");
    REQUIRE(bytes.ok());
    auto built = SffDocument::read(*bytes);
    REQUIRE(built.ok());
    CHECK(built->version == SffVersion::V2);

    // Elecbyte's decoder reads sprmake2's file too (see the RLE5 note below).
    std::map<std::pair<int, int>, fs::path> exported;
    if (const auto sff2png = tool("sff2png.exe")) {
        fs::create_directories(dir / "check");
        REQUIRE(run(dir, *sff2png, "out.sff check/c") == 0);
        for (const DefSprite& d : readDefSprites(dir / "check" / "c-sff.def"))
            exported[{d.group, d.number}] = dir / fs::path(d.file);
    }

    std::set<SffCompression> formats;
    std::size_t identical = 0;
    std::size_t rle5Overflows = 0;
    for (const Expected& e : expected) {
        const SffSprite& original = source.sprites[e.index];
        INFO("sprite " << e.index << " (" << original.group << "," << original.number << ")");
        const auto found = built->findSprite(original.group, original.number);
        REQUIRE(found.has_value());
        const SffSprite& s = built->sprites[*found];
        const SffCompression stored = built->sprites[built->resolveLink(*found)].compression;
        CHECK(s.axisX == original.axisX);
        CHECK(s.axisY == original.axisY);
        if (!s.isLinked())
            formats.insert(s.compression);

        auto strict = built->decodeSprite(*found, DecodeMode::Strict);
        if (strict && strict->pixelBuffer() == e.image.pixelBuffer()) {
            requireSamePicture(e.image, *strict);
            ++identical;
        } else {
            // sprmake2's RLE5 encoder writes packets with 128 extra runs. The count overflows
            // into the "explicit color" bit, so the data no longer means what it intended.
            // Elecbyte's own decoder (sff2png) reads such packets exactly like ours does.
            REQUIRE(stored == SffCompression::Rle5);
            ++rle5Overflows;
        }
        const auto theirsFile = exported.find({original.group, original.number});
        if (theirsFile != exported.end()) {
            auto png = readFile(theirsFile->second);
            REQUIRE(png.ok());
            auto theirs = decodePng(*png);
            REQUIRE(theirs.ok());
            auto lenient = built->decodeSprite(*found, DecodeMode::Lenient);
            REQUIRE(lenient.ok());
            REQUIRE(lenient->pixelBuffer() == theirs->pixelBuffer());
        }
    }

    auto again = built->write();
    REQUIRE(again.ok());
    CHECK(*again == *bytes);

    std::ostringstream report;
    report << "sprmake2 (" << name << ", 5-bit " << compress5 << "): " << identical << "/" << expected.size()
           << " sprites decoded to the exported pixels";
    if (rle5Overflows)
        report << ", " << rle5Overflows << " damaged by sprmake2's RLE5 overflow (decoded like sff2png)";
    if (!exported.empty())
        report << ", all equal to sff2png's decoding";
    report << ", re-save " << (*again == *bytes ? "byte-identical" : "DIFFERENT") << ", formats:";
    for (SffCompression c : formats)
        report << ' ' << sffCompressionName(c);
    std::cout << report.str() << std::endl;
    WARN(report.str());
    return formats;
}

} // namespace

TEST_CASE("sff2png reads our SFF v2.0 files with the same pixels", "[oracle][sff2png]")
{
    checkWithSff2png(SffVersion::V2, 0x0AC1E);
}

TEST_CASE("sff2png reads our SFF v1 files with the same pixels", "[oracle][sff2png]")
{
    checkWithSff2png(SffVersion::V1, 0x0AC1F);
}

TEST_CASE("sprmake2 builds SFFs from our exported sprites that we decode identically", "[oracle][sprmake2]")
{
    auto bytes = oracleDocument(SffVersion::V2, 0x5B3).write();
    REQUIRE(bytes.ok());
    auto source = SffDocument::read(*bytes);
    REQUIRE(source.ok());

    const std::set<SffCompression> lz5 = checkWithSprmake2(*source, "fixture", "lz5");
    CHECK(lz5.count(SffCompression::Lz5) == 1);
    CHECK(lz5.count(SffCompression::Rle8) == 1);
    const std::set<SffCompression> rle5 = checkWithSprmake2(*source, "fixture", "rle5");
    CHECK(rle5.count(SffCompression::Rle5) == 1);
}

TEST_CASE("sprmake2 rebuilds Kung Fu Man from our decoded sprites", "[oracle][sprmake2][kfm]")
{
    const auto kfm = corpusDir("OFS_CORPUS_KFM");
    if (!kfm)
        SKIP("OFS_CORPUS_KFM is not set");
    auto source = SffDocument::load(*kfm / "kfm.sff");
    REQUIRE(source.ok());
    const std::set<SffCompression> formats = checkWithSprmake2(*source, "kfm", "lz5");
    CHECK_FALSE(formats.empty());
}
