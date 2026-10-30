#include "core/widgets/GraphicsView.h"
#include "core/settings/Config.h"
#include "core/widgets/RenderSurface.h"
#include "core/widgets/Rulers.h"
#include "core/widgets/TimelineWidget.h"

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

// The pointer after a plain move event, so no button or key state from an earlier step counts.
Qt::CursorShape pointerAt(GraphicsView& view, const QPoint& at, Qt::KeyboardModifiers modifiers = {},
                          Qt::MouseButtons buttons = Qt::NoButton)
{
    QMouseEvent move(QEvent::MouseMove, QPointF(at), view.mapToGlobal(QPointF(at)), Qt::NoButton, buttons, modifiers);
    QApplication::sendEvent(&view, &move);
    return view.cursor().shape();
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

    void graphicsOptions()
    {
        QImage onion(2, 1, QImage::Format_ARGB32);
        onion.setPixelColor(0, 0, QColor(200, 100, 0));
        onion.setPixelColor(1, 0, QColor(0, 0, 0, 0));
        QCOMPARE(GraphicsView::onionEffect(onion, 0).pixelColor(0, 0), QColor(200, 100, 0));
        const QColor gray = GraphicsView::onionEffect(onion, 1).pixelColor(0, 0);
        QVERIFY(gray.red() == gray.green() && gray.green() == gray.blue());
        QCOMPARE(GraphicsView::onionEffect(onion, 2).pixelColor(0, 0), QColor(227, 177, 127));
        QCOMPARE(GraphicsView::onionEffect(onion, 3).pixelColor(0, 0), QColor(100, 50, 0));
        QCOMPARE(GraphicsView::onionEffect(onion, 3).pixelColor(1, 0).alpha(), 0);

        GraphicsView view;
        view.resize(200, 200);
        view.setOrigin(QPoint(100, 100));
        view.setBackground(GraphicsView::Background::Color);
        view.setMainCrossVisible(false);
        const QPoint inside = view.toWidget(QPointF(10, 10)).toPoint();
        const QColor back = view.grab().toImage().pixelColor(inside);
        view.setBoxes({{QRect(-20, -20, 40, 40), QColor(0, 0, 255), false, true}});
        view.setBoxOpacity(0.0);
        QCOMPARE(view.grab().toImage().pixelColor(inside), back);
        view.setBoxOpacity(1.0);
        QCOMPARE(view.grab().toImage().pixelColor(inside), QColor(0, 0, 255));
        view.setBoxes({});

        view.setMainCrossVisible(true);
        view.setMainCrossPosition(QPoint(0, 0));
        QImage marker(5, 5, QImage::Format_ARGB32);
        marker.fill(QColor(255, 0, 255));
        view.setCrossStyle(GraphicsView::CrossStyle::Custom, marker);
        const QImage custom = view.grab().toImage();
        QCOMPARE(custom.pixelColor(view.toWidget(QPointF(0, 0)).toPoint()), QColor(255, 0, 255));
        view.setCrossStyle(GraphicsView::CrossStyle::Default);
        const QPoint arm = view.toWidget(QPointF(3, 0)).toPoint();
        const QColor classic = view.grab().toImage().pixelColor(arm);
        view.setCrossStyle(GraphicsView::CrossStyle::Modern);
        QVERIFY(view.grab().toImage().pixelColor(arm) != classic);
    }

    void touchpadZoom()
    {
        GraphicsView view;
        view.resize(200, 200);
        const int start = view.zoomIndex();
        auto scroll = [&view](int delta) {
            QWheelEvent event(QPointF(100, 100), QPointF(100, 100), QPoint(), QPoint(0, delta), Qt::NoButton,
                              Qt::NoModifier, Qt::NoScrollPhase, false);
            QCoreApplication::sendEvent(&view, &event);
        };
        scroll(40);
        scroll(40);
        QCOMPARE(view.zoomIndex(), start);
        scroll(40);
        QCOMPARE(view.zoomIndex(), start + 1);
        scroll(-120);
        QCOMPARE(view.zoomIndex(), start);
    }

    void checkerBackground()
    {
        GraphicsView view;
        view.resize(200, 200);
        view.setRulersVisible(false);
        view.setMainCrossVisible(false);
        view.setBackground(GraphicsView::Background::Checker);
        const QImage shown = view.grab().toImage();
        QCOMPARE(shown.pixelColor(1, 1), QColor(240, 240, 240));
        QCOMPARE(shown.pixelColor(9, 1), QColor(204, 204, 204));
        QCOMPARE(shown.pixelColor(9, 9), QColor(240, 240, 240));
        QCOMPARE(shown.pixelColor(1, 9), QColor(204, 204, 204));
    }

    void fitArea()
    {
        GraphicsView view;
        view.resize(400, 300);
        view.setRulersVisible(false);
        QImage near(320, 240, QImage::Format_ARGB32);
        near.fill(Qt::red);
        QImage far(10, 10, QImage::Format_ARGB32);
        far.fill(Qt::blue);
        GraphicsView::Image screen;
        screen.image = near;
        screen.topLeft = QPoint(-160, 0);
        GraphicsView::Image away;
        away.image = far;
        away.topLeft = QPoint(-7000, 0);
        view.setImages({screen, away});
        view.setFitArea(QRectF(-160, 0, 320, 240));
        view.zoomFit();
        const QPointF centre = view.toWidget(QPointF(0, 120));
        QVERIFY(std::abs(centre.x() - 200) <= 2 && std::abs(centre.y() - 150) <= 2);
        QVERIFY(view.zoom() >= 1.0);
    }

    void workspaceGradient()
    {
        GraphicsView view;
        view.resize(200, 200);
        view.setRulersVisible(false);
        view.setMainCrossVisible(false);
        view.setBackground(GraphicsView::Background::Color);
        view.setBackgroundGradient(QColor(192, 192, 192), QColor(160, 160, 160));
        const QImage shown = view.grab().toImage();
        QVERIFY(std::abs(shown.pixelColor(100, 0).red() - 192) <= 1);
        QVERIFY(std::abs(shown.pixelColor(100, 199).red() - 160) <= 1);
        view.setBackgroundColor(QColor(10, 20, 30));
        QCOMPARE(view.grab().toImage().pixelColor(100, 199), QColor(10, 20, 30));
    }

    void rulersFollowPalette()
    {
        GraphicsView view;
        view.resize(200, 200);
        view.setOrigin(QPoint(100, 100));
        view.setBackground(GraphicsView::Background::Color);
        for (const QColor& window : {QColor(51, 51, 51), QColor(221, 221, 221)}) {
            QPalette palette = view.palette();
            palette.setColor(QPalette::Window, window);
            palette.setColor(QPalette::WindowText, window.lightness() < 128 ? QColor(204, 204, 204) : Qt::black);
            view.setPalette(palette);
            const QImage shown = view.grab().toImage();
            const QColor band = shown.pixelColor(125, 3);
            QCOMPARE(band.lightness() < 128, window.lightness() < 128);
            QCOMPARE(band.rgb(), Rulers::Colours::from(palette).band.rgb());
            QCOMPARE(shown.pixelColor(3, 125), band);
        }
    }

    void crossShape()
    {
        GraphicsView view;
        view.resize(200, 200);
        view.setOrigin(QPoint(100, 100));
        view.setBackground(GraphicsView::Background::Color);
        view.setMainCrossPosition(QPoint(0, 0));
        const QImage shown = view.grab().toImage();
        const QColor background = shown.pixelColor(view.toWidget(QPointF(30, 30)).toPoint());
        auto at = [&](int x, int y) {
            return shown.pixelColor(view.toWidget(QPointF(x, y)).toPoint());
        };
        QCOMPARE(at(0, 0), background);
        for (const QPoint arm : {QPoint(1, 0), QPoint(6, 0), QPoint(-6, 0), QPoint(0, 3), QPoint(0, -6)})
            QCOMPARE(at(arm.x(), arm.y()), QColor(Qt::black));
        for (const QPoint edge : {QPoint(7, 0), QPoint(-7, 0), QPoint(0, 7), QPoint(0, -7), QPoint(3, -1),
                                  QPoint(-1, -1), QPoint(1, 1), QPoint(2, 1), QPoint(-1, 5)})
            QCOMPARE(at(edge.x(), edge.y()), QColor(Qt::white));
        for (const QPoint outside : {QPoint(8, 0), QPoint(2, 2), QPoint(-2, -2), QPoint(0, 8)})
            QCOMPARE(at(outside.x(), outside.y()), background);
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

    // Same shapes as Fighter Factory: an open hand where a drag pans, the four-way arrow over what
    // a drag moves.
    void pointers()
    {
        GraphicsView view;
        view.resize(400, 300);
        view.setOrigin(QPoint(200, 150));
        view.setImages({{block(20, 10), QPoint(-10, -10), 1.0, true, {}, {}, {}}});
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        auto shape = [&view] {
            return view.cursor().shape();
        };
        auto hover = [&view](const QPoint& at, Qt::KeyboardModifiers modifiers = {}) {
            return pointerAt(view, at, modifiers);
        };

        QTest::mouseMove(&view, QPoint(60, 250));
        QCOMPARE(shape(), Qt::OpenHandCursor);
        QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(60, 250));
        QCOMPARE(shape(), Qt::OpenHandCursor);
        QTest::mouseMove(&view, QPoint(70, 240));
        QCOMPARE(shape(), Qt::ClosedHandCursor);
        QTest::mouseRelease(&view, Qt::LeftButton, {}, QPoint(70, 240));
        QCOMPARE(shape(), Qt::OpenHandCursor);

        // A drag on the rulers pans too.
        QPoint origin = view.origin();
        QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(100, 5));
        QTest::mouseMove(&view, QPoint(110, 5));
        QTest::mouseRelease(&view, Qt::LeftButton, {}, QPoint(110, 5));
        QCOMPARE(view.origin(), origin + QPoint(10, 0));

        const QPoint sprite = view.toWidget(QPointF(0, -5)).toPoint();
        QCOMPARE(hover(sprite), Qt::SizeAllCursor);
        QCOMPARE(hover(sprite, Qt::ControlModifier), Qt::OpenHandCursor);
        // Ctrl+drag pans with the hand kept open.
        origin = view.origin();
        QSignalSpy dragged(&view, &GraphicsView::imageDragged);
        QTest::mousePress(&view, Qt::LeftButton, Qt::ControlModifier, sprite);
        QTest::mouseMove(&view, sprite + QPoint(6, 0));
        QCOMPARE(shape(), Qt::OpenHandCursor);
        QTest::mouseRelease(&view, Qt::LeftButton, Qt::ControlModifier, sprite + QPoint(6, 0));
        QCOMPARE(view.origin(), origin + QPoint(6, 0));
        QCOMPARE(dragged.count(), 0);

        // The Color picker shows a cross on the image and doesn't drag it.
        view.setColorPicker(true);
        const QPoint picked = view.toWidget(QPointF(0, -5)).toPoint();
        QCOMPARE(hover(picked), Qt::CrossCursor);
        origin = view.origin();
        QTest::mousePress(&view, Qt::LeftButton, {}, picked);
        QTest::mouseMove(&view, picked + QPoint(5, 5));
        QTest::mouseRelease(&view, Qt::LeftButton, {}, picked + QPoint(5, 5));
        QCOMPARE(dragged.count(), 0);
        QCOMPARE(view.origin(), origin);
        view.setColorPicker(false);

        view.setGuidesVisible(true);
        view.setGuides(QPoint(-60, 40));
        const QPointF guide = view.toWidget(QPointF(-60, 40));
        QCOMPARE(hover(QPoint(int(guide.x()), 60)), Qt::SizeHorCursor);
        QCOMPARE(hover(QPoint(320, int(guide.y()))), Qt::SizeVerCursor);
    }

    void timelinePlayhead()
    {
        // 8 px per tick from x 120: frame 0 is x 120 to 200, frame 1 x 200 to 280.
        TimelineWidget timeline;
        timeline.resize(600, 60);
        timeline.setAnimation(QStringLiteral("a"), {{10, false}, {10, false}});
        timeline.show();
        QVERIFY(QTest::qWaitForWindowExposed(&timeline));
        QSignalSpy clicked(&timeline, &TimelineWidget::frameClicked);
        const int head = int(timeline.playheadX());
        QTest::mouseMove(&timeline, QPoint(head + 2, 40));
        QCOMPARE(timeline.cursor().shape(), Qt::SizeHorCursor);
        QTest::mouseMove(&timeline, QPoint(head + 40, 40));
        QCOMPARE(timeline.cursor().shape(), Qt::ArrowCursor);
        // Dragging goes through the frames.
        QTest::mousePress(&timeline, Qt::LeftButton, {}, QPoint(head + 2, 40));
        QTest::mouseMove(&timeline, QPoint(240, 40));
        QTest::mouseRelease(&timeline, Qt::LeftButton, {}, QPoint(240, 40));
        QCOMPARE(clicked.size(), 2);
        QCOMPARE(clicked.last().at(0).toInt(), 1);
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
        // The handles of the selected box show resize arrows, its inside the four-way arrow.
        QCOMPARE(pointerAt(view, corner + QPoint(4, 4)), Qt::SizeFDiagCursor);
        QCOMPARE(pointerAt(view, view.toWidget(QPointF(13, -18)).toPoint()), Qt::SizeBDiagCursor);
        QCOMPARE(pointerAt(view, view.toWidget(QPointF(3, -18)).toPoint()), Qt::SizeVerCursor);
        QCOMPARE(pointerAt(view, view.toWidget(QPointF(0, -10)).toPoint()), Qt::SizeAllCursor);
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
        QCOMPARE(pointerAt(view, QPoint(100, 100)), Qt::CrossCursor);
        QTest::mousePress(&view, Qt::LeftButton, {}, view.toWidget(QPointF(2.5, -3.5)).toPoint());
        QCOMPARE(pointerAt(view, view.toWidget(QPointF(8.5, 1.5)).toPoint(), {}, Qt::LeftButton), Qt::SizeFDiagCursor);
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
