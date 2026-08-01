#include "formats/maker/SprMakerDef.h"

#include "formats/codecs/Png.h"
#include "formats/maker/MakerText.h"
#include "formats/palette/PaletteFormats.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace OFS::Formats {

using namespace Maker;

namespace {

enum class Section
{
    None,
    Output,
    Option,
    Pal,
    Sprite,
};

Error lineError(std::size_t line, const std::string& message)
{
    return Error{"line " + std::to_string(line) + ": " + message};
}

bool parseCompression(std::string_view value, SprMakerCompression& out)
{
    const std::string v = lower(value);
    if (v == "none")
        out = SprMakerCompression::None;
    else if (v == "rle8")
        out = SprMakerCompression::Rle8;
    else if (v == "rle5")
        out = SprMakerCompression::Rle5;
    else if (v == "lz5")
        out = SprMakerCompression::Lz5;
    else
        return false;
    return true;
}

bool parseBool(std::string_view value, bool& out)
{
    std::int32_t number = 0;
    if (!parseInt(value, number))
        return false;
    out = number != 0;
    return true;
}

bool parsePair(std::string_view value, std::int32_t& first, std::int32_t& second)
{
    const std::vector<std::string_view> parts = split(value, ',');
    return parts.size() == 2 && parseInt(parts[0], first) && parseInt(parts[1], second);
}

bool parsePaletteRef(std::string_view value, std::optional<SprMakerId>& out)
{
    std::int32_t single = 0;
    if (parseInt(value, single)) {
        if (single != -1)
            return false;
        out.reset();
        return true;
    }
    SprMakerId id;
    if (!parsePair(value, id.group, id.number))
        return false;
    out = id;
    return true;
}

bool parseColorRange(std::string_view value, std::optional<SprMakerColorRange>& out)
{
    std::int32_t single = 0;
    if (parseInt(value, single)) {
        if (single != -1)
            return false;
        out.reset();
        return true;
    }
    SprMakerColorRange range;
    if (!parsePair(value, range.first, range.last) || range.first < 0 || range.last > 255 || range.first > range.last)
        return false;
    out = range;
    return true;
}

std::optional<Error> applyOption(const std::string& key, std::string_view value, SprMakerDef& def,
                                 SprMakerSpriteOptions& options)
{
    bool ok = true;
    SprMakerCompression compression = SprMakerCompression::None;
    if (key == "input.dir") {
        def.inputDir = std::string(value);
    } else if (key == "sprite.compress.5") {
        ok = parseCompression(value, compression) && compression != SprMakerCompression::Rle8;
        options.compress5 = compression;
    } else if (key == "sprite.compress.8") {
        ok = parseCompression(value, compression) &&
             (compression == SprMakerCompression::None || compression == SprMakerCompression::Rle8);
        options.compress8 = compression;
    } else if (key == "sprite.compress.24") {
        ok = parseCompression(value, compression) && compression == SprMakerCompression::None;
        options.compress24 = compression;
    } else if (key == "sprite.decompressonload") {
        ok = parseBool(value, options.decompressOnLoad);
    } else if (key == "sprite.detectduplicates") {
        ok = parseBool(value, options.detectDuplicates);
    } else if (key == "sprite.autocrop") {
        ok = parseBool(value, options.autoCrop);
    } else if (key == "sprite.usepal") {
        ok = parsePaletteRef(value, options.usePalette);
    } else if (key == "sprite.removecolors") {
        ok = parseColorRange(value, options.removeColors);
    } else if (key == "pal.detectduplicates") {
        ok = parseBool(value, def.palDetectDuplicates);
    } else if (key == "pal.discardduplicates") {
        ok = parseBool(value, def.palDiscardDuplicates);
    } else if (key == "pal.reverseact") {
        ok = parseBool(value, def.palReverseAct);
    } else if (key == "pal.reversepng") {
        ok = parseBool(value, def.palReversePng);
    } else {
        return Error{"unknown option '" + key + "'"};
    }
    if (!ok)
        return Error{"invalid value '" + std::string(value) + "' for " + key};
    return std::nullopt;
}

bool keyValue(std::string_view text, std::string& key, std::string_view& value)
{
    const std::size_t eq = text.find('=');
    if (eq == std::string_view::npos)
        return false;
    key = lower(trim(text.substr(0, eq)));
    value = trim(text.substr(eq + 1));
    return true;
}

bool validFileName(const std::string& name)
{
    return !name.empty() && name.find_first_of(",;?\r\n") == std::string::npos && trim(name) == name;
}

std::string idText(const SprMakerId& id)
{
    return std::to_string(id.group) + "," + std::to_string(id.number);
}

void addLine(std::string& out, const std::string& line)
{
    out += line;
    out += "\r\n";
}

std::string flag(bool value)
{
    return value ? "1" : "0";
}

// Writes the switches that differ from `previous`, or all of them when it is null.
void appendSpriteSwitches(std::string& out, const SprMakerSpriteOptions& o, const SprMakerSpriteOptions* previous)
{
    if (!previous || previous->compress5 != o.compress5)
        addLine(out, "sprite.compress.5 = " + std::string(sprMakerCompressionName(o.compress5)));
    if (!previous || previous->compress8 != o.compress8)
        addLine(out, "sprite.compress.8 = " + std::string(sprMakerCompressionName(o.compress8)));
    if (!previous || previous->compress24 != o.compress24)
        addLine(out, "sprite.compress.24 = " + std::string(sprMakerCompressionName(o.compress24)));
    if (!previous || previous->decompressOnLoad != o.decompressOnLoad)
        addLine(out, "sprite.decompressonload = " + flag(o.decompressOnLoad));
    if (!previous || previous->detectDuplicates != o.detectDuplicates)
        addLine(out, "sprite.detectduplicates = " + flag(o.detectDuplicates));
    if (!previous || previous->autoCrop != o.autoCrop)
        addLine(out, "sprite.autocrop = " + flag(o.autoCrop));
}

void appendPaletteSwitches(std::string& out, const SprMakerSpriteOptions& o, const SprMakerSpriteOptions* previous)
{
    if (!previous || previous->usePalette != o.usePalette)
        addLine(out, "sprite.usepal = " + (o.usePalette ? idText(*o.usePalette) : std::string("-1")));
    if (!previous || previous->removeColors != o.removeColors) {
        const std::string range =
            o.removeColors ? std::to_string(o.removeColors->first) + "," + std::to_string(o.removeColors->last)
                           : std::string("-1");
        addLine(out, "sprite.removecolors = " + range);
    }
}

} // namespace

