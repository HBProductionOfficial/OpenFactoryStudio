#include "core/bg/BgRenderer.h"
#include "core/anims/AirDocument.h"
#include "core/bg/BgDocument.h"
#include "core/bg/BgSimulator.h"
#include "core/sprites/SpriteDocument.h"
#include "core/widgets/Blending.h"

#include <QPainter>
#include <QTest>

#include <cstdlib>

using namespace OFS;

namespace {

// 0,0 is 4x2 with axis 2,0, 1,0 is 2x2 with axis 0,0. Pixel values 0..7 over a gray palette.
SpriteDocument sprites()
{
    SpriteDocument doc;
    Formats::Palette gray(256);
    for (int i = 0; i < 256; ++i)
        gray[std::size_t(i)] = Formats::Rgba{std::uint8_t(i * 10), std::uint8_t(i * 10), std::uint8_t(i * 10), 255};
    doc.palettes() << SpriteDocument::makePalette(1, 1, gray);
    const int ids[][4] = {{0, 0, 4, 2}, {1, 0, 2, 2}};
    for (const auto& id : ids) {
        SpriteEntry entry;
        entry.header.group = std::uint16_t(id[0]);
        entry.header.number = std::uint16_t(id[1]);
        entry.header.axisX = std::int16_t(id[0] == 0 ? 2 : 0);
        std::vector<std::uint8_t> pixels(std::size_t(id[2] * id[3]));
        for (std::size_t i = 0; i < pixels.size(); ++i)
            pixels[i] = std::uint8_t(i);
        entry.pixels = SpriteDocument::makePixels(Formats::Image::indexed(id[2], id[3], pixels));
        entry.palette = doc.palettes()[0];
        doc.sprites() << entry;
    }
    return doc;
}

QList<BgDrawItem> render(const char* text, BgView view = {}, int tick = 0)
{
    static SpriteDocument doc = sprites();
    const QString def = QString::fromLatin1(text);
    static AirDocument air;
    air = AirDocument::parse(def);
    const BgDocument bg = BgDocument::parse(def.split(QLatin1Char('\n')));
    BgSimulator simulator(bg.groups().first());
    simulator.runTo(tick);
    BgRenderer renderer(&doc, &air);
    return renderer.render(simulator, view);
}

} // namespace

class BgRendererTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void placement()
    {
        // The axis at the start, x from the screen centre and y from the top.
        QList<BgDrawItem> items = render("[BGdef]\n[BG a]\nspriteno = 0,0\nstart = 10, 20\n");
        QCOMPARE(items.size(), 1);
        QCOMPARE(items[0].topLeft, QPointF(160 + 10 - 2, 20));
        QCOMPARE(items[0].image.size(), QSize(4, 2));
        QCOMPARE(qAlpha(items[0].image.pixel(0, 0)), 255);
        items = render("[BGdef]\n[BG a]\nspriteno = 0,0\nmask = 1\n");
        QCOMPARE(qAlpha(items[0].image.pixel(0, 0)), 0);
        QCOMPARE(qAlpha(items[0].image.pixel(1, 0)), 255);

        BgView view;
        view.camera = QPointF(100, 10);
        items = render("[BGdef]\n[BG a]\nspriteno = 0,0\nstart = 10, 20\ndelta = 0.5, 1\n", view);
        QCOMPARE(items[0].topLeft, QPointF(160 + 10 - 50 - 2, 10));
        items = render("[BGdef]\n[BG a]\nspriteno = 0,0\nvelocity = 2, 0\n", {}, 5);
        QCOMPARE(items[0].topLeft.x(), 160 + 10 - 2.0);
        view = BgView();
        view.layer = 1;
        QVERIFY(render("[BGdef]\n[BG a]\nspriteno = 0,0\n", view).isEmpty());
        QVERIFY(render("[BGdef]\n[BG a]\ntype = dummy\n").isEmpty());
    }

    void tiling()
    {
        QList<BgDrawItem> items = render("[BGdef]\n[BG a]\nspriteno = 0,0\ntile = 1, 0\ntilespacing = 2, 0\n");
        QVERIFY(items.size() >= 320 / 6);
        QVERIFY(items.first().topLeft.x() <= -4 + 0.001);
        QVERIFY(items.last().topLeft.x() >= 316);
        for (int i = 1; i < items.size(); ++i)
            QCOMPARE(items[i].topLeft.x() - items[i - 1].topLeft.x(), 6.0);
        items = render("[BGdef]\n[BG a]\nspriteno = 0,0\ntile = 3, 2\n");
        QCOMPARE(items.size(), 6);
        QCOMPARE(items[1].topLeft, QPointF(158 + 4, 0));
        QCOMPARE(items[3].topLeft, QPointF(158, 2));
    }

    void animation()
    {
        const char* def = "[BGdef]\n[BG a]\ntype = anim\nactionno = 10\nstart = 0, 50\n"
                          "[Begin Action 10]\n1,0, 3,1, 5\n0,0, 0,0, 5, H\n";
        QList<BgDrawItem> items = render(def);
        QCOMPARE(items[0].image.size(), QSize(2, 2));
        QCOMPARE(items[0].topLeft, QPointF(160 + 3, 51));
        QCOMPARE(qAlpha(items[0].image.pixel(0, 0)), 0);
        items = render(def, {}, 5);
        QCOMPARE(items[0].image.size(), QSize(4, 2));
        QCOMPARE(items[0].topLeft, QPointF(160 - 2, 50));
        QCOMPARE(qGray(items[0].image.pixel(0, 0)), 30);
    }

    void parallax()
    {
        const char* def = "[BGdef]\n[BG p]\ntype = parallax\nspriteno = 0,0\nwidth = 8, 16\n";
        QList<BgDrawItem> items = render(def);
        QCOMPARE(items.size(), 1);
        QCOMPARE(items[0].topLeft, QPointF(152, 0));
        QCOMPARE(items[0].image.size(), QSize(16, 2));
        QCOMPARE(qAlpha(items[0].image.pixel(2, 0)), 0);
        QCOMPARE(qAlpha(items[0].image.pixel(5, 0)), 255);
        QCOMPARE(qAlpha(items[0].image.pixel(1, 1)), 255);
        BgView view;
        view.camera = QPointF(10, 0);
        items = render(def, view);
        QCOMPARE(items[0].topLeft.x(), 160 + (146 - 160) * 2.0);

        items = render("[BGdef]\n[BG p]\ntype = parallax\nspriteno = 0,0\nxscale = 1, 2\nstart = 10, 0\n");
        QCOMPARE(items[0].topLeft.x(), 168.0);
        QCOMPARE(items[0].image.width(), 4 + 10);
    }

    void windowsAndBlending()
    {
        QList<BgDrawItem> items = render("[BGdef]\n[BG a]\nspriteno = 0,0\nwindow = 0,0, 99,49\n");
        QCOMPARE(items[0].clip.value(), QRectF(0, 0, 100, 50));
        BgView view;
        view.camera = QPointF(20, 0);
        items = render("[BGdef]\n[BG a]\nspriteno = 0,0\nmaskwindow = 10,0, 20,10\nwindowdelta = 1, 0\n", view);
        QCOMPARE(items[0].clip.value(), QRectF(160 + 10 - 20, 0, 10, 10));
        view.windows = false;
        QVERIFY(!render("[BGdef]\n[BG a]\nspriteno = 0,0\nwindow = 0,0, 9,9\n", view)[0].clip);

        QCOMPARE(render("[BGdef]\n[BG a]\nspriteno = 0,0\ntrans = add\n")[0].mode, QPainter::CompositionMode_Plus);
        QCOMPARE(render("[BGdef]\n[BG a]\nspriteno = 0,0\ntrans = sub\n")[0].mode,
                 QPainter::CompositionMode_Difference);
        items = render("[BGdef]\n[BG a]\nspriteno = 0,0\ntrans = addalpha\nalpha = 128, 128\n");
        QCOMPARE(items[0].mode, QPainter::CompositionMode_SourceOver);
        QCOMPARE(items[0].opacity, 0.5);
        items = render("[BGdef]\n[BG a]\nspriteno = 0,0\ntrans = addalpha\nalpha = 64, 256\n");
        QCOMPARE(items[0].mode, QPainter::CompositionMode_Plus);
        QCOMPARE(items[0].opacity, 0.25);
    }

    // Qt's Difference mode gives the absolute difference, which once turned a dark background light.
    void subtraction()
    {
        QImage frame(4, 1, QImage::Format_ARGB32_Premultiplied);
        frame.fill(QColor(100, 150, 200));
        QImage image(4, 1, QImage::Format_ARGB32);
        image.fill(QColor(30, 200, 50));
        image.setPixelColor(3, 0, Qt::transparent);
        {
            QPainter p(&frame);
            drawBlended(p, QRectF(0, 0, 4, 1), image, QPainter::CompositionMode_Difference);
        }
        QCOMPARE(frame.pixelColor(0, 0), QColor(70, 0, 150));
        QCOMPARE(frame.pixelColor(3, 0), QColor(100, 150, 200));
        frame.fill(QColor(100, 150, 200));
        {
            QPainter p(&frame);
            drawBlended(p, QRectF(0, 0, 4, 1), image, QPainter::CompositionMode_Difference, 0.5);
        }
        const QColor half = frame.pixelColor(0, 0);
        QVERIFY(std::abs(half.red() - 85) <= 1 && std::abs(half.green() - 50) <= 1 && std::abs(half.blue() - 175) <= 1);
        // A weighted addition weights the image first and clips only the sum: 200 + 100 / 4 = 225.
        frame.fill(QColor(200, 200, 200));
        image.fill(QColor(100, 100, 100));
        {
            QPainter p(&frame);
            drawBlended(p, QRectF(0, 0, 4, 1), image, QPainter::CompositionMode_Plus, 0.25);
        }
        QVERIFY(std::abs(frame.pixelColor(0, 0).red() - 225) <= 1);
    }
};

QTEST_MAIN(BgRendererTest)
#include "BgRendererTest.moc"
