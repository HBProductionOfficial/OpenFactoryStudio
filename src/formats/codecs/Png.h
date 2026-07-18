#pragma once

#include "formats/FormatsExport.h"
#include "formats/image/Image.h"
#include "formats/util/Bytes.h"
#include "formats/util/Result.h"

#include <cstdint>

namespace OFS::Formats {

struct PngInfo
{
    int width = 0;
    int height = 0;
    int bitDepth = 0;
    int colorType = 0; // 0 gray, 2 RGB, 3 palette, 4 gray+alpha, 6 RGBA
    int paletteSize = 0;
};

OFS_FORMATS_EXPORT Result<PngInfo> readPngInfo(ByteSpan data);

// Palette images keep the stored indices even when the palette has duplicate colors. Other color
// types become Rgba32.
OFS_FORMATS_EXPORT Result<Image> decodePng(ByteSpan data);

enum class PngColor : std::uint8_t
{
    Automatic,
    Rgb,
    Rgba,
};

struct PngEncodeOptions
{
    PngColor color = PngColor::Automatic;
    bool transparentIndex0 = false;
};

OFS_FORMATS_EXPORT Result<Bytes> encodePng(const Image& image, const PngEncodeOptions& options = {});

} // namespace OFS::Formats
