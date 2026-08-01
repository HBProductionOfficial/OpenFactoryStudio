#include "formats/image/Resize.h"
#include "fixtures/FixtureFactory.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>
#include <set>
#include <vector>

using namespace OFS::Formats;
using namespace OFS::Formats::Test;

namespace {

Image indexed(int w, int h, std::vector<std::uint8_t> pixels)
{
    return Image::indexed(w, h, std::move(pixels), grayscalePalette());
}

constexpr ResizeFilter kFilters[] = {ResizeFilter::Nearest,     ResizeFilter::Bilinear,  ResizeFilter::EpxNearest,
                                     ResizeFilter::EpxBilinear, ResizeFilter::HqNearest, ResizeFilter::HqBilinear};

} // namespace

TEST_CASE("Scale3x follows its rules on a known pattern", "[resize][epx]")
{
    // A diagonal edge:
    // 1 1 0
    // 1 0 0
    // 0 0 0
    const Image img = indexed(3, 3, {1, 1, 0, 1, 0, 0, 0, 0, 0});
    const Image out = scale3x(img);
    REQUIRE(out.width() == 9);
    REQUIRE(out.height() == 9);
    REQUIRE(out.isIndexed());
    // The centre pixel has 1 above and to the left: its top-left corner and the edges next to it
    // take that color, the rest stays 0.
    const std::vector<std::uint8_t> centre = {
        out.indexAt(3, 3), out.indexAt(4, 3), out.indexAt(5, 3), out.indexAt(3, 4), out.indexAt(4, 4),
        out.indexAt(5, 4), out.indexAt(3, 5), out.indexAt(4, 5), out.indexAt(5, 5),
    };
    CHECK(centre == std::vector<std::uint8_t>{1, 0, 0, 0, 0, 0, 0, 0, 0});
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x)
            CHECK(out.indexAt(x, y) == 1);
    }
    CHECK(scale3x(indexed(2, 2, {7, 7, 7, 7})).pixelBuffer() == std::vector<std::uint8_t>(36, 7));
}

TEST_CASE("Scale3x reproduces the reference rules everywhere", "[resize][epx]")
{
    FixtureFactory f(0x53C3);
    for (int round = 0; round < 20; ++round) {
        const Image img = f.indexedImage(1 + round % 7, 1 + round % 5, 3, PixelPattern::Noise);
        const Image out = scale3x(img);
        auto p = [&](int x, int y) {
            return img.indexAt(std::clamp(x, 0, img.width() - 1), std::clamp(y, 0, img.height() - 1));
        };
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                const int a = p(x - 1, y - 1), b = p(x, y - 1), c = p(x + 1, y - 1);
                const int d = p(x - 1, y), e = p(x, y), ff = p(x + 1, y);
                const int g = p(x - 1, y + 1), h = p(x, y + 1), i = p(x + 1, y + 1);
                int expected[9] = {e, e, e, e, e, e, e, e, e};
                if (b != h && d != ff) {
                    expected[0] = d == b ? d : e;
                    expected[1] = (d == b && e != c) || (b == ff && e != a) ? b : e;
                    expected[2] = b == ff ? ff : e;
                    expected[3] = (d == b && e != g) || (d == h && e != a) ? d : e;
                    expected[5] = (b == ff && e != i) || (h == ff && e != c) ? ff : e;
                    expected[6] = d == h ? d : e;
                    expected[7] = (d == h && e != i) || (h == ff && e != g) ? h : e;
                    expected[8] = h == ff ? ff : e;
                }
                for (int k = 0; k < 9; ++k)
                    CHECK(out.indexAt(3 * x + k % 3, 3 * y + k / 3) == expected[k]);
            }
        }
    }
}

