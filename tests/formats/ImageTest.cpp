#include "formats/image/Image.h"
#include "fixtures/FixtureFactory.h"
#include "formats/image/ImageOps.h"

#include <catch2/catch_test_macros.hpp>

using namespace OFS::Formats;
using namespace OFS::Formats::Test;

namespace {

Image indexed(int w, int h, std::initializer_list<std::uint8_t> pixels)
{
    return Image::indexed(w, h, std::vector<std::uint8_t>(pixels), grayscalePalette());
}

} // namespace

TEST_CASE("Image stores pixels and converts to RGBA", "[image]")
{
    Image img = indexed(2, 2, {0, 1, 2, 255});
    CHECK(img.isIndexed());
    CHECK(img.pixelCount() == 4);
    CHECK(img.indexAt(1, 1) == 255);
    CHECK(img.indexAt(5, 5) == 0);
    CHECK(img.maxIndex() == 255);
    CHECK(img.colorAt(0, 0).a == 0);
    CHECK(img.colorAt(0, 0, false).a == 255);
    CHECK(img.colorAt(1, 1) == rgb(255, 255, 255));

    const Image rgbaImage = img.toRgba();
    REQUIRE(rgbaImage.isRgba());
    CHECK(rgbaImage.colorAt(0, 0) == rgb(0, 0, 0, 0));
    CHECK(rgbaImage.colorAt(1, 1) == rgb(255, 255, 255, 255));

    img.setIndex(0, 0, 9);
    CHECK(img.indexAt(0, 0) == 9);

    Image color(1, 1, PixelFormat::Rgba32);
    color.setColor(0, 0, rgb(1, 2, 3, 4));
    CHECK(color.colorAt(0, 0) == rgb(1, 2, 3, 4));

    Image small = Image::indexed(1, 1, {7}, Palette{rgb(9, 9, 9)});
    CHECK(small.colorAt(0, 0) == rgb(0, 0, 0));

    CHECK(Image::indexed(3, 3, {1, 2}).pixels().size() == 9);
    CHECK(Image(-1, 5, PixelFormat::Indexed8).isNull());
}

TEST_CASE("contentBounds and cropToContent trim transparent borders", "[image]")
{
    // . . . .
    // . 3 . .
    // . . 4 .
    const Image img = indexed(4, 3, {0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4, 0});
    const Rect r = contentBounds(img);
    CHECK(r == Rect{1, 1, 2, 2});
    Rect bounds;
    const Image cropped = cropToContent(img, &bounds);
    CHECK(bounds == r);
    CHECK(cropped.width() == 2);
    CHECK(cropped.height() == 2);
    CHECK(cropped.pixelBuffer() == std::vector<std::uint8_t>{3, 0, 0, 4});
    CHECK(cropped.palette() == img.palette());

    const Image empty = cropToContent(indexed(3, 3, {0, 0, 0, 0, 0, 0, 0, 0, 0}), &bounds);
    CHECK(empty.width() == 1);
    CHECK(empty.height() == 1);

    Image rgbaImage(3, 1, PixelFormat::Rgba32);
    rgbaImage.setColor(2, 0, rgb(1, 1, 1, 1));
    CHECK(contentBounds(rgbaImage) == Rect{2, 0, 1, 1});

    CHECK(crop(img, Rect{3, 2, 10, 10}).pixelBuffer() == std::vector<std::uint8_t>{0});
}

TEST_CASE("flips mirror the pixels", "[image]")
{
    const Image img = indexed(3, 2, {1, 2, 3, 4, 5, 6});
    CHECK(flipHorizontal(img).pixelBuffer() == std::vector<std::uint8_t>{3, 2, 1, 6, 5, 4});
    CHECK(flipVertical(img).pixelBuffer() == std::vector<std::uint8_t>{4, 5, 6, 1, 2, 3});
    CHECK(flipHorizontal(flipHorizontal(img)) == img);

    FixtureFactory f(1);
    const Image color = f.rgbaImage(5, 4);
    CHECK(flipVertical(flipVertical(color)) == color);
    CHECK(flipHorizontal(color).colorAt(0, 0) == color.colorAt(4, 0));
}

