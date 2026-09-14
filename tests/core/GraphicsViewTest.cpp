#include "core/widgets/GraphicsView.h"
#include "core/settings/Config.h"
#include "core/widgets/RenderSurface.h"

#include <QApplication>
#include <QFile>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QWheelEvent>
#include <cmath>

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
    void cursorPositionRoundsDown()
    {
        GraphicsView view;
        view.resize(200, 200);
        view.setOrigin(QPoint(100, 100));
        view.setZoomIndex(view.zoomIndex() + 2); // 4x: each unit is 4 pixels
        QList<QPoint> seen;
        connect(&view, &GraphicsView::cursorMoved, this, [&seen](const QPoint& p) { seen << p; });
        for (const QPoint at : {QPoint(98, 103), QPoint(101, 97), QPoint(105, 105)}) {
            QMouseEvent move(QEvent::MouseMove, QPointF(at), view.mapToGlobal(QPointF(at)), Qt::NoButton, Qt::NoButton,
                             Qt::NoModifier);
            QApplication::sendEvent(&view, &move);
        }
        const double zoom = view.zoom();
        QCOMPARE(seen.value(0), QPoint(int(std::floor(-2 / zoom)), int(std::floor(3 / zoom))));
        QCOMPARE(seen.value(0).x(), -1);
        QCOMPARE(seen.value(1).y(), -1);
        QCOMPARE(seen.value(2), QPoint(int(std::floor(5 / zoom)), int(std::floor(5 / zoom))));
    }

    void rendererChoice()
    {
        QTemporaryDir dir;
        Config config;
        config.setValue("native_painting", true);
        Rendering::choose(config, dir.path());
        QCOMPARE(Rendering::backend(), Rendering::Backend::Native);
        QCOMPARE(Rendering::backendName(), QStringLiteral("Native"));
        config.setValue("native_painting", false);
        Rendering::choose(config, dir.path());
        if (QGuiApplication::platformName() == QLatin1String("offscreen"))
            QCOMPARE(Rendering::backend(), Rendering::Backend::Native);
        // renderer.lock is left by a start that crashed.
        QFile guard(dir.filePath("renderer.lock"));
        QVERIFY(guard.open(QIODevice::WriteOnly));
        guard.close();
        Rendering::choose(config, dir.path());
        QCOMPARE(Rendering::backend(), Rendering::Backend::Native);
        QVERIFY(!QFile::exists(dir.filePath("renderer.lock")));
        Rendering::finish();
        GraphicsView view;
        QVERIFY(view.findChildren<QWidget*>().isEmpty());
    }

    void boxesOverCrosses()
    {
        GraphicsView view;
        view.resize(200, 200);
        view.setOrigin(QPoint(100, 100));
        view.setBackground(GraphicsView::Background::Color);
        view.setMainCrossPosition(QPoint(0, 0));
        view.setBoxes({{QRect(-20, -20, 40, 40), QColor(0, 255, 255), false, true}});
        const QPoint arm = view.toWidget(QPointF(3, 0)).toPoint();
        QCOMPARE(view.grab().toImage().pixelColor(arm), QColor(Qt::black));
        view.setBoxesOverCrosses(true);
        const QColor tinted = view.grab().toImage().pixelColor(arm);
        QVERIFY(tinted != QColor(Qt::black));
        QCOMPARE(tinted.red(), 0);
        QVERIFY(tinted.green() > 0 && tinted.blue() > 0);
    }

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
        QCOMPARE(view.zoom(), 8.0);
        QCOMPARE(GraphicsView::smallZoomRange().last(), 4.0);
        view.zoomReset();
        QCOMPARE(view.zoom(), 1.0);
        view.setZoomLevels({1, 2, 4});
        QCOMPARE(view.zoomIndex(), 0);
    }

    void wheelZoom()
    {
        GraphicsView view;
        view.resize(400, 300);
        view.setOrigin(QPoint(200, 150));
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        const QPoint cursor(250, 100);
        auto wheel = [&view, cursor](int delta) {
            QWheelEvent event(cursor, view.mapToGlobal(cursor), QPoint(), QPoint(0, delta), Qt::NoButton,
                              Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(&view, &event);
        };
        wheel(120);
        QCOMPARE(view.zoom(), 2.0);
        QCOMPARE(view.origin(), QPoint(200, 150));
        view.setZoomCenter(GraphicsView::ZoomCenter::Workspace);
        const QPointF middle(20 + 380 / 2, 20 + 280 / 2);
        const QPointF before = view.toUnits(middle);
        wheel(-120);
        QCOMPARE(view.zoom(), 1.0);
        QVERIFY((view.toUnits(middle) - before).manhattanLength() < 1.0);
    }

    void dragging()
    {
        GraphicsView view;
        view.resize(400, 300);
        view.setOrigin(QPoint(200, 150));
        view.setImages({{block(20, 10), QPoint(-10, -10), 1.0, true, {}, {}, {}}});
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
        view.setImages({{block(100, 50), QPoint(-50, -50), 1.0, false, {}, {}, {}}});
        view.zoomFit();
        QCOMPARE(view.zoom(), 3.0); // 4x would not fit 400 px across
        const QPointF centre = view.toWidget(QPointF(0, -25));
        QVERIFY(std::abs(centre.x() - (20 + 400 / 2.0)) <= 2);
        view.setImages({});
        view.zoomFit();
        QCOMPARE(view.zoom(), 3.0);
    }
};

class GraphicsViewBoxTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void boxes()
    {
        // Zoom 4 with the origin at (200, 150): unit (x, y) is at (200 + 4x, 150 + 4y).
        GraphicsView view;
        view.resize(400, 300);
        view.setRulersVisible(false);
        view.setZoomLevels({4});
        view.setOrigin(QPoint(200, 150));
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QRect r;
        r.setCoords(-10, -20, 10, 0);
        view.setBoxes({{r, Qt::blue, false, true}});
        QSignalSpy pressed(&view, &GraphicsView::boxPressed);
        QSignalSpy edited(&view, &GraphicsView::boxEdited);

        QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(200, 110));
        QCOMPARE(pressed.last().at(0).toInt(), 0);
        QTest::mouseMove(&view, QPoint(212, 118));
        QTest::mouseRelease(&view, Qt::LeftButton, {}, QPoint(212, 118));
        QCOMPARE(edited.size(), 1);
        QRect moved;
        moved.setCoords(-7, -18, 13, 2);
        QCOMPARE(edited.last().at(1).toRect(), moved);

        view.setBoxes({{moved, Qt::blue, true, true}});
        const QPoint corner = view.toWidget(QPointF(13, 2)).toPoint();
        QTest::mousePress(&view, Qt::LeftButton, {}, corner);
        QTest::mouseMove(&view, corner + QPoint(20, 0));
        QTest::mouseRelease(&view, Qt::LeftButton, {}, corner + QPoint(20, 0));
        QRect wider;
        wider.setCoords(-7, -18, 18, 2);
        QCOMPARE(edited.last().at(1).toRect(), wider);
        QTest::mouseClick(&view, Qt::LeftButton, {}, QPoint(350, 280));
        QCOMPARE(pressed.last().at(0).toInt(), -1);

        QSignalSpy nudged(&view, &GraphicsView::boxNudged);
        QSignalSpy deleted(&view, &GraphicsView::boxDeleteRequested);
        view.setFocus();
        QTest::keyClick(&view, Qt::Key_Right);
        QTest::keyClick(&view, Qt::Key_Up);
        QTest::keyClick(&view, Qt::Key_Delete);
        QCOMPARE(nudged.size(), 2);
        QCOMPARE(nudged.at(1).at(1).toInt(), -1);
        QCOMPARE(deleted.size(), 1);

        QSignalSpy drawn(&view, &GraphicsView::boxDrawn);
        QSignalSpy right(&view, &GraphicsView::rightClicked);
        view.setDrawingBoxes(true, Qt::red);
        QTest::mousePress(&view, Qt::LeftButton, {}, view.toWidget(QPointF(2.5, -3.5)).toPoint());
        QTest::mouseMove(&view, view.toWidget(QPointF(8.5, 1.5)).toPoint());
        QTest::mouseRelease(&view, Qt::LeftButton, {}, view.toWidget(QPointF(8.5, 1.5)).toPoint());
        QCOMPARE(drawn.size(), 1);
        QRect box;
        box.setCoords(2, -4, 8, 1);
        QCOMPARE(drawn.last().at(0).toRect(), box);
        QTest::mouseClick(&view, Qt::RightButton, {}, QPoint(100, 100));
        QCOMPARE(right.size(), 1);
    }

    void transformedImages()
    {
        GraphicsView view;
        view.resize(400, 300);
        view.setRulersVisible(false);
        view.setZoomLevels({1});
        view.setOrigin(QPoint(200, 150));
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        QImage image(10, 10, QImage::Format_RGBA8888);
        image.fill(Qt::green);
        view.setImages({{image, QPoint(0, 0), 1.0, true, QTransform::fromScale(-1, 1), {}, {}}});
        QSignalSpy dragged(&view, &GraphicsView::imageDragged);
        QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(195, 155));
        QTest::mouseMove(&view, QPoint(198, 155));
        QTest::mouseRelease(&view, Qt::LeftButton, {}, QPoint(198, 155));
        QCOMPARE(dragged.size(), 1);
        QCOMPARE(view.grab().toImage().pixelColor(195, 155), QColor(Qt::green));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    GraphicsViewTest views;
    GraphicsViewBoxTest boxes;
    return QTest::qExec(&views, argc, argv) | QTest::qExec(&boxes, argc, argv);
}
#include "GraphicsViewTest.moc"
