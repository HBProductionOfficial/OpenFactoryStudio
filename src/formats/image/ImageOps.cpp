#include "formats/image/ImageOps.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace OFS::Formats {

namespace {

std::size_t at(const Image& image, int x, int y)
{
    return (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width()) + static_cast<std::size_t>(x)) *
           static_cast<std::size_t>(image.bytesPerPixel());
}

bool visible(const Image& image, int x, int y, std::uint8_t transparentIndex)
{
    const std::uint8_t* p = image.pixels().data() + at(image, x, y);
    return image.isIndexed() ? p[0] != transparentIndex : p[3] != 0;
}

Image blankLike(const Image& image, int width, int height)
{
    Image out(width, height, image.format());
    out.setPalette(image.palette());
    return out;
}

} // namespace

Rect contentBounds(const Image& image, std::uint8_t transparentIndex)
{
    int minX = image.width();
    int minY = image.height();
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (!visible(image, x, y, transparentIndex))
                continue;
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }
    }
    Rect r;
    if (maxX < 0)
        return r;
    r.x = minX;
    r.y = minY;
    r.width = maxX - minX + 1;
    r.height = maxY - minY + 1;
    return r;
}

Image crop(const Image& image, Rect rect)
{
    const int left = std::clamp(rect.x, 0, image.width());
    const int top = std::clamp(rect.y, 0, image.height());
    const int right = std::clamp(rect.x + std::max(0, rect.width), 0, image.width());
    const int bottom = std::clamp(rect.y + std::max(0, rect.height), 0, image.height());
    Image out = blankLike(image, right - left, bottom - top);
    const auto rowBytes = static_cast<std::size_t>(out.width()) * static_cast<std::size_t>(image.bytesPerPixel());
    for (int y = 0; y < out.height(); ++y) {
        if (rowBytes == 0)
            break;
        std::memcpy(out.pixels().data() + at(out, 0, y), image.pixels().data() + at(image, left, top + y), rowBytes);
    }
    return out;
}

Image cropToContent(const Image& image, Rect* bounds, std::uint8_t transparentIndex)
{
    Rect r = contentBounds(image, transparentIndex);
    if (r.isEmpty()) {
        r.x = 0;
        r.y = 0;
        r.width = std::min(1, image.width());
        r.height = std::min(1, image.height());
        if (bounds)
            *bounds = r;
        Image out = blankLike(image, r.width, r.height);
        return out;
    }
    if (bounds)
        *bounds = r;
    return crop(image, r);
}

Image flipHorizontal(const Image& image)
{
    Image out = blankLike(image, image.width(), image.height());
    const auto bpp = static_cast<std::size_t>(image.bytesPerPixel());
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x)
            std::memcpy(out.pixels().data() + at(out, image.width() - 1 - x, y),
                        image.pixels().data() + at(image, x, y), bpp);
    }
    return out;
}

Image flipVertical(const Image& image)
{
    Image out = blankLike(image, image.width(), image.height());
    const auto rowBytes = static_cast<std::size_t>(image.width()) * static_cast<std::size_t>(image.bytesPerPixel());
    for (int y = 0; y < image.height(); ++y) {
        if (rowBytes == 0)
            break;
        std::memcpy(out.pixels().data() + at(out, 0, image.height() - 1 - y), image.pixels().data() + at(image, 0, y),
                    rowBytes);
    }
    return out;
}

Image resizeNearest(const Image& image, int width, int height)
{
    Image out = blankLike(image, width, height);
    if (image.isNull() || out.isNull())
        return out;
    const auto bpp = static_cast<std::size_t>(image.bytesPerPixel());
    const auto sw = static_cast<std::int64_t>(image.width());
    const auto sh = static_cast<std::int64_t>(image.height());
    const auto dw = static_cast<std::int64_t>(out.width());
    const auto dh = static_cast<std::int64_t>(out.height());
    for (std::int64_t y = 0; y < dh; ++y) {
        const auto sy = static_cast<int>(std::min(sh - 1, ((2 * y + 1) * sh) / (2 * dh)));
        for (std::int64_t x = 0; x < dw; ++x) {
            const auto sx = static_cast<int>(std::min(sw - 1, ((2 * x + 1) * sw) / (2 * dw)));
            std::memcpy(out.pixels().data() + at(out, static_cast<int>(x), static_cast<int>(y)),
                        image.pixels().data() + at(image, sx, sy), bpp);
        }
    }
    return out;
}

