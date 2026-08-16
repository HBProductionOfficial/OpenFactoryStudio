#include "core/widgets/GraphicsView.h"

#include <QApplication>
#include <QSignalSpy>
#include <QTest>
#include <QWheelEvent>

using namespace OFS;

namespace {

QImage block(int width, int height)
{
    QImage image(width, height, QImage::Format_RGBA8888);
    image.fill(Qt::red);
    return image;
}

} // namespace

class GraphicsViewTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void coordinates()
    {
        GraphicsView view;
        view.resize(400, 300);
        view.setOrigin(QPoint(200, 150));
        QCOMPARE(view.zoom(), 1.0);
        QCOMPARE(view.toWidget(QPointF(10, -5)), QPointF(210, 145));
        QCOMPARE(view.toUnits(QPointF(210, 145)), QPointF(10, -5));

        view.zoomIn();
        QCOMPARE(view.zoom(), 2.0);
        QCOMPARE(view.origin(), QPoint(200, 150));
        QCOMPARE(view.toWidget(QPointF(10, -5)), QPointF(220, 140));
        view.setZoomIndex(100);
        QCOMPARE(view.zoom(), 32.0);
        view.zoomReset();
        QCOMPARE(view.zoom(), 1.0);
        view.setZoomLevels({1, 2, 4});
        QCOMPARE(view.zoomIndex(), 0);
    }

    void wheelZoomKeepsCursorPoint()
    {
        GraphicsView view;
        view.resize(400, 300);
        view.setOrigin(QPoint(200, 150));
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        const QPoint cursor(250, 100);
        const QPointF before = view.toUnits(cursor);
        QWheelEvent wheel(cursor, view.mapToGlobal(cursor), QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        QApplication::sendEvent(&view, &wheel);
        QCOMPARE(view.zoom(), 2.0);
        QCOMPARE(view.toUnits(cursor), before);
    }

    void dragging()
    {
        GraphicsView view;
        view.resize(400, 300);
        view.setOrigin(QPoint(200, 150));
        view.setImages({{block(20, 10), QPoint(-10, -10), 1.0, true}});
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QSignalSpy dragged(&view, &GraphicsView::imageDragged);
        QSignalSpy finished(&view, &GraphicsView::dragFinished);

        QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(200, 145));
        QTest::mouseMove(&view, QPoint(203, 143));
        QTest::mouseMove(&view, QPoint(205, 143));
        QTest::mouseRelease(&view, Qt::LeftButton, {}, QPoint(205, 143));
        int dx = 0;
        int dy = 0;
        for (const QList<QVariant>& args : std::as_const(dragged)) {
            dx += args[0].toInt();
            dy += args[1].toInt();
        }
        QCOMPARE(dx, 5);
        QCOMPARE(dy, -2);
        QCOMPARE(finished.count(), 1);
        QCOMPARE(view.origin(), QPoint(200, 150));

        QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(60, 250));
        QTest::mouseMove(&view, QPoint(70, 240));
        QTest::mouseRelease(&view, Qt::LeftButton, {}, QPoint(70, 240));
        QCOMPARE(view.origin(), QPoint(210, 140));

        view.setGuidesVisible(true);
        view.setGuides(QPoint(-50, 40));
        QSignalSpy guides(&view, &GraphicsView::guidesChanged);
        const QPointF guide = view.toWidget(QPointF(-50, 40));
        QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(int(guide.x()), 60));
        QTest::mouseMove(&view, QPoint(int(guide.x()) + 12, 60));
        QTest::mouseRelease(&view, Qt::LeftButton, {}, QPoint(int(guide.x()) + 12, 60));
        QCOMPARE(view.guides(), QPoint(-38, 40));
        QVERIFY(guides.count() > 0);
    }

    void fit()
    {
        GraphicsView view;
        view.resize(420, 320);
        view.setImages({{block(100, 50), QPoint(-50, -50)}});
        view.zoomFit();
        QCOMPARE(view.zoom(), 3.0); // 4x would not fit 400 px across
        const QPointF centre = view.toWidget(QPointF(0, -25));
        QVERIFY(std::abs(centre.x() - (20 + 400 / 2.0)) <= 2);
        view.setImages({});
        view.zoomFit();
        QCOMPARE(view.zoom(), 3.0);
    }
};

QTEST_MAIN(GraphicsViewTest)
#include "GraphicsViewTest.moc"