std::string_view sprMakerCompressionName(SprMakerCompression compression)
{
    switch (compression) {
    case SprMakerCompression::None:
        return "none";
    case SprMakerCompression::Rle8:
        return "rle8";
    case SprMakerCompression::Rle5:
        return "rle5";
    case SprMakerCompression::Lz5:
        return "lz5";
    }
    return "none";
}

Result<SprMakerDef> SprMakerDef::parse(std::string_view text)
{
    SprMakerDef def;
    SprMakerSpriteOptions current;
    Section section = Section::None;
    const std::vector<std::string_view> all = lines(text);
    for (std::size_t index = 0; index < all.size(); ++index) {
        const std::size_t lineNo = index + 1;
        std::string_view line = all[index];
        if (const std::size_t comment = line.find(';'); comment != std::string_view::npos)
            line = line.substr(0, comment);
        line = trim(line);
        if (line.empty())
            continue;

        if (line.front() == '[') {
            const std::size_t close = line.find(']');
            if (close == std::string_view::npos)
                return lineError(lineNo, "malformed section header");
            const std::string name = lower(trim(line.substr(1, close - 1)));
            if (name == "output")
                section = Section::Output;
            else if (name == "option")
                section = Section::Option;
            else if (name == "pal")
                section = Section::Pal;
            else if (name == "sprite")
                section = Section::Sprite;
            else
                return lineError(lineNo, "unknown section [" + std::string(trim(line.substr(1, close - 1))) + "]");
            continue;
        }

        switch (section) {
        case Section::None:
            return lineError(lineNo, "text outside of a section");
        case Section::Output:
        case Section::Option: {
            std::string key;
            std::string_view value;
            if (!keyValue(line, key, value))
                return lineError(lineNo, "expected 'key = value'");
            if (section == Section::Output) {
                if (key != "filename")
                    return lineError(lineNo, "unknown [Output] key '" + key + "'");
                def.outputFile = std::string(value);
            } else if (auto error = applyOption(key, value, def, current)) {
                return lineError(lineNo, error->message);
            }
            break;
        }
        case Section::Pal: {
            const std::vector<std::string_view> fields = split(line, ',');
            SprMakerPalette palette;
            if ((fields.size() != 3 && fields.size() != 5) || !parseInt(fields[0], palette.id.group) ||
                !parseInt(fields[1], palette.id.number))
                return lineError(lineNo, "expected 'group, number, file, first color, last color'");
            palette.file = std::string(trim(fields[2]));
            if (fields.size() == 5) {
                std::int32_t first = 0;
                std::int32_t last = 0;
                if (!parseInt(fields[3], first) || !parseInt(fields[4], last) || first < 0 || last > 255 ||
                    first > last)
                    return lineError(lineNo, "invalid palette color range");
                palette.firstColor = first;
                palette.lastColor = last;
            }
            if (palette.file.empty())
                return lineError(lineNo, "palette file name is missing");
            def.palettes.push_back(std::move(palette));
            break;
        }
        case Section::Sprite: {
            const std::vector<std::string_view> parts = split(line, '?');
            const std::vector<std::string_view> fields = split(parts[0], ',');
            SprMakerSprite sprite;
            if ((fields.size() != 3 && fields.size() != 5) || !parseInt(fields[0], sprite.id.group) ||
                !parseInt(fields[1], sprite.id.number) ||
                (fields.size() == 5 && (!parseInt(fields[3], sprite.axisX) || !parseInt(fields[4], sprite.axisY))))
                return lineError(lineNo, "expected 'group, number, file, axis x, axis y'");
            sprite.file = std::string(trim(fields[2]));
            if (sprite.file.empty())
                return lineError(lineNo, "sprite file name is missing");
            sprite.options = current;
            for (std::size_t k = 1; k < parts.size(); ++k) {
                std::string key;
                std::string_view value;
                if (!keyValue(parts[k], key, value))
                    return lineError(lineNo, "expected '? key = value'");
                if (key == "usepal") {
                    if (!parsePaletteRef(value, sprite.options.usePalette))
                        return lineError(lineNo, "invalid value '" + std::string(value) + "' for usepal");
                } else if (key == "crop") {
                    const std::vector<std::string_view> c = split(value, ',');
                    SprMakerCrop crop;
                    if (c.size() != 4 || !parseInt(c[0], crop.x1) || !parseInt(c[1], crop.y1) ||
                        !parseInt(c[2], crop.x2) || !parseInt(c[3], crop.y2))
                        return lineError(lineNo, "invalid value '" + std::string(value) + "' for crop");
                    sprite.crop = crop;
                } else {
                    return lineError(lineNo, "unknown sprite option '" + key + "'");
                }
            }
            def.sprites.push_back(std::move(sprite));
            break;
        }
        }
    }
    if (def.outputFile.empty())
        return Error{"the [Output] filename is missing"};
    return def;
}

