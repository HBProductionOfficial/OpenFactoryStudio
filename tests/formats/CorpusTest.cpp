#include "formats/TestEnv.h"
#include "formats/fnt/FntDocument.h"
#include "formats/sff/SffDocument.h"
#include "formats/snd/SndDocument.h"
#include "formats/util/FileIo.h"
#include "formats/wav/WavFile.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace OFS::Formats;
using namespace OFS::Formats::Test;

namespace {

std::string lowerExtension(const std::filesystem::path& path)
{
    std::string ext = path.extension().string();
    for (char& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

std::vector<std::filesystem::path> filesWithExtension(const std::filesystem::path& root, const std::string& ext)
{
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (auto it = std::filesystem::recursive_directory_iterator(
             root, std::filesystem::directory_options::skip_permission_denied, ec);
         !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        if (it->is_regular_file(ec) && lowerExtension(it->path()) == ext)
            files.push_back(it->path());
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::string display(const std::filesystem::path& path)
{
    std::string text;
    for (const char8_t c : path.u8string())
        text.push_back(static_cast<char>(c));
    return text;
}

struct SffStats
{
    std::size_t files = 0;
    std::size_t readErrors = 0;
    std::size_t sprites = 0;
    std::size_t linked = 0;
    std::size_t palettes = 0;
    std::size_t decodeErrors = 0;
    std::size_t lenientOnly = 0;
    std::size_t resaveIdentical = 0;
    std::map<std::string, std::size_t> versions;
    std::map<std::string, std::size_t> formats;
    std::vector<std::string> problems;
};

void checkSff(const std::filesystem::path& path, SffStats& st)
{
    ++st.files;
    auto bytes = readFile(path);
    if (!bytes) {
        ++st.readErrors;
        st.problems.push_back(display(path) + ": " + bytes.error().message);
        return;
    }
    auto doc = SffDocument::read(*bytes);
    if (!doc) {
        ++st.readErrors;
        st.problems.push_back(display(path) + ": " + doc.error().message);
        return;
    }
    ++st.versions[sffVersionString(doc->versionBytes)];
    st.palettes += doc->palettes.size();
    for (std::size_t i = 0; i < doc->sprites.size(); ++i) {
        const SffSprite& s = doc->sprites[i];
        ++st.sprites;
        if (s.isLinked()) {
            ++st.linked;
        } else {
            ++st.formats[std::string(sffCompressionName(s.compression))];
        }
        if (doc->decodeSprite(i, DecodeMode::Strict))
            continue;
        if (doc->decodeSprite(i, DecodeMode::Lenient)) {
            ++st.lenientOnly;
        } else {
            ++st.decodeErrors;
            st.problems.push_back(display(path) + " sprite " + std::to_string(i) + ": " +
                                  doc->decodeSprite(i, DecodeMode::Lenient).error().message);
        }
    }
    auto again = doc->write();
    if (again && *again == *bytes)
        ++st.resaveIdentical;
    else
        st.problems.push_back(display(path) + ": re-save differs from the original");
}

std::string summary(const SffStats& st)
{
    std::ostringstream out;
    out << st.files << " SFF files, " << st.sprites << " sprites (" << st.linked << " linked), " << st.palettes
        << " palettes; read errors " << st.readErrors << ", sprite decode errors " << st.decodeErrors
        << ", damaged sprites decoded leniently " << st.lenientOnly << ", byte-identical re-saves "
        << st.resaveIdentical << "/" << st.files << "\n  versions:";
    for (const auto& [v, n] : st.versions)
        out << ' ' << v << '=' << n;
    out << "\n  formats:";
    for (const auto& [f, n] : st.formats)
        out << ' ' << f << '=' << n;
    for (std::size_t i = 0; i < st.problems.size() && i < 20; ++i)
        out << "\n  " << st.problems[i];
    return out.str();
}

} // namespace

TEST_CASE("KFM: every sprite and palette decodes and an untouched re-save is identical", "[corpus][kfm]")
{
    const auto dir = corpusDir("OFS_CORPUS_KFM");
    if (!dir)
        SKIP("OFS_CORPUS_KFM is not set");
    const std::filesystem::path file = *dir / "kfm.sff";
    REQUIRE(std::filesystem::exists(file));

    auto bytes = readFile(file);
    REQUIRE(bytes.ok());
    auto doc = SffDocument::read(*bytes);
    REQUIRE(doc.ok());
    INFO("version " << sffVersionString(doc->versionBytes));
    CHECK(doc->version != SffVersion::V1);

    std::size_t decoded = 0;
    std::map<std::string, std::size_t> formats;
    for (std::size_t i = 0; i < doc->sprites.size(); ++i) {
        INFO("sprite " << i << " (" << doc->sprites[i].group << "," << doc->sprites[i].number << ")");
        auto image = doc->decodeSprite(i, DecodeMode::Strict);
        REQUIRE(image.ok());
        CHECK(image->width() == doc->sprites[doc->resolveLink(i)].width);
        CHECK(image->height() == doc->sprites[doc->resolveLink(i)].height);
        if (image->isIndexed()) {
            REQUIRE(doc->spritePalette(i) != nullptr);
            CHECK(image->maxIndex() < doc->spritePalette(i)->size());
        }
        ++formats[doc->sprites[i].isLinked() ? std::string("linked")
                                             : std::string(sffCompressionName(doc->sprites[i].compression))];
        ++decoded;
    }
    for (const SffPalette& p : doc->palettes)
        CHECK_FALSE(p.colors.empty());

    auto again = doc->write();
    REQUIRE(again.ok());
    CHECK(again->size() == bytes->size());
    CHECK(*again == *bytes);

    SffDocument converted = *doc;
    const bool hasTrueColor = std::any_of(doc->sprites.begin(), doc->sprites.end(), [](const SffSprite& s) {
        return !s.isLinked() && !sffIsIndexed(s.compression);
    });
    if (!hasTrueColor) {
        REQUIRE(converted.convertTo(SffVersion::V1).ok());
        auto v1 = converted.write();
        REQUIRE(v1.ok());
        auto v1doc = SffDocument::read(*v1);
        REQUIRE(v1doc.ok());
        for (std::size_t i = 0; i < doc->sprites.size(); ++i)
            REQUIRE(*v1doc->decodeSpriteRgba(i) == *doc->decodeSpriteRgba(i));
    }

    std::ostringstream report;
    report << "KFM " << sffVersionString(doc->versionBytes) << ": " << decoded << " sprites decoded, "
           << doc->palettes.size() << " palettes, re-save " << (*again == *bytes ? "byte-identical" : "DIFFERENT")
           << "; formats:";
    for (const auto& [f, n] : formats)
        report << ' ' << f << '=' << n;
    std::cout << report.str() << std::endl;
    WARN(report.str());
}

TEST_CASE("KFM: sounds decode and an untouched re-save is identical", "[corpus][kfm]")
{
    const auto dir = corpusDir("OFS_CORPUS_KFM");
    if (!dir)
        SKIP("OFS_CORPUS_KFM is not set");
    auto bytes = readFile(*dir / "kfm.snd");
    REQUIRE(bytes.ok());
    auto doc = SndDocument::read(*bytes);
    REQUIRE(doc.ok());
    CHECK_FALSE(doc->sounds.empty());
    for (const SndSound& s : doc->sounds) {
        auto wav = WavFile::parse(s.data);
        REQUIRE(wav.ok());
        CHECK(wav->isSupported());
        CHECK(wav->frameCount() > 0);
    }
    CHECK(doc->write() == *bytes);
    std::cout << "KFM SND: " << doc->sounds.size() << " sounds, re-save byte-identical" << std::endl;
}

TEST_CASE("MUGEN: every SFF file reads, decodes and re-saves identically", "[corpus][mugen]")
{
    const auto dir = corpusDir("OFS_CORPUS_MUGEN");
    if (!dir)
        SKIP("OFS_CORPUS_MUGEN is not set");
    SffStats st;
    for (const auto& file : filesWithExtension(*dir, ".sff"))
        checkSff(file, st);
    const std::string text = summary(st);
    std::cout << text << std::endl;
    WARN(text);
    CHECK(st.files > 0);
    CHECK(st.readErrors == 0);
    CHECK(st.decodeErrors == 0);
    CHECK(st.resaveIdentical == st.files);
}

TEST_CASE("MUGEN: every SND file reads and re-saves identically", "[corpus][mugen]")
{
    const auto dir = corpusDir("OFS_CORPUS_MUGEN");
    if (!dir)
        SKIP("OFS_CORPUS_MUGEN is not set");
    std::size_t files = 0;
    std::size_t sounds = 0;
    std::size_t wavOk = 0;
    std::size_t wavUnsupported = 0;
    std::size_t identical = 0;
    std::vector<std::string> problems;
    for (const auto& file : filesWithExtension(*dir, ".snd")) {
        ++files;
        auto bytes = readFile(file);
        REQUIRE(bytes.ok());
        auto doc = SndDocument::read(*bytes);
        if (!doc) {
            problems.push_back(display(file) + ": " + doc.error().message);
            continue;
        }
        for (const SndSound& s : doc->sounds) {
            ++sounds;
            auto wav = WavFile::parse(s.data);
            if (wav) {
                ++wavOk;
                if (!wav->isSupported())
                    ++wavUnsupported;
            } else {
                problems.push_back(display(file) + " sound " + std::to_string(s.group) + "," +
                                   std::to_string(s.number) + ": " + wav.error().message);
            }
        }
        if (doc->write() == *bytes)
            ++identical;
        else
            problems.push_back(display(file) + ": re-save differs");
    }
    std::ostringstream report;
    report << files << " SND files, " << sounds << " sounds; WAV parsed " << wavOk << " (unsupported sample format "
           << wavUnsupported << "), byte-identical re-saves " << identical << "/" << files;
    for (std::size_t i = 0; i < problems.size() && i < 20; ++i)
        report << "\n  " << problems[i];
    std::cout << report.str() << std::endl;
    WARN(report.str());
    CHECK(files > 0);
    CHECK(identical == files);
    CHECK(wavOk == sounds);
}

TEST_CASE("MUGEN: every FNT file reads and re-saves identically", "[corpus][mugen]")
{
    const auto dir = corpusDir("OFS_CORPUS_MUGEN");
    if (!dir)
        SKIP("OFS_CORPUS_MUGEN is not set");
    std::size_t files = 0;
    std::size_t identical = 0;
    std::size_t images = 0;
    std::size_t glyphs = 0;
    std::vector<std::string> problems;
    for (const auto& file : filesWithExtension(*dir, ".fnt")) {
        ++files;
        auto bytes = readFile(file);
        REQUIRE(bytes.ok());
        auto doc = FntDocument::read(*bytes);
        if (!doc) {
            problems.push_back(display(file) + ": " + doc.error().message);
            continue;
        }
        if (doc->image())
            ++images;
        else
            problems.push_back(display(file) + ": image does not decode");
        glyphs += doc->info().glyphs.size();
        if (doc->write() == *bytes)
            ++identical;
        else
            problems.push_back(display(file) + ": re-save differs");
    }
    std::ostringstream report;
    report << files << " FNT files, images decoded " << images << ", glyphs mapped " << glyphs
           << ", byte-identical re-saves " << identical << "/" << files;
    for (std::size_t i = 0; i < problems.size() && i < 20; ++i)
        report << "\n  " << problems[i];
    std::cout << report.str() << std::endl;
    WARN(report.str());
    CHECK(files > 0);
    CHECK(identical == files);
    CHECK(images == files);
}
