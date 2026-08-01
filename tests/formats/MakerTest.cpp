#include "fixtures/FixtureFactory.h"
#include "formats/codecs/Png.h"
#include "formats/maker/SndMakerScript.h"
#include "formats/maker/SprMakerDef.h"
#include "formats/palette/PaletteFormats.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>
#include <string>

using namespace OFS::Formats;
using namespace OFS::Formats::Test;

namespace {

const char* const kSampleDef = R"(; Builds a test SFF
[Output]
 ;Filename of the SFF file to create (required).
filename = chars/test/test.sff

[Option]
;input.dir =
sprite.compress.5 = lz5
sprite.compress.8 = rle8
sprite.compress.24 = none
sprite.decompressonload = 0
sprite.detectduplicates = 0
sprite.autocrop = 1
pal.detectduplicates = 1
pal.discardduplicates = 1
pal.reverseact = 0
pal.reversepng = 0

[Pal]
1,1, test.act, 0,255
1,2, test2.act, 0, 255
4000, 0, small.png, 0,31

[Option] ;You can set option states at any time
sprite.usepal = -1   ;Use -1 to add and use self's pal (autopal)

[Sprite]
9000, 1, face b.png,  0,  0     ? usepal = 1,2  ;the line's own palette

[Option]
sprite.usepal = 1,1
SPRITE.RemoveColors = -1

[Sprite]
   0, 0, stand00.png,  18,105
   0, 1, stand01.png,  -18,-104 ? crop = 1,2,30,40 ? usepal = -1
	5, 0, sturn00.png,	16,	105

[Option]
sprite.decompressonload = 1
sprite.usepal = 4000,0
sprite.removecolors = 32,255
[Sprite]
6000, 7, num.pcx, 0, 0
)";

SprMakerDef randomDef(SplitMix64& rng)
{
    SprMakerDef def;
    def.outputFile = "out/" + std::to_string(rng.below(1000)) + ".sff";
    if (rng.chance(30))
        def.inputDir = "work/x";
    def.palDetectDuplicates = rng.chance(50);
    def.palDiscardDuplicates = rng.chance(50);
    def.palReverseAct = rng.chance(50);
    def.palReversePng = rng.chance(50);
    for (int i = 0, n = static_cast<int>(rng.below(4)); i < n; ++i)
        def.palettes.push_back({{static_cast<std::int32_t>(rng.below(5)), i},
                                "p" + std::to_string(i) + ".act",
                                0,
                                rng.chance(50) ? 31 : 255});
    SprMakerSpriteOptions options;
    for (int i = 0, n = static_cast<int>(rng.below(12)); i < n; ++i) {
        if (rng.chance(30)) {
            static constexpr SprMakerCompression kFive[] = {SprMakerCompression::None, SprMakerCompression::Lz5,
                                                            SprMakerCompression::Rle5};
            options.compress5 = kFive[rng.below(3)];
            options.compress8 = rng.chance(50) ? SprMakerCompression::Rle8 : SprMakerCompression::None;
            options.decompressOnLoad = rng.chance(50);
            options.detectDuplicates = rng.chance(50);
            options.autoCrop = rng.chance(50);
            if (rng.chance(50))
                options.usePalette = SprMakerId{1, static_cast<std::int32_t>(rng.below(3))};
            else
                options.usePalette.reset();
            if (rng.chance(50))
                options.removeColors = SprMakerColorRange{32, 255};
            else
                options.removeColors.reset();
        }
        SprMakerSprite s;
        s.id = {static_cast<std::int32_t>(rng.below(100)), i};
        s.file = "s" + std::to_string(i) + ".png";
        s.axisX = rng.range(-300, 300);
        s.axisY = rng.range(-300, 300);
        s.options = options;
        if (rng.chance(20))
            s.crop = SprMakerCrop{0, 1, 2 + rng.range(0, 9), 3};
        def.sprites.push_back(std::move(s));
    }
    return def;
}