Result<std::string> SprMakerDef::write() const
{
    if (outputFile.empty() || outputFile.find_first_of(";\r\n") != std::string::npos)
        return Error{"invalid output file name '" + outputFile + "'"};
    if (inputDir.find_first_of(";\r\n") != std::string::npos)
        return Error{"invalid input folder '" + inputDir + "'"};
    for (const SprMakerPalette& p : palettes) {
        if (!validFileName(p.file))
            return Error{"invalid palette file name '" + p.file + "'"};
        if (p.firstColor < 0 || p.lastColor > 255 || p.firstColor > p.lastColor)
            return Error{"invalid color range for palette " + idText(p.id)};
    }
    for (const SprMakerSprite& s : sprites) {
        if (!validFileName(s.file))
            return Error{"invalid sprite file name '" + s.file + "'"};
        if (const auto& r = s.options.removeColors; r && (r->first < 0 || r->last > 255 || r->first > r->last))
            return Error{"invalid color range for sprite " + idText(s.id)};
    }

    std::string out;
    auto add = [&out](const std::string& line) {
        addLine(out, line);
    };
    const SprMakerSpriteOptions first = sprites.empty() ? SprMakerSpriteOptions{} : sprites.front().options;
    add("[Output]");
    add("filename = " + outputFile);
    add("");
    add("[Option]");
    if (!inputDir.empty())
        add("input.dir = " + inputDir);
    appendSpriteSwitches(out, first, nullptr);
    add("pal.detectduplicates = " + flag(palDetectDuplicates));
    add("pal.discardduplicates = " + flag(palDiscardDuplicates));
    add("pal.reverseact = " + flag(palReverseAct));
    add("pal.reversepng = " + flag(palReversePng));
    add("");
    if (!palettes.empty()) {
        add("[Pal]");
        for (const SprMakerPalette& p : palettes)
            add(idText(p.id) + ", " + p.file + ", " + std::to_string(p.firstColor) + "," + std::to_string(p.lastColor));
        add("");
    }

    // The palette switches go after [Pal], right before the sprites that use them.
    add("[Option]");
    appendPaletteSwitches(out, first, nullptr);
    add("");
    add("[Sprite]");
    const SprMakerSpriteOptions* previous = &first;
    for (const SprMakerSprite& s : sprites) {
        if (s.options != *previous) {
            add("");
            add("[Option]");
            appendSpriteSwitches(out, s.options, previous);
            appendPaletteSwitches(out, s.options, previous);
            add("");
            add("[Sprite]");
            previous = &s.options;
        }
        std::string line = std::to_string(s.id.group) + ", " + std::to_string(s.id.number) + ", " + s.file + ", " +
                           std::to_string(s.axisX) + ", " + std::to_string(s.axisY);
        if (s.crop)
            line += "   ? crop = " + std::to_string(s.crop->x1) + "," + std::to_string(s.crop->y1) + "," +
                    std::to_string(s.crop->x2) + "," + std::to_string(s.crop->y2);
        add(line);
    }
    return out;
}

