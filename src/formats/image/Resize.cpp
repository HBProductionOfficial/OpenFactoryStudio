#include "formats/image/Resize.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

// hqx is linked statically, so stop it from declaring DLL imports on Windows.
#if defined(_WIN32)
#  define DLL_EXPORT
#endif
extern "C" {
#include <hqx.h>
}
#if defined(_WIN32)
#  undef DLL_EXPORT
#endif

// hqx's lookup table. We fill only the colors in use instead of calling hqxInit(), so the
// untouched pages of the 64 MB table never use memory.
extern "C" std::uint32_t RGBtoYUV[16777216];

namespace OFS::Formats {

namespace {

std::size_t at(const Image& image, int x, int y)
{
    return (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width()) + static_cast<std::size_t>(x)) *
           static_cast<std::size_t>(image.bytesPerPixel());
}

// Sets fully transparent pixels to 0,0,0,0 so exact compares see them as one color.
Image normalizeTransparent(const Image& image)
{
    if (!image.isRgba())
        return image;
    Image out = image;
    std::uint8_t* p = out.pixels().data();
    for (std::size_t i = 0; i < out.pixelCount(); ++i, p += 4) {
        if (p[3] == 0)
            std::memset(p, 0, 4);
    }
    return out;
}

Image scale3xExact(const Image& image)
{
    Image out(image.width() * 3, image.height() * 3, image.format());
    out.setPalette(image.palette());
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
            const std::uint8_t* a = px(x - 1, y - 1);
            const std::uint8_t* b = px(x, y - 1);
            const std::uint8_t* c = px(x + 1, y - 1);
            const std::uint8_t* d = px(x - 1, y);
            const std::uint8_t* e = px(x, y);
            const std::uint8_t* f = px(x + 1, y);
            const std::uint8_t* g = px(x - 1, y + 1);
            const std::uint8_t* h = px(x, y + 1);
            const std::uint8_t* i = px(x + 1, y + 1);
            std::array<const std::uint8_t*, 9> o;
            o.fill(e);
            if (!same(b, h) && !same(d, f)) {
                const bool db = same(d, b);
                const bool bf = same(b, f);
                const bool dh = same(d, h);
                const bool hf = same(h, f);
                o[0] = db ? d : e;
                o[1] = (db && !same(e, c)) || (bf && !same(e, a)) ? b : e;
                o[2] = bf ? f : e;
                o[3] = (db && !same(e, g)) || (dh && !same(e, a)) ? d : e;
                o[5] = (bf && !same(e, i)) || (hf && !same(e, c)) ? f : e;
                o[6] = dh ? d : e;
                o[7] = (dh && !same(e, i)) || (hf && !same(e, g)) ? h : e;
                o[8] = hf ? f : e;
            }
            for (int k = 0; k < 9; ++k)
                std::memcpy(dst + at(out, 3 * x + k % 3, 3 * y + k / 3), o[static_cast<std::size_t>(k)], bpp);
        }
    }
    return out;
}

// --- hqx -------------------------------------------------------------------------------------

std::mutex& hqxMutex()
{
    static std::mutex mutex;
    return mutex;
}

// Same value hqxInit() computes.
std::uint32_t yuvOf(std::uint32_t rgb24)
{
    const double r = (rgb24 >> 16) & 0xFF;
    const double g = (rgb24 >> 8) & 0xFF;
    const double b = rgb24 & 0xFF;
    auto wrap = [](double value) {
        return static_cast<std::uint32_t>(static_cast<std::int64_t>(value));
    };
    const std::uint32_t y = wrap(0.299 * r + 0.587 * g + 0.114 * b);
    const std::uint32_t u = wrap(-0.169 * r - 0.331 * g + 0.5 * b) + 128;
    const std::uint32_t v = wrap(0.5 * r - 0.419 * g - 0.081 * b) + 128;
    return (y << 16) + (u << 8) + v;
}

// Same thresholds as hqx's Diff().
bool yuvDifferent(std::uint32_t yuv1, std::uint32_t yuv2)
{
    auto distance = [&](std::uint32_t mask) {
        return std::abs(static_cast<long long>(yuv1 & mask) - static_cast<long long>(yuv2 & mask));
    };
    return distance(0x00FF0000) > 0x00300000 || distance(0x0000FF00) > 0x00000700 || distance(0x000000FF) > 0x00000006;
}