Image resizeBilinear(const Image& image, int width, int height)
{
    const Image src = image.toRgba(true);
    Image out(width, height, PixelFormat::Rgba32);
    if (src.isNull() || out.isNull())
        return out;

    std::vector<float> pre(src.pixelCount() * 4);
    for (std::size_t i = 0; i < src.pixelCount(); ++i) {
        const float a = static_cast<float>(src.pixels()[i * 4 + 3]) / 255.0f;
        pre[i * 4 + 0] = static_cast<float>(src.pixels()[i * 4 + 0]) * a;
        pre[i * 4 + 1] = static_cast<float>(src.pixels()[i * 4 + 1]) * a;
        pre[i * 4 + 2] = static_cast<float>(src.pixels()[i * 4 + 2]) * a;
        pre[i * 4 + 3] = a;
    }

    const float scaleX = static_cast<float>(src.width()) / static_cast<float>(out.width());
    const float scaleY = static_cast<float>(src.height()) / static_cast<float>(out.height());
    const int maxX = src.width() - 1;
    const int maxY = src.height() - 1;
    auto texel = [&](int x, int y, int c) {
        return pre[(static_cast<std::size_t>(y) * static_cast<std::size_t>(src.width()) + static_cast<std::size_t>(x)) *
                       4 +
                   static_cast<std::size_t>(c)];
    };

    for (int y = 0; y < out.height(); ++y) {
        const float fy = std::clamp((static_cast<float>(y) + 0.5f) * scaleY - 0.5f, 0.0f, static_cast<float>(maxY));
        const int row0 = static_cast<int>(fy);
        const int row1 = std::min(row0 + 1, maxY);
        const float ty = fy - static_cast<float>(row0);
        for (int x = 0; x < out.width(); ++x) {
            const float fx = std::clamp((static_cast<float>(x) + 0.5f) * scaleX - 0.5f, 0.0f, static_cast<float>(maxX));
            const int col0 = static_cast<int>(fx);
            const int col1 = std::min(col0 + 1, maxX);
            const float tx = fx - static_cast<float>(col0);
            float v[4];
            for (int c = 0; c < 4; ++c) {
                const float top = texel(col0, row0, c) + (texel(col1, row0, c) - texel(col0, row0, c)) * tx;
                const float bottom = texel(col0, row1, c) + (texel(col1, row1, c) - texel(col0, row1, c)) * tx;
                v[c] = top + (bottom - top) * ty;
            }
            Rgba color = rgb(0, 0, 0, 0);
            if (v[3] > 0.0f) {
                auto channel = [](float value) {
                    return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
                };
                color.r = channel(v[0] / v[3]);
                color.g = channel(v[1] / v[3]);
                color.b = channel(v[2] / v[3]);
                color.a = channel(v[3] * 255.0f);
            }
            out.setColor(x, y, color);
        }
    }
    return out;
}

Image scale2x(const Image& image)
{
    Image out = blankLike(image, image.width() * 2, image.height() * 2);
    if (image.isNull())
        return out;
    const auto bpp = static_cast<std::size_t>(image.bytesPerPixel());
    const std::uint8_t* src = image.pixels().data();
    std::uint8_t* dst = out.pixels().data();
    auto px = [&](int x, int y) {
        x = std::clamp(x, 0, image.width() - 1);
        y = std::clamp(y, 0, image.height() - 1);
        return src + at(image, x, y);
    };
    auto same = [bpp](const std::uint8_t* a, const std::uint8_t* b) {
        return std::memcmp(a, b, bpp) == 0;
    };

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const std::uint8_t* p = px(x, y);
            const std::uint8_t* a = px(x, y - 1);
            const std::uint8_t* b = px(x + 1, y);
            const std::uint8_t* c = px(x - 1, y);
            const std::uint8_t* d = px(x, y + 1);
            const std::uint8_t* e0 = (same(c, a) && !same(c, d) && !same(a, b)) ? a : p;
            const std::uint8_t* e1 = (same(a, b) && !same(a, c) && !same(b, d)) ? b : p;
            const std::uint8_t* e2 = (same(d, c) && !same(d, b) && !same(c, a)) ? c : p;
            const std::uint8_t* e3 = (same(b, d) && !same(b, a) && !same(d, c)) ? d : p;
            std::memcpy(dst + at(out, 2 * x, 2 * y), e0, bpp);
            std::memcpy(dst + at(out, 2 * x + 1, 2 * y), e1, bpp);
            std::memcpy(dst + at(out, 2 * x, 2 * y + 1), e2, bpp);
            std::memcpy(dst + at(out, 2 * x + 1, 2 * y + 1), e3, bpp);
        }
    }
    return out;
}

int nearestColorIndex(const Palette& palette, Rgba color, int skipIndex)
{
    int best = -1;
    long bestDistance = 0;
    for (std::size_t i = 0; i < palette.size(); ++i) {
        if (static_cast<int>(i) == skipIndex)
            continue;
        const long dr = static_cast<long>(palette[i].r) - color.r;
        const long dg = static_cast<long>(palette[i].g) - color.g;
        const long db = static_cast<long>(palette[i].b) - color.b;
        const long distance = dr * dr + dg * dg + db * db;
        if (best < 0 || distance < bestDistance) {
            best = static_cast<int>(i);
            bestDistance = distance;
            if (distance == 0)
                break;
        }
    }
    if (best < 0)
        return palette.empty() ? 0 : std::clamp(skipIndex, 0, static_cast<int>(palette.size()) - 1);
    return best;
}

Image adaptToPalette(const Image& image, const Palette& palette, const AdaptOptions& options)
{
    const Palette usable(palette.begin(),
                         palette.begin() + static_cast<std::ptrdiff_t>(std::min<std::size_t>(palette.size(), 256)));
    const Image src = image.toRgba(true);
    Image out(src.width(), src.height(), PixelFormat::Indexed8);
    out.setPalette(palette);
    const int skip = options.allowTransparentIndexForOpaque ? -1 : options.transparentIndex;

    std::unordered_map<std::uint32_t, std::uint8_t> cache;
    const std::uint8_t* s = src.pixels().data();
    std::uint8_t* d = out.pixels().data();
    for (std::size_t i = 0; i < src.pixelCount(); ++i, s += 4) {
        if (s[3] < options.alphaThreshold) {
            d[i] = options.transparentIndex;
            continue;
        }
        const std::uint32_t key = (std::uint32_t{s[0]} << 16) | (std::uint32_t{s[1]} << 8) | s[2];
        auto it = cache.find(key);
        if (it == cache.end()) {
            const int index = nearestColorIndex(usable, rgb(s[0], s[1], s[2]), skip);
            it = cache.emplace(key, static_cast<std::uint8_t>(index)).first;
        }
        d[i] = it->second;
    }
    return out;
}

} // namespace OFS::Formats