std::map<std::string, const Bytes*> filesByName(const std::vector<MakerFile>& files)
{
    std::map<std::string, const Bytes*> out;
    for (const MakerFile& f : files)
        out[f.name] = &f.data;
    return out;
}

} // namespace

TEST_CASE("SprMaker2 .def files are read with their options in effect", "[maker][sprmaker]")
{
    auto def = SprMakerDef::parse(kSampleDef);
    REQUIRE(def.ok());
    CHECK(def->outputFile == "chars/test/test.sff");
    CHECK(def->inputDir.empty());
    CHECK(def->palDetectDuplicates);
    CHECK(def->palDiscardDuplicates);
    CHECK_FALSE(def->palReverseAct);

    REQUIRE(def->palettes.size() == 3);
    CHECK(def->palettes[1] == SprMakerPalette{{1, 2}, "test2.act", 0, 255});
    CHECK(def->palettes[2] == SprMakerPalette{{4000, 0}, "small.png", 0, 31});

    REQUIRE(def->sprites.size() == 5);
    const SprMakerSprite& face = def->sprites[0];
    CHECK(face.id == SprMakerId{9000, 1});
    CHECK(face.file == "face b.png");
    CHECK(face.options.usePalette == SprMakerId{1, 2});
    CHECK(face.options.compress5 == SprMakerCompression::Lz5);
    CHECK(face.options.compress8 == SprMakerCompression::Rle8);
    CHECK(face.options.autoCrop);

    const SprMakerSprite& stand = def->sprites[1];
    CHECK(stand.axisX == 18);
    CHECK(stand.axisY == 105);
    CHECK(stand.options.usePalette == SprMakerId{1, 1});
    CHECK_FALSE(stand.options.removeColors);
    CHECK_FALSE(stand.crop);

    const SprMakerSprite& cropped = def->sprites[2];
    CHECK(cropped.axisX == -18);
    CHECK(cropped.crop == SprMakerCrop{1, 2, 30, 40});
    CHECK_FALSE(cropped.options.usePalette);
    CHECK(def->sprites[3].options.usePalette == SprMakerId{1, 1});
    CHECK(def->sprites[3].axisY == 105);

    const SprMakerSprite& small = def->sprites[4];
    CHECK(small.options.decompressOnLoad);
    CHECK(small.options.usePalette == SprMakerId{4000, 0});
    CHECK(small.options.removeColors == SprMakerColorRange{32, 255});

    auto text = def->write();
    REQUIRE(text.ok());
    CHECK(text->find("\r\n") != std::string::npos);
    auto again = SprMakerDef::parse(*text);
    REQUIRE(again.ok());
    CHECK(*again == *def);
}

TEST_CASE("SprMaker2 .def files round-trip through write and parse", "[maker][sprmaker]")
{
    SplitMix64 rng(0x5D3F);
    for (int round = 0; round < 200; ++round) {
        const SprMakerDef def = randomDef(rng);
        auto text = def.write();
        REQUIRE(text.ok());
        INFO(*text);
        auto back = SprMakerDef::parse(*text);
        REQUIRE(back.ok());
        CHECK(*back == def);
    }
}

