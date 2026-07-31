#pragma once

#include "formats/FormatsExport.h"
#include "formats/image/Image.h"
#include "formats/image/ImageOps.h"

#include <cstdint>
#include <string_view>

namespace OFS::Formats {

// Scale3x (AdvMAME3x). Compares pixels exactly, so indexed images stay indexed.
OFS_FORMATS_EXPORT Image scale3x(const Image& image);

// Factor 2 is Scale2x, 3 is Scale3x and 4 is Scale2x twice.
OFS_FORMATS_EXPORT Image scaleEpx(const Image& image, int factor);

// hq2x to hq4x. The hqx library has no alpha, so transparent pixels get a color unlike any
// visible one and the blend uses premultiplied alpha. Always gives Rgba32.
OFS_FORMATS_EXPORT Image scaleHq(const Image& image, int factor);

// The EPX and HQ filters magnify by 2, 3 or 4 first, then the second filter reaches the exact size.
enum class ResizeFilter : std::uint8_t
{
    Nearest,
    Bilinear,
    EpxNearest,
    EpxBilinear,
    HqNearest,
    HqBilinear,
};

OFS_FORMATS_EXPORT std::string_view resizeFilterName(ResizeFilter filter);

OFS_FORMATS_EXPORT int pixelArtFactor(double scale);

OFS_FORMATS_EXPORT int scaledLength(int length, double scale);

struct ResizeOptions
{
    ResizeFilter filter = ResizeFilter::Nearest;
    // Maps bilinear and HQ results of indexed images back to their palette.
    bool keepPalette = true;
    AdaptOptions adapt;
};

OFS_FORMATS_EXPORT Image resizeImage(const Image& image, int width, int height, const ResizeOptions& options = {});

} // namespace OFS::Formats