TEST_CASE("the EPX family scales by 2, 3 and 4", "[resize][epx]")
{
    FixtureFactory f(0xE9);
    const Image img = f.indexedImage(5, 4, 4, PixelPattern::Noise);
    CHECK(scaleEpx(img, 1) == img);
    CHECK(scaleEpx(img, 2) == scale2x(img));
    CHECK(scaleEpx(img, 3) == scale3x(img));
    CHECK(scaleEpx(img, 4) == scale2x(scale2x(img)));
    CHECK(scaleEpx(img, 9).width() == 20);
    CHECK(scaleEpx(img, 4).palette() == img.palette());

    Image rgba(2, 2, PixelFormat::Rgba32);
    rgba.setColor(0, 0, rgb(9, 9, 9, 255));
    rgba.setColor(1, 0, rgb(1, 2, 3, 0));
    rgba.setColor(0, 1, rgb(4, 5, 6, 0));
    rgba.setColor(1, 1, rgb(7, 8, 9, 0));
    const Image out = scaleEpx(rgba, 2);
    REQUIRE(out.isRgba());
    // The transparent pixels right of and below the opaque one count as the same color, so its
    // inner corner is cut off. With their stored colors compared it would stay opaque.
    CHECK(out.colorAt(0, 0) == rgb(9, 9, 9, 255));
    CHECK(out.colorAt(1, 0) == rgb(9, 9, 9, 255));
    CHECK(out.colorAt(1, 1) == rgb(0, 0, 0, 0));
    CHECK(scale2x(rgba).colorAt(1, 1) == rgb(9, 9, 9, 255));
}

TEST_CASE("hqx magnifies by 2, 3 and 4", "[resize][hqx]")
{
    FixtureFactory f(0x4A);
    const Image img = f.indexedImage(6, 5, 8, PixelPattern::Noise);
    for (int factor : {2, 3, 4}) {
        const Image out = scaleHq(img, factor);
        REQUIRE(out.isRgba());
        CHECK(out.width() == 6 * factor);
        CHECK(out.height() == 5 * factor);
    }
    CHECK(scaleHq(img, 1) == img.toRgba());
    CHECK(scaleHq(img, 7).width() == 24);

    Image flat(3, 3, PixelFormat::Rgba32);
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x)
            flat.setColor(x, y, rgb(10, 200, 30));
    }
    const Image flatOut = scaleHq(flat, 3);
    for (int y = 0; y < 9; ++y) {
        for (int x = 0; x < 9; ++x)
            CHECK(flatOut.colorAt(x, y) == rgb(10, 200, 30));
    }

    Image diagonal(4, 4, PixelFormat::Rgba32);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x)
            diagonal.setColor(x, y, x > y ? rgb(255, 255, 255) : rgb(0, 0, 0));
    }
    const Image smooth = scaleHq(diagonal, 4);
    bool blended = false;
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            const Rgba c = smooth.colorAt(x, y);
            CHECK(c.a == 255);
            CHECK(c.r == c.g);
            blended = blended || (c.r != 0 && c.r != 255);
        }
    }
    CHECK(blended);
    CHECK(scaleHq(flipHorizontal(diagonal), 2) == flipHorizontal(scaleHq(diagonal, 2)));
}

TEST_CASE("hqx never bleeds the color of transparent pixels", "[resize][hqx]")
{
    Image img(7, 7, PixelFormat::Rgba32);
    for (int y = 0; y < 7; ++y) {
        for (int x = 0; x < 7; ++x) {
            const bool inside = std::abs(x - 3) + std::abs(y - 3) <= 2;
            img.setColor(x, y, inside ? rgb(220, 10, 10, 255) : rgb(0, 255, 0, 0));
        }
    }
    for (int factor : {2, 3, 4}) {
        const Image out = scaleHq(img, factor);
        bool partial = false;
        for (int y = 0; y < out.height(); ++y) {
            for (int x = 0; x < out.width(); ++x) {
                const Rgba c = out.colorAt(x, y);
                if (c.a == 0)
                    continue;
                INFO("factor " << factor << " at " << x << "," << y);
                CHECK(c.r == 220);
                CHECK(c.g == 10);
                CHECK(c.b == 10);
                partial = partial || c.a != 255;
            }
        }
        CHECK(partial);
    }

    const Palette palette = {rgb(255, 0, 255), rgb(0, 0, 0), rgb(255, 255, 255)};
    const Image sprite = Image::indexed(3, 3, {0, 1, 0, 1, 2, 1, 0, 1, 0}, palette);
    const Image out = scaleHq(sprite, 2);
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 6; ++x) {
            const Rgba c = out.colorAt(x, y);
            if (c.a != 0)
                CHECK(c.g == c.r);
        }
    }
}