TEST_CASE("SprMaker2 .def errors name the line", "[maker][sprmaker]")
{
    auto error = [](const std::string& text) {
        auto def = SprMakerDef::parse(text);
        REQUIRE_FALSE(def.ok());
        return def.error().message;
    };
    const std::string head = "[Output]\nfilename = a.sff\n";
    CHECK(error(head + "[Option]\nsprite.compress.8 = lz5\n").rfind("line 4:", 0) == 0);
    CHECK(error(head + "[Option]\nsprite.wobble = 1\n").find("unknown option") != std::string::npos);
    CHECK(error(head + "[Sprites]\n").find("unknown section") != std::string::npos);
    CHECK(error(head + "[Sprite]\n0, 0\n").rfind("line 4:", 0) == 0);
    CHECK(error(head + "[Sprite]\n0, 0, a.png, x, 0\n").rfind("line 4:", 0) == 0);
    CHECK(error(head + "[Sprite]\n0, 0, a.png, 0, 0 ? removecolors = -1\n").find("unknown sprite option") !=
          std::string::npos);
    CHECK(error(head + "[Sprite]\n0, 0, a.png, 0, 0 ? crop = 1,2,3\n").rfind("line 4:", 0) == 0);
    CHECK(error(head + "[Pal]\n1,1, a.act, 0, 300\n").rfind("line 4:", 0) == 0);
    CHECK(error(head + "[Option]\nsprite.usepal = 3\n").rfind("line 4:", 0) == 0);
    CHECK(error(head + "[Option]\nsprite.removecolors = 40,20\n").rfind("line 4:", 0) == 0);
    CHECK(error("filename = a.sff\n").rfind("line 1:", 0) == 0);
    CHECK(error("[Option]\nsprite.autocrop = 1\n").find("filename") != std::string::npos);
    CHECK(error(head + "[Option\n").rfind("line 3:", 0) == 0);

    SprMakerDef def;
    def.outputFile = "a.sff";
    def.sprites.push_back({{0, 0}, "a,b.png", 0, 0, {}, {}});
    CHECK_FALSE(def.write().ok());
    def.sprites[0].file = " padded.png";
    CHECK_FALSE(def.write().ok());
    def.sprites[0].file = "ok.png";
    CHECK(def.write().ok());
    def.outputFile.clear();
    CHECK_FALSE(def.write().ok());
}

TEST_CASE("damaged .def and SndMaker text never crashes the readers", "[maker][fuzz]")
{
    SplitMix64 rng(0xF422);
    const std::string base = kSampleDef;
    const std::string sound = "out.snd\na.wav\n1\n2\nb.wav\n-3\n4\n";
    for (int round = 0; round < 2000; ++round) {
        std::string text = rng.chance(50) ? base : sound;
        for (int k = 0, n = 1 + static_cast<int>(rng.below(6)); k < n; ++k) {
            const auto at = rng.below(static_cast<std::uint32_t>(text.size()));
            static constexpr char kNoise[] = {',', '?', '=', ';', '[', ']', '\n', '\r', ' ', '-', '9', 'x', '\0'};
            text[at] = kNoise[rng.below(static_cast<std::uint32_t>(std::size(kNoise)))];
        }
        if (rng.chance(20))
            text.resize(rng.below(static_cast<std::uint32_t>(text.size())));
        (void)SprMakerDef::parse(text);
        (void)SndMakerScript::parse(text);
    }
    SUCCEED("no crashes");
}

