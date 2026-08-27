#include "core/widgets/WaveformView.h"

#include <QSignalSpy>
#include <QTest>

using namespace OFS;
using namespace OFS::Formats;

class WaveformViewTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void viewing()
    {
        WaveformView view;
        view.resize(500, 200);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        std::vector<std::int16_t> samples(1000);
        for (std::size_t i = 0; i < samples.size(); ++i)
            samples[i] = std::int16_t((i % 50) * 400 - 10000);
        view.setBuffer(SampleBuffer::fromInt16(samples, 1, 11025));
        QCOMPARE(view.framesPerPixel(), 2.0);
        QCOMPARE(view.frameAt(250), qint64(500));
        QCOMPARE(view.frameAt(251), qint64(502));
        QCOMPARE(view.xAt(500), 250.0);

        QSignalSpy selected(&view, &WaveformView::selectionChanged);
        QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(100, 100));
        QTest::mouseMove(&view, QPoint(150, 100));
        QTest::mouseRelease(&view, Qt::LeftButton, {}, QPoint(150, 100));
        QCOMPARE(view.cursor(), qint64(200));
        QCOMPARE(view.selection().start, std::size_t(200));
        QCOMPARE(view.selection().count, std::size_t(100));
        QVERIFY(!selected.isEmpty());
        QTest::mouseClick(&view, Qt::LeftButton, Qt::ShiftModifier, QPoint(50, 100));
        QCOMPARE(view.selection().start, std::size_t(100));
        QCOMPARE(view.selection().count, std::size_t(100));
        QTest::mouseClick(&view, Qt::LeftButton, {}, QPoint(300, 100));
        QCOMPARE(view.selection().count, std::size_t(0));
        QCOMPARE(view.cursor(), qint64(600));

        view.zoomIn();
        QCOMPARE(view.framesPerPixel(), 1.0);
        QCOMPARE(view.firstFrame(), 0.0);
        view.zoomOut();
        view.zoomOut();
        QCOMPARE(view.framesPerPixel(), 4.0);
        view.zoomFit();
        QCOMPARE(view.framesPerPixel(), 2.0);

        view.setSelection(SampleRange{200, 100});
        QSignalSpy moved(&view, &WaveformView::moveRequested);
        view.setTool(WaveformView::Tool::MoveSelection);
        QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(120, 100));
        QTest::mouseMove(&view, QPoint(170, 100));
        QTest::mouseRelease(&view, Qt::LeftButton, {}, QPoint(170, 100));
        QCOMPARE(moved.size(), 1);
        QCOMPARE(moved.first().at(1).toLongLong(), qint64(300));
        QSignalSpy scaled(&view, &WaveformView::amplitudeRequested);
        view.setTool(WaveformView::Tool::VerticalResize);
        QTest::mousePress(&view, Qt::LeftButton, {}, QPoint(120, 100));
        QTest::mouseMove(&view, QPoint(120, 60));
        QTest::mouseRelease(&view, Qt::LeftButton, {}, QPoint(120, 60));
        QCOMPARE(scaled.size(), 1);
        QVERIFY(scaled.first().at(1).toDouble() > 1.0);
    }
};

QTEST_MAIN(WaveformViewTest)
#include "WaveformViewTest.moc"