// A color for transparent pixels that hqx sees as unlike as many visible colors as possible.
std::uint32_t backgroundKey(std::vector<std::uint32_t> visible)
{
    std::sort(visible.begin(), visible.end());
    visible.erase(std::unique(visible.begin(), visible.end()), visible.end());
    static constexpr std::uint32_t kCandidates[] = {0xFF00FF, 0x00FF00, 0x00FFFF, 0xFFFF00, 0x0000FF,
                                                    0xFF0000, 0x000000, 0xFFFFFF, 0x808080};
    std::uint32_t best = kCandidates[0];
    std::size_t bestClashes = static_cast<std::size_t>(-1);
    for (const std::uint32_t candidate : kCandidates) {
        const std::uint32_t key = yuvOf(candidate);
        std::size_t clashes = 0;
        for (const std::uint32_t c : visible) {
            if (!yuvDifferent(key, yuvOf(c)))
                ++clashes;
        }
        if (clashes < bestClashes) {
            best = candidate;
            bestClashes = clashes;
            if (clashes == 0)
                break;
        }
    }
    return best;
}

void runHqx(int factor, const std::vector<std::uint32_t>& in, std::vector<std::uint32_t>& out, int width, int height)
{
    switch (factor) {
    case 2:
        hq2x_32(in.data(), out.data(), width, height);
        break;
    case 3:
        hq3x_32(in.data(), out.data(), width, height);
        break;
    default:
        hq4x_32(in.data(), out.data(), width, height);
        break;
    }
}

std::uint8_t premultiply(std::uint8_t value, std::uint8_t alpha)
{
    return static_cast<std::uint8_t>((unsigned{value} * alpha + 127) / 255);
}

} // namespace

Image scale3x(const Image& image)
{
    return scale3xExact(normalizeTransparent(image));
}

Image scaleEpx(const Image& image, int factor)
{
    if (factor < 2)
        return image;
    const Image src = normalizeTransparent(image);
    if (factor == 2)
        return scale2x(src);
    if (factor == 3)
        return scale3xExact(src);
    return scale2x(scale2x(src));
}

Image scaleHq(const Image& image, int factor)
{
    const Image src = image.toRgba(true);
    if (factor < 2)
        return src;
    factor = std::min(factor, 4);
    const int width = src.width();
    const int height = src.height();
    Image result(width * factor, height * factor, PixelFormat::Rgba32);
    if (src.isNull())
        return result;

    const std::size_t count = src.pixelCount();
    const std::uint8_t* p = src.pixels().data();
    bool transparent = false;
    std::vector<std::uint32_t> visible;
    visible.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint8_t* px = p + i * 4;
        if (px[3] == 0)
            transparent = true;
        else
            visible.push_back((std::uint32_t{px[0]} << 16) | (std::uint32_t{px[1]} << 8) | px[2]);
        if (px[3] != 255)
            transparent = true;
    }
    const std::uint32_t key = transparent ? backgroundKey(visible) : 0;
    std::vector<std::uint32_t> base(count);
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint8_t* px = p + i * 4;
        base[i] = px[3] == 0 ? key : (std::uint32_t{px[0]} << 16) | (std::uint32_t{px[1]} << 8) | px[2];
    }

    const std::size_t outCount = result.pixelCount();
    std::vector<std::uint32_t> in(count);
    std::vector<std::uint32_t> out(outCount);
    std::uint8_t* dst = result.pixels().data();

    const std::lock_guard<std::mutex> lock(hqxMutex());
    for (const std::uint32_t c : base)
        RGBtoYUV[c & 0xFFFFFF] = yuvOf(c & 0xFFFFFF);

    if (!transparent) {
        for (std::size_t i = 0; i < count; ++i)
            in[i] = base[i] | 0xFF000000u;
        runHqx(factor, in, out, width, height);
        for (std::size_t i = 0; i < outCount; ++i, dst += 4) {
            dst[0] = static_cast<std::uint8_t>(out[i] >> 16);
            dst[1] = static_cast<std::uint8_t>(out[i] >> 8);
            dst[2] = static_cast<std::uint8_t>(out[i]);
            dst[3] = 255;
        }
        return result;
    }

    // hqx picks edges from RGB only, so the alpha byte can carry other values through the same
    // blend. Its weights are multiples of 1/16, so a nibble stored as value * 16 comes out exact.
    // Two runs per byte (high and low nibble) give the exact weighted sum.
    std::vector<std::uint16_t> sum(outCount);
    auto weighted = [&](auto value) {
        for (int nibble = 1; nibble >= 0; --nibble) {
            for (std::size_t i = 0; i < count; ++i) {
                const unsigned v = (value(i) >> (4 * nibble)) & 0x0F;
                in[i] = base[i] | (std::uint32_t{v} << 28);
            }
            runHqx(factor, in, out, width, height);
            for (std::size_t i = 0; i < outCount; ++i) {
                const auto part = static_cast<std::uint16_t>(out[i] >> 24);
                sum[i] = static_cast<std::uint16_t>(nibble == 1 ? part * 16 : sum[i] + part);
            }
        }
    };

    // Alpha first, then the colors premultiplied by alpha so transparent pixels add nothing.
    weighted([&](std::size_t i) { return unsigned{p[i * 4 + 3]}; });
    const std::vector<std::uint16_t> alphaSum = sum;
    for (std::size_t i = 0; i < outCount; ++i)
        dst[i * 4 + 3] = static_cast<std::uint8_t>((alphaSum[i] + 8) / 16);
    for (std::size_t c = 0; c < 3; ++c) {
        weighted([&](std::size_t i) { return unsigned{premultiply(p[i * 4 + c], p[i * 4 + 3])}; });
        for (std::size_t i = 0; i < outCount; ++i) {
            const unsigned alpha = alphaSum[i];
            dst[i * 4 + c] = alpha == 0
                                 ? std::uint8_t{0}
                                 : static_cast<std::uint8_t>(std::min(255u, (sum[i] * 255u + alpha / 2) / alpha));
        }
    }
    return result;
}