Result<SprMakerExport> exportSprMaker(const SffDocument& sff, const SprMakerExportOptions& options)
{
    SprMakerExport result;
    SprMakerDef& def = result.def;
    def.outputFile = options.outputFile;

    std::set<std::pair<std::int32_t, std::int32_t>> usedIds;
    std::vector<SprMakerId> paletteIds(sff.palettes.size());
    std::vector<Palette> padded(sff.palettes.size());
    std::vector<std::string> paletteFiles(sff.palettes.size());
    for (std::size_t i = 0; i < sff.palettes.size(); ++i) {
        const SffPalette& p = sff.palettes[i];
        SprMakerId id{p.group, p.number};
        if (!usedIds.insert({id.group, id.number}).second) {
            const SprMakerId original = id;
            while (id.number < 0xFFFF && usedIds.count({id.group, id.number}))
                ++id.number;
            if (usedIds.count({id.group, id.number}))
                return Error{"no free palette number in group " + std::to_string(id.group)};
            usedIds.insert({id.group, id.number});
            result.warnings.push_back("palette " + idText(original) + " appears more than once; exported as " +
                                      idText(id));
        }
        paletteIds[i] = id;
        const std::size_t size = p.colors.size() <= 32 ? 32 : 256;
        if (p.colors.size() != 32 && p.colors.size() != 256)
            result.warnings.push_back("palette " + idText(id) + " has " + std::to_string(p.colors.size()) +
                                      " colors; SprMaker2 makes 32 and 256-color palettes, so it is " +
                                      (p.colors.size() > 256 ? "cut" : "padded") + " to " + std::to_string(size));
        padded[i] = p.colors;
        padded[i].resize(size, rgb(0, 0, 0));

        const bool linked = p.linkedIndex >= 0 && static_cast<std::size_t>(p.linkedIndex) < i &&
                            sameRgb(padded[static_cast<std::size_t>(p.linkedIndex)], padded[i]);
        if (linked) {
            paletteFiles[i] = paletteFiles[static_cast<std::size_t>(p.linkedIndex)];
        } else {
            paletteFiles[i] = "pal_" + std::to_string(id.group) + "_" + std::to_string(id.number) + ".act";
            result.files.push_back({paletteFiles[i], writeAct(padded[i], ActOrder::Reversed)});
        }
        def.palettes.push_back({id, paletteFiles[i], 0, static_cast<int>(size) - 1});
    }

    std::vector<std::size_t> order = options.sprites;
    if (order.empty()) {
        order.resize(sff.sprites.size());
        for (std::size_t i = 0; i < order.size(); ++i)
            order[i] = i;
    }
    std::set<std::pair<std::int32_t, std::int32_t>> spriteIds;
    std::map<std::size_t, std::string> imageFiles;
    for (const std::size_t index : order) {
        if (index >= sff.sprites.size())
            return Error{"sprite index " + std::to_string(index) + " is out of range"};
        const SffSprite& s = sff.sprites[index];
        const SprMakerId id{s.group, s.number};
        const std::string name = "sprite " + idText(id);
        if (!spriteIds.insert({id.group, id.number}).second) {
            result.warnings.push_back(name + " appears more than once; only the first one is exported");
            continue;
        }
        if (s.paletteIndex < 0 || static_cast<std::size_t>(s.paletteIndex) >= sff.palettes.size()) {
            result.warnings.push_back(name + " has no palette and is left out");
            continue;
        }
        const auto paletteIndex = static_cast<std::size_t>(s.paletteIndex);
        auto image = sff.decodeSprite(index, DecodeMode::Lenient);
        if (!image)
            return withContext(name, image.error());
        if (!image->isIndexed()) {
            result.warnings.push_back(name + " is a true-color sprite, which SprMaker2 cannot store; left out");
            continue;
        }
        const bool fiveBit = padded[paletteIndex].size() == 32;
        if (fiveBit && image->maxIndex() >= 32) {
            result.warnings.push_back(name + " uses colors outside its 32-color palette and is left out");
            continue;
        }

        const std::size_t owner = sff.resolveLink(index);
        auto file = imageFiles.find(owner);
        if (file == imageFiles.end()) {
            image->setPalette(padded[paletteIndex]);
            auto png = encodePng(*image);
            if (!png)
                return withContext(name, png.error());
            const std::string fileName = std::to_string(s.group) + "_" + std::to_string(s.number) + ".png";
            result.files.push_back({fileName, std::move(*png)});
            file = imageFiles.emplace(owner, fileName).first;
        }

        SprMakerSprite sprite;
        sprite.id = id;
        sprite.file = file->second;
        sprite.axisX = s.axisX;
        sprite.axisY = s.axisY;
        sprite.options.compress5 = options.compress5;
        sprite.options.compress8 = options.compress8;
        sprite.options.decompressOnLoad = (s.flags & 1) != 0;
        sprite.options.usePalette = paletteIds[paletteIndex];
        if (fiveBit)
            sprite.options.removeColors = SprMakerColorRange{32, 255};
        def.sprites.push_back(std::move(sprite));
    }
    return result;
}

} // namespace OFS::Formats
