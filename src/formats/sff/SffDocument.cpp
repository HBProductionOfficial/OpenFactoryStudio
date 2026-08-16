#include "formats/sff/SffDocument.h"

#include "formats/codecs/Lz5.h"
#include "formats/codecs/Pcx.h"
#include "formats/codecs/Png.h"
#include "formats/codecs/Rle5.h"
#include "formats/codecs/Rle8.h"
#include "formats/sff/SffInternal.h"
#include "formats/util/ByteReader.h"
#include "formats/util/FileIo.h"

#include <algorithm>
#include <set>
#include <utility>

namespace OFS::Formats {

using namespace SffDetail;

std::string_view sffVersionName(SffVersion version)
{
    switch (version) {
    case SffVersion::V1:
        return "v1";
    case SffVersion::V2:
        return "v2.0";
    case SffVersion::V201:
        return "v2.01";
    }
    return "?";
}

std::array<std::uint8_t, 4> sffVersionBytes(SffVersion version)
{
    switch (version) {
    case SffVersion::V1:
        return {0, 1, 0, 1};
    case SffVersion::V2:
        return {0, 0, 0, 2};
    case SffVersion::V201:
        return {0, 1, 0, 2};
    }
    return {0, 0, 0, 0};
}

std::string sffVersionString(const std::array<std::uint8_t, 4>& bytes)
{
    return std::to_string(bytes[3]) + '.' + std::to_string(bytes[2]) + '.' + std::to_string(bytes[1]) + '.' +
           std::to_string(bytes[0]);
}

std::string_view sffCompressionName(SffCompression compression)
{
    switch (compression) {
    case SffCompression::Raw:
        return "raw";
    case SffCompression::Invalid:
        return "invalid";
    case SffCompression::Rle8:
        return "rle8";
    case SffCompression::Rle5:
        return "rle5";
    case SffCompression::Lz5:
        return "lz5";
    case SffCompression::Png8:
        return "png8";
    case SffCompression::Png24:
        return "png24";
    case SffCompression::Png32:
        return "png32";
    case SffCompression::Pcx:
        return "pcx";
    }
    return "unknown";
}

bool sffSupports(SffVersion version, SffCompression compression)
{
    switch (compression) {
    case SffCompression::Pcx:
        return version == SffVersion::V1;
    case SffCompression::Raw:
    case SffCompression::Rle8:
    case SffCompression::Rle5:
    case SffCompression::Lz5:
        return version != SffVersion::V1;
    case SffCompression::Png8:
    case SffCompression::Png24:
    case SffCompression::Png32:
        return version == SffVersion::V201;
    case SffCompression::Invalid:
        return false;
    }
    return false;
}

bool sffIsIndexed(SffCompression compression)
{
    return compression != SffCompression::Png24 && compression != SffCompression::Png32;
}

SffDocument::SffDocument(SffVersion v)
    : version(v)
    , versionBytes(sffVersionBytes(v))
    , compatVersionBytes(sffVersionBytes(v))
{}

Result<SffDocument> SffDocument::load(const std::filesystem::path& path)
{
    auto data = readFile(path);
    if (!data)
        return data.error();
    return read(*data);
}

Status SffDocument::save(const std::filesystem::path& path, const SffWriteOptions& options) const
{
    auto data = write(options);
    if (!data)
        return data.error();
    return writeFile(path, *data);
}

std::size_t SffDocument::resolveLink(std::size_t index) const
{
    // Links always point to an earlier sprite, so this terminates.
    while (index < sprites.size() && sprites[index].linkedIndex >= 0 && !sprites[index].image &&
           static_cast<std::size_t>(sprites[index].linkedIndex) < index)
        index = static_cast<std::size_t>(sprites[index].linkedIndex);
    return index;
}

const Palette* SffDocument::spritePalette(std::size_t index) const
{
    if (index >= sprites.size())
        return nullptr;
    const std::int32_t p = sprites[index].paletteIndex;
    if (p < 0 || static_cast<std::size_t>(p) >= palettes.size())
        return nullptr;
    return &palettes[static_cast<std::size_t>(p)].colors;
}

Result<Image> SffDocument::decodeSprite(std::size_t index, DecodeMode mode) const
{
    if (index >= sprites.size())
        return Error{"sprite index out of range"};
    const SffSprite& s = sprites[resolveLink(index)];
    if (!s.image && s.isLinked())
        return Error{"sprite " + std::to_string(index) + " has an invalid link"};
    return decodePayload(s, spritePalette(index), mode);
}

Result<Image> SffDocument::decodePayload(const SffSprite& s, const Palette* palette, DecodeMode mode)
{
    auto withPalette = [&](Image image) {
        if (image.isIndexed() && palette)
            image.setPalette(*palette);
        return image;
    };

    if (s.image)
        return withPalette(*s.image);
    if (s.isLinked())
        return Error{"a linked sprite has no data of its own"};

    const std::size_t pixelCount = std::size_t{s.width} * s.height;
    if (pixelCount > kMaxDecodePixels)
        return Error{"sprite " + std::to_string(s.group) + "," + std::to_string(s.number) + " is too large to decode"};
    const ByteSpan data(s.data);
    auto unpackIndexed = [&](Result<std::vector<std::uint8_t>> pixels) -> Result<Image> {
        if (!pixels)
            return pixels.error();
        return withPalette(Image::indexed(s.width, s.height, std::move(*pixels)));
    };
    auto stream = [&]() -> Result<ByteSpan> {
        if (data.size() < 4)
            return Error{"sprite data is too short"};
        return data.subspan(4);
    };

    switch (s.compression) {
    case SffCompression::Pcx: {
        auto image = decodePcx(data, PcxPalette::Optional, mode);
        if (!image)
            return image.error();
        return withPalette(std::move(*image));
    }
    case SffCompression::Raw:
        // SprMaker2 can't make raw true-color data, so its byte order is unknown.
        if (s.colorDepth > 8)
            return Error{"raw true-color sprites are not supported"};
        if (data.size() < pixelCount && mode == DecodeMode::Strict)
            return Error{"raw sprite data is too short"};
        {
            std::vector<std::uint8_t> pixels(pixelCount, 0);
            const std::span<const std::uint8_t> in = asU8(data);
            std::copy_n(in.begin(), std::min(pixelCount, in.size()), pixels.begin());
            return withPalette(Image::indexed(s.width, s.height, std::move(pixels)));
        }
    case SffCompression::Rle8: {
        auto body = stream();
        if (!body)
            return body.error();
        return unpackIndexed(decodeRle8(*body, pixelCount, mode));
    }
    case SffCompression::Rle5: {
        auto body = stream();
        if (!body)
            return body.error();
        return unpackIndexed(decodeRle5(*body, pixelCount, mode));
    }
    case SffCompression::Lz5: {
        auto body = stream();
        if (!body)
            return body.error();
        return unpackIndexed(decodeLz5(*body, pixelCount, mode));
    }
    case SffCompression::Png8:
    case SffCompression::Png24:
    case SffCompression::Png32: {
        auto body = stream();
        if (!body)
            return body.error();
        auto image = decodePng(*body);
        if (!image)
            return image.error();
        if (s.compression != SffCompression::Png8 && image->isIndexed())
            return image->toRgba(false);
        return withPalette(std::move(*image));
    }
    case SffCompression::Invalid:
        break;
    }
    return Error{"sprite " + std::to_string(s.group) + "," + std::to_string(s.number) + " uses unknown format " +
                 std::to_string(static_cast<int>(s.compression))};
}

Result<Image> SffDocument::decodeSpriteRgba(std::size_t index, const Palette* palette, DecodeMode mode) const
{
    auto image = decodeSprite(index, mode);
    if (!image)
        return image.error();
    if (image->isIndexed() && palette)
        image->setPalette(*palette);
    return image->toRgba(true);
}

std::optional<std::size_t> SffDocument::findSprite(std::uint16_t group, std::uint16_t number) const
{
    for (std::size_t i = 0; i < sprites.size(); ++i) {
        if (sprites[i].group == group && sprites[i].number == number)
            return i;
    }
    return std::nullopt;
}

std::optional<std::size_t> SffDocument::findPalette(std::uint16_t group, std::uint16_t number) const
{
    for (std::size_t i = 0; i < palettes.size(); ++i) {
        if (palettes[i].group == group && palettes[i].number == number)
            return i;
    }
    return std::nullopt;
}

void SffDocument::setSpriteImage(std::size_t index, Image image)
{
    if (index >= sprites.size())
        return;
    SffSprite& s = sprites[index];
    s.width = static_cast<std::uint16_t>(std::clamp(image.width(), 0, 0xFFFF));
    s.height = static_cast<std::uint16_t>(std::clamp(image.height(), 0, 0xFFFF));
    s.linkedIndex = -1;
    s.data.clear();
    if (image.isRgba()) {
        if (sffIsIndexed(s.compression))
            s.compression = SffCompression::Png32;
        s.colorDepth = s.compression == SffCompression::Png24 ? 24 : 32;
    } else {
        if (!sffIsIndexed(s.compression))
            s.compression = SffCompression::Png8;
        s.colorDepth =
            image.maxIndex() < 32 && (s.compression == SffCompression::Rle5 || s.compression == SffCompression::Lz5)
                ? 5
                : 8;
    }
    s.image = std::move(image);
}

namespace {

bool isTrueColor(const SffSprite& s)
{
    if (s.image)
        return s.image->isRgba();
    return !s.isLinked() && !sffIsIndexed(s.compression);
}

void makePaletteIdsUnique(std::vector<SffPalette>& palettes)
{
    std::set<std::pair<std::uint16_t, std::uint16_t>> used;
    for (SffPalette& p : palettes) {
        while (used.count({p.group, p.number}) && p.number < 0xFFFF)
            ++p.number;
        used.insert({p.group, p.number});
    }
}

} // namespace

Status SffDocument::convertTo(SffVersion target, const SffConvertOptions& options)
{
    if (target != SffVersion::V201) {
        for (std::size_t i = 0; i < sprites.size(); ++i) {
            if (isTrueColor(sprites[i]))
                return Error{"sprite " + std::to_string(i) + " is true color, which SFF " +
                             std::string(sffVersionName(target)) + " cannot store"};
        }
    }

    const bool familyChange = (version == SffVersion::V1) != (target == SffVersion::V1);
    if (familyChange) {
        header.fill(0);
        raw = SffDocumentRaw{};
        for (SffSprite& s : sprites)
            s.raw = SffSpriteRaw{};
    }

    if (version == SffVersion::V1 && target != SffVersion::V1) {
        // v1 stores a palette in nearly every sprite, so merge identical ones.
        std::vector<SffPalette> merged;
        std::vector<std::int32_t> remap(palettes.size(), 0);
        for (std::size_t i = 0; i < palettes.size(); ++i) {
            std::size_t j = 0;
            while (j < merged.size() && !sameRgb(merged[j].colors, palettes[i].colors))
                ++j;
            if (j == merged.size()) {
                merged.push_back(palettes[i]);
                merged.back().linkedIndex = -1;
            }
            remap[i] = static_cast<std::int32_t>(j);
        }
        for (SffSprite& s : sprites) {
            if (s.paletteIndex >= 0 && static_cast<std::size_t>(s.paletteIndex) < remap.size())
                s.paletteIndex = remap[static_cast<std::size_t>(s.paletteIndex)];
            s.samePalette = false;
            s.flags = 0;
            if (s.compression == SffCompression::Pcx)
                s.colorDepth = 8;
        }
        palettes = std::move(merged);
        if (options.characterPalettes && !palettes.empty()) {
            std::int32_t main = sprites.empty() ? 0 : sprites.front().paletteIndex;
            if (auto standing = findSprite(0, 0))
                main = sprites[*standing].paletteIndex;
            if (main >= 0 && static_cast<std::size_t>(main) < palettes.size()) {
                const auto m = static_cast<std::size_t>(main);
                std::rotate(palettes.begin(), palettes.begin() + static_cast<std::ptrdiff_t>(m),
                            palettes.begin() + static_cast<std::ptrdiff_t>(m) + 1);
                for (SffSprite& s : sprites) {
                    if (s.paletteIndex == main)
                        s.paletteIndex = 0;
                    else if (s.paletteIndex >= 0 && s.paletteIndex < main)
                        ++s.paletteIndex;
                }
                palettes.front().group = 1;
                palettes.front().number = 1;
            }
        }
        makePaletteIdsUnique(palettes);
    }

    if (target == SffVersion::V1 && version != SffVersion::V1) {
        for (std::size_t i = 0; i < sprites.size(); ++i) {
            SffSprite& s = sprites[i];
            if (!s.isLinked() || s.image)
                continue;
            const std::size_t root = resolveLink(i);
            if (root == i)
                continue;
            const Palette* own = spritePalette(i);
            const Palette* theirs = spritePalette(root);
            if (own && theirs && sameRgb(*own, *theirs))
                continue;
            const SffSprite& t = sprites[root];
            s.linkedIndex = -1;
            s.data = t.data;
            s.image = t.image;
            s.compression = t.compression;
            s.colorDepth = t.colorDepth;
            s.width = t.width;
            s.height = t.height;
        }
        for (SffPalette& p : palettes)
            p.linkedIndex = -1;
        for (SffSprite& s : sprites)
            s.flags = 0;
    }

    version = target;
    versionBytes = sffVersionBytes(target);
    compatVersionBytes = sffVersionBytes(target);
    return {};
}

} // namespace OFS::Formats
