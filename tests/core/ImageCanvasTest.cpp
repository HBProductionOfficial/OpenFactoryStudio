#include "core/widgets/ImageCanvas.h"

#include <QApplication>
#include <QSignalSpy>
#include <QTest>
#include <QWheelEvent>

using namespace OFS;
using namespace OFS::Formats;

namespace {

ImageDocument document(int width, int height)
{
    return ImageDocument::fromImage(
        Image::indexed(width, height, std::vector<std::uint8_t>(std::size_t(width) * std::size_t(height), 1),
                       grayscalePalette(256)),
        "c");
}

} // namespace

class ImageCanvasTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void zoomLimits()
    {
        ImageCanvas canvas;
        canvas.resize(400, 300);
        const ImageDocument doc = document(12, 10);
        canvas.setDocument(&doc);
        QCOMPARE(canvas.minimumZoom(), 5.0); // 50 / the smaller side
        canvas.zoomReset();
        QCOMPARE(canvas.zoom(), 5.0);
        canvas.zoomIn();
        QCOMPARE(canvas.zoom(), 6.0);
        canvas.setZoom(100);
        QCOMPARE(canvas.zoom(), 32.0);
        canvas.zoomOut();
        QCOMPARE(canvas.zoom(), 31.0);
        canvas.zoomFit();
        QCOMPARE(canvas.zoom(), 30.0); // 300 / 10
        canvas.setZoom(8);
        canvas.show();
        QVERIFY(QTest::qWaitForWindowExposed(&canvas));
        QWheelEvent wheel(QPointF(10, 10), canvas.mapToGlobal(QPointF(10, 10)), QPoint(), QPoint(0, 120), Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&canvas, &wheel);
        QCOMPARE(canvas.zoom(), 16.0);
    }

    void mapping()
    {
        ImageCanvas canvas;
        canvas.resize(400, 300);
        const ImageDocument doc = document(12, 10);
        canvas.setDocument(&doc);
        canvas.setZoom(10);
        // The canvas centre is at the widget centre: (200, 150) is image point (6, 5).
        QCOMPARE(canvas.toWidget(QPointF(6, 5)), QPointF(200, 150));
        QCOMPARE(canvas.pixelAt(QPointF(201, 151)), QPoint(6, 5));
        QCOMPARE(canvas.pixelAt(QPointF(199, 149)), QPoint(5, 4));
        QCOMPARE(canvas.gridPointAt(QPointF(204, 156)), QPoint(6, 6));
    }

    void mouse()
    {
        ImageCanvas canvas;
        canvas.resize(400, 300);
        const ImageDocument doc = document(12, 10);
        canvas.setDocument(&doc);
        canvas.setZoom(10);
        canvas.show();
        QVERIFY(QTest::qWaitForWindowExposed(&canvas));
        QSignalSpy pressed(&canvas, &ImageCanvas::pressed);
        QSignalSpy dragged(&canvas, &ImageCanvas::dragged);
        QSignalSpy released(&canvas, &ImageCanvas::released);
        QTest::mousePress(&canvas, Qt::LeftButton, {}, QPoint(201, 151));
        QTest::mouseMove(&canvas, QPoint(221, 151));
        QTest::mouseRelease(&canvas, Qt::LeftButton, {}, QPoint(221, 151));
        QCOMPARE(pressed.count(), 1);
        QCOMPARE(pressed[0][0].toPoint(), QPoint(6, 5));
        QCOMPARE(dragged.last()[0].toPoint(), QPoint(8, 5));
        QCOMPARE(released.count(), 1);

        QTest::mousePress(&canvas, Qt::LeftButton, Qt::ControlModifier, QPoint(200, 150));
        QTest::mouseMove(&canvas, QPoint(230, 150));
        QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::ControlModifier, QPoint(230, 150));
        QCOMPARE(pressed.count(), 1);
        QCOMPARE(canvas.toWidget(QPointF(6, 5)), QPointF(230, 150));
    }
};

QTEST_MAIN(ImageCanvasTest)
#include "ImageCanvasTest.moc"
