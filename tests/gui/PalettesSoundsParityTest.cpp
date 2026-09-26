// Tests for the Palettes panel, Remap, the Advanced palette editor and the Sounds editor.

#include "GuiTestSupport.h"
#include "core/actions/ActionRegistry.h"
#include "core/project/Project.h"
#include "core/sounds/SoundDocument.h"
#include "core/sounds/SoundFile.h"
#include "core/sprites/SpriteDocument.h"
#include "core/sprites/SpriteFile.h"
#include "core/widgets/GraphicsView.h"
#include "core/widgets/PaletteWidget.h"
#include "core/widgets/WaveformView.h"
#include "formats/audio/AudioCodec.h"
#include "formats/codecs/Png.h"
#include "formats/snd/SndDocument.h"
#include "shell/AdvancedPaletteEditor.h"
#include "shell/FileDialogs.h"
#include "shell/ImageEditor.h"
#include "shell/MainWindow.h"
#include "shell/ProjectView.h"
#include "shell/SoundPage.h"
#include "shell/SpritePage.h"
#include "shell/dialogs/PaletteDialogs.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QTableView>
#include <QTemporaryDir>
#include <QToolButton>
#include <QUndoStack>

using namespace OFS;
using namespace OFS::GuiTest;

namespace {

// Slot i holds i in the given channel and 0 in the others. Slot 0 has alpha 0.
Formats::Palette ramp(int channel)
{
    Formats::Palette colors(256);
    for (int i = 0; i < 256; ++i) {
        std::uint8_t c[3] = {0, 0, 0};
        c[channel] = std::uint8_t(i);
        colors[std::size_t(i)] = Formats::Rgba{c[0], c[1], c[2], std::uint8_t(i == 0 ? 0 : 255)};
    }
    return colors;
}

QByteArray toBytes(const Formats::Bytes& bytes)
{
    return QByteArray(reinterpret_cast<const char*>(bytes.data()), qsizetype(bytes.size()));
}

// Palettes 1,1 (red ramp) and 1,2 (green ramp). Sprites 0,0 (8-bit on 1,1), 0,1 (32-bit) and 5,0
// (8-bit on 1,2). Pixel (x, y) of the 8-bit sprites is index x + 4y.
SpriteDocument paletteSprites()
{
    SpriteDocument doc;
    doc.palettes() << SpriteDocument::makePalette(1, 1, ramp(0)) << SpriteDocument::makePalette(1, 2, ramp(1));
    std::vector<std::uint8_t> pixels(16);
    for (int i = 0; i < 16; ++i)
        pixels[std::size_t(i)] = std::uint8_t(i);
    auto add = [&doc](int group, int number, Formats::Image image, int palette) {
        SpriteEntry entry;
        entry.header.group = std::uint16_t(group);
        entry.header.number = std::uint16_t(number);
        entry.pixels = SpriteDocument::makePixels(std::move(image));
        if (palette >= 0)
            entry.palette = doc.palettes()[palette];
        doc.sprites() << entry;
    };
    add(0, 0, Formats::Image::indexed(4, 4, pixels), 0);
    std::vector<std::uint8_t> rgba(4 * 4 * 4, 200);
    rgba[3] = 0;
    add(0, 1, Formats::Image::rgba(4, 4, rgba), -1);
    add(5, 0, Formats::Image::indexed(4, 4, pixels), 1);
    return doc;
}

void writeSpriteProject(const QTemporaryDir& dir, const SpriteDocument& doc, const QByteArray& sound = {})
{
    const auto bytes = doc.toSff().write();
    QVERIFY(bytes.ok());
    writeFile(dir.filePath("p.sff"), toBytes(bytes.value()));
    QByteArray def = "[Info]\nname = \"P\"\nmugenversion = 1.0\n[Files]\ncns = p.cns\nsprite = p.sff\n";
    if (!sound.isEmpty()) {
        writeFile(dir.filePath("p.snd"), sound);
        def += "sound = p.snd\n";
    }
    writeFile(dir.filePath("p.def"), def);
    writeFile(dir.filePath("p.cns"), "[Data]\nlife = 1000\n");
}

Formats::Bytes wav(std::vector<std::int16_t> samples, int channels = 1, int rate = 8000)
{
    auto bytes = Formats::encodeAudio(
        Formats::SampleBuffer::fromInt16(std::move(samples), std::uint16_t(channels), std::uint32_t(rate)));
    return bytes ? bytes.value() : Formats::Bytes();
}

template <typename T>
T* visibleChild(QWidget* parent, const char* name)
{
    for (T* w : parent->findChildren<T*>(QLatin1String(name))) {
        if (w->isVisible())
            return w;
    }
    return nullptr;
}

// One "caption[ keys][ (off)][ [x]]" per item, "|" for a separator, "<title>" for a submenu.
QStringList describe(const QMenu* menu)
{
    QStringList items;
    for (QAction* a : menu->actions()) {
        if (!a->isVisible())
            continue;
        if (a->isSeparator()) {
            items << QStringLiteral("|");
            continue;
        }
        if (a->menu()) {
            items << QLatin1Char('<') + a->text() + QLatin1Char('>');
            continue;
        }
        QString item = a->text().section(QLatin1Char('\t'), 0, 0);
        if (!a->shortcut().isEmpty())
            item += QLatin1Char(' ') + a->shortcut().toString(QKeySequence::PortableText);
        if (!a->isEnabled())
            item += QStringLiteral(" (off)");
        if (a->isCheckable())
            item += a->isChecked() ? QStringLiteral(" [x]") : QStringLiteral(" [ ]");
        items << item;
    }
    return items;
}

void sendMouse(QWidget* widget, QEvent::Type type, QPoint at, Qt::MouseButton button,
               Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    const Qt::MouseButtons buttons = type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::MouseButtons(button);
    QMouseEvent event(type, at, widget->mapToGlobal(at), type == QEvent::MouseMove ? Qt::NoButton : button, buttons,
                      modifiers);
    QCoreApplication::sendEvent(widget, &event);
}

} // namespace

class PalettesSoundsParityTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_context = makeContext();
        Config& config = m_context->config();
        config.setValue("confirm_delete", false);
        config.setValue("switch_to_imgpal", true);
        config.setValue("show_addpalette_dialog", true);
        config.setValue("show_addsound_dialog", true);
        config.setValue("showmsg_pal_file", "0");
        config.setValue("show_organizer", false);
    }

    void cleanupTestCase() { delete m_context; }

    void panelAndMenus()
    {
        QTemporaryDir dir;
        Formats::SndDocument snd;
        snd.sounds.push_back(Formats::SndSound{0, 0, wav(std::vector<std::int16_t>(100, 5))});
        writeSpriteProject(dir, paletteSprites(), toBytes(snd.write()));
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        QVERIFY(window.openProject(dir.filePath("p.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        ProjectView* view = window.currentProjectView();

        const struct
        {
            const char* id;
            const char* caption;
            const char* keys;
            const char* tip;
        } buttons[] = {
            {"pal_add", "Add", nullptr, "Add one or more palettes"},
            {"pal_delete", "Delete", nullptr, "Remove the current palette"},
            {"pal_open", "Change", nullptr, "Define new colors to the palette from a file..."},
            {"pal_save", "Save as", nullptr, "Save the palette as a file..."},
            {"pal_undo", "Undo", "Ctrl+Shift+Z", "Undo"},
            {"pal_redo", "Redo", "Ctrl+Shift+Y", "Redo"},
            {"tools_pal_editor", "Advanced palette editor", "Ctrl+Shift+P",
             "Edit the palette in the 'Advanced palette editor'"},
            {"pal_switch_count", "Switch color count (32-256)", nullptr,
             "Switch the palette color count (32 to 256 or 256 to 32)"},
            {"pal_switch_color", "Exchange position of selected colors", nullptr,
             "Exchange position of selected colors"},
            {"pal_pick_color", "Color picker", nullptr, "Enable the Color picker for quick selection"},
            {"pal_set_color", "Sets the color of the selected slots", nullptr, "Sets the color of the selected slots"},
            {"pal_remap", "Remap", "Ctrl+Shift+R", "Open the palette Remap tool"},
            {"pal_apply_sprite", "Apply to the sprites", nullptr, "Apply the current palette to the specified sprites"},
        };
        for (const auto& b : buttons) {
            auto* button = visibleChild<QToolButton>(view, b.id);
            QVERIFY2(button, b.id);
            QCOMPARE(button->toolTip(),
                     QLatin1String(b.caption) +
                         (b.keys ? QStringLiteral("  (") + keyText(b.keys) + QLatin1Char(')') : QString()));
            QCOMPARE(button->defaultAction()->statusTip(), QLatin1String(b.tip));
        }
        auto* page = view->findChild<SpritePage*>();
        QVERIFY(!page->file()->undoStack()->canUndo());
        QVERIFY(actions.action("pal_undo")->isEnabled());
        QVERIFY(actions.action("pal_redo")->isEnabled());
        for (const char* id : {"pal_prev_grp", "pal_next_grp"}) {
            QVERIFY2(actions.keys(QLatin1String(id)).isEmpty(), id);
            QVERIFY2(!view->findChild<QToolButton*>(id), id);
            QVERIFY2(!menuIds(menu(window, "menu_palettes")).contains(QLatin1String(id)), id);
        }
        QCOMPARE(actions.keys("pal_prev"), QKeySequence("Ctrl+Alt+Left"));
        QCOMPARE(actions.keys("pal_next"), QKeySequence("Ctrl+Alt+Right"));
        QVERIFY(!view->findChild<QToolButton*>("pal_prev"));
        for (const char* id : {"pal_clear", "pal_clear_unused", "pal_delete_duplicates"}) {
            QVERIFY2(!view->findChild<QToolButton*>(id), id);
            QVERIFY2(menuIds(menu(window, "menu_palettes")).contains(QLatin1String(id)), id);
        }

        QCOMPARE(describe(menu(window, "menu_palettes")),
                 (QStringList{"Clear", "Clear all unused", "Add", "Delete", "Remove all duplicates", "Change...",
                              "Save as...", "|", "Switch color count (32-256)", "Remap Ctrl+Shift+R", "|",
                              "Undo Ctrl+Shift+Z", "Redo Ctrl+Shift+Y", "Exchange position of selected colors",
                              "Sets the color of the selected slots", "Color picker [ ]", "Apply to the sprites"}));
        QMenu* tools = menu(window, "menu_tools");
        Q_EMIT tools->aboutToShow(); // fills in the engine tools and tool scripts
        QCOMPARE(describe(tools), (QStringList{"Options... Ctrl+K", "|", "Advanced palette editor Ctrl+Shift+P",
                                               "Script editor", "|", "Export to HTML", "Quick action script"}));
        Q_EMIT tools->aboutToHide();

        QCOMPARE(actions.action("pal_remap")->shortcut(), QKeySequence("Ctrl+Shift+R"));
        QVERIFY(actions.action("snd_resample")->shortcut().isEmpty());
        for (const char* editor : {"editor_def", "editor_snd", "editor_cmd"}) {
            actions.action(editor)->trigger();
            for (const char* id : {"pal_add", "pal_clear", "pal_remap", "pal_apply_sprite"})
                QVERIFY2(actions.action(id)->isEnabled(), qPrintable(QStringLiteral("%1 in %2").arg(id, editor)));
        }
        actions.action("editor_snd")->trigger();
        QCOMPARE(actions.action("snd_resample")->shortcut(), QKeySequence("Ctrl+Shift+R"));
        QVERIFY(actions.action("pal_remap")->shortcut().isEmpty());

        actions.action("editor_spr")->trigger();
        QVERIFY(visibleChild<QToolButton>(view, "pal_pick_color"));
        QVERIFY(visibleChild<QCheckBox>(view, "spr_switch_pal"));
        page->setCurrent(0);
        actions.action("spr_edit_img")->trigger();
        QVERIFY(!visibleChild<QToolButton>(view, "pal_pick_color"));
        QVERIFY(!visibleChild<QCheckBox>(view, "spr_switch_pal"));
        QVERIFY(visibleChild<QSpinBox>(view, "pal_grp"));
        QVERIFY(visibleChild<QSpinBox>(view, "pal_num"));
        actions.action("spr_edit_img")->trigger();
        QVERIFY(visibleChild<QCheckBox>(view, "spr_switch_pal"));
        actions.action("file_close")->trigger();
    }

    // Palette grid drawing and how the panel follows the current sprite.
    void gridAndFollowing()
    {
        QTemporaryDir dir;
        SpriteDocument doc = paletteSprites();
        Formats::Palette colors = ramp(0);
        colors[1] = {255, 255, 255, 255};
        colors[2] = {255, 0, 0, 128};
        doc.palettes()[0] = SpriteDocument::makePalette(1, 1, colors);
        doc.sprites()[0].palette = doc.palettes()[0];
        writeSpriteProject(dir, doc);
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        QVERIFY(window.openProject(dir.filePath("p.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        ProjectView* view = window.currentProjectView();
        auto* page = view->findChild<SpritePage*>();
        auto* grid = view->findChild<PaletteWidget*>("pal_view");
        auto* label = view->findChild<QLabel*>("pal_sel_lb");
        QVERIFY(grid && label);
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        page->setCurrent(0);

        QImage shown = grid->grab().toImage();
        const QRect cell0 = grid->cellRect(0);
        QCOMPARE(shown.pixelColor(cell0.topLeft() + QPoint(1, 1)), QColor(160, 160, 164));
        QCOMPARE(shown.pixelColor(cell0.topRight() + QPoint(-1, 1)), QColor(Qt::white));
        QCOMPARE(shown.pixelColor(cell0.bottomRight() + QPoint(-1, -1)), QColor(160, 160, 164));
        const QColor half = shown.pixelColor(grid->cellRect(2).topRight() + QPoint(-1, 1));
        QVERIFY2(half.red() == 255 && std::abs(half.green() - 127) <= 2, qPrintable(half.name()));
        grid->setSelection({1});
        shown = grid->grab().toImage();
        const QRect cell1 = grid->cellRect(1);
        QCOMPARE(shown.pixelColor(cell1.topLeft() + QPoint(2, 2)), QColor(Qt::black));
        QCOMPARE(shown.pixelColor(cell1.center()), QColor(Qt::white));

        page->setCurrent(2);
        QCOMPARE(label->text(), QStringLiteral("2 / 2"));
        page->setCurrent(1);
        QCOMPARE(label->text(), QStringLiteral("2 / 2"));
        SpriteDocument orphan = page->file()->document();
        orphan.sprites()[0].palette = nullptr;
        page->file()->apply(QStringLiteral("Orphan"), orphan);
        page->setCurrent(0);
        QCOMPARE(label->text(), QStringLiteral("2 / 2"));
        page->file()->undoStack()->undo();
        page->setCurrent(2);
        page->setCurrent(0);
        QCOMPARE(label->text(), QStringLiteral("1 / 2"));

        actions.action("spr_edit_img")->trigger();
        actions.action("spr_tool_pen")->trigger();
        ImageEditor* editor = page->imageEditor();
        grid->setSelection({});
        sendMouse(grid, QEvent::MouseButtonPress, grid->cellRect(5).center(), Qt::LeftButton);
        sendMouse(grid, QEvent::MouseButtonRelease, grid->cellRect(5).center(), Qt::LeftButton);
        QCOMPARE(editor->foreground().index, 5);
        QCOMPARE(grid->selection(), QList<int>{5});
        sendMouse(grid, QEvent::MouseButtonPress, grid->cellRect(6).center(), Qt::RightButton);
        sendMouse(grid, QEvent::MouseButtonRelease, grid->cellRect(6).center(), Qt::RightButton);
        QCOMPARE(editor->background().index, 6);
        QCOMPARE(grid->selection(), QList<int>{5});
        QCOMPARE(editor->foreground().index, 5);
        actions.action("spr_edit_img")->trigger();
        actions.action("file_close")->trigger();
    }

    void addAndDeletePalettes()
    {
        QTemporaryDir dir;
        writeSpriteProject(dir, paletteSprites());
        QByteArray act;
        for (int i = 0; i < 256; ++i)
            act += QByteArray(1, char(i)) + QByteArray(1, char(0)) + QByteArray(1, char(255 - i));
        for (const char* name : {"c.act", "a.act", "700_5.act"})
            writeFile(dir.filePath(QLatin1String(name)), act);
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("p.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        ProjectView* view = window.currentProjectView();
        auto* page = view->findChild<SpritePage*>();
        SpriteFile* file = page->file();
        auto* label = view->findChild<QLabel*>("pal_sel_lb");
        Config& config = m_context->config();
        config.setValue("showmsg_pal_file", "0");
        page->setCurrentPalette(0);

        QString caption;
        QString filter;
        FileDialogs::setOpenPromptOverride([&](const QString& c, const QString&, const QString& f) {
            caption = c;
            filter = f;
            return dir.filePath("c.act") + "\n" + dir.filePath("a.act");
        });
        whenModal<QMessageBox>([](QMessageBox* box) {
            QCOMPARE(box->windowTitle(), boxTitle("Add palette"));
            QCOMPARE(box->text(), QStringLiteral("Do you want to load palettes from files?"));
            QCOMPARE(box->defaultButton(), box->button(QMessageBox::Yes));
            QVERIFY(box->checkBox());
            QCOMPARE(box->checkBox()->text(), QStringLiteral("Don't show this again"));
            whenModal<QDialog>([](QDialog* add) {
                QCOMPARE(add->objectName(), QStringLiteral("AddPal"));
                QCOMPARE(add->windowTitle(), QStringLiteral("Add"));
                auto* detect = add->findChild<QCheckBox*>("detect");
                auto* group = add->findChild<QSpinBox*>("Grp");
                auto* number = add->findChild<QSpinBox*>("Num");
                QVERIFY(detect && !detect->isChecked());
                QCOMPARE(group->value(), 0);
                QCOMPARE(group->maximum(), 65535);
                QCOMPARE(number->value(), 0);
                QVERIFY(!add->findChild<QCheckBox*>("rev")->isChecked());
                QCOMPARE(add->findChild<QRadioButton*>("OneInc")->text(), QStringLiteral("1,2,3,4,..."));
                QVERIFY(add->findChild<QRadioButton*>("OneInc")->isChecked());
                QCOMPARE(add->findChild<QRadioButton*>("TenInc")->text(), QStringLiteral("10,20,30,..."));
                detect->setChecked(true);
                QVERIFY(group->isEnabled() && number->isEnabled());
                detect->setChecked(false);
                group->setValue(600);
                number->setValue(1);
                add->accept();
            });
            box->button(QMessageBox::Yes)->click();
        });
        actions.action("pal_add")->trigger();
        QCOMPARE(caption, QStringLiteral("Add palettes"));
        QVERIFY(
            filter.startsWith(QStringLiteral("All supported (*.act *.pcx *.png *.gif *.bmp *.pal *.aco *.ase *.txt)")));
        const SpriteDocument& doc = file->document();
        QCOMPARE(doc.palettes().size(), 4);
        QCOMPARE(int(doc.palettes()[1]->palette.number), 1);
        QCOMPARE(int(doc.palettes()[2]->palette.number), 2);
        QCOMPARE(int(doc.palettes()[1]->palette.group), 600);
        QCOMPARE(label->text(), QStringLiteral("3 / 4"));
        QCOMPARE(doc.palettes()[1]->palette.colors[1], (Formats::Rgba{254, 0, 1, 255})); // ACT read in reverse
        QCOMPARE(file->undoStack()->count(), 1);
        file->undoStack()->undo();

        FileDialogs::setOpenPromptOverride(
            [&dir](const QString&, const QString&, const QString&) { return dir.filePath("700_5.act"); });
        config.setValue("showmsg_pal_file", "1"); // Always, so no question
        page->setCurrentPalette(0);
        whenModal<QDialog>([](QDialog* add) {
            add->findChild<QCheckBox*>("detect")->setChecked(true);
            add->findChild<QCheckBox*>("rev")->setChecked(true);
            add->accept();
        });
        actions.action("pal_add")->trigger();
        FileDialogs::setOpenPromptOverride({});
        config.setValue("showmsg_pal_file", "0");
        QCOMPARE(int(file->document().palettes()[1]->palette.group), 700);
        QCOMPARE(int(file->document().palettes()[1]->palette.number), 5);
        QCOMPARE(file->document().palettes()[1]->palette.colors[1], (Formats::Rgba{1, 0, 254, 255}));

        config.setValue("confirm_delete", true);
        QStringList texts;
        answerBoxes({QMessageBox::No}, &texts);
        actions.action("pal_delete")->trigger();
        QCOMPARE(texts, QStringList{"Do you really want to delete the palette?"});
        QCOMPARE(file->document().palettes().size(), 3);
        answerBoxes({QMessageBox::Yes}, nullptr);
        actions.action("pal_delete")->trigger();
        QCOMPARE(file->document().palettes().size(), 2);
        config.setValue("confirm_delete", false);
        while (file->undoStack()->canUndo())
            file->undoStack()->undo();
        actions.action("file_close")->trigger();
    }

    void remapTool()
    {
        QTemporaryDir dir;
        writeSpriteProject(dir, paletteSprites());
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("p.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        auto* page = window.currentProjectView()->findChild<SpritePage*>();
        page->setCurrent(0);
        actions.action("pal_remap")->trigger();
        auto* remap = window.findChild<RemapDialog*>("RemapDlg");
        QVERIFY(remap && remap->isVisible() && !remap->isModal());
        QCOMPARE(remap->windowTitle(), QStringLiteral("Remap"));
        QTableView* table = remap->table();
        QVERIFY(QTest::qWaitForWindowExposed(remap));
        QStringList headers;
        for (int c = 0; c < 4; ++c)
            headers << table->model()->headerData(c, Qt::Horizontal).toString();
        QCOMPARE(headers, (QStringList{"Src Grp", "Src Num", "Dst Grp", "Dst Num"}));
        QCOMPARE(remap->rows().size(), 1);
        QCOMPARE(remap->rows()[0].targetNumber, 1);
        QVERIFY(!table->horizontalScrollBar()->isVisible());
        const int columns = table->horizontalHeader()->sectionPosition(3) + table->horizontalHeader()->sectionSize(3);
        QVERIFY(columns <= table->viewport()->width());
        QVERIFY(!remap->findChild<QDialogButtonBox*>());

        const QModelIndex cell = table->model()->index(0, 3);
        QVERIFY(table->editTriggers() & QAbstractItemView::DoubleClicked);
        const QPoint at = table->visualRect(cell).center();
        sendMouse(table->viewport(), QEvent::MouseButtonPress, at, Qt::LeftButton);
        sendMouse(table->viewport(), QEvent::MouseButtonRelease, at, Qt::LeftButton);
        sendMouse(table->viewport(), QEvent::MouseButtonDblClick, at, Qt::LeftButton);
        sendMouse(table->viewport(), QEvent::MouseButtonRelease, at, Qt::LeftButton);
        auto* editor = qobject_cast<QSpinBox*>(table->indexWidget(cell));
        if (!editor)
            editor = table->viewport()->findChild<QSpinBox*>();
        QVERIFY(editor);
        editor->setValue(2);
        QTest::keyClick(editor, Qt::Key_Return);
        QTRY_COMPARE(remap->rows()[0].targetNumber, 2);
        QCOMPARE(page->view()->images().last().image.pixelColor(1, 0), QColor(0, 1, 0));

        remap->findChild<QPushButton*>("pushButton")->click();
        QCOMPARE(remap->rows().size(), 2);
        table->selectRow(1);
        remap->findChild<QPushButton*>("pushButton_2")->click();
        QCOMPARE(remap->rows().size(), 1);
        QCOMPARE(remap->rows()[0].targetNumber, 2);
        remap->close();
        actions.action("pal_remap")->trigger();
        auto* again = window.findChild<RemapDialog*>("RemapDlg");
        QVERIFY(again && again->isVisible());
        QCOMPARE(again->rows().size(), 1);
        QCOMPARE(again->rows()[0].targetNumber, 2);
        again->findChild<QPushButton*>("pushButton_3")->click();
        QVERIFY(again->rows().isEmpty());
        again->close();
        actions.action("pal_remap")->trigger();
        QVERIFY(window.findChild<RemapDialog*>("RemapDlg")->rows().isEmpty());
        window.findChild<RemapDialog*>("RemapDlg")->close();
        actions.action("file_close")->trigger();
    }

    void advancedPaletteEditor()
    {
        QTemporaryDir dir;
        writeSpriteProject(dir, paletteSprites());
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("p.def")));
        m_context->actions().action("editor_spr")->trigger();
        auto* page = window.currentProjectView()->findChild<SpritePage*>();
        SpriteFile* file = page->file();
        AdvancedPaletteEditor editor(*m_context, file, 0, 0, &window);
        editor.show();
        QVERIFY(QTest::qWaitForWindowExposed(&editor));
        auto button = [&editor](const char* name) {
            auto* b = editor.findChild<QToolButton*>(QLatin1String(name));
            [&] {
                QVERIFY2(b, name);
            }();
            return b;
        };
        const char* const functions[][2] = {
            {"openPal", "Open palette"},
            {"savePal", "Save current palette"},
            {"savGrpIdx", "Add to sprites editor"},
            {"zoomOut", "Decrease zoom"},
            {"zoomIn", "Increase zoom"},
            {"openImg", "Open image"},
            {"saveImg", "Save image"},
            {"undo", "Undo"},
            {"redo", "Redo"},
            {"genPal", "Generate palette from images"},
            {"insPal", "Insert the current palette in the images"},
            {"optPal", "Optimized palette for the current image"},
            {"grayScale", "Generate a minimal grayscale palette"},
            {"corGrad", "Interval of selected colors in gradient"},
            {"setCor", "Sets the color of the selected slots"},
            {"delDupl", "Remove the duplicate colors"},
            {"addDiff", "Add different colors from the other palette"},
            {"adaptImg", "Adapt image to the palette"},
            {"autoOrg", "Sort colors by tones"},
            {"swPos", "Exchange position of selected colors"},
            {"invPal", "Invert all colors"},
            {"commChanges", "Commit changes"},
            {"commSprChanges", "Commit sprite changes"},
        };
        QGridLayout* grid = nullptr;
        for (QGridLayout* layout : editor.findChildren<QGridLayout*>()) {
            if (layout->indexOf(button("openPal")) >= 0)
                grid = layout;
        }
        QVERIFY(grid);
        for (int i = 0; i < 23; ++i) {
            QToolButton* b = button(functions[i][0]);
            QCOMPARE(b->toolTip(), QLatin1String(functions[i][1]));
            int row = -1, column = -1, rows = 0, columns = 0;
            grid->getItemPosition(grid->indexOf(b), &row, &column, &rows, &columns);
            QCOMPARE(row, i / 7);
            QCOMPARE(column, i % 7);
        }
        QCOMPARE(button("corRL")->toolTip(), QStringLiteral("Send selected colors to the left palette"));
        QCOMPARE(button("corLR")->toolTip(), QStringLiteral("Send selected colors to the right palette"));
        QCOMPARE(button("selRL")->toolTip(), QStringLiteral("Send selection to the left palette"));
        QCOMPARE(button("selLR")->toolTip(), QStringLiteral("Send selection to the right palette"));
        auto* selection = editor.findChild<QGroupBox*>("color_sel");
        QVERIFY(selection);
        QCOMPARE(selection->title(), QStringLiteral("Color selection:"));
        auto* mask = selection->findChild<QCheckBox*>("mask");
        QVERIFY(mask && !mask->isChecked());
        QCOMPARE(mask->text(), QStringLiteral("Mask"));
        QCOMPARE(selection->findChild<QToolButton*>("maskCor")->text(), QStringLiteral("Mask's color"));
        QToolButton* picker = selection->findChild<QToolButton*>("pickCor");
        QVERIFY(picker && picker->isCheckable() && !picker->isChecked());
        QCOMPARE(picker->toolTip(), QStringLiteral("Color picker"));
        const struct
        {
            const char* name;
            int range;
        } sliders[] = {{"hueHue", 360}, {"hueSat", 100}, {"rgbR", 255}, {"rgbG", 255}, {"rgbB", 255}};
        for (const auto& s : sliders) {
            auto* slider = editor.findChild<QSlider*>(QLatin1String(s.name));
            QVERIFY2(slider, s.name);
            QCOMPARE(slider->minimum(), -s.range);
            QCOMPARE(slider->maximum(), s.range);
            QCOMPARE(slider->value(), 0);
        }

        QString caption;
        QString filter;
        auto record = [&](const QString& answer) {
            return [&, answer](const QString& c, const QString&, const QString& f) {
                caption = c;
                filter = f;
                return answer;
            };
        };
        FileDialogs::setSavePromptOverride(record(dir.filePath("out.act")));
        button("savePal")->click();
        QCOMPARE(caption, QStringLiteral("Save palette"));
        QCOMPARE(filter, QStringLiteral("Adobe Color Table (*.act)"));
        const QByteArray act = readFile(dir.filePath("out.act"));
        QCOMPARE(act.size(), 768);
        QCOMPARE(act.mid(0, 3), QByteArray("\xff\x00\x00", 3)); // slot 255 first
        QCOMPARE(act.mid(3, 3), QByteArray("\xfe\x00\x00", 3));
        QCOMPARE(act.mid(765, 3), QByteArray(3, '\0'));

        FileDialogs::setSavePromptOverride(record(dir.filePath("char.png")));
        button("saveImg")->click();
        QCOMPARE(caption, QStringLiteral("Save image"));
        QVERIFY(filter.startsWith(QStringLiteral("Portable Network Graphic (*.png)")));
        const QImage saved(dir.filePath("char.png"));
        QCOMPARE(saved.format(), QImage::Format_Indexed8);
        QCOMPARE(saved.colorCount(), 256);
        QCOMPARE(qAlpha(saved.color(0)), 0);
        QCOMPARE(qAlpha(saved.color(1)), 255);
        QCOMPARE(saved.pixelIndex(1, 1), 5);
        FileDialogs::setSavePromptOverride({});

        std::vector<std::uint8_t> rgb = {16, 16, 128, 255, 0, 16, 128, 255, 16, 0, 128, 255, 0, 0, 128, 255};
        const auto png = Formats::encodePng(Formats::Image::rgba(2, 2, rgb));
        QVERIFY(png.ok());
        writeFile(dir.filePath("rgb.png"), toBytes(png.value()));
        editor.setCheckedColumn(AdvancedPaletteEditor::ImageOrPalette);
        FileDialogs::setOpenPromptOverride(record(dir.filePath("rgb.png")));
        QString asked;
        whenModal<QMessageBox>([&asked](QMessageBox* box) {
            asked = box->windowTitle() + "|" + box->text() + "|" +
                    (box->defaultButton() == box->button(QMessageBox::No) ? "No" : "?");
            box->button(QMessageBox::No)->click();
        });
        button("openImg")->click();
        QCOMPARE(caption, QStringLiteral("Open image"));
        QCOMPARE(filter, QStringLiteral("All supported (*.pcx *.png *.gif *.bmp);;Personal Computer eXchange (*.pcx);;"
                                        "Portable Network Graphic (*.png);;Graphics Interchange Format (*.gif);;"
                                        "Bitmap (*.bmp)"));
        QCOMPARE(asked, boxTitle("Generate optimized palette?") +
                            "|This image isn't indexed. Do you want to generate an optimized palette for it?|No");
        QVERIFY(!editor.column(AdvancedPaletteEditor::ImageOrPalette).image.isIndexed());
        answerBoxes({QMessageBox::Yes}, nullptr);
        button("openImg")->click();
        const AdvancedPaletteEditor::Column& right = editor.column(AdvancedPaletteEditor::ImageOrPalette);
        QVERIFY(right.image.isIndexed());
        QCOMPARE(right.palette[0], (Formats::Rgba{0, 0, 128, 255}));
        QCOMPARE(right.palette[1], (Formats::Rgba{0, 16, 128, 255}));
        QCOMPARE(right.palette[2], (Formats::Rgba{16, 0, 128, 255}));
        QCOMPARE(right.palette[3], (Formats::Rgba{16, 16, 128, 255}));

        const QByteArray before = readFile(dir.filePath("rgb.png"));
        QStringList texts;
        answerBoxes({QMessageBox::No}, &texts);
        button("insPal")->click();
        QCOMPARE(texts, QStringList{"The 1 chosen image(s) will be rewritten with the current palette. Continue?"});
        QCOMPARE(readFile(dir.filePath("rgb.png")), before);
        answerBoxes({QMessageBox::Yes}, nullptr);
        button("insPal")->click();
        QVERIFY(readFile(dir.filePath("rgb.png")) != before);
        QCOMPARE(QImage(dir.filePath("rgb.png")).format(), QImage::Format_Indexed8);
        FileDialogs::setOpenPromptOverride({});

        editor.grid(AdvancedPaletteEditor::Character)->setSelection({1, 2});
        editor.grid(AdvancedPaletteEditor::ImageOrPalette)->setSelection({10, 11});
        button("corLR")->click();
        QCOMPARE(editor.column(AdvancedPaletteEditor::ImageOrPalette).palette[10], (Formats::Rgba{1, 0, 0, 255}));
        QCOMPARE(editor.column(AdvancedPaletteEditor::ImageOrPalette).palette[11], (Formats::Rgba{2, 0, 0, 255}));
        editor.setCheckedColumn(AdvancedPaletteEditor::Character);

        auto* paletteSelector = editor.findChild<QScrollBar*>("palSel");
        QCOMPARE(paletteSelector->maximum(), 1);
        whenModal<QDialog>([](QDialog* add) {
            QCOMPARE(add->objectName(), QStringLiteral("AddPal"));
            QVERIFY(!add->findChild<QCheckBox*>("detect"));
            QVERIFY(add->findChild<QCheckBox*>("rev"));
            QVERIFY(add->findChild<QRadioButton*>("TenInc"));
            add->findChild<QSpinBox*>("Grp")->setValue(800);
            add->findChild<QSpinBox*>("Num")->setValue(2);
            add->accept();
        });
        button("savGrpIdx")->click();
        QCOMPARE(file->document().palettes().size(), 3);
        QCOMPARE(int(file->document().palettes().last()->palette.group), 800);
        QCOMPARE(int(file->document().palettes().last()->palette.number), 2);
        QCOMPARE(paletteSelector->maximum(), 2);

        PaletteWidget* left = editor.grid(AdvancedPaletteEditor::Character);
        left->setSelection({3, 4});
        whenModal<QColorDialog>([](QColorDialog* dialog) {
            QCOMPARE(dialog->windowTitle(), QStringLiteral("Color"));
            QVERIFY(dialog->testOption(QColorDialog::ShowAlphaChannel));
            QCOMPARE(dialog->currentColor(), QColor(3, 0, 0));
            dialog->setCurrentColor(QColor(10, 20, 30, 128));
            dialog->accept();
        });
        button("setCor")->click();
        QCOMPARE(editor.column(AdvancedPaletteEditor::Character).palette[3], (Formats::Rgba{10, 20, 30, 128}));
        QCOMPARE(editor.column(AdvancedPaletteEditor::Character).palette[4], (Formats::Rgba{10, 20, 30, 128}));
        QCOMPARE(left->selection(), (QList<int>{3, 4}));

        QWidget* image = editor.findChild<QScrollArea*>("charArea")->widget();
        left->setSelection({5});
        mask->setChecked(true);
        QImage shown = image->grab().toImage();
        QCOMPARE(shown.pixelColor(1, 1), QColor(255, 0, 255));
        QCOMPARE(shown.pixelColor(2, 1), QColor(6, 0, 0));
        mask->setChecked(false);
        QCOMPARE(image->grab().toImage().pixelColor(1, 1), QColor(5, 0, 0));

        picker->click();
        left->setSelection({});
        sendMouse(image, QEvent::MouseButtonPress, QPoint(2, 1), Qt::LeftButton);
        sendMouse(image, QEvent::MouseButtonRelease, QPoint(2, 1), Qt::LeftButton);
        sendMouse(image, QEvent::MouseButtonPress, QPoint(3, 0), Qt::LeftButton);
        sendMouse(image, QEvent::MouseButtonRelease, QPoint(3, 0), Qt::LeftButton);
        QCOMPARE(left->selection(), (QList<int>{3, 6}));
        picker->click();

        answerDiscard(QMessageBox::Yes);
        editor.reject();
        QVERIFY(!editor.isVisible());
        file->undoStack()->undo();
        QVERIFY(!file->isModified());
        m_context->actions().action("file_close")->trigger();
    }

    void soundsPanelAndMenu()
    {
        QTemporaryDir dir;
        openSounds(dir);
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        QVERIFY(window.openProject(dir.filePath("p.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_snd")->trigger();
        ProjectView* view = window.currentProjectView();
        const struct
        {
            const char* id;
            const char* caption;
            const char* keys;
            const char* tip;
        } buttons[] = {
            {"snd_add", "Add", "Ctrl+A", "Add one or more sounds"},
            {"snd_delete", "Delete", "Ctrl+Del", "Remove the current sound"},
            {"snd_open_sample", "Change sound", "Ctrl+Shift+C", "Change the current sound from a file..."},
            {"snd_save_sample", "Save sound as", "Ctrl+Alt+S", "Save the sound as WAV..."},
            {"snd_play", "Play", "Ctrl+P", "Play the current sound"},
            {"snd_play_loop", "Play with loop", "Ctrl+Shift+L", "Play the current sound with infinite loop"},
            {"snd_pause", "Pause", nullptr, "Pause"},
            {"snd_stop", "Stop", "Ctrl+T", "Stop the sound"},
            {"snd_prev_grp", "Previous group", "Ctrl+Shift+Left", "Previous group of sounds from the current"},
            {"snd_next_grp", "Next group", "Ctrl+Shift+Right", "Next group of sounds from the current"},
            {"snd_tool_offset", "Move view offset", nullptr, "Move view offset"},
            {"snd_tool_zoom", "Zoom", nullptr, "Zoom"},
            {"snd_tool_select", "Free selection", nullptr, "Free selection"},
            {"snd_tool_move", "Move selection", nullptr, "Move selection"},
            {"snd_tool_hresize", "Horizontal resizer", nullptr, "Horizontal resizer"},
            {"snd_tool_vresize", "Vertical resizer", nullptr, "Vertical resizer"},
            {"snd_save_changes", "Save changes", nullptr, "Save changes"},
            {"snd_discard_changes", "Discard changes", nullptr, "Discard changes"},
        };
        for (const auto& b : buttons) {
            auto* button = visibleChild<QToolButton>(view, b.id);
            QVERIFY2(button, b.id);
            QCOMPARE(button->toolTip(),
                     QLatin1String(b.caption) +
                         (b.keys ? QStringLiteral("  (") + keyText(b.keys) + QLatin1Char(')') : QString()));
            QCOMPARE(button->defaultAction()->statusTip(), QLatin1String(b.tip));
        }
        QVERIFY(!view->findChild<QToolButton*>("snd_prev"));
        QVERIFY(!view->findChild<QToolButton*>("snd_next"));
        QCOMPARE(actions.action("snd_prev")->shortcut(), QKeySequence("Ctrl+Left"));
        QCOMPARE(actions.action("snd_next")->shortcut(), QKeySequence("Ctrl+Right"));
        for (const char* name : {"snd_grp", "snd_num"}) {
            auto* spin = view->findChild<QSpinBox*>(QLatin1String(name));
            QVERIFY2(spin, name);
            QCOMPARE(spin->minimum(), 0);
            QCOMPARE(spin->maximum(), 2147483647);
        }
        QCOMPARE(describe(menu(window, "menu_sounds")), (QStringList{"New",
                                                                     "Open",
                                                                     "Save",
                                                                     "Save as...",
                                                                     "|",
                                                                     "Add Ctrl+A",
                                                                     "Delete Ctrl+Del",
                                                                     "Duplicate Ctrl+D",
                                                                     "Change sound... Ctrl+Shift+C",
                                                                     "Save sound as... Ctrl+Alt+S",
                                                                     "Resample Ctrl+Shift+R",
                                                                     "Play Ctrl+P [ ]",
                                                                     "Play with loop Ctrl+Shift+L [ ]",
                                                                     "Pause [ ]",
                                                                     "Stop Ctrl+T [x]",
                                                                     "Delete group",
                                                                     "Rename group",
                                                                     "|",
                                                                     "<Select>",
                                                                     "Remove selected",
                                                                     "Fill silence",
                                                                     "Fade in",
                                                                     "Fade out",
                                                                     "|",
                                                                     "Save changes",
                                                                     "Discard changes",
                                                                     "|",
                                                                     "Export... Ctrl+E",
                                                                     "Import... Ctrl+I",
                                                                     "Import from another project... Ctrl+Shift+I"}));
        QCOMPARE(describe(menu(window, "menu_snd_select")), (QStringList{"All", "Deselect"}));
        actions.action("file_close")->trigger();
    }

    void waveform()
    {
        QTemporaryDir dir;
        openSounds(dir);
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        QVERIFY(window.openProject(dir.filePath("p.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_snd")->trigger();
        auto* page = window.currentProjectView()->findChild<SoundPage*>();
        QVERIFY(page);
        WaveformView* wave = page->view();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        page->setCurrent(0);
        wave->zoomFit();
        const QColor ink = wave->waveColor();
        const int rulerHeight = 20;
        const int bottom = wave->height() - wave->findChild<QScrollBar*>()->height();
        auto count = [&](const QImage& image, int from, int to, int top, int end) {
            const int middle = (top + end) / 2;
            int above = 0;
            int below = 0;
            for (int x = from; x < to; ++x)
                for (int y = top; y < end; ++y)
                    if (image.pixelColor(x, y) == ink)
                        (y < middle ? above : below) += 1;
            return qMakePair(above, below);
        };
        QImage shown = wave->grab().toImage();
        const int width = int(wave->xAt(double(page->samples().frameCount())));
        const auto start = count(shown, 2, width / 4, rulerHeight, bottom);
        const auto end = count(shown, width * 3 / 4, width - 2, rulerHeight, bottom);
        QVERIFY2(start.first == 0 && start.second > 0, "the negative half below the line");
        QVERIFY2(end.first > 0 && end.second == 0, "the positive half above the line");
        const double tick = wave->xAt(8000.0 / 60.0);
        QVERIFY(tick >= 28);
        const QColor rulerInk = wave->palette().color(QPalette::WindowText);
        QCOMPARE(shown.pixelColor(int(tick), rulerHeight - 2).rgb(), rulerInk.rgb());
        wave->setSelection(Formats::SampleRange{100, 200});
        wave->setCursor(1500);
        shown = wave->grab().toImage();
        const int inside = int(wave->xAt(200));
        QCOMPARE(shown.pixelColor(inside, rulerHeight + 3), QColor(240, 140, 30));
        QCOMPARE(shown.pixelColor(int(wave->xAt(1500)), rulerHeight + 3), QColor(40, 120, 255));
        QCOMPARE(count(shown, inside - 3, inside + 3, rulerHeight, bottom).second, 0); // darker in the selection
        page->setCurrent(1);
        wave->zoomFit();
        shown = wave->grab().toImage();
        const int bandEnd = rulerHeight + (bottom - rulerHeight) / 2;
        const int stereoWidth = int(wave->xAt(double(page->samples().frameCount())));
        const auto leftBand = count(shown, 2, stereoWidth - 2, rulerHeight, bandEnd);
        const auto rightBand = count(shown, 2, stereoWidth - 2, bandEnd, bottom);
        QVERIFY(leftBand.first > 0 && leftBand.second == 0);
        QVERIFY(rightBand.first == 0 && rightBand.second > 0);

        page->setCurrent(0);
        wave->zoomFit();
        wave->zoomIn();
        wave->zoomIn();
        actions.action("snd_tool_offset")->trigger();
        const double framesPerPixel = wave->framesPerPixel();
        const int y = (rulerHeight + bottom) / 2;
        sendMouse(wave, QEvent::MouseButtonPress, QPoint(300, y), Qt::LeftButton);
        sendMouse(wave, QEvent::MouseMove, QPoint(200, y), Qt::LeftButton);
        sendMouse(wave, QEvent::MouseButtonRelease, QPoint(200, y), Qt::LeftButton);
        QVERIFY(std::abs(wave->firstFrame() - 100 * framesPerPixel) < 1);
        QCOMPARE(wave->framesPerPixel(), framesPerPixel);
        actions.action("snd_tool_zoom")->trigger();
        const qint64 under = wave->frameAt(250);
        sendMouse(wave, QEvent::MouseButtonPress, QPoint(250, y), Qt::LeftButton);
        sendMouse(wave, QEvent::MouseButtonRelease, QPoint(250, y), Qt::LeftButton);
        QCOMPARE(wave->framesPerPixel(), framesPerPixel / 2);
        QVERIFY(std::abs(wave->frameAt(250) - under) <= 1);
        sendMouse(wave, QEvent::MouseButtonPress, QPoint(250, y), Qt::RightButton);
        sendMouse(wave, QEvent::MouseButtonRelease, QPoint(250, y), Qt::RightButton);
        QCOMPARE(wave->framesPerPixel(), framesPerPixel);
        QVERIFY(std::abs(wave->frameAt(250) - under) <= 1);
        actions.action("snd_tool_select")->trigger();
        QVERIFY(!page->hasChanges());
        actions.action("file_close")->trigger();
    }

    void soundCommands()
    {
        QTemporaryDir dir;
        openSounds(dir);
        writeFile(dir.filePath("other.wav"), toBytes(wav(std::vector<std::int16_t>(300, 7), 1, 11025)));
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("p.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_snd")->trigger();
        auto* page = window.currentProjectView()->findChild<SoundPage*>();
        SoundFile* file = page->file();

        page->setCurrent(2);
        QString caption;
        QString filter;
        FileDialogs::setOpenPromptOverride([&](const QString& c, const QString&, const QString& f) {
            caption = c;
            filter = f;
            return dir.filePath("other.wav");
        });
        actions.action("snd_open_sample")->trigger();
        FileDialogs::setOpenPromptOverride({});
        QVERIFY(!QApplication::activeModalWidget());
        QCOMPARE(caption, QStringLiteral("Change sound..."));
        QVERIFY(filter.startsWith(QStringLiteral("All supported (*.wav *.ogg *.flac *.mp3)")));
        QCOMPARE(file->document().sounds()[2].group, 5);
        QCOMPARE(file->document().sounds()[2].number, 0);
        QCOMPARE(page->samples().frameCount(), std::size_t(300));
        QCOMPARE(page->samples().sampleRate(), 11025u);
        QCOMPARE(file->undoStack()->count(), 1);
        file->undoStack()->undo();

        page->setCurrent(3);
        const Formats::Bytes original = *file->document().sounds()[3].data;
        QVERIFY(
            QByteArray(reinterpret_cast<const char*>(original.data()), qsizetype(original.size())).contains("LIST"));
        actions.action("snd_select_all")->trigger();
        auto* end = window.currentProjectView()->findChild<QSpinBox*>("snd_sel_end");
        end->setValue(100);
        actions.action("snd_fill_silence")->trigger();
        actions.action("snd_save_changes")->trigger();
        const QByteArray edited = toBytes(*file->document().sounds()[3].data);
        QCOMPARE(edited.size(), 44 + 400 * 2);
        QCOMPARE(edited.mid(0, 4), QByteArray("RIFF"));
        QCOMPARE(edited.mid(8, 8), QByteArray("WAVEfmt "));
        QCOMPARE(edited.mid(16, 4), QByteArray("\x10\x00\x00\x00", 4));
        QCOMPARE(edited.mid(36, 4), QByteArray("data"));
        file->undoStack()->undo();

        m_context->config().setValue("confirm_delete", true);
        page->setCurrent(2);
        whenModal<QDialog>([](QDialog* dialog) {
            QCOMPARE(dialog->objectName(), QStringLiteral("DelGrp"));
            QCOMPARE(dialog->findChild<QSpinBox*>("Grp")->value(), 5);
            QVERIFY(dialog->findChild<QRadioButton*>("AllGroup")->isChecked());
            dialog->accept();
        });
        actions.action("snd_delete_grp")->trigger();
        m_context->config().setValue("confirm_delete", false);
        QVERIFY(!QApplication::activeModalWidget());
        QCOMPARE(file->document().count(), 3);
        QCOMPARE(file->document().find(5, 0), -1);
        QCOMPARE(file->document().find(5, 1), -1);
        QCOMPARE(file->undoStack()->count(), 1);
        file->undoStack()->undo();
        page->setCurrent(2);
        whenModal<QDialog>([](QDialog* dialog) {
            QCOMPARE(dialog->objectName(), QStringLiteral("RenGrp"));
            QCOMPARE(dialog->findChild<QSpinBox*>("Grp")->value(), 5);
            dialog->findChild<QRadioButton*>("Intervall")->setChecked(true);
            dialog->findChild<QSpinBox*>("Start")->setValue(1);
            dialog->findChild<QSpinBox*>("End")->setValue(1);
            dialog->findChild<QSpinBox*>("Grp_2")->setValue(9);
            dialog->accept();
        });
        actions.action("snd_rename_grp")->trigger();
        QCOMPARE(file->document().find(5, 0), 2);
        QCOMPARE(file->document().find(9, 1), 3);
        QCOMPARE(file->undoStack()->count(), 1);
        file->undoStack()->undo();
        QVERIFY(!file->isModified());
        actions.action("file_close")->trigger();
    }

private:
    // Sounds 0,0 (mono 8000 Hz, 2000 frames, first half negative, second half positive), 0,1
    // (stereo, left positive, right negative), 5,0, 5,1 (400 frames, a WAV with a LIST chunk) and 7,0.
    static void openSounds(const QTemporaryDir& dir)
    {
        std::vector<std::int16_t> halves(2000);
        for (std::size_t i = 0; i < halves.size(); ++i) {
            const std::int16_t size = (i % 2) ? 16000 : 8000;
            halves[i] = i < 1000 ? std::int16_t(-size) : size;
        }
        std::vector<std::int16_t> stereo(2 * 1000);
        for (std::size_t i = 0; i < 1000; ++i) {
            stereo[2 * i] = (i % 2) ? 16000 : 8000;
            stereo[2 * i + 1] = (i % 2) ? -16000 : -8000;
        }
        // RIFF, fmt (PCM, mono, 8000 Hz, 16 bits), LIST, data.
        QByteArray list;
        auto le32 = [](quint32 v) {
            return QByteArray(1, char(v & 0xFF)) + QByteArray(1, char((v >> 8) & 0xFF)) +
                   QByteArray(1, char((v >> 16) & 0xFF)) + QByteArray(1, char((v >> 24) & 0xFF));
        };
        auto le16 = [](quint16 v) {
            return QByteArray(1, char(v & 0xFF)) + QByteArray(1, char((v >> 8) & 0xFF));
        };
        QByteArray body = "WAVE";
        body += "fmt " + le32(16) + le16(1) + le16(1) + le32(8000) + le32(16000) + le16(2) + le16(16);
        body += "LIST" + le32(4) + "INFO";
        body += "data" + le32(800) + QByteArray(800, char(1));
        list = "RIFF" + le32(quint32(body.size())) + body;
        Formats::SndDocument snd;
        snd.sounds.push_back(Formats::SndSound{0, 0, wav(halves)});
        snd.sounds.push_back(Formats::SndSound{0, 1, wav(stereo, 2)});
        snd.sounds.push_back(Formats::SndSound{5, 0, wav(std::vector<std::int16_t>(500, 300))});
        snd.sounds.push_back(
            Formats::SndSound{5, 1,
                              Formats::Bytes(reinterpret_cast<const std::byte*>(list.constData()),
                                             reinterpret_cast<const std::byte*>(list.constData()) + list.size())});
        snd.sounds.push_back(Formats::SndSound{7, 0, wav(std::vector<std::int16_t>(200, -300))});
        writeSpriteProject(dir, paletteSprites(), toBytes(snd.write()));
    }

    AppContext* m_context = nullptr;
};

QTEST_MAIN(PalettesSoundsParityTest)
#include "PalettesSoundsParityTest.moc"