TEST_CASE("the Resize dialog's filters produce the requested size", "[resize]")
{
    CHECK(pixelArtFactor(0.5) == 1);
    CHECK(pixelArtFactor(1.0) == 1);
    CHECK(pixelArtFactor(1.01) == 2);
    CHECK(pixelArtFactor(2.0) == 2);
    CHECK(pixelArtFactor(2.5) == 3);
    CHECK(pixelArtFactor(3.0) == 3);
    CHECK(pixelArtFactor(3.5) == 4);
    CHECK(pixelArtFactor(10.0) == 4);
    CHECK(scaledLength(10, 1.5) == 15);
    CHECK(scaledLength(3, 0.1) == 1);
    CHECK(scaledLength(7, 0.5) == 4);

    std::set<std::string_view> names;
    for (ResizeFilter filter : kFilters)
        names.insert(resizeFilterName(filter));
    CHECK(names.size() == 6);
    CHECK(resizeFilterName(ResizeFilter::EpxBilinear) == "EPX + Bilinear");

    FixtureFactory f(0x512E);
    const Image sprite = f.indexedImage(9, 7, 40, PixelPattern::Runs);
    const struct
    {
        int w, h;
    } sizes[] = {{9, 7}, {18, 14}, {13, 10}, {27, 21}, {40, 3}, {4, 3}, {1, 1}, {50, 50}};
    for (ResizeFilter filter : kFilters) {
        for (const auto& size : sizes) {
            INFO(resizeFilterName(filter) << " to " << size.w << "x" << size.h);
            ResizeOptions options;
            options.filter = filter;
            const Image out = resizeImage(sprite, size.w, size.h, options);
            CHECK(out.width() == size.w);
            CHECK(out.height() == size.h);
            CHECK(out.isIndexed());
            CHECK(out.palette() == sprite.palette());

            // Without keepPalette, results computed in RGBA stay RGBA. Nearest neighbour and EPX
            // (and any filter whose pixel-art step already gives the exact size) stay indexed.
            options.keepPalette = false;
            const Image raw = resizeImage(sprite, size.w, size.h, options);
            if (filter == ResizeFilter::Nearest || filter == ResizeFilter::EpxNearest)
                CHECK(raw.isIndexed());
            if (raw.isIndexed())
                CHECK(raw.palette() == sprite.palette());
            const bool resampled = size.w == 13 || size.w == 40 || size.w == 50;
            if (resampled && (filter == ResizeFilter::Bilinear || filter == ResizeFilter::EpxBilinear ||
                              filter == ResizeFilter::HqNearest || filter == ResizeFilter::HqBilinear))
                CHECK(raw.isRgba());
        }
    }

    ResizeOptions epx;
    epx.filter = ResizeFilter::EpxNearest;
    CHECK(resizeImage(sprite, 27, 21, epx) == scale3x(sprite));
    ResizeOptions hq;
    hq.filter = ResizeFilter::HqBilinear;
    hq.keepPalette = false;
    CHECK(resizeImage(sprite, 36, 28, hq) == scaleHq(sprite, 4));
    CHECK(resizeImage(sprite, 18, 14).pixelBuffer() == resizeNearest(sprite, 18, 14).pixelBuffer());
    CHECK(resizeImage(sprite, 0, 5).isNull());
}

TEST_CASE("every filter survives tiny and large images", "[resize]")
{
    FixtureFactory f(0xB16);
    const Image one = Image::indexed(1, 1, {3}, f.palette(8));
    const Image column = f.indexedImage(1, 300, 16, PixelPattern::Runs);
    Image big = f.rgbaImage(320, 240);
    for (ResizeFilter filter : kFilters) {
        INFO(resizeFilterName(filter));
        ResizeOptions options;
        options.filter = filter;
        CHECK(resizeImage(one, 4, 4, options).width() == 4);
        CHECK(resizeImage(one, 1, 1, options).width() == 1);
        CHECK(resizeImage(column, 3, 900, options).height() == 900);
        CHECK(resizeImage(column, 1, 100, options).height() == 100);
        const Image grown = resizeImage(big, 1280, 960, options);
        CHECK(grown.width() == 1280);
        CHECK(grown.isRgba());
        CHECK(resizeImage(big, 100, 50, options).height() == 50);
    }
    for (int factor : {2, 3, 4}) {
        CHECK(scaleHq(one, factor).width() == factor);
        CHECK(scaleEpx(one, factor).width() == factor);
        CHECK(scaleHq(column, factor).height() == 300 * factor);
    }
}
