#include "formats/codecs/Lz5.h"
#include "formats/codecs/Pcx.h"
#include "formats/codecs/Png.h"
#include "formats/codecs/Rle5.h"
#include "formats/codecs/Rle8.h"
#include "formats/sff/SffDocument.h"
#include "formats/sff/SffInternal.h"
#include "formats/util/ByteWriter.h"

#include <algorithm>
#include <limits>
#include <map>
#include <string>

namespace OFS::Formats {

using namespace SffDetail;

namespace {

std::string spriteContext(std::size_t index)
{
    return "sprite " + std::to_string(index);
}

void putPrefixed(ByteWriter& out, std::uint32_t size, ByteSpan body)
{
    out.u32(size);
    out.bytes(body);
}

Result<Bytes> encodePayload(const Image& image, SffCompression compression, const Palette* palette)
{
    const auto pixelCount = static_cast<std::uint32_t>(image.pixelCount());
    ByteWriter out;
    switch (compression) {
    case SffCompression::Raw:
        if (!image.isIndexed())
            return Error{"raw sprites must be indexed"};
        out.bytes(image.pixels());
        return out.take();
    case SffCompression::Rle8:
        if (!image.isIndexed())
            return Error{"RLE8 sprites must be indexed"};
        putPrefixed(out, pixelCount, encodeRle8(image.pixels()));
        return out.take();
    case SffCompression::Rle5:
    case SffCompression::Lz5: {
        if (!image.isIndexed())
            return Error{"RLE5/LZ5 sprites must be indexed"};
        auto body = compression == SffCompression::Rle5 ? encodeRle5(image.pixels()) : encodeLz5(image.pixels());
        if (!body)
            return body.error();
        putPrefixed(out, pixelCount, *body);
        return out.take();
    }
    case SffCompression::Png8: {
        if (!image.isIndexed())
            return Error{"PNG8 sprites must be indexed"};
        Image withPalette = image;
        if (palette)
            withPalette.setPalette(*palette);
        auto png = encodePng(withPalette);
        if (!png)
            return png.error();
        putPrefixed(out, pixelCount, *png);
        return out.take();
    }
    case SffCompression::Png24:
    case SffCompression::Png32: {
        PngEncodeOptions options;
        options.color = compression == SffCompression::Png24 ? PngColor::Rgb : PngColor::Rgba;
        auto png = encodePng(image, options);
        if (!png)
            return png.error();
        putPrefixed(out, pixelCount * pngPrefixBytesPerPixel(compression), *png);
        return out.take();
    }
    case SffCompression::Invalid:
    case SffCompression::Pcx:
        break;
    }
    return Error{"cannot encode SFF v2 sprites as " + std::string(sffCompressionName(compression))};
}

struct Payload
{
    Bytes bytes;
    SffCompression compression = SffCompression::Raw;
    std::uint8_t depth = 8;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    bool stored = false;
};

Result<Image> spritePixels(const SffDocument& doc, std::size_t index)
{
    const SffSprite& s = doc.sprites[index];
    if (s.image)
        return *s.image;
    return doc.decodeSprite(index, DecodeMode::Lenient);
}

Result<Payload> buildV2Payload(const SffDocument& doc, std::size_t index, const SffCompressionPolicy& policy)
{
    const SffSprite& s = doc.sprites[index];
    Payload result;
    if (policy.keepExisting && !policy.smallest && !s.image && !s.data.empty() &&
        sffSupports(doc.version, s.compression)) {
        result.bytes = s.data;
        result.compression = s.compression;
        result.depth = s.colorDepth;
        result.width = s.width;
        result.height = s.height;
        result.stored = true;
        return result;
    }

    auto pixels = spritePixels(doc, index);
    if (!pixels)
        return pixels.error();
    const Image& image = *pixels;
    if (image.width() > 0xFFFF || image.height() > 0xFFFF)
        return Error{"sprite is larger than 65535 pixels"};

    std::vector<SffCompression> candidates;
    if (image.isRgba()) {
        if (doc.version != SffVersion::V201)
            return Error{"true-color sprites need SFF v2.01"};
        SffCompression c = policy.trueColor;
        if (policy.keepExisting && !sffIsIndexed(s.compression))
            c = s.compression;
        if (c != SffCompression::Png24 && c != SffCompression::Png32)
            c = SffCompression::Png32;
        candidates.push_back(c);
    } else {
        const bool fiveBit = image.maxIndex() < 32;
        auto usable = [&](SffCompression c) {
            if (!sffSupports(doc.version, c) || !sffIsIndexed(c))
                return false;
            return fiveBit || (c != SffCompression::Rle5 && c != SffCompression::Lz5);
        };
        if (policy.smallest) {
            for (SffCompression c : {SffCompression::Lz5, SffCompression::Rle5, SffCompression::Rle8,
                                     SffCompression::Png8, SffCompression::Raw}) {
                if (usable(c))
                    candidates.push_back(c);
            }
        } else {
            SffCompression c = fiveBit ? policy.indexed5 : policy.indexed8;
            if (policy.keepExisting && usable(s.compression))
                c = s.compression;
            if (!usable(c))
                c = fiveBit ? SffCompression::Lz5 : SffCompression::Rle8;
            candidates.push_back(c);
        }
    }

    const Palette* palette = doc.spritePalette(index);
    bool found = false;
    for (SffCompression c : candidates) {
        auto encoded = encodePayload(image, c, palette);
        if (!encoded)
            return encoded.error();
        if (!found || encoded->size() < result.bytes.size()) {
            result.bytes = std::move(*encoded);
            result.compression = c;
            found = true;
        }
    }
    switch (result.compression) {
    case SffCompression::Rle5:
    case SffCompression::Lz5:
        result.depth = 5;
        break;
    case SffCompression::Png24:
        result.depth = 24;
        break;
    case SffCompression::Png32:
        result.depth = 32;
        break;
    default:
        result.depth = 8;
        break;
    }
    result.width = static_cast<std::uint16_t>(image.width());
    result.height = static_cast<std::uint16_t>(image.height());
    return result;
}

class DuplicateFinder
{
public:
    DuplicateFinder(const SffDocument& doc, bool comparePalettes)
        : m_doc(doc)
        , m_comparePalettes(comparePalettes)
    {}

