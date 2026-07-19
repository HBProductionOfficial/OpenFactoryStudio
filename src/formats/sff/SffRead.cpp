#include "formats/codecs/Pcx.h"
#include "formats/sff/SffDocument.h"
#include "formats/sff/SffInternal.h"
#include "formats/util/ByteReader.h"

#include <algorithm>
#include <string>

namespace OFS::Formats {

using namespace SffDetail;

namespace {

std::string at(const char* what, std::size_t index)
{
    return std::string(what) + ' ' + std::to_string(index);
}

bool fits(std::uint64_t offset, std::uint64_t length, std::uint64_t size)
{
    return offset <= size && length <= size - offset;
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

    if (imageCount > data.size() / kV1SubheaderSize)
        return Error{"SFF v1 image count is larger than the file can hold"};
    if (imageCount > 0 && firstOffset < kV1SubheaderSize)
        return Error{"SFF v1 first subfile offset is invalid"};

    doc.sprites.reserve(imageCount);
    std::uint64_t offset = firstOffset;
    std::int32_t lastPalette = -1; // palette of the most recent sprite with its own pixels

    for (std::size_t i = 0; i < imageCount; ++i) {
        if (!fits(offset, kV1SubheaderSize, data.size()))
            return Error{at("SFF v1 subfile", i) + " lies outside the file"};
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
        if (length == 0) {
            if (link >= i)
                return Error{at("SFF v1 sprite", i) + " links to a sprite that does not precede it"};
            const SffSprite& target = doc.sprites[link];
            s.linkedIndex = link;
            s.paletteIndex = target.paletteIndex;
            s.width = target.width;
            s.height = target.height;
        } else {
            // Use the next-subfile offset to find the end of the data, because some writers store
            // a length that counts a palette they left out.
            std::uint64_t end = 0;
            if (next > start && next <= data.size())
                end = next;
            else if (fits(start, length, data.size()))
                end = start + length;
            else
                return Error{at("SFF v1 sprite", i) + " data extends past the end of the file"};
            s.data.assign(data.begin() + static_cast<std::ptrdiff_t>(start),
                          data.begin() + static_cast<std::ptrdiff_t>(end));

            auto pcx = readPcxHeader(s.data);
            if (!pcx)
                return withContext(at("SFF v1 sprite", i), pcx.error());
            if (pcx->width() > 0xFFFF || pcx->height() > 0xFFFF)
                return Error{at("SFF v1 sprite", i) + " is too large"};
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

        if (i + 1 < imageCount) {
            if (next <= offset)
                return Error{at("SFF v1 subfile", i) + " has an invalid next-subfile offset"};
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
    const std::uint32_t spriteCount = r.u32();
    const std::uint32_t paletteOffset = r.u32();
    const std::uint32_t paletteCount = r.u32();
    const std::uint32_t literalOffset = r.u32();
    const std::uint32_t literalLength = r.u32();
    const std::uint32_t translatedOffset = r.u32();
    const std::uint32_t translatedLength = r.u32();
    doc.raw.fromFile = true;

    if (!fits(paletteOffset, std::uint64_t{paletteCount} * kV2PaletteNodeSize, data.size()))
        return Error{"SFF v2 palette table lies outside the file"};
    if (!fits(spriteOffset, std::uint64_t{spriteCount} * kV2SpriteNodeSize, data.size()))
        return Error{"SFF v2 sprite table lies outside the file"};
    if (!fits(literalOffset, literalLength, data.size()))
        return Error{"SFF v2 literal data block lies outside the file"};
    if (!fits(translatedOffset, translatedLength, data.size()))
        return Error{"SFF v2 translated data block lies outside the file"};

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
        if (length == 0) {
            if (link >= i)
                return Error{at("SFF v2 palette", i) + " links to a palette that does not precede it"};
            p.linkedIndex = link;
            p.colors = doc.palettes[link].colors;
            vote(offset, paletteCursor, paletteOffsets[link]);
        } else {
            if (!fits(offset, length, literalLength))
                return Error{at("SFF v2 palette", i) + " lies outside the literal data block"};
            if (std::uint64_t{colorCount} * 4 > length)
                return Error{at("SFF v2 palette", i) + " is shorter than its color count"};
            const std::span<const std::uint8_t> bytes = asU8(data.subspan(literalOffset + offset, length));
            p.colors.resize(colorCount);
            for (std::size_t c = 0; c < colorCount; ++c)
                p.colors[c] = rgb(bytes[c * 4], bytes[c * 4 + 1], bytes[c * 4 + 2], bytes[c * 4 + 3]);
            paletteCursor = std::uint64_t{offset} + length;
        }
        doc.palettes.push_back(std::move(p));
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
            if (link >= i)
                return Error{at("SFF v2 sprite", i) + " links to a sprite that does not precede it"};
            s.linkedIndex = link;
            vote(offset, cursor[block], spriteOffsets[link]);
        } else {
            const std::uint32_t blockOffset = block ? translatedOffset : literalOffset;
            const std::uint32_t blockLength = block ? translatedLength : literalLength;
            if (!fits(offset, length, blockLength))
                return Error{at("SFF v2 sprite", i) + " lies outside its data block"};
            const ByteSpan bytes = data.subspan(std::size_t{blockOffset} + offset, length);
            s.data.assign(bytes.begin(), bytes.end());
            cursor[block] = std::uint64_t{offset} + length;
        }
        doc.sprites.push_back(std::move(s));
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
