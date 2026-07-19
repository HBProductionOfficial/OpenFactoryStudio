#pragma once

#include "formats/FormatsExport.h"
#include "formats/codecs/DecodeMode.h"
#include "formats/image/Image.h"
#include "formats/util/Bytes.h"
#include "formats/util/Result.h"

#include <cstdint>
#include <optional>

namespace OFS::Formats {

struct PcxHeader
{
    std::uint8_t version = 5;
    std::uint8_t encoding = 1; // 1 = RLE, 0 = uncompressed (rare)
    std::uint8_t bitsPerPixel = 8;
    std::uint16_t xMin = 0;
    std::uint16_t yMin = 0;
    std::uint16_t xMax = 0;
    std::uint16_t yMax = 0;
    std::uint16_t hDpi = 0;
    std::uint16_t vDpi = 0;
    std::uint8_t planes = 1;
    std::uint16_t bytesPerLine = 0; // per plane, at least the width
    std::uint16_t paletteInfo = 1;

    int width() const noexcept { return int{xMax} - int{xMin} + 1; }
    int height() const noexcept { return int{yMax} - int{yMin} + 1; }
};

inline constexpr std::size_t kPcxHeaderSize = 128;
inline constexpr std::size_t kPcxPaletteSize = 769; // 0x0C marker + 256 RGB triplets

enum class PcxPalette : std::uint8_t
{
    Required,
    Optional, // SFF v1 sprites that reuse the previous palette store none
};

OFS_FORMATS_EXPORT Result<PcxHeader> readPcxHeader(ByteSpan data);

// The 256 colors after the 0x0C marker at the end of the file.
OFS_FORMATS_EXPORT std::optional<Palette> readPcxPalette(ByteSpan data);

OFS_FORMATS_EXPORT Result<Image> decodePcx(ByteSpan data, PcxPalette palette = PcxPalette::Required,
                                           DecodeMode mode = DecodeMode::Strict);

struct PcxEncodeOptions
{
    bool includePalette = true;
    std::uint16_t dpi = 72;
};

// 8-bit RLE, version 5. Scanlines are padded to an even length as the format needs.
OFS_FORMATS_EXPORT Result<Bytes> encodePcx(const Image& image, const PcxEncodeOptions& options = {});

} // namespace OFS::Formats