TEST_CASE("nearest resize samples pixel centers", "[image]")
{
    const Image img = indexed(2, 2, {1, 2, 3, 4});
    const Image big = resizeNearest(img, 4, 4);
    CHECK(big.pixelBuffer() == std::vector<std::uint8_t>{1, 1, 2, 2, 1, 1, 2, 2, 3, 3, 4, 4, 3, 3, 4, 4});
    CHECK(resizeNearest(big, 2, 2) == img);
    CHECK(resizeNearest(img, 0, 3).isNull());
}

TEST_CASE("bilinear resize interpolates with premultiplied alpha", "[image]")
{
    Image img(2, 1, PixelFormat::Rgba32);
    img.setColor(0, 0, rgb(200, 0, 0, 255));
    img.setColor(1, 0, rgb(0, 0, 0, 0));
    const Image out = resizeBilinear(img, 4, 1);
    REQUIRE(out.isRgba());
    CHECK(out.colorAt(0, 0) == rgb(200, 0, 0, 255));
    const Rgba mid = out.colorAt(1, 0);
    CHECK(mid.r == 200);
    CHECK(mid.a > 0);
    CHECK(mid.a < 255);
    CHECK(out.colorAt(3, 0).a == 0);

    const Image same = resizeBilinear(img, 2, 1);
    CHECK(same.colorAt(0, 0) == img.colorAt(0, 0));
    CHECK(resizeBilinear(indexed(1, 1, {5}), 3, 3).isRgba());
}

TEST_CASE("scale2x follows the EPX rules", "[image]")
{
    // A B .
    // B . .
    // . . .
    const Image img = indexed(3, 3, {1, 2, 0, 2, 0, 0, 0, 0, 0});
    const Image out = scale2x(img);
    REQUIRE(out.width() == 6);
    REQUIRE(out.height() == 6);
    // Pixel (1,1) has A=above=2 and C=left=2, so its top-left quarter becomes 2.
    CHECK(out.indexAt(2, 2) == 2);
    CHECK(out.indexAt(3, 2) == 0);
    CHECK(out.indexAt(2, 3) == 0);
    CHECK(out.indexAt(5, 5) == 0);
    CHECK(out.indexAt(0, 0) == 1);

    const Image flat = indexed(2, 2, {7, 7, 7, 7});
    CHECK(scale2x(flat).pixelBuffer() == std::vector<std::uint8_t>(16, 7));
}

TEST_CASE("adaptToPalette maps colors to the nearest palette entry", "[image]")
{
    const Palette palette = {rgb(0, 0, 0), rgb(255, 0, 0), rgb(0, 255, 0), rgb(10, 10, 10)};
    Image img(4, 1, PixelFormat::Rgba32);
    img.setColor(0, 0, rgb(250, 5, 5, 255));
    img.setColor(1, 0, rgb(0, 200, 30, 200));
    img.setColor(2, 0, rgb(0, 0, 0, 255));
    img.setColor(3, 0, rgb(255, 0, 0, 10));
    const Image out = adaptToPalette(img, palette);
    REQUIRE(out.isIndexed());
    CHECK(out.pixelBuffer() == std::vector<std::uint8_t>{1, 2, 3, 0});
    CHECK(out.palette() == palette);

    AdaptOptions options;
    options.allowTransparentIndexForOpaque = true;
    CHECK(adaptToPalette(img, palette, options).indexAt(2, 0) == 0);

    CHECK(nearestColorIndex(palette, rgb(9, 9, 9)) == 3);
    CHECK(nearestColorIndex({}, rgb(9, 9, 9)) == 0);
    CHECK(nearestColorIndex(Palette{rgb(1, 1, 1)}, rgb(9, 9, 9), 0) == 0);
}
