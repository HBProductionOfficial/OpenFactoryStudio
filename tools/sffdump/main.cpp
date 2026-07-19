// ofs-sffdump: inspects and converts SFF sprite files and SND sound files.
//
//   ofs-sffdump info <file.sff>
//   ofs-sffdump list <file.sff> [--json]
//   ofs-sffdump extract <file.sff> <outdir>
//   ofs-sffdump convert <in.sff> <out.sff> --to v1|v2.0|v2.01 [--smallest] [--link-duplicates]
//   ofs-sffdump snd-list <file.snd>

#include "formats/codecs/Png.h"
#include "formats/sff/SffDocument.h"
#include "formats/snd/SndDocument.h"
#include "formats/util/FileIo.h"
#include "formats/wav/WavFile.h"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace OFS::Formats;

namespace {

constexpr int kOk = 0;
constexpr int kFailed = 1;
constexpr int kUsage = 2;

int usage()
{
    std::cerr << "usage:\n"
                 "  ofs-sffdump info <file.sff>\n"
                 "  ofs-sffdump list <file.sff> [--json]\n"
                 "  ofs-sffdump extract <file.sff> <outdir>\n"
                 "  ofs-sffdump convert <in.sff> <out.sff> --to v1|v2.0|v2.01 [--smallest] [--link-duplicates]\n"
                 "  ofs-sffdump snd-list <file.snd>\n";
    return kUsage;
}

int fail(const std::string& message)
{
    std::cerr << "ofs-sffdump: " << message << '\n';
    return kFailed;
}

Result<SffDocument> loadSff(const std::string& file)
{
    auto doc = SffDocument::load(std::filesystem::path(file));
    if (!doc)
        return withContext(file, doc.error());
    return doc;
}

std::string paletteLabel(const SffDocument& doc, std::size_t index)
{
    const SffSprite& s = doc.sprites[index];
    if (!sffIsIndexed(doc.sprites[doc.resolveLink(index)].compression))
        return "-";
    if (s.paletteIndex < 0 || static_cast<std::size_t>(s.paletteIndex) >= doc.palettes.size())
        return std::to_string(s.paletteIndex) + " (missing)";
    if (doc.version == SffVersion::V1)
        return std::to_string(s.paletteIndex);
    const SffPalette& p = doc.palettes[static_cast<std::size_t>(s.paletteIndex)];
    return std::to_string(s.paletteIndex) + " (" + std::to_string(p.group) + "," + std::to_string(p.number) + ")";
}

// Printable text in the header's free area (comments some tools leave there).
std::string headerComment(const SffDocument& doc)
{
    const std::size_t start = doc.version == SffVersion::V1 ? 36 : 76;
    std::string text;
    for (std::size_t i = start; i < doc.header.size(); ++i) {
        const auto c = static_cast<char>(doc.header[i]);
        if (c == 0)
            continue;
        text += (c >= 32 && c < 127) ? c : ' ';
    }
    return text;
}

int info(const std::string& file)
{
    auto doc = loadSff(file);
    if (!doc)
        return fail(doc.error().message);
    std::map<std::string, std::size_t> formats;
    std::size_t linked = 0;
    std::size_t translated = 0;
    std::set<std::uint16_t> groups;
    for (const SffSprite& s : doc->sprites) {
        groups.insert(s.group);
        if (s.isLinked())
            ++linked;
        else
            ++formats[std::string(sffCompressionName(s.compression))];
        if (s.flags & 1)
            ++translated;
    }
    std::size_t linkedPalettes = 0;
    for (const SffPalette& p : doc->palettes) {
        if (p.linkedIndex >= 0)
            ++linkedPalettes;
    }

    std::cout << "file:      " << file << '\n'
              << "version:   " << sffVersionString(doc->versionBytes) << " (" << sffVersionName(doc->version) << ")\n";
    if (doc->version == SffVersion::V1) {
        std::cout << "palette:   " << (doc->sharedPalette ? "shared" : "individual") << '\n';
    } else {
        std::cout << "compat:    " << sffVersionString(doc->compatVersionBytes) << '\n'
                  << "links:     "
                  << (doc->linkOffsets == SffLinkOffsets::TargetOffset ? "target offsets" : "running offsets") << '\n';
    }
    std::cout << "sprites:   " << doc->sprites.size() << " (" << linked << " linked, " << translated
              << " in the translated block)\n"
              << "groups:    " << groups.size() << '\n'
              << "palettes:  " << doc->palettes.size() << " (" << linkedPalettes << " linked)\n"
              << "formats:  ";
    for (const auto& [name, count] : formats)
        std::cout << ' ' << name << '=' << count;
    std::cout << '\n';
    const std::string comment = headerComment(*doc);
    if (!comment.empty())
        std::cout << "comment:   " << comment << '\n';
    return kOk;
}

int list(const std::string& file, bool json)
{
    auto doc = loadSff(file);
    if (!doc)
        return fail(doc.error().message);
    if (json)
        std::cout << "{\n  \"version\": \"" << sffVersionString(doc->versionBytes) << "\",\n  \"sprites\": [";
    else
        std::cout << "index group number  width height  axisx axisy  format depth palette link\n";
    for (std::size_t i = 0; i < doc->sprites.size(); ++i) {
        const SffSprite& s = doc->sprites[i];
        const SffSprite& owner = doc->sprites[doc->resolveLink(i)];
        const std::string format = s.isLinked() ? "linked" : std::string(sffCompressionName(s.compression));
        if (json) {
            std::cout << (i ? "," : "") << "\n    {\"index\": " << i << ", \"group\": " << s.group
                      << ", \"number\": " << s.number << ", \"width\": " << owner.width
                      << ", \"height\": " << owner.height << ", \"axisX\": " << s.axisX << ", \"axisY\": " << s.axisY
                      << ", \"format\": \"" << format << "\", \"depth\": " << int{owner.colorDepth}
                      << ", \"palette\": " << s.paletteIndex << ", \"link\": " << s.linkedIndex << "}";
        } else {
            char line[160];
            std::snprintf(line, sizeof line, "%5zu %5u %6u  %5u %6u  %5d %5d  %-6s %5d  ", i, unsigned{s.group},
                          unsigned{s.number}, unsigned{owner.width}, unsigned{owner.height}, int{s.axisX}, int{s.axisY},
                          format.c_str(), int{owner.colorDepth});
            std::cout << line << paletteLabel(*doc, i);
            if (s.isLinked())
                std::cout << "  -> " << s.linkedIndex;
            std::cout << '\n';
        }
    }
    if (json) {
        std::cout << "\n  ],\n  \"palettes\": [";
        for (std::size_t i = 0; i < doc->palettes.size(); ++i) {
            const SffPalette& p = doc->palettes[i];
            std::cout << (i ? "," : "") << "\n    {\"index\": " << i << ", \"group\": " << p.group
                      << ", \"number\": " << p.number << ", \"colors\": " << p.colors.size()
                      << ", \"link\": " << p.linkedIndex << "}";
        }
        std::cout << "\n  ]\n}\n";
    }
    return kOk;
}

int extract(const std::string& file, const std::string& outDir)
{
    auto doc = loadSff(file);
    if (!doc)
        return fail(doc.error().message);
    const std::filesystem::path dir(outDir);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec)
        return fail("cannot create " + outDir);