    Result<std::int32_t> find(std::size_t index)
    {
        auto pixels = spritePixels(m_doc, index);
        if (!pixels)
            return pixels.error();
        const std::uint64_t key = hash(*pixels);
        auto range = m_seen.equal_range(key);
        for (auto it = range.first; it != range.second; ++it) {
            const Entry& e = m_entries[it->second];
            if (e.image.format() != pixels->format() || e.image.width() != pixels->width() ||
                e.image.height() != pixels->height() || e.image.pixelBuffer() != pixels->pixelBuffer())
                continue;
            if (m_comparePalettes && pixels->isIndexed()) {
                const Palette* a = m_doc.spritePalette(index);
                const Palette* b = m_doc.spritePalette(e.index);
                if (!a || !b || !sameRgb(*a, *b))
                    continue;
            }
            return static_cast<std::int32_t>(e.index);
        }
        m_seen.emplace(key, m_entries.size());
        m_entries.push_back(Entry{index, std::move(*pixels)});
        return std::int32_t{-1};
    }

private:
    struct Entry
    {
        std::size_t index;
        Image image;
    };

    static std::uint64_t hash(const Image& image)
    {
        std::uint64_t h = 1469598103934665603ull;
        auto mix = [&h](std::uint64_t v) {
            h ^= v;
            h *= 1099511628211ull;
        };
        mix(static_cast<std::uint64_t>(image.width()));
        mix(static_cast<std::uint64_t>(image.height()));
        mix(static_cast<std::uint64_t>(image.format()));
        for (const std::uint8_t b : image.pixels())
            mix(b);
        return h;
    }