TEST_CASE("an SFF exports to a SprMaker2 project", "[maker][sprmaker]")
{
    FixtureFactory f(0xE59);
    SffDocument sff = f.sff(SffVersion::V201, 24, 20);
    sff.sprites[3].flags = 1; // decompress on load
    SffSprite repeat = sff.sprites[0];
    sff.sprites.push_back(repeat);

    auto exported = exportSprMaker(sff);
    REQUIRE(exported.ok());
    const SprMakerDef& def = exported->def;
    const auto files = filesByName(exported->files);
    CHECK(def.outputFile == "sprites.sff");
    CHECK_FALSE(def.palReverseAct);
    CHECK_FALSE(def.palDetectDuplicates);
    CHECK_FALSE(def.palDiscardDuplicates);

    REQUIRE(def.palettes.size() == sff.palettes.size());
    for (std::size_t i = 0; i < sff.palettes.size(); ++i) {
        const SprMakerPalette& p = def.palettes[i];
        INFO("palette " << i);
        CHECK(p.id == SprMakerId{sff.palettes[i].group, sff.palettes[i].number});
        CHECK(p.firstColor == 0);
        CHECK(p.lastColor == static_cast<int>(sff.palettes[i].colors.size()) - 1);
        REQUIRE(files.count(p.file) == 1);
        auto act = readAct(*files.at(p.file), ActOrder::Reversed);
        REQUIRE(act.ok());
        Palette expected = sff.palettes[i].colors;
        expected.resize(256, rgb(0, 0, 0));
        CHECK(sameRgb(*act, expected));
    }
    CHECK(def.palettes[3].file == def.palettes[0].file);

    std::size_t trueColor = 0;
    for (std::size_t i = 0; i + 1 < sff.sprites.size(); ++i) {
        auto image = sff.decodeSprite(i);
        REQUIRE(image.ok());
        trueColor += image->isRgba() ? 1 : 0;
    }
    CHECK(trueColor > 0);
    CHECK(def.sprites.size() == sff.sprites.size() - 1 - trueColor);
    CHECK(exported->warnings.size() == 1 + trueColor);

    std::set<std::string> owners;
    for (const SprMakerSprite& s : def.sprites) {
        const auto index =
            sff.findSprite(static_cast<std::uint16_t>(s.id.group), static_cast<std::uint16_t>(s.id.number));
        REQUIRE(index.has_value());
        const SffSprite& original = sff.sprites[*index];
        INFO("sprite " << s.id.group << "," << s.id.number);
        CHECK(s.axisX == original.axisX);
        CHECK(s.axisY == original.axisY);
        CHECK(s.options.decompressOnLoad == ((original.flags & 1) != 0));
        CHECK(s.options.compress5 == SprMakerCompression::Lz5);
        CHECK(s.options.compress8 == SprMakerCompression::Rle8);
        const SffPalette& palette = sff.palettes[static_cast<std::size_t>(original.paletteIndex)];
        CHECK(s.options.usePalette == SprMakerId{palette.group, palette.number});
        CHECK(s.options.removeColors.has_value() == (palette.colors.size() == 32));
        REQUIRE(files.count(s.file) == 1);
        auto png = decodePng(*files.at(s.file));
        REQUIRE(png.ok());
        auto pixels = sff.decodeSprite(*index);
        REQUIRE(pixels.ok());
        CHECK(png->pixelBuffer() == pixels->pixelBuffer());
        const std::size_t root = sff.resolveLink(*index);
        if (root != *index)
            CHECK(s.file ==
                  std::to_string(sff.sprites[root].group) + "_" + std::to_string(sff.sprites[root].number) + ".png");
        owners.insert(s.file);
    }
    CHECK(exported->files.size() == owners.size() + 3);

    SprMakerExportOptions options;
    options.sprites = {2, 1};
    options.outputFile = "sub.sff";
    options.compress5 = SprMakerCompression::Rle5;
    auto subset = exportSprMaker(sff, options);
    REQUIRE(subset.ok());
    REQUIRE(subset->def.sprites.size() == 2);
    CHECK(subset->def.sprites[0].id.number == sff.sprites[2].number);
    CHECK(subset->def.sprites[0].options.compress5 == SprMakerCompression::Rle5);
    auto text = subset->def.write();
    REQUIRE(text.ok());
    CHECK(SprMakerDef::parse(*text).value() == subset->def);
    options.sprites = {999};
    CHECK_FALSE(exportSprMaker(sff, options).ok());
}

TEST_CASE("repeated palette ids get a free number on export", "[maker][sprmaker]")
{
    SffDocument sff(SffVersion::V2);
    for (int i = 0; i < 3; ++i) {
        SffPalette p;
        p.group = 1;
        p.number = 1;
        p.colors = grayscalePalette(i == 2 ? 16 : 256);
        sff.palettes.push_back(p);
    }
    sff.palettes[1].number = 2;
    sff.palettes[2].number = 2;
    SffSprite s;
    s.paletteIndex = 2;
    sff.sprites.push_back(s);
    sff.setSpriteImage(0, Image::indexed(2, 1, {0, 15}));
    auto exported = exportSprMaker(sff);
    REQUIRE(exported.ok());
    REQUIRE(exported->def.palettes.size() == 3);
    CHECK(exported->def.palettes[2].id == SprMakerId{1, 3});
    CHECK(exported->def.palettes[2].lastColor == 31); // 16 colors padded to 32
    CHECK(exported->warnings.size() == 2);
    REQUIRE(exported->def.sprites.size() == 1);
    CHECK(exported->def.sprites[0].options.usePalette == SprMakerId{1, 3});
    CHECK(exported->def.sprites[0].options.removeColors == SprMakerColorRange{32, 255});
}

