#pragma once

#include "formats/FormatsExport.h"
#include "formats/image/Image.h"

#include <cstdint>

namespace OFS::Formats {

struct Rect
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool isEmpty() const noexcept { return width <= 0 || height <= 0; }
    friend bool operator==(const Rect&, const Rect&) = default;
};

OFS_FORMATS_EXPORT Rect contentBounds(const Image& image, std::uint8_t transparentIndex = 0);

OFS_FORMATS_EXPORT Image crop(const Image& image, Rect rect);

// Callers shift the sprite axis by (-bounds.x, -bounds.y). A fully transparent image becomes 1x1
// because sprites can't be empty.
OFS_FORMATS_EXPORT Image cropToContent(const Image& image, Rect* bounds = nullptr, std::uint8_t transparentIndex = 0);

OFS_FORMATS_EXPORT Image flipHorizontal(const Image& image);
OFS_FORMATS_EXPORT Image flipVertical(const Image& image);

OFS_FORMATS_EXPORT Image resizeNearest(const Image& image, int width, int height);

// Always gives RGBA. Alpha is premultiplied so transparent pixels don't darken the edges.
OFS_FORMATS_EXPORT Image resizeBilinear(const Image& image, int width, int height);

OFS_FORMATS_EXPORT Image scale2x(const Image& image);

struct AdaptOptions
{
    std::uint8_t alphaThreshold = 128;
    std::uint8_t transparentIndex = 0;
    // Off by default because MUGEN always draws that index as transparent.
    bool allowTransparentIndexForOpaque = false;
};

OFS_FORMATS_EXPORT int nearestColorIndex(const Palette& palette, Rgba color, int skipIndex = -1);

OFS_FORMATS_EXPORT Image adaptToPalette(const Image& image, const Palette& palette, const AdaptOptions& options = {});

} // namespace OFS::Formats
