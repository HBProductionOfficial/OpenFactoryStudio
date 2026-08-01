#pragma once

#include "formats/FormatsExport.h"
#include "formats/codecs/DecodeMode.h"
#include "formats/image/Image.h"
#include "formats/util/Bytes.h"
#include "formats/util/Result.h"

#include <cstdint>

namespace OFS::Formats {

// biCompression values. Jpeg and Png are not supported.
enum class BmpCompression : std::uint32_t
{
    Rgb = 0,
    Rle8 = 1,
    Rle4 = 2,
    Bitfields = 3,
    Jpeg = 4,
    Png = 5,
    AlphaBitfields = 6,
};

struct BmpInfo
{
    int width = 0;
    int height = 0;
    bool topDown = false; // stored with a negative height
    int bitsPerPixel = 0;
    BmpCompression compression = BmpCompression::Rgb;
    std::uint32_t headerSize = 0;
    std::uint32_t dataOffset = 0;
    std::uint32_t redMask = 0;
    std::uint32_t greenMask = 0;
    std::uint32_t blueMask = 0;
    std::uint32_t alphaMask = 0;
    Palette palette;
};

OFS_FORMATS_EXPORT Result<BmpInfo> readBmpInfo(ByteSpan data);

// Up to 8 bits gives Indexed8, more gives Rgba32. A 32-bit image whose alpha is all zero counts
// as opaque, because most writers leave that byte unused.
OFS_FORMATS_EXPORT Result<Image> decodeBmp(ByteSpan data, DecodeMode mode = DecodeMode::Strict);

enum class BmpColor : std::uint8_t
{
    Automatic,
    Rgb,
    Rgba,
};

struct BmpEncodeOptions
{
    BmpColor color = BmpColor::Automatic;
    bool transparentIndex0 = false;
    std::uint16_t dpi = 72;
};

// 32-bit files use a BITMAPV4HEADER with BI_BITFIELDS masks so readers pick up the alpha.
OFS_FORMATS_EXPORT Result<Bytes> encodeBmp(const Image& image, const BmpEncodeOptions& options = {});

} // namespace OFS::Formats