    const SffDocument& m_doc;
    bool m_comparePalettes;
    std::multimap<std::uint64_t, std::size_t> m_seen;
    std::vector<Entry> m_entries;
};

bool fitsU32(std::size_t v)
{
    return v <= std::numeric_limits<std::uint32_t>::max();
}

void writeHeaderPrefix(ByteWriter& out, const SffDocument& doc)
{
    out.bytes(std::span<const std::uint8_t>(doc.header));
    for (std::size_t i = 0; i < kSignature.size(); ++i)
        out.patchU8(i, static_cast<std::uint8_t>(kSignature[i]));
    for (std::size_t i = 0; i < 4; ++i)
        out.patchU8(kVersionOffset + i, doc.versionBytes[i]);
}

Result<Bytes> writeV2(const SffDocument& doc, const SffWriteOptions& options)
{
    if (doc.palettes.size() > 0xFFFF || doc.sprites.size() > 0xFFFF + std::size_t{1})
        return Error{"too many sprites or palettes for SFF v2"};

    struct PaletteNode
    {
        std::uint16_t group, number, count, link;
        std::uint32_t offset, length;
    };
    struct SpriteNode
    {
        std::uint16_t group, number, width, height;
        std::int16_t axisX, axisY;
        std::uint16_t link;
        std::uint8_t format, depth;
        std::uint32_t offset, length;
        std::uint16_t palette, flags;
    };

    ByteWriter literal;
    ByteWriter translated;

    std::vector<PaletteNode> paletteNodes;
    std::vector<bool> paletteIsLink;
    paletteNodes.reserve(doc.palettes.size());
    for (std::size_t i = 0; i < doc.palettes.size(); ++i) {
        const SffPalette& p = doc.palettes[i];
        if (p.colors.size() > 0xFFFF)
            return Error{"palette " + std::to_string(i) + " has too many colors"};
        std::int32_t target = -1;
        if (p.linkedIndex >= 0 && static_cast<std::size_t>(p.linkedIndex) < i &&
            doc.palettes[static_cast<std::size_t>(p.linkedIndex)].colors == p.colors) {
            target = p.linkedIndex;
        } else if (options.linkDuplicatePalettes) {
            for (std::size_t j = 0; j < i && target < 0; ++j) {
                if (!paletteIsLink[j] && doc.palettes[j].colors == p.colors)
                    target = static_cast<std::int32_t>(j);
            }
        }
        PaletteNode node{};
        node.group = p.group;
        node.number = p.number;
        if (target >= 0) {
            const auto t = static_cast<std::size_t>(target);
            node.link = static_cast<std::uint16_t>(target);
            node.count = static_cast<std::uint16_t>(doc.palettes[t].colors.size());
            node.length = 0;
            node.offset = doc.linkOffsets == SffLinkOffsets::TargetOffset ? paletteNodes[t].offset
                                                                          : static_cast<std::uint32_t>(literal.size());
        } else {
            node.link = 0;
            node.count = static_cast<std::uint16_t>(p.colors.size());
            node.offset = static_cast<std::uint32_t>(literal.size());
            node.length = static_cast<std::uint32_t>(p.colors.size() * 4);
            for (const Rgba& c : p.colors) {
                literal.u8(c.r);
                literal.u8(c.g);
                literal.u8(c.b);
                literal.u8(c.a);
            }
        }
        paletteNodes.push_back(node);
        paletteIsLink.push_back(target >= 0);
    }

    std::optional<DuplicateFinder> duplicates;
    if (options.linkDuplicateSprites)
        duplicates.emplace(doc, false);

    std::vector<SpriteNode> spriteNodes;
    std::vector<std::size_t> roots;
    spriteNodes.reserve(doc.sprites.size());
    for (std::size_t i = 0; i < doc.sprites.size(); ++i) {
        const SffSprite& s = doc.sprites[i];
        if (s.paletteIndex < 0 || s.paletteIndex > 0xFFFF)
            return Error{spriteContext(i) + " has an invalid palette index"};
        std::int32_t target = -1;
        bool keptLink = false;
        if (!s.image && s.isLinked()) {
            if (static_cast<std::size_t>(s.linkedIndex) >= i)
                return Error{spriteContext(i) + " links to a sprite that does not precede it"};
            target = s.linkedIndex;
            keptLink = true;
        } else if (duplicates) {
            auto dup = duplicates->find(i);
            if (!dup)
                return withContext(spriteContext(i), dup.error());
            target = *dup;
        }

        SpriteNode node{};
        node.group = s.group;
        node.number = s.number;
        node.axisX = s.axisX;
        node.axisY = s.axisY;
        node.palette = static_cast<std::uint16_t>(s.paletteIndex);
        if (target >= 0) {
            const auto t = static_cast<std::size_t>(target);
            const SpriteNode& rootNode = spriteNodes[roots[t]];
            const bool ownFields = keptLink && s.raw.fromFile && s.compression != SffCompression::Pcx;
            node.width = rootNode.width;
            node.height = rootNode.height;
            node.link = static_cast<std::uint16_t>(target);
            node.format = ownFields ? static_cast<std::uint8_t>(s.compression) : rootNode.format;
            node.depth = ownFields ? s.colorDepth : rootNode.depth;
            node.flags = ownFields ? s.flags : rootNode.flags;
            node.length = 0;
            const ByteWriter& block = (node.flags & 1) ? translated : literal;
            node.offset = doc.linkOffsets == SffLinkOffsets::TargetOffset ? spriteNodes[t].offset
                                                                          : static_cast<std::uint32_t>(block.size());
            roots.push_back(roots[t]);
        } else {
            auto payload = buildV2Payload(doc, i, options.compression);
            if (!payload)
                return withContext(spriteContext(i), payload.error());
            if (!sffIsIndexed(payload->compression) && doc.version != SffVersion::V201)
                return Error{spriteContext(i) + " is true color, which needs SFF v2.01"};
            node.width = payload->width;
            node.height = payload->height;
            node.link = 0;
            node.format = static_cast<std::uint8_t>(payload->compression);
            node.depth = payload->depth;
            node.flags = s.flags;
            // Elecbyte's tools refuse raw sprites in the translated block.
            if (!payload->stored && payload->compression == SffCompression::Raw)
                node.flags = static_cast<std::uint16_t>(node.flags & ~1u);
            ByteWriter& block = (node.flags & 1) ? translated : literal;
            node.offset = static_cast<std::uint32_t>(block.size());
            node.length = static_cast<std::uint32_t>(payload->bytes.size());
            block.bytes(payload->bytes);
            roots.push_back(i);
        }
        spriteNodes.push_back(node);
    }

    const std::size_t paletteTable = kHeaderSize;
    const std::size_t spriteTable = paletteTable + paletteNodes.size() * kV2PaletteNodeSize;
    const std::size_t literalOffset = spriteTable + spriteNodes.size() * kV2SpriteNodeSize;
    const std::size_t translatedOffset = literalOffset + literal.size();
    if (!fitsU32(translatedOffset + translated.size()))
        return Error{"SFF file would be larger than 4 GiB"};

    ByteWriter out(translatedOffset + translated.size());
    writeHeaderPrefix(out, doc);
    for (std::size_t i = 0; i < 4; ++i)
        out.patchU8(kV2CompatOffset + i, doc.compatVersionBytes[i]);
    const std::uint32_t table[8] = {
        static_cast<std::uint32_t>(spriteTable),      static_cast<std::uint32_t>(spriteNodes.size()),
        static_cast<std::uint32_t>(paletteTable),     static_cast<std::uint32_t>(paletteNodes.size()),
        static_cast<std::uint32_t>(literalOffset),    static_cast<std::uint32_t>(literal.size()),
        static_cast<std::uint32_t>(translatedOffset), static_cast<std::uint32_t>(translated.size())};
    for (std::size_t i = 0; i < 8; ++i)
        out.patchU32(kV2TableOffset + i * 4, table[i]);

    for (const PaletteNode& n : paletteNodes) {
        out.u16(n.group);
        out.u16(n.number);
        out.u16(n.count);
        out.u16(n.link);
        out.u32(n.offset);
        out.u32(n.length);
    }
    for (const SpriteNode& n : spriteNodes) {
        out.u16(n.group);
        out.u16(n.number);
        out.u16(n.width);
        out.u16(n.height);
        out.s16(n.axisX);
        out.s16(n.axisY);
        out.u16(n.link);
        out.u8(n.format);
        out.u8(n.depth);
        out.u32(n.offset);
        out.u32(n.length);
        out.u16(n.palette);
        out.u16(n.flags);
    }
    out.bytes(literal.data());
    out.bytes(translated.data());
    return out.take();
}

bool samePcxPalette(const Palette& a, const Palette& b)
{
    for (std::size_t i = 0; i < 256; ++i) {
        const Rgba x = i < a.size() ? a[i] : rgb(0, 0, 0);
        const Rgba y = i < b.size() ? b[i] : rgb(0, 0, 0);
        if (!sameRgb(x, y))
            return false;
    }
    return true;
}

void putPcxPalette(Bytes& out, std::size_t at, const Palette& palette)
{
    for (std::size_t i = 0; i < 256; ++i) {
        const Rgba c = i < palette.size() ? palette[i] : rgb(0, 0, 0);
        out[at + i * 3] = std::byte{c.r};
        out[at + i * 3 + 1] = std::byte{c.g};
        out[at + i * 3 + 2] = std::byte{c.b};
    }
}

struct V1Payload
{
    Bytes bytes;
    bool samePalette = false;
    bool verbatim = false;
};

Result<V1Payload> buildV1Payload(const SffDocument& doc, std::size_t index, const Palette* previous)
{
    const SffSprite& s = doc.sprites[index];
    const Palette* palette = doc.spritePalette(index);
    if (!palette)
        return Error{"sprite has no palette"};
    const bool canShare = previous && samePcxPalette(*previous, *palette);

    V1Payload result;
    if (!s.image && s.compression == SffCompression::Pcx && !s.data.empty()) {
        if (s.samePalette && canShare) {
            result.bytes = s.data;
            result.samePalette = true;
            result.verbatim = true;
            return result;
        }
        const std::optional<Palette> embedded = readPcxPalette(s.data);
        if (!s.samePalette && embedded && samePcxPalette(*embedded, *palette)) {
            result.bytes = s.data;
            result.verbatim = true;
            return result;
        }
        // The sprite has to carry the document's palette now.
        result.bytes = s.data;
        if (embedded) {
            putPcxPalette(result.bytes, result.bytes.size() - 768, *palette);
        } else {
            result.bytes.push_back(std::byte{0x0C});
            result.bytes.resize(result.bytes.size() + 768);
            putPcxPalette(result.bytes, result.bytes.size() - 768, *palette);
        }
        return result;
    }

    auto pixels = spritePixels(doc, index);
    if (!pixels)
        return pixels.error();
    if (!pixels->isIndexed())
        return Error{"true-color sprites cannot be stored in SFF v1"};
    Image image = std::move(*pixels);
    image.setPalette(*palette);
    PcxEncodeOptions pcxOptions;
    pcxOptions.includePalette = !canShare;
    auto pcx = encodePcx(image, pcxOptions);
    if (!pcx)
        return pcx.error();
    result.bytes = std::move(*pcx);
    result.samePalette = canShare;
    return result;
}

Result<Bytes> writeV1(const SffDocument& doc, const SffWriteOptions& options)
{
    if (doc.sprites.size() > 0xFFFF + std::size_t{1})
        return Error{"too many sprites for SFF v1"};

    ByteWriter out;
    writeHeaderPrefix(out, doc);
    const std::uint32_t groups = distinctGroups(doc.sprites);
    const bool keepGroupCount = doc.raw.fromFile && groups == doc.raw.v1DistinctGroups;
    out.patchU32(kV1GroupCountOffset, keepGroupCount ? doc.raw.v1GroupCount : groups);
    out.patchU32(kV1GroupCountOffset + 4, static_cast<std::uint32_t>(doc.sprites.size()));
    out.patchU32(kV1GroupCountOffset + 8, static_cast<std::uint32_t>(kHeaderSize));
    out.patchU32(kV1GroupCountOffset + 12,
                 doc.raw.fromFile ? doc.raw.v1SubheaderSize : static_cast<std::uint32_t>(kV1SubheaderSize));
    out.patchU8(kV1PaletteTypeOffset, doc.sharedPalette ? 1 : 0);

    std::optional<DuplicateFinder> duplicates;
    if (options.linkDuplicateSprites)
        duplicates.emplace(doc, true);

    const Palette* previous = nullptr; // palette of the last sprite written with its own pixels
    for (std::size_t i = 0; i < doc.sprites.size(); ++i) {
        const SffSprite& s = doc.sprites[i];
        const std::size_t offset = out.size();
        std::int32_t target = -1;
        if (!s.image && s.isLinked()) {
            if (static_cast<std::size_t>(s.linkedIndex) >= i)
                return Error{spriteContext(i) + " links to a sprite that does not precede it"};
            target = s.linkedIndex;
        } else if (duplicates) {
            auto dup = duplicates->find(i);
            if (!dup)
                return withContext(spriteContext(i), dup.error());
            target = *dup;
        }

        Bytes payload;
        std::uint32_t length = 0;
        std::uint16_t link = 0;
        bool same = s.samePalette;
        if (target >= 0) {
            link = static_cast<std::uint16_t>(target);
        } else {
            auto built = buildV1Payload(doc, i, previous);
            if (!built)
                return withContext(spriteContext(i), built.error());
            payload = std::move(built->bytes);
            same = built->samePalette;
            length = (built->verbatim && s.raw.fromFile && s.raw.v1Length != 0)
                         ? s.raw.v1Length
                         : static_cast<std::uint32_t>(payload.size());
            link = s.raw.fromFile ? s.raw.v1Link : 0;
            previous = doc.spritePalette(i);
        }
        if (!fitsU32(offset + kV1SubheaderSize + payload.size()))
            return Error{"SFF file would be larger than 4 GiB"};
        out.u32(static_cast<std::uint32_t>(offset + kV1SubheaderSize + payload.size()));
        out.u32(length);
        out.s16(s.axisX);
        out.s16(s.axisY);
        out.u16(s.group);
        out.u16(s.number);
        out.u16(link);
        out.u8(same ? 1 : 0);
        out.bytes(std::span<const std::uint8_t>(s.raw.v1Reserved));
        out.bytes(payload);
    }
    return out.take();
}

} // namespace

Result<Bytes> SffDocument::write(const SffWriteOptions& options) const
{
    if (version == SffVersion::V1)
        return writeV1(*this, options);
    return writeV2(*this, options);
}

} // namespace OFS::Formats