TEST_CASE("SndMaker lists are read like SndMaker reads them", "[maker][sndmaker]")
{
    auto script = SndMakerScript::parse("chars/kfm/kfm.snd\r\nwork/a b.wav\r\n 5 \r\n0\r\nc.wav\r\n-1\r\n7\r\n\r\n"
                                        "ignored.wav\r\n1\r\n1\r\n");
    REQUIRE(script.ok());
    CHECK(script->outputFile == "chars/kfm/kfm.snd");
    REQUIRE(script->sounds.size() == 2);
    CHECK(script->sounds[0] == SndMakerSound{"work/a b.wav", 5, 0});
    CHECK(script->sounds[1] == SndMakerSound{"c.wav", -1, 7});

    auto spaced = SndMakerScript::parse("o.snd\n  x.wav \n1\n2");
    REQUIRE(spaced.ok());
    CHECK(spaced->sounds[0].file == "  x.wav ");

    auto text = script->write();
    REQUIRE(text.ok());
    CHECK(*text == "chars/kfm/kfm.snd\r\nwork/a b.wav\r\n5\r\n0\r\nc.wav\r\n-1\r\n7\r\n\r\n");
    CHECK(SndMakerScript::parse(*text).value() == *script);

    CHECK_FALSE(SndMakerScript::parse("").ok());
    CHECK_FALSE(SndMakerScript::parse("\na.wav\n1\n1\n").ok());
    CHECK_FALSE(SndMakerScript::parse("o.snd\na.wav\n1\n").ok());
    CHECK_FALSE(SndMakerScript::parse("o.snd\na.wav\none\n1\n").ok());
    CHECK(SndMakerScript::parse("o.snd\n").value().sounds.empty());

    SndMakerScript bad;
    bad.outputFile = "o.snd";
    bad.sounds.push_back({"", 0, 0});
    CHECK_FALSE(bad.write().ok());
    bad.sounds[0].file = "a\nb.wav";
    CHECK_FALSE(bad.write().ok());
}

TEST_CASE("an SND exports to a SndMaker list", "[maker][sndmaker]")
{
    FixtureFactory f(0x5ED);
    SndDocument snd = f.snd(6);
    snd.sounds[4].group = snd.sounds[1].group;
    snd.sounds[4].number = snd.sounds[1].number;
    SndMakerExportOptions options;
    options.outputFile = "data/out.snd";
    options.pathPrefix = "work/sounds/";
    auto exported = exportSndMaker(snd, options);
    REQUIRE(exported.ok());
    CHECK(exported->script.outputFile == "data/out.snd");
    REQUIRE(exported->script.sounds.size() == 6);
    REQUIRE(exported->files.size() == 6);
    std::set<std::string> names;
    for (std::size_t i = 0; i < 6; ++i) {
        const SndMakerSound& s = exported->script.sounds[i];
        CHECK(s.group == snd.sounds[i].group);
        CHECK(s.number == snd.sounds[i].number);
        CHECK(s.file == "work/sounds/" + exported->files[i].name);
        CHECK(exported->files[i].data == snd.sounds[i].data);
        names.insert(exported->files[i].name);
    }
    CHECK(names.size() == 6);
    CHECK(exported->files[4].name.find("_2.wav") != std::string::npos);

    options.sounds = {5, 0};
    auto subset = exportSndMaker(snd, options);
    REQUIRE(subset.ok());
    REQUIRE(subset->files.size() == 2);
    CHECK(subset->files[0].data == snd.sounds[5].data);
    options.sounds = {6};
    CHECK_FALSE(exportSndMaker(snd, options).ok());
}
