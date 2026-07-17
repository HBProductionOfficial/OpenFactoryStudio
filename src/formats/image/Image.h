#pragma once

#include "formats/FormatsExport.h"
#include "formats/image/Color.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace OFS::Formats {

enum class PixelFormat : std::uint8_t
{
    Indexed8,
    Rgba32,
};

// Row-major pixels with no row padding. The palette of an indexed image can be empty when it is
// not known yet, like for an SFF v1 sprite that reuses the previous palette.
class OFS_FORMATS_EXPORT Image
{
public:
    Image() = default;
    Image(int width, int height, PixelFormat format);

    static Image indexed(int width, int height, std::vector<std::uint8_t> pixels, Palette palette = {});
    static Image rgba(int width, int height, std::vector<std::uint8_t> pixels);

    int width() const noexcept { return m_width; }
    int height() const noexcept { return m_height; }
    PixelFormat format() const noexcept { return m_format; }
    bool isNull() const noexcept { return m_width == 0 || m_height == 0; }
    bool isIndexed() const noexcept { return m_format == PixelFormat::Indexed8; }
    bool isRgba() const noexcept { return m_format == PixelFormat::Rgba32; }
    int bytesPerPixel() const noexcept { return m_format == PixelFormat::Rgba32 ? 4 : 1; }
    std::size_t pixelCount() const noexcept;

    std::span<std::uint8_t> pixels() noexcept { return m_pixels; }
    std::span<const std::uint8_t> pixels() const noexcept { return m_pixels; }
    std::vector<std::uint8_t>& pixelBuffer() noexcept { return m_pixels; }
    const std::vector<std::uint8_t>& pixelBuffer() const noexcept { return m_pixels; }

    const Palette& palette() const noexcept { return m_palette; }
    Palette& palette() noexcept { return m_palette; }
    void setPalette(Palette palette) { m_palette = std::move(palette); }

    bool contains(int x, int y) const noexcept { return x >= 0 && y >= 0 && x < m_width && y < m_height; }

    std::uint8_t indexAt(int x, int y) const noexcept;
    void setIndex(int x, int y, std::uint8_t index) noexcept;

    Rgba colorAt(int x, int y, bool index0Transparent = true) const noexcept;
    void setColor(int x, int y, Rgba color) noexcept;

    std::uint8_t maxIndex() const noexcept;

    Image toRgba(bool index0Transparent = true) const;

    friend bool operator==(const Image&, const Image&) = default;

private:
    std::size_t offset(int x, int y) const noexcept
    {
        return (static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(x)) *
               static_cast<std::size_t>(bytesPerPixel());
    }

    int m_width = 0;
    int m_height = 0;
    PixelFormat m_format = PixelFormat::Indexed8;
    std::vector<std::uint8_t> m_pixels;
    Palette m_palette;
};

} // namespace OFS::Formats
