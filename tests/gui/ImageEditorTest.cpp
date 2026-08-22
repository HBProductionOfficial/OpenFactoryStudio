#include "shell/ImageEditor.h"

#include "core/widgets/ImageCanvas.h"

#include <QCheckBox>
#include <QSlider>
#include <QTest>
#include <QToolBar>
#include <QUndoStack>

using namespace OFS;
using namespace OFS::Formats;

namespace {

// 6x5, palette index 1 on the left half and 2 on the right half.
Image picture()
{
    std::vector<std::uint8_t> pixels(30);
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 6; ++x)
            pixels[std::size_t(y * 6 + x)] = x < 3 ? 1 : 2;
    return Image::indexed(6, 5, pixels, grayscalePalette(256));
}

// Whether the tool options bar shows `widget`. A toolbar hides its widgets through their actions.
bool shown(QWidget* widget)
{
    auto* bar = qobject_cast<QToolBar*>(widget->parentWidget());
    if (!bar)
        return false;
    for (QAction* a : bar->actions()) {
        if (bar->widgetForAction(a) == widget)
            return a->isVisible();
    }
    return false;
}

bool selected(const ImageDocument& doc, int x, int y)
{
    return doc.hasSelection() && doc.selection[std::size_t(y * doc.width + x)] != 0;
}

int selectedCount(const ImageDocument& doc)
{
    int count = 0;
    for (std::uint8_t s : doc.selection)
        count += s != 0;
    return count;
}

} // namespace

class ImageEditorTest : public QObject
{
    Q_OBJECT

private:
    void setUp(ImageEditor& editor)
    {
        editor.resize(1000, 400); // wide enough for the whole options bar
        editor.open(picture(), QStringLiteral("0,0"));
        editor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&editor));
        editor.canvas()->setZoom(20);
    }
    static QPoint at(ImageEditor& editor, double x, double y)
    {
        return editor.canvas()->toWidget(QPointF(x, y)).toPoint();
    }
    static QPoint pixel(ImageEditor& editor, int x, int y) { return at(editor, x + 0.5, y + 0.5); }
    static void drag(ImageEditor& editor, QPoint from, QPoint to, Qt::MouseButton button = Qt::LeftButton)
    {
        QTest::mousePress(editor.canvas(), button, {}, from);
        QTest::mouseMove(editor.canvas(), (from + to) / 2);
        QTest::mouseMove(editor.canvas(), to);
        QTest::mouseRelease(editor.canvas(), button, {}, to);
    }

