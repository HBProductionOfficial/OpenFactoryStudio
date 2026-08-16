#pragma once

#include "formats/FormatsExport.h"
#include "formats/codecs/DecodeMode.h"
#include "formats/image/Image.h"
#include "formats/util/Bytes.h"
#include "formats/util/Result.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace OFS::Formats {

enum class SffVersion : std::uint8_t
{
    V1,   // 1.0.1.0: PCX sprites (WinMUGEN)
    V2,   // 2.0.0.0: palette list, RLE8/RLE5/LZ5 sprites (MUGEN 1.0)
    V201, // 2.0.1.0: adds PNG sprites (MUGEN 1.1)
};

OFS_FORMATS_EXPORT std::string_view sffVersionName(SffVersion version);

// Stored at offset 12 in reverse order, so 2.0.1.0 is 00 01 00 02.
OFS_FORMATS_EXPORT std::array<std::uint8_t, 4> sffVersionBytes(SffVersion version);
OFS_FORMATS_EXPORT std::string sffVersionString(const std::array<std::uint8_t, 4>& bytes);

// The values are the SFF v2 format codes.
enum class SffCompression : std::uint8_t
{
    Raw = 0,
    Invalid = 1, // some writers store it for linked sprites
    Rle8 = 2,
    Rle5 = 3,
    Lz5 = 4,
    Png8 = 10,
    Png24 = 11,
    Png32 = 12,
    Pcx = 0xF0, // SFF v1 only
};

OFS_FORMATS_EXPORT std::string_view sffCompressionName(SffCompression compression);
OFS_FORMATS_EXPORT bool sffSupports(SffVersion version, SffCompression compression);
OFS_FORMATS_EXPORT bool sffIsIndexed(SffCompression compression);

struct SffPalette
{
    std::uint16_t group = 0;
    std::uint16_t number = 0;
    // SFF v2 keeps the stored fourth byte in alpha because writers disagree on its meaning.
    Palette colors;
    std::int32_t linkedIndex = -1;
};

struct SffSpriteRaw
{
    bool fromFile = false;
    // SFF v1 subheader. Editors keep the sprite name in the 13 comment bytes.
    std::uint32_t v1Length = 0;
    std::uint16_t v1Link = 0;
    std::array<std::uint8_t, 13> v1Reserved{};
};

struct SffSprite
{
    std::uint16_t group = 0;
    std::uint16_t number = 0;
    std::int16_t axisX = 0;
    std::int16_t axisY = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    SffCompression compression = SffCompression::Rle8;
    std::uint8_t colorDepth = 8;
    std::int32_t paletteIndex = 0;
    std::int32_t linkedIndex = -1;
    // SFF v2: bit 0 puts the data in the "translated" block, which is decompressed on load.
    std::uint16_t flags = 0;
    bool samePalette = false;
    // As stored. SFF v2 compressed data starts with the 4-byte uncompressed size.
    Bytes data;
    // When set, replaces `data` and is encoded again on save.
    std::optional<Image> image;
    SffSpriteRaw raw;

    bool isLinked() const noexcept { return linkedIndex >= 0; }
};

// What links store in their unused data offset.
enum class SffLinkOffsets : std::uint8_t
{
    RunningOffset, // where the next data would go (Elecbyte's tools)
    TargetOffset,  // the target's data offset (some editors)
};

// SFF v1 header values that real files often get wrong, kept for byte-exact saves.
struct SffDocumentRaw
{
    bool fromFile = false;
    std::uint32_t v1GroupCount = 0;
    std::uint32_t v1DistinctGroups = 0;
    std::uint32_t v1SubheaderSize = 32;
};

struct SffCompressionPolicy
{
    bool keepExisting = true;
    SffCompression indexed8 = SffCompression::Rle8;
    SffCompression indexed5 = SffCompression::Lz5;
    SffCompression trueColor = SffCompression::Png32;
    bool smallest = false;
};

struct SffWriteOptions
{
    SffCompressionPolicy compression;
    bool linkDuplicateSprites = false;
    bool linkDuplicatePalettes = false;
};

struct SffConvertOptions
{
    // v1 to v2: the palette of sprite 0,0 gets the id 1,1 so MUGEN applies the character's ACT
    // palettes to it.
    bool characterPalettes = true;
};

// Unchanged sprites keep their stored bytes, so an unchanged file saves back byte for byte.
class OFS_FORMATS_EXPORT SffDocument
{
public:
    SffVersion version = SffVersion::V201;
    std::array<std::uint8_t, 4> versionBytes = {0, 1, 0, 2};
    std::array<std::uint8_t, 4> compatVersionBytes = {0, 1, 0, 2};
    bool sharedPalette = false; // SFF v1 header byte 32
    std::vector<SffSprite> sprites;
    std::vector<SffPalette> palettes;
    std::array<std::uint8_t, 512> header{};
    SffLinkOffsets linkOffsets = SffLinkOffsets::RunningOffset;
    SffDocumentRaw raw;

    SffDocument() = default;
    explicit SffDocument(SffVersion v);

    static Result<SffDocument> read(ByteSpan data);
    static Result<SffDocument> load(const std::filesystem::path& path);

    Result<Bytes> write(const SffWriteOptions& options = {}) const;
    Status save(const std::filesystem::path& path, const SffWriteOptions& options = {}) const;

    // v1 links always share the palette, so v2 links with another palette become copies. Fails
    // and changes nothing when a true-color sprite would have to go into v1 or v2.0.
    Status convertTo(SffVersion target, const SffConvertOptions& options = {});

    Result<Image> decodeSprite(std::size_t index, DecodeMode mode = DecodeMode::Lenient) const;
    static Result<Image> decodePayload(const SffSprite& sprite, const Palette* palette,
                                       DecodeMode mode = DecodeMode::Lenient);
    Result<Image> decodeSpriteRgba(std::size_t index, const Palette* palette = nullptr,
                                   DecodeMode mode = DecodeMode::Lenient) const;

    const Palette* spritePalette(std::size_t index) const;
    std::size_t resolveLink(std::size_t index) const;

    std::optional<std::size_t> findSprite(std::uint16_t group, std::uint16_t number) const;
    std::optional<std::size_t> findPalette(std::uint16_t group, std::uint16_t number) const;

    void setSpriteImage(std::size_t index, Image image);
};

} // namespace OFS::Formats
