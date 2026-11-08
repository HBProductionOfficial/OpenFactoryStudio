#include "formats/codecs/Pcx.h"
#include "formats/sff/SffDocument.h"
#include "formats/sff/SffInternal.h"
#include "formats/util/ByteReader.h"
#include "formats/util/ReadLimits.h"

#include <algorithm>
#include <string>
#include <vector>

namespace OFS::Formats {

using namespace SffDetail;

namespace {

std::string spriteName(const SffSprite& s)
{
    return "Sprite " + std::to_string(s.group) + "," + std::to_string(s.number);
}

std::string paletteName(const SffPalette& p)
{
    return "Palette " + std::to_string(p.group) + "," + std::to_string(p.number);
}

bool fits(std::uint64_t offset, std::uint64_t length, std::uint64_t size)
{
    return offset <= size && length <= size - offset;
}

// Where each chain of links ends: the first entry that isn't a link, or -1 for a chain that runs
// in a loop. Each entry is looked at once, so long chains don't make reading slow.
template <typename T>
std::vector<std::int64_t> linkEnds(const std::vector<T>& items)
{
    constexpr std::int64_t kUnknown = -2;
    constexpr std::int64_t kOnPath = -3;
    std::vector<std::int64_t> ends(items.size(), kUnknown);
    std::vector<std::size_t> path;
    for (std::size_t i = 0; i < items.size(); ++i) {
        std::size_t at = i;
        while (ends[at] == kUnknown && items[at].linkedIndex >= 0) {
            ends[at] = kOnPath;
            path.push_back(at);
            at = static_cast<std::size_t>(items[at].linkedIndex);
        }
        if (ends[at] == kUnknown)
            ends[at] = static_cast<std::int64_t>(at);
        const std::int64_t end = ends[at] == kOnPath ? -1 : ends[at];
        for (std::size_t p : path)
            ends[p] = end;
        path.clear();
    }
    return ends;
}

Result<SffDocument> readV1(ByteSpan data, SffDocument doc)
{
    ByteReader r(data);
    r.seek(kV1GroupCountOffset);
    const std::uint32_t groupCount = r.u32();
    const std::uint32_t imageCount = r.u32();
    const std::uint32_t firstOffset = r.u32();
    const std::uint32_t subheaderSize = r.u32();
    doc.sharedPalette = r.u8() != 0;
    doc.raw.fromFile = true;
    doc.raw.v1GroupCount = groupCount;
    doc.raw.v1SubheaderSize = subheaderSize;

    if (imageCount > 0 && firstOffset < kV1SubheaderSize)
        return Error{"SFF v1 first subfile offset is invalid"};

    // A damaged file keeps the sprites before the damage. Sprites that can't be read stay in the
    // list without an image.
    std::size_t readable = imageCount;
    if (imageCount > data.size() / kV1SubheaderSize) {
        readable = data.size() / kV1SubheaderSize;
        addReadProblem(doc.readProblems,
                       "The header lists " + std::to_string(imageCount) + " sprites, more than the file can hold.");
    }
    doc.sprites.reserve(readable);
    std::uint64_t offset = firstOffset;
    std::int32_t lastPalette = -1; // palette of the most recent sprite with its own pixels
    // Subheaders can overlap, so many sprites can point to the same data.
    CopyBudget budget(data.size());

    for (std::size_t i = 0; i < readable; ++i) {
        if (!fits(offset, kV1SubheaderSize, data.size())) {
            addReadProblem(doc.readProblems, "The file ends after sprite " + std::to_string(i) + " of " +
                                                 std::to_string(imageCount) + ".");
            break;
        }
        r.seek(static_cast<std::size_t>(offset));
        const std::uint32_t next = r.u32();
        const std::uint32_t length = r.u32();
        SffSprite s;
        s.axisX = r.s16();
        s.axisY = r.s16();
        s.group = r.u16();
        s.number = r.u16();
        const std::uint16_t link = r.u16();
        s.samePalette = r.u8() != 0;
        for (std::uint8_t& b : s.raw.v1Reserved)
            b = r.u8();
        s.raw.fromFile = true;
        s.raw.v1Length = length;
        s.raw.v1Link = link;
        s.compression = SffCompression::Pcx;
        s.colorDepth = 8;

        const std::uint64_t start = offset + kV1SubheaderSize;
        if (length == 0 && link >= i) {
            addReadProblem(doc.readProblems, spriteName(s) +
                                                 " links to a sprite that doesn't come before it, so it has no "
                                                 "image.");
            s.paletteIndex = lastPalette;
        } else if (length == 0) {
            // Straight to the sprite with the pixels, as a save writes it.
            const SffSprite& target = doc.sprites[link];
            s.linkedIndex = target.isLinked() ? target.linkedIndex : link;
            s.paletteIndex = target.paletteIndex;
            s.width = target.width;
            s.height = target.height;
        } else {
            // Use the next-subfile offset to find the end of the data, because some writers store
            // a length that counts a palette they left out.
            std::uint64_t end = 0;
            if (next > start && next <= data.size()) {
                end = next;
            } else if (fits(start, length, data.size())) {
                end = start + length;
            } else {
                addReadProblem(doc.readProblems,
                               spriteName(s) + " is cut off at the end of the file and was left out.");
                break;
            }
            const bool copied = budget.take(end - start);
            if (copied)
                s.data.assign(data.begin() + static_cast<std::ptrdiff_t>(start),
                              data.begin() + static_cast<std::ptrdiff_t>(end));

            auto pcx = readPcxHeader(s.data);
            if (!copied || !pcx || pcx->width() > 0xFFFF || pcx->height() > 0xFFFF) {
                // Kept as it is, so a save doesn't lose the bytes.
                addReadProblem(doc.readProblems,
                               spriteName(s) + (copied ? " has a damaged image."
                                                       : " uses data that too many other sprites use, so it has no "
                                                         "image."));
                s.paletteIndex = lastPalette;
                doc.sprites.push_back(std::move(s));
                if (i + 1 < readable) {
                    if (next <= offset)
                        break;
                    offset = next;
                }
                continue;
            }
            s.width = static_cast<std::uint16_t>(pcx->width());
            s.height = static_cast<std::uint16_t>(pcx->height());

            if (s.samePalette && lastPalette >= 0) {
                s.paletteIndex = lastPalette;
            } else if (auto colors = readPcxPalette(s.data)) {
                SffPalette p;
                p.group = s.group;
                p.number = s.number;
                p.colors = std::move(*colors);
                doc.palettes.push_back(std::move(p));
                s.paletteIndex = static_cast<std::int32_t>(doc.palettes.size() - 1);
            } else if (lastPalette >= 0) {
                s.paletteIndex = lastPalette;
            } else {
                SffPalette p;
                p.group = s.group;
                p.number = s.number;
                p.colors = grayscalePalette();
                doc.palettes.push_back(std::move(p));
                s.paletteIndex = static_cast<std::int32_t>(doc.palettes.size() - 1);
            }
            lastPalette = s.paletteIndex;
        }
        doc.sprites.push_back(std::move(s));

        if (i + 1 < readable) {
            if (next <= offset) {
                addReadProblem(doc.readProblems,
                               spriteName(doc.sprites.back()) +
                                   " doesn't point to the next sprite, so the sprites after it are missing.");
                break;
            }
            offset = next;
        }
    }
    doc.raw.v1DistinctGroups = distinctGroups(doc.sprites);
    return doc;
}

Result<SffDocument> readV2(ByteSpan data, SffDocument doc)
{
    ByteReader r(data);
    r.seek(kV2TableOffset);
    const std::uint32_t spriteOffset = r.u32();
    std::uint32_t spriteCount = r.u32();
    const std::uint32_t paletteOffset = r.u32();
    std::uint32_t paletteCount = r.u32();
    const std::uint32_t literalOffset = r.u32();
    std::uint32_t literalLength = r.u32();
    const std::uint32_t translatedOffset = r.u32();
    std::uint32_t translatedLength = r.u32();
    doc.raw.fromFile = true;

    // A file cut off at the end keeps what is still in it.
    auto fitting = [&data](std::uint32_t offset, std::size_t size) {
        return offset <= data.size() ? static_cast<std::uint32_t>((data.size() - offset) / size) : 0u;
    };
    if (!fits(paletteOffset, std::uint64_t{paletteCount} * kV2PaletteNodeSize, data.size())) {
        addReadProblem(doc.readProblems, "The file is cut off: some palettes are missing.");
        paletteCount = std::min(paletteCount, fitting(paletteOffset, kV2PaletteNodeSize));
    }
    if (!fits(spriteOffset, std::uint64_t{spriteCount} * kV2SpriteNodeSize, data.size())) {
        addReadProblem(doc.readProblems, "The file is cut off: some sprites are missing.");
        spriteCount = std::min(spriteCount, fitting(spriteOffset, kV2SpriteNodeSize));
    }
    if (!fits(literalOffset, literalLength, data.size())) {
        addReadProblem(doc.readProblems, "The file is cut off: some images and palettes are missing.");
        literalLength = fitting(literalOffset, 1);
    }
    if (!fits(translatedOffset, translatedLength, data.size())) {
        addReadProblem(doc.readProblems, "The file is cut off: some images are missing.");
        translatedLength = fitting(translatedOffset, 1);
    }
    // Sprites store the palette number in 16 bits.
    constexpr std::uint32_t kMaxPalettes = 0x10000;
    if (paletteCount > kMaxPalettes) {
        addReadProblem(doc.readProblems,
                       "The file lists " + std::to_string(paletteCount) +
                           " palettes, but sprites can only use the first 65536. The rest were left out.");
        paletteCount = kMaxPalettes;
    }
    CopyBudget budget(data.size());

    // Linked nodes store an offset nobody reads. Count which style the file uses so the writer
    // can repeat it.
    int runningVotes = 0;
    int targetVotes = 0;
    auto vote = [&](std::uint32_t stored, std::uint64_t running, std::uint64_t target) {
        if (stored == running && stored != target)
            ++runningVotes;
        else if (stored == target && stored != running)
            ++targetVotes;
    };

    std::vector<std::uint32_t> paletteOffsets(paletteCount, 0);
    std::uint64_t paletteCursor = 0;
    doc.palettes.reserve(paletteCount);
    for (std::size_t i = 0; i < paletteCount; ++i) {
        r.seek(paletteOffset + i * kV2PaletteNodeSize);
        SffPalette p;
        p.group = r.u16();
        p.number = r.u16();
        const std::uint16_t colorCount = r.u16();
        const std::uint16_t link = r.u16();
        const std::uint32_t offset = r.u32();
        const std::uint32_t length = r.u32();
        paletteOffsets[i] = offset;
        if (length == 0 && link < paletteCount && link != i) {
            p.linkedIndex = link; // the colors are copied below
            if (link < i)
                vote(offset, paletteCursor, paletteOffsets[link]);
        } else if (length == 0) {
            addReadProblem(doc.readProblems, paletteName(p) + " links to a palette that doesn't exist, so it is gray.");
            p.colors = grayscalePalette();
        } else if (!fits(offset, length, literalLength)) {
            addReadProblem(doc.readProblems, paletteName(p) + " lies outside the file's data, so it is gray.");
            p.colors = grayscalePalette();
        } else {
            std::size_t count = colorCount;
            if (std::uint64_t{colorCount} * 4 > length) {
                addReadProblem(doc.readProblems, paletteName(p) + " is shorter than it says: some colors are missing.");
                count = length / 4;
            }
            if (budget.take(count * sizeof(Rgba))) {
                const std::span<const std::uint8_t> bytes = asU8(data.subspan(literalOffset + offset, length));
                p.colors.resize(count);
                for (std::size_t c = 0; c < count; ++c)
                    p.colors[c] = rgb(bytes[c * 4], bytes[c * 4 + 1], bytes[c * 4 + 2], bytes[c * 4 + 3]);
            } else {
                addReadProblem(doc.readProblems,
                               paletteName(p) + " uses colors that too many other palettes use, so it is gray.");
                p.colors = grayscalePalette();
            }
            paletteCursor = std::uint64_t{offset} + length;
        }
        doc.palettes.push_back(std::move(p));
    }
    // A link gets a copy of the colors at the end of its chain. Links to a later palette become
    // palettes of their own.
    const std::vector<std::int64_t> paletteEnds = linkEnds(doc.palettes);
    for (std::size_t i = 0; i < doc.palettes.size(); ++i) {
        SffPalette& p = doc.palettes[i];
        if (p.linkedIndex < 0)
            continue;
        if (static_cast<std::size_t>(p.linkedIndex) > i)
            p.linkedIndex = -1;
        const std::int64_t end = paletteEnds[i];
        if (end < 0) {
            addReadProblem(doc.readProblems, paletteName(p) + " is part of a loop of links, so it is gray.");
            p.linkedIndex = -1;
            p.colors = grayscalePalette();
        } else if (const Palette& colors = doc.palettes[static_cast<std::size_t>(end)].colors;
                   budget.take(colors.size() * sizeof(Rgba))) {
            p.colors = colors;
        } else {
            addReadProblem(doc.readProblems,
                           paletteName(p) + " uses colors that too many other palettes use, so it is gray.");
            p.linkedIndex = -1;
            p.colors = grayscalePalette();
        }
    }

    std::vector<std::uint32_t> spriteOffsets(spriteCount, 0);
    std::uint64_t cursor[2] = {paletteCursor, 0};
    doc.sprites.reserve(spriteCount);
    for (std::size_t i = 0; i < spriteCount; ++i) {
        r.seek(spriteOffset + i * kV2SpriteNodeSize);
        SffSprite s;
        s.group = r.u16();
        s.number = r.u16();
        s.width = r.u16();
        s.height = r.u16();
        s.axisX = r.s16();
        s.axisY = r.s16();
        const std::uint16_t link = r.u16();
        s.compression = static_cast<SffCompression>(r.u8());
        s.colorDepth = r.u8();
        const std::uint32_t offset = r.u32();
        const std::uint32_t length = r.u32();
        s.paletteIndex = r.u16();
        s.flags = r.u16();
        s.raw.fromFile = true;
        spriteOffsets[i] = offset;
        const int block = (s.flags & 1) ? 1 : 0;
        if (length == 0) {
            // A link to nothing or to the sprite itself leaves it without an image, and the rest of
            // the file still loads.
            if (link < spriteCount && link != i) {
                s.linkedIndex = link;
                if (link < i)
                    vote(offset, cursor[block], spriteOffsets[link]);
            } else {
                addReadProblem(doc.readProblems, spriteName(s) +
                                                     (link == i ? " links to itself"
                                                                : " links to a sprite that "
                                                                  "doesn't exist") +
                                                     ", so it has no image.");
            }
        } else {
            const std::uint32_t blockOffset = block ? translatedOffset : literalOffset;
            const std::uint32_t blockLength = block ? translatedLength : literalLength;
            if (fits(offset, length, blockLength) && !budget.take(length)) {
                addReadProblem(doc.readProblems,
                               spriteName(s) + " uses data that too many other sprites use, so it has no image.");
            } else if (fits(offset, length, blockLength)) {
                const ByteSpan bytes = data.subspan(std::size_t{blockOffset} + offset, length);
                s.data.assign(bytes.begin(), bytes.end());
                cursor[block] = std::uint64_t{offset} + length;
            } else {
                addReadProblem(doc.readProblems, spriteName(s) + " lies outside the file's data, so it has no image.");
            }
        }
        doc.sprites.push_back(std::move(s));
    }
    // MUGEN and Fighter Factory also read links to a later sprite. The rest of the program expects
    // links to point back, so the first sprite of each group takes the pixels and the others link
    // straight to it, the same way a save writes it.
    const std::vector<std::int64_t> spriteEnds = linkEnds(doc.sprites);
    std::vector<std::int64_t> owners(doc.sprites.size(), -1);
    for (std::size_t i = 0; i < doc.sprites.size(); ++i) {
        SffSprite& s = doc.sprites[i];
        const std::int64_t end = spriteEnds[i];
        if (end < 0) {
            addReadProblem(doc.readProblems, spriteName(s) + " is part of a loop of links, so it has no image.");
            s.linkedIndex = -1;
            continue;
        }
        std::int64_t& owner = owners[static_cast<std::size_t>(end)];
        if (owner >= 0) {
            s.linkedIndex = static_cast<std::int32_t>(owner);
            continue;
        }
        owner = static_cast<std::int64_t>(i);
        if (static_cast<std::size_t>(end) == i)
            continue;
        SffSprite& target = doc.sprites[static_cast<std::size_t>(end)];
        s.data = std::move(target.data);
        s.width = target.width;
        s.height = target.height;
        s.compression = target.compression;
        s.colorDepth = target.colorDepth;
        s.flags = target.flags;
        s.linkedIndex = -1;
        target.data.clear();
    }
    if (targetVotes > runningVotes)
        doc.linkOffsets = SffLinkOffsets::TargetOffset;
    // Some tools write PNG sprites into files marked 2.0.0.0. Read those as v2.01 but keep the
    // stored version bytes.
    if (doc.version == SffVersion::V2) {
        for (const SffSprite& s : doc.sprites) {
            if (!s.isLinked() && sffSupports(SffVersion::V201, s.compression) &&
                !sffSupports(SffVersion::V2, s.compression)) {
                doc.version = SffVersion::V201;
                break;
            }
        }
    }
    return doc;
}

} // namespace

Result<SffDocument> SffDocument::read(ByteSpan data)
{
    if (data.size() < kHeaderSize)
        return Error{"SFF header is truncated"};
    if (toText(data.first(kSignature.size())) != kSignature)
        return Error{"not an SFF file (signature missing)"};

    SffDocument doc;
    std::copy_n(asU8(data).begin(), kHeaderSize, doc.header.begin());
    std::copy_n(doc.header.begin() + kVersionOffset, 4, doc.versionBytes.begin());
    std::copy_n(doc.header.begin() + kV2CompatOffset, 4, doc.compatVersionBytes.begin());

    const std::uint8_t major = doc.versionBytes[3];
    if (major == 1) {
        doc.version = SffVersion::V1;
        return readV1(data, std::move(doc));
    }
    if (major == 2) {
        doc.version = doc.versionBytes[1] >= 1 ? SffVersion::V201 : SffVersion::V2;
        return readV2(data, std::move(doc));
    }
    return Error{"unsupported SFF version " + sffVersionString(doc.versionBytes)};
}

} // namespace OFS::Formats
