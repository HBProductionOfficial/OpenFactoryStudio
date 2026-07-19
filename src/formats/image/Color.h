#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace OFS::Formats {

// SFF v2 palettes keep their stored fourth byte in `a`. MUGEN ignores it, because index 0 is
// always transparent, and so do most helpers here.
struct Rgba
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    friend constexpr bool operator==(const Rgba&, const Rgba&) = default;
};

constexpr Rgba rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
{
    Rgba c;
    c.r = r;
    c.g = g;
    c.b = b;
    c.a = a;
    return c;
}

constexpr bool sameRgb(const Rgba& x, const Rgba& y)
{
    return x.r == y.r && x.g == y.g && x.b == y.b;
}

using Palette = std::vector<Rgba>;

inline bool sameRgb(const Palette& x, const Palette& y)
{
    if (x.size() != y.size())
        return false;
    for (std::size_t i = 0; i < x.size(); ++i) {
        if (!sameRgb(x[i], y[i]))
            return false;
    }
    return true;
}

inline Palette grayscalePalette(std::size_t count = 256)
{
    Palette palette(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto v = static_cast<std::uint8_t>(count > 1 ? (i * 255) / (count - 1) : 0);
        palette[i] = rgb(v, v, v);
    }
    return palette;
}

} // namespace OFS::Formats