std::string_view resizeFilterName(ResizeFilter filter)
{
    switch (filter) {
    case ResizeFilter::Nearest:
        return "Nearest Neighbor";
    case ResizeFilter::Bilinear:
        return "Bilinear";
    case ResizeFilter::EpxNearest:
        return "EPX + Nearest neighbor";
    case ResizeFilter::EpxBilinear:
        return "EPX + Bilinear";
    case ResizeFilter::HqNearest:
        return "HQ + Nearest neighbor";
    case ResizeFilter::HqBilinear:
        return "HQ + Bilinear";
    }
    return {};
}

int pixelArtFactor(double scale)
{
    if (!(scale > 1.0))
        return 1;
    if (scale <= 2.0)
        return 2;
    if (scale <= 3.0)
        return 3;
    return 4;
}

int scaledLength(int length, double scale)
{
    const double value = std::round(static_cast<double>(length) * scale);
    if (!(value >= 1.0))
        return 1;
    return value > 1.0e9 ? 1000000000 : static_cast<int>(value);
}

Image resizeImage(const Image& image, int width, int height, const ResizeOptions& options)
{
    if (image.isNull() || width <= 0 || height <= 0) {
        Image empty(width, height, image.format());
        empty.setPalette(image.palette());
        return empty;
    }
    const double scale =
        std::max(static_cast<double>(width) / image.width(), static_cast<double>(height) / image.height());
    const int factor = pixelArtFactor(scale);
    const bool bilinear = options.filter == ResizeFilter::Bilinear || options.filter == ResizeFilter::EpxBilinear ||
                          options.filter == ResizeFilter::HqBilinear;

    Image magnified;
    switch (options.filter) {
    case ResizeFilter::Nearest:
    case ResizeFilter::Bilinear:
        magnified = image;
        break;
    case ResizeFilter::EpxNearest:
    case ResizeFilter::EpxBilinear:
        magnified = scaleEpx(image, factor);
        break;
    case ResizeFilter::HqNearest:
    case ResizeFilter::HqBilinear:
        magnified = factor > 1 ? scaleHq(image, factor) : image;
        break;
    }

    Image out;
    if (magnified.width() == width && magnified.height() == height)
        out = std::move(magnified);
    else if (bilinear)
        out = resizeBilinear(magnified, width, height);
    else
        out = resizeNearest(magnified, width, height);

    if (image.isIndexed() && out.isRgba() && options.keepPalette)
        return adaptToPalette(out, image.palette(), options.adapt);
    return out;
}

} // namespace OFS::Formats
