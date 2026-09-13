#include "core/widgets/PaletteWidget.h"

#include <QSignalSpy>
#include <QTest>

using namespace OFS;

class PaletteWidgetTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fitWidth()
    {
        PaletteWidget grid;
        grid.resize(268, 200);
        QCOMPARE(grid.columns(), 16);
        grid.setFitWidth(true);
        QCOMPARE(grid.columns(), 22);
        QCOMPARE(grid.cellRect(0).left(), 1);
        QCOMPARE(grid.cellRect(22).left(), 1);
        QCOMPARE(grid.cellRect(22).top() - grid.cellRect(0).top(), 12);
        QCOMPARE(grid.slotAt(grid.cellRect(255).center()), 255);
        QCOMPARE(grid.slotAt(grid.cellRect(255).center() + QPoint(12, 0)), -1);
        grid.resize(594, 200);
        QCOMPARE(grid.columns(), 49);
        grid.resize(193, 200);
        QCOMPARE(grid.columns(), 16);
    }

    void layoutAndGestures()
    {
        PaletteWidget grid;
        grid.resize(240, 200);
        QCOMPARE(grid.slotCount(), 256);
        grid.setColors(Formats::Palette(32, Formats::Rgba{10, 20, 30, 255}));
        QCOMPARE(grid.slotCount(), 32);
        QCOMPARE(grid.slotAt(grid.cellRect(17).center()), 17);
        QCOMPARE(grid.slotAt(grid.cellRect(15).center() + QPoint(0, 2 * 12)), -1);
        QCOMPARE(grid.cellRect(1).left() - grid.cellRect(0).left(), 12);
        QCOMPARE(grid.cellRect(16).top() - grid.cellRect(0).top(), 12);
        QCOMPARE(grid.cellRect(0).left(), (240 - 16 * 12 - 1) / 2 + 1);

        QSignalSpy pressed(&grid, &PaletteWidget::slotPressed);
        auto click = [&grid](int slot, Qt::KeyboardModifiers modifiers = {}, Qt::MouseButton button = Qt::LeftButton) {
            QTest::mouseClick(&grid, button, modifiers, grid.cellRect(slot).center());
        };
        click(3);
        QCOMPARE(grid.selection(), (QList<int>{3}));
        click(7, Qt::ControlModifier);
        QCOMPARE(grid.selection(), (QList<int>{3, 7}));
        click(3, Qt::ControlModifier);
        QCOMPARE(grid.selection(), (QList<int>{7}));
        click(10, Qt::ShiftModifier);
        QCOMPARE(grid.selection(), (QList<int>{3, 4, 5, 6, 7, 8, 9, 10}));
        click(5, Qt::ShiftModifier);
        QCOMPARE(grid.selection(), (QList<int>{5, 6, 7, 8, 9, 10}));
        click(20, {}, Qt::RightButton);
        QCOMPARE(grid.selection(), (QList<int>{5, 6, 7, 8, 9, 10}));
        QCOMPARE(pressed.size(), 6);
        QCOMPARE(pressed.last().at(1).value<Qt::MouseButton>(), Qt::RightButton);

        QSignalSpy doubleClicked(&grid, &PaletteWidget::slotDoubleClicked);
        QTest::mouseDClick(&grid, Qt::LeftButton, {}, grid.cellRect(12).center());
        QCOMPARE(doubleClicked.size(), 1);
        QCOMPARE(doubleClicked.first().at(0).toInt(), 12);
        QCOMPARE(grid.selection(), (QList<int>{12}));

        QSignalSpy moved(&grid, &PaletteWidget::slotMoved);
        QTest::mousePress(&grid, Qt::LeftButton, {}, grid.cellRect(5).center());
        QMouseEvent move(QEvent::MouseMove, QPointF(grid.cellRect(9).center()),
                         grid.mapToGlobal(grid.cellRect(9).center()), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&grid, &move);
        QTest::mouseRelease(&grid, Qt::LeftButton, {}, grid.cellRect(9).center());
        QCOMPARE(moved.size(), 1);
        QCOMPARE(moved.first().at(0).toInt(), 5);
        QCOMPARE(moved.first().at(1).toInt(), 9);
        QVERIFY(grid.selection().isEmpty());

        grid.setSelection({2, 30});
        grid.setColors(Formats::Palette(16, Formats::Rgba{}));
        QCOMPARE(grid.selection(), (QList<int>{2}));
    }
};

QTEST_MAIN(PaletteWidgetTest)
#include "PaletteWidgetTest.moc"
