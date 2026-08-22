#include "core/sprites/ImageDocument.h"

#include <QTest>

using namespace OFS;
using namespace OFS::Formats;
using namespace OFS::ImageEdits;

namespace {

// 12 x 10: index 1 (red) block x1..5, 2 (green) block x6..10 on rows 1..8, border 0 (transparent).
Image picture()
{
    Palette colors = grayscalePalette(256);
    colors[1] = {255, 0, 0, 255};
    colors[2] = {0, 200, 0, 255};
    colors[3] = {0, 0, 255, 255};
    colors[4] = {255, 255, 255, 255};
    std::vector<std::uint8_t> pixels(120, 0);
    for (int y = 1; y <= 8; ++y)
        for (int x = 1; x <= 10; ++x)
            pixels[std::size_t(y * 12 + x)] = x <= 5 ? 1 : 2;
    return Image::indexed(12, 10, pixels, colors);
}

PaintColor index(int i)
{
    PaintColor c;
    c.index = i;
    return c;
}

PaintColor rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255)
{
    PaintColor c;
    c.rgba = {r, g, b, a};
    return c;
}

int count(const ImageDocument& doc, int value)
{
    int n = 0;
    for (int y = 0; y < doc.height; ++y)
        for (int x = 0; x < doc.width; ++x)
            n += doc.pixel(x, y).index == value;
    return n;
}

} // namespace

class ImageDocumentTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void painting()
    {
        ImageDocument doc = ImageDocument::fromImage(picture(), "9100,0");
        QCOMPARE(doc.layers.size(), 1);
        QCOMPARE(doc.layers[0].name, QStringLiteral("9100,0"));
        QVERIFY(paint(doc, {5, 4}, {5, 4}, 5, index(3)));
        QCOMPARE(doc.pixel(5, 4).index, 3);
        QCOMPARE(doc.pixel(9, 8).index, 3);
        QCOMPARE(doc.pixel(4, 4).index, 1);
        QCOMPARE(doc.pixel(10, 4).index, 2);
        QVERIFY(!paint(doc, {5, 4}, {5, 4}, 1, index(3)));
        paint(doc, {0, 0}, {11, 0}, 1, index(4));
        for (int x = 0; x < 12; ++x)
            QCOMPARE(doc.pixel(x, 0).index, 4);
        paint(doc, {0, 0}, {0, 0}, 1, index(0));
        QCOMPARE(doc.pixel(0, 0).index, 0);
        QVERIFY(remap(doc, {1, 1}, {1, 1}, 12, {{index(1), index(4)}, {index(2), index(3)}, {index(1), index(2)}}));
        QCOMPARE(doc.pixel(1, 1).index, 2);
        QCOMPARE(doc.pixel(10, 1).index, 3);
    }

    void fillingAndPicking()
    {
        ImageDocument doc = ImageDocument::fromImage(picture(), "p");
        QVERIFY(fill(doc, {2, 2}, index(3), true));
        QCOMPARE(count(doc, 3), 40);
        QCOMPARE(count(doc, 2), 40);
        QVERIFY(fill(doc, {0, 0}, index(4), true));
        QCOMPARE(count(doc, 0), 0);
        paint(doc, {2, 2}, {2, 2}, 1, index(2));
        QVERIFY(fill(doc, {8, 2}, index(1), false));
        QCOMPARE(doc.pixel(2, 2).index, 1);
        QCOMPARE(pick(doc, {0, 0}).index, 4);

        ImageDocument rgbaDoc = ImageDocument::fromImage(Image::rgba(1, 1, {10, 20, 30, 128}), "p");
        QCOMPARE(pick(rgbaDoc, {0, 0}).rgba.a, std::uint8_t(255));
    }

    void selecting()
    {
        ImageDocument doc = ImageDocument::fromImage(picture(), "p");
        QVERIFY(!doc.hasSelection());
        QVERIFY(doc.editable(0, 0));
        selectColor(doc, {2, 2}, true, SelectMode::Replace);
        QVERIFY(doc.editable(2, 2));
        QVERIFY(!doc.editable(8, 2));
        selectColor(doc, {8, 2}, true, SelectMode::Add);
        QVERIFY(doc.editable(8, 2));
        selectColor(doc, {2, 2}, true, SelectMode::Subtract);
        QVERIFY(!doc.editable(2, 2));
        selectColor(doc, {0, 0}, true, SelectMode::AllExcept);
        QVERIFY(doc.editable(2, 2) && !doc.editable(0, 0));
        fill(doc, {0, 0}, index(4), true);
        QCOMPARE(count(doc, 4), 0);
        paint(doc, {0, 0}, {11, 9}, 12, index(3));
        QCOMPARE(count(doc, 3), 80);

        doc = ImageDocument::fromImage(picture(), "p");
        selectRect(doc, QRect(1, 5, 3, 2));
        paint(doc, {0, 0}, {0, 0}, 12, index(4));
        QCOMPARE(count(doc, 4), 6);
        selectRect(doc, QRect(4, 4, 0, 0));
        QVERIFY(!doc.hasSelection());
        selectNone(doc);
        QVERIFY(!paint(doc, {0, 0}, {0, 0}, 12, index(3)));
        invertSelection(doc);
        QVERIFY(doc.editable(5, 5));
        selectAll(doc);
        invertSelection(doc);
        QVERIFY(!doc.editable(5, 5));
        selectAll(doc);
        paintSelection(doc, {0, 0}, {0, 0}, 2, true);
        QVERIFY(doc.editable(1, 1) && !doc.editable(2, 2));
        paintSelection(doc, {1, 1}, {1, 1}, 1, false);
        QVERIFY(!doc.editable(1, 1) && doc.editable(0, 0));
    }

    void movingSelections()
    {
        ImageDocument doc = ImageDocument::fromImage(picture(), "p");
        selectRect(doc, QRect(1, 1, 2, 2));
        QVERIFY(moveSelection(doc, 3, 0, index(0)));
        QCOMPARE(doc.pixel(1, 1).index, 0);
        QCOMPARE(doc.pixel(4, 1).index, 1);
        QVERIFY(doc.editable(4, 1) && !doc.editable(1, 1));
        QVERIFY(moveSelection(doc, 0, 1, index(3)));
        QCOMPARE(count(doc, 3), 2);
    }

    void layers()
    {
        ImageDocument doc = ImageDocument::fromImage(picture(), "9100,0");
        addLayer(doc);
        QCOMPARE(doc.layers.size(), 2);
        QCOMPARE(doc.layers[1].name, QStringLiteral("Layer 2"));
        QCOMPARE(doc.current, 1);
        paint(doc, {0, 0}, {0, 0}, 1, index(3));
        duplicateLayer(doc);
        QCOMPARE(doc.layers[2].name, QStringLiteral("Layer 2 copy"));
        QCOMPARE(doc.current, 2);
        doc.current = 1;
        deleteLayer(doc);
        QCOMPARE(doc.layers.size(), 2);
        QCOMPARE(doc.current, 1);
        addLayer(doc);
        doc.layers[0].visible = false;
        mergeVisible(doc);
        QCOMPARE(doc.layers.size(), 2);
        QVERIFY(!doc.layers[0].visible);
        QCOMPARE(doc.layers[1].name, QStringLiteral("Layer 2 copy"));
        QCOMPARE(doc.current, 1);
        mergeAll(doc);
        QCOMPARE(doc.layers.size(), 1);
        QCOMPARE(doc.layers[0].name, QStringLiteral("9100,0"));
        QCOMPARE(doc.pixel(0, 0).index, 3);
        QCOMPARE(doc.pixel(2, 2).index, 0);
        deleteLayer(doc);
        QCOMPARE(doc.layers.size(), 1);
    }

    void canvas()
    {
        ImageDocument doc = ImageDocument::fromImage(picture(), "p");
        resizeCanvas(doc, 14, 13, index(4));
        QCOMPARE(doc.width, 14);
        QCOMPARE(doc.origin, QPoint(1, 1));
        QCOMPARE(doc.pixel(2, 2).index, 1);
        QCOMPARE(doc.pixel(0, 0).index, 4);
        resizeCanvas(doc, 9, 7, index(0));
        QCOMPARE(doc.origin, QPoint(-2, -2));
        setIndexed(doc, false);
        QVERIFY(!doc.indexed);
        QCOMPARE(doc.layers[0].image.colorAt(8, 6, false).a, std::uint8_t(255));
        doc = ImageDocument::fromImage(picture(), "p");
        setIndexed(doc, false);
        setIndexed(doc, true);
        QCOMPARE(doc.pixel(0, 0).index, 0);
        QCOMPARE(doc.pixel(2, 2).index, 1);

        doc = ImageDocument::fromImage(picture(), "p");
        paint(doc, {0, 0}, {0, 0}, 1, index(3));
        rotate(doc, 90, RotateFilter::Nearest, index(0));
        QCOMPARE(doc.width, 10);
        QCOMPARE(doc.height, 12);
        QCOMPARE(doc.pixel(9, 0).index, 3);
        rotate(doc, -90, RotateFilter::Nearest, index(0));
        QVERIFY(doc.layers[0].image.pixelBuffer() == [] {
            ImageDocument d = ImageDocument::fromImage(picture(), "p");
            paint(d, {0, 0}, {0, 0}, 1, index(3));
            return d.layers[0].image.pixelBuffer();
        }());
        rotate(doc, 45, RotateFilter::Nearest, index(0));
        QCOMPARE(doc.width, 16);
        QCOMPARE(doc.height, 16);
        doc = ImageDocument::fromImage(picture(), "p");
        addLayer(doc);
        paint(doc, {0, 0}, {0, 0}, 1, index(3));
        flip(doc, true);
        QCOMPARE(doc.pixel(11, 0).index, 3);
        doc.current = 0;
        QCOMPARE(doc.pixel(1, 1).index, 2);
    }

    void flattening()
    {
        ImageDocument doc = ImageDocument::fromImage(picture(), "p");
        addLayer(doc);
        paint(doc, {1, 1}, {1, 1}, 1, index(3));
        Image flat = flatten(doc);
        QCOMPARE(flat.indexAt(1, 1), std::uint8_t(3));
        QCOMPARE(flat.indexAt(2, 2), std::uint8_t(1));
        doc.layers[1].visible = false;
        QCOMPARE(flatten(doc).indexAt(1, 1), std::uint8_t(1));

        // Expected values from Fighter Factory.
        ImageDocument red = ImageDocument::fromImage(Image::rgba(1, 1, {255, 0, 0, 255}), "r");
        addLayer(red);
        paint(red, {0, 0}, {0, 0}, 1, rgba(0, 0, 255));
        red.layers[1].blend = 1; // Add
        QCOMPARE(flatten(red).colorAt(0, 0).g, std::uint8_t(0));
        QCOMPARE(flatten(red).colorAt(0, 0).r, std::uint8_t(255));
        QCOMPARE(flatten(red).colorAt(0, 0).b, std::uint8_t(255));
        red.layers[1].blend = 2; // Multiply
        QCOMPARE(flatten(red).colorAt(0, 0).r, std::uint8_t(0));
        red.layers[1].blend = 0;
        red.layers[1].opacity = 50;
        const Rgba half = flatten(red).colorAt(0, 0);
        QVERIFY(std::abs(int(half.r) - 128) <= 1 && std::abs(int(half.b) - 127) <= 1);
    }

    void explorerLayersAndClipboard()
    {
        ImageDocument doc = ImageDocument::fromImage(Image::rgba(2, 2, std::vector<std::uint8_t>(16, 255)), "9100,1");
        addLayers(doc, {{"9100,0", Image::rgba(4, 3, std::vector<std::uint8_t>(48, 128)), QPoint(3, 2)}}, true,
                  QPoint(0, 0), rgba(0, 0, 0, 0));
        QCOMPARE(doc.layers.size(), 2);
        QCOMPARE(doc.layers[0].name, QStringLiteral("9100,0"));
        QCOMPARE(doc.current, 1);
        QCOMPARE(doc.width, 5);
        QCOMPARE(doc.height, 4);
        QCOMPARE(doc.origin, QPoint(3, 2));
        ImageDocument eight = ImageDocument::fromImage(picture(), "p");
        addLayers(eight, {{"x", Image::rgba(1, 1, {1, 2, 3, 255}), QPoint(5, 5)}}, false, QPoint(0, 0), index(0));
        QVERIFY(!eight.indexed);
        QCOMPARE(eight.width, 12);

        ImageDocument clip = ImageDocument::fromImage(picture(), "p");
        selectRect(clip, QRect(1, 1, 2, 1));
        const Image copied = copy(clip);
        paste(clip, copied);
        QCOMPARE(clip.layers.size(), 2);
        QCOMPARE(clip.layers[1].name, QStringLiteral("Layer 2"));
        QCOMPARE(clip.pixel(1, 1).index, 1);
        QCOMPARE(clip.pixel(3, 1).index, 0);
    }
};

QTEST_MAIN(ImageDocumentTest)
#include "ImageDocumentTest.moc"