    std::set<std::string> used;
    std::size_t written = 0;
    int status = kOk;
    for (std::size_t i = 0; i < doc->sprites.size(); ++i) {
        const SffSprite& s = doc->sprites[i];
        auto image = doc->decodeSprite(i);
        if (!image) {
            std::cerr << "sprite " << i << ": " << image.error().message << '\n';
            status = kFailed;
            continue;
        }
        PngEncodeOptions options;
        options.transparentIndex0 = true;
        auto png = encodePng(*image, options);
        if (!png) {
            std::cerr << "sprite " << i << ": " << png.error().message << '\n';
            status = kFailed;
            continue;
        }
        std::string name = std::to_string(s.group) + "-" + std::to_string(s.number);
        if (!used.insert(name).second)
            name += "_" + std::to_string(i);
        used.insert(name);
        auto saved = writeFile(dir / (name + ".png"), *png);
        if (!saved)
            return fail(saved.error().message);
        ++written;
    }
    std::cout << written << " sprites written to " << outDir << '\n';
    return status;
}

int convert(const std::vector<std::string>& args)
{
    if (args.size() < 2)
        return usage();
    std::string to;
    SffWriteOptions options;
    for (std::size_t i = 2; i < args.size(); ++i) {
        if (args[i] == "--to" && i + 1 < args.size())
            to = args[++i];
        else if (args[i] == "--smallest")
            options.compression.smallest = true;
        else if (args[i] == "--link-duplicates")
            options.linkDuplicateSprites = options.linkDuplicatePalettes = true;
        else
            return usage();
    }
    SffVersion target = SffVersion::V201;
    if (to == "v1")
        target = SffVersion::V1;
    else if (to == "v2.0" || to == "v2")
        target = SffVersion::V2;
    else if (to != "v2.01")
        return usage();

    auto doc = loadSff(args[0]);
    if (!doc)
        return fail(doc.error().message);
    if (auto converted = doc->convertTo(target); !converted)
        return fail(converted.error().message);
    if (auto saved = doc->save(std::filesystem::path(args[1]), options); !saved)
        return fail(saved.error().message);
    std::cout << args[0] << " -> " << args[1] << " (" << sffVersionName(target) << ", " << doc->sprites.size()
              << " sprites)\n";
    return kOk;
}

int sndList(const std::string& file)
{
    auto doc = SndDocument::load(std::filesystem::path(file));
    if (!doc)
        return fail(file + ": " + doc.error().message);
    std::cout << "index group number    bytes  format\n";
    for (std::size_t i = 0; i < doc->sounds.size(); ++i) {
        const SndSound& s = doc->sounds[i];
        char line[96];
        std::snprintf(line, sizeof line, "%5zu %5d %6d %8zu  ", i, s.group, s.number, s.data.size());
        std::cout << line;
        auto wav = WavFile::parse(s.data);
        if (!wav) {
            std::cout << "not a WAV file (" << wav.error().message << ")\n";
            continue;
        }
        std::ostringstream format;
        format << wav->format.channels << " ch, " << wav->format.sampleRate << " Hz, " << wav->format.bitsPerSample
               << " bit" << (wav->format.sampleFormat() == kWavFormatFloat ? " float" : "");
        if (!wav->isSupported())
            format << " (format tag " << wav->format.formatTag << ")";
        char duration[32];
        std::snprintf(duration, sizeof duration, ", %.3f s", wav->durationSeconds());
        std::cout << format.str() << duration << '\n';
    }
    return kOk;
}

} // namespace

int main(int argc, char* argv[])
{
    const std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty())
        return usage();
    const std::string& command = args[0];
    const std::vector<std::string> rest(args.begin() + 1, args.end());
    if (command == "info" && rest.size() == 1)
        return info(rest[0]);
    if (command == "list" && (rest.size() == 1 || (rest.size() == 2 && rest[1] == "--json")))
        return list(rest[0], rest.size() == 2);
    if (command == "extract" && rest.size() == 2)
        return extract(rest[0], rest[1]);
    if (command == "convert")
        return convert(rest);
    if (command == "snd-list" && rest.size() == 1)
        return sndList(rest[0]);
    return usage();
}
