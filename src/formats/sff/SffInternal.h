#pragma once

#include "formats/sff/SffDocument.h"

#include <cstddef>
#include <cstdint>
#include <set>
#include <string_view>

namespace OFS::Formats::SffDetail {

inline constexpr std::string_view kSignature{"ElecbyteSpr\0", 12};
inline constexpr std::size_t kHeaderSize = 512;
inline constexpr std::size_t kV1SubheaderSize = 32;
inline constexpr std::size_t kV2PaletteNodeSize = 16;
inline constexpr std::size_t kV2SpriteNodeSize = 28;

// So damaged headers can't make the reader allocate huge buffers.
inline constexpr std::size_t kMaxDecodePixels = std::size_t{1} << 26;

// Header offsets.
inline constexpr std::size_t kVersionOffset = 12;
inline constexpr std::size_t kV1GroupCountOffset = 16;
inline constexpr std::size_t kV1PaletteTypeOffset = 32;
inline constexpr std::size_t kV2CompatOffset = 24;
inline constexpr std::size_t kV2TableOffset = 36;

inline std::uint32_t distinctGroups(const std::vector<SffSprite>& sprites)
{
    std::set<std::uint16_t> groups;
    for (const SffSprite& s : sprites)
        groups.insert(s.group);
    return static_cast<std::uint32_t>(groups.size());
}

// The size prefix of PNG payloads counts this many bytes per pixel.
inline std::uint32_t pngPrefixBytesPerPixel(SffCompression c)
{
    switch (c) {
    case SffCompression::Png24:
        return 3;
    case SffCompression::Png32:
        return 4;
    default:
        return 1;
    }
}

} // namespace OFS::Formats::SffDetail