private Q_SLOTS:
    void painting()
    {
        ImageEditor editor;
        setUp(editor);
        editor.setTool(ImageEditor::Tool::Pen);
        editor.setBrushSize(ImageEditor::Tool::Pen, 2);
        editor.setForeground(PaintColor{7, {}});
        editor.setBackground(PaintColor{8, {}});
        QTest::mouseClick(editor.canvas(), Qt::LeftButton, {}, pixel(editor, 1, 1));
        const ImageDocument& doc = editor.document();
        QCOMPARE(doc.pixel(1, 1).index, 7);
        QCOMPARE(doc.pixel(2, 2).index, 7);
        QCOMPARE(doc.pixel(1, 3).index, 1);
        QCOMPARE(doc.pixel(3, 1).index, 2);
        QTest::mouseClick(editor.canvas(), Qt::RightButton, {}, pixel(editor, 4, 3));
        QCOMPARE(doc.pixel(5, 4).index, 8);
        QCOMPARE(editor.undoStack()->count(), 2);
        editor.setBrushSize(ImageEditor::Tool::Pen, 1);
        drag(editor, pixel(editor, 0, 0), pixel(editor, 5, 0));
        QCOMPARE(doc.pixel(3, 0).index, 7);
        QCOMPARE(editor.undoStack()->count(), 3);
        editor.undoStack()->undo();
        QCOMPARE(doc.pixel(3, 0).index, 2);
        QVERIFY(editor.isModified());

        editor.setTool(ImageEditor::Tool::Eraser);
        QTest::mouseClick(editor.canvas(), Qt::LeftButton, {}, pixel(editor, 0, 4));
        QCOMPARE(doc.pixel(0, 4).index, 8);
        QTest::mouseClick(editor.canvas(), Qt::RightButton, {}, pixel(editor, 1, 4));
        QCOMPARE(doc.pixel(1, 4).index, 1);

        editor.setTool(ImageEditor::Tool::PaintBucket);
        editor.setForeground(PaintColor{9, {}});
        QTest::mouseClick(editor.canvas(), Qt::LeftButton, {}, pixel(editor, 0, 2));
        QCOMPARE(doc.pixel(0, 3).index, 9);
        QCOMPARE(doc.pixel(4, 2).index, 2);
        editor.setContiguous(ImageEditor::Tool::PaintBucket, false);
        editor.setForeground(PaintColor{3, {}});
        QTest::mouseClick(editor.canvas(), Qt::LeftButton, {}, pixel(editor, 4, 2));
        QCOMPARE(doc.pixel(3, 0).index, 3);
        QCOMPARE(doc.pixel(5, 2).index, 3);

        editor.setTool(ImageEditor::Tool::Picker);
        QTest::mouseClick(editor.canvas(), Qt::LeftButton, {}, pixel(editor, 1, 1));
        QCOMPARE(editor.foreground().index, 7);
        QCOMPARE(editor.displayColor(editor.foreground()), QColor(7, 7, 7));
        QTest::mouseClick(editor.canvas(), Qt::RightButton, {}, pixel(editor, 5, 4));
        QCOMPARE(editor.background().index, 8);
        ImageDocument transparent = doc;
        transparent.setPixel(0, 0, PaintColor{0, {}});
        editor.change(transparent);
        QTest::mouseClick(editor.canvas(), Qt::LeftButton, {}, pixel(editor, 0, 0));
        QCOMPARE(editor.displayColor(editor.foreground()), QColor(160, 160, 164)); // index 0 shows as gray

        editor.setTool(ImageEditor::Tool::CanvasMove);
        const int steps = editor.undoStack()->count();
        drag(editor, pixel(editor, 1, 1), pixel(editor, 3, 3));
        QCOMPARE(editor.undoStack()->count(), steps);
    }

    void selecting()
    {
        ImageEditor editor;
        setUp(editor);
        const ImageDocument& doc = editor.document();

        editor.setTool(ImageEditor::Tool::MagicWand);
        QTest::mouseClick(editor.canvas(), Qt::LeftButton, {}, pixel(editor, 0, 0));
        QCOMPARE(selectedCount(doc), 15);
        QVERIFY(selected(doc, 2, 4));
        QVERIFY(!selected(doc, 3, 0));
        QTest::mouseClick(editor.canvas(), Qt::LeftButton, Qt::ShiftModifier, pixel(editor, 4, 0));
        QCOMPARE(selectedCount(doc), 30);
        QTest::mouseClick(editor.canvas(), Qt::LeftButton, Qt::AltModifier, pixel(editor, 0, 0));
        QCOMPARE(selectedCount(doc), 15);
        QVERIFY(selected(doc, 3, 0));
        QTest::mouseClick(editor.canvas(), Qt::RightButton, {}, pixel(editor, 0, 0));
        QCOMPARE(selectedCount(doc), 15);
        QVERIFY(selected(doc, 5, 4));
        QCOMPARE(editor.undoStack()->count(), 0);

        editor.setTool(ImageEditor::Tool::RectSelect);
        drag(editor, at(editor, 1, 1), at(editor, 3, 3));
        QCOMPARE(selectedCount(doc), 4);
        QVERIFY(selected(doc, 1, 1) && selected(doc, 2, 2));
        drag(editor, at(editor, 3, 3), at(editor, 1, 1));
        QCOMPARE(selectedCount(doc), 4);
        QVERIFY(selected(doc, 1, 1) && selected(doc, 2, 2));
        QTest::mouseClick(editor.canvas(), Qt::LeftButton, {}, at(editor, 2, 2));
        QVERIFY(!doc.hasSelection());

        editor.setTool(ImageEditor::Tool::FreeSelect);
        editor.setBrushSize(ImageEditor::Tool::FreeSelect, 2);
        QTest::mouseClick(editor.canvas(), Qt::LeftButton, {}, pixel(editor, 0, 0));
        QCOMPARE(selectedCount(doc), 4);
        QTest::mouseClick(editor.canvas(), Qt::LeftButton, {}, pixel(editor, 4, 3));
        QCOMPARE(selectedCount(doc), 8);
        editor.setBrushSize(ImageEditor::Tool::FreeSelect, 1);
        QTest::mouseClick(editor.canvas(), Qt::RightButton, {}, pixel(editor, 0, 0));
        QCOMPARE(selectedCount(doc), 7);

        editor.setTool(ImageEditor::Tool::RectSelect);
        drag(editor, at(editor, 0, 0), at(editor, 1, 1));
        editor.setBackground(PaintColor{4, {}});
        editor.setTool(ImageEditor::Tool::SelectionMove);
        drag(editor, pixel(editor, 0, 0), pixel(editor, 0, 2));
        QCOMPARE(doc.pixel(0, 0).index, 4);
        QCOMPARE(doc.pixel(0, 2).index, 1);
        QVERIFY(selected(doc, 0, 2));
        QVERIFY(!selected(doc, 0, 0));
        QCOMPARE(editor.undoStack()->count(), 1);
        QCOMPARE(doc.pixel(1, 0).index, 1);
    }

    void remapAndOptions()
    {
        ImageEditor editor;
        setUp(editor);
        const ImageDocument& doc = editor.document();
        editor.setTool(ImageEditor::Tool::RemapPen);
        QTest::mouseClick(editor.canvas(), Qt::LeftButton, Qt::ShiftModifier, pixel(editor, 0, 0));
        QTest::mouseClick(editor.canvas(), Qt::RightButton, Qt::ShiftModifier, pixel(editor, 5, 0));
        QCOMPARE(editor.remapSlots()[0].source->index, 1);
        QCOMPARE(editor.remapSlots()[0].replacement->index, 2);
        QCOMPARE(editor.undoStack()->count(), 0);
        editor.setBrushSize(ImageEditor::Tool::RemapPen, 4);
        QTest::mouseClick(editor.canvas(), Qt::LeftButton, {}, pixel(editor, 1, 0));
        QCOMPARE(doc.pixel(1, 0).index, 2);
        QCOMPARE(doc.pixel(2, 3).index, 2);
        QCOMPARE(doc.pixel(0, 0).index, 1);
        QCOMPARE(doc.pixel(4, 0).index, 2);
        QCOMPARE(editor.undoStack()->count(), 1);

        auto* size = editor.findChild<QSlider*>("toolSize");
        auto* contiguous = editor.findChild<QCheckBox*>("toolContiguous");
        QVERIFY(size && contiguous);
        QVERIFY(shown(size));
        QCOMPARE(size->value(), 4);
        QCOMPARE(size->maximum(), 128);
        editor.setTool(ImageEditor::Tool::Pen);
        QCOMPARE(size->value(), 1);
        QVERIFY(!shown(contiguous));
        size->setValue(8);
        QCOMPARE(editor.brushSize(ImageEditor::Tool::Pen), 8);
        QCOMPARE(editor.brushSize(ImageEditor::Tool::Eraser), 1);
        editor.setTool(ImageEditor::Tool::MagicWand);
        QVERIFY(!shown(size));
        QVERIFY(shown(contiguous));
        QVERIFY(contiguous->isChecked());
        contiguous->setChecked(false);
        QVERIFY(!editor.contiguous(ImageEditor::Tool::MagicWand));
        editor.setTool(ImageEditor::Tool::Picker);
        QVERIFY(!shown(size) && !shown(contiguous));
    }

    // Playing the layers leaves their visibility as it was.
    void play()
    {
        ImageEditor editor;
        setUp(editor);
        ImageDocument doc = editor.document();
        ImageEdits::addLayer(doc);
        doc.layers[0].visible = false;
        editor.apply(QStringLiteral("Add layer"), doc);
        editor.play();
        QVERIFY(editor.isPlaying());
        QTRY_VERIFY_WITH_TIMEOUT(!editor.isPlaying(), 2000);
        QVERIFY(!editor.document().layers[0].visible);
        QVERIFY(editor.document().layers[1].visible);
    }

    void rotateFilters()
    {
        // The canvas grows to the rotated bounding box.
        for (ImageEdits::RotateFilter filter : {ImageEdits::RotateFilter::Epx, ImageEdits::RotateFilter::Hq}) {
            ImageDocument doc = ImageDocument::fromImage(picture(), QStringLiteral("0,0"));
            ImageEdits::rotate(doc, 45, filter, PaintColor{});
            QCOMPARE(doc.width, 8);
            QCOMPARE(doc.height, 8);
            QVERIFY(doc.indexed);
            const int centre = doc.layer().image.indexAt(4, 4);
            QVERIFY(centre == 1 || centre == 2);
        }
        ImageDocument rgba = ImageDocument::fromImage(picture(), QStringLiteral("0,0"));
        ImageEdits::setIndexed(rgba, false);
        ImageEdits::rotate(rgba, 30, ImageEdits::RotateFilter::Hq, PaintColor{});
        QVERIFY(!rgba.indexed);
        QCOMPARE(rgba.layer().image.colorAt(rgba.width / 2, rgba.height / 2, false).a, 255);
    }
};

QTEST_MAIN(ImageEditorTest)
#include "ImageEditorTest.moc"
