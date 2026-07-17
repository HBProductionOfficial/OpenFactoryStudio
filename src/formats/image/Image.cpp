#include "formats/image/Image.h"

#include <algorithm>

namespace OFS::Formats {

Image::Image(int width, int height, PixelFormat format)
    : m_width(std::max(0, width))
    , m_height(std::max(0, height))
    , m_format(format)
{
    m_pixels.assign(pixelCount() * static_cast<std::size_t>(bytesPerPixel()), 0);
}

Image Image::indexed(int width, int height, std::vector<std::uint8_t> pixels, Palette palette)
{
    Image image;
    image.m_width = std::max(0, width);
    image.m_height = std::max(0, height);
    image.m_format = PixelFormat::Indexed8;
    image.m_pixels = std::move(pixels);
    image.m_pixels.resize(image.pixelCount());
    image.m_palette = std::move(palette);
    return image;
}

Image Image::rgba(int width, int height, std::vector<std::uint8_t> pixels)
{
    Image image;
    image.m_width = std::max(0, width);
    image.m_height = std::max(0, height);
    image.m_format = PixelFormat::Rgba32;
    image.m_pixels = std::move(pixels);
    image.m_pixels.resize(image.pixelCount() * 4);
    return image;
}

std::size_t Image::pixelCount() const noexcept
{
    return static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
}

std::uint8_t Image::indexAt(int x, int y) const noexcept
{
    if (!isIndexed() || !contains(x, y))
        return 0;
    return m_pixels[offset(x, y)];
}

void Image::setIndex(int x, int y, std::uint8_t index) noexcept
{
    if (isIndexed() && contains(x, y))
        m_pixels[offset(x, y)] = index;
}

Rgba Image::colorAt(int x, int y, bool index0Transparent) const noexcept
{
    if (!contains(x, y))
        return rgb(0, 0, 0, 0);
    const std::size_t at = offset(x, y);
    if (isRgba())
        return rgb(m_pixels[at], m_pixels[at + 1], m_pixels[at + 2], m_pixels[at + 3]);
    const std::uint8_t index = m_pixels[at];
    Rgba c = index < m_palette.size() ? m_palette[index] : rgb(0, 0, 0);
    c.a = (index0Transparent && index == 0) ? 0 : 255;
    return c;
}

void Image::setColor(int x, int y, Rgba color) noexcept
{
    if (!isRgba() || !contains(x, y))
        return;
    const std::size_t at = offset(x, y);
    m_pixels[at] = color.r;
    m_pixels[at + 1] = color.g;
    m_pixels[at + 2] = color.b;
    m_pixels[at + 3] = color.a;
}

std::uint8_t Image::maxIndex() const noexcept
{
    if (!isIndexed() || m_pixels.empty())
        return 0;
    return *std::max_element(m_pixels.begin(), m_pixels.end());
}

Image Image::toRgba(bool index0Transparent) const
{
    if (isRgba())
        return *this;
    Image out(m_width, m_height, PixelFormat::Rgba32);
    std::uint8_t* dst = out.m_pixels.data();
    for (const std::uint8_t index : m_pixels) {
        const Rgba c = index < m_palette.size() ? m_palette[index] : rgb(0, 0, 0);
        dst[0] = c.r;
        dst[1] = c.g;
        dst[2] = c.b;
        dst[3] = (index0Transparent && index == 0) ? 0 : 255;
        dst += 4;
    }
    return out;
}

} // namespace OFS::Formats
