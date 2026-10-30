// Tests for the Sprites editor and its image editor.

#include "GuiTestSupport.h"
#include "core/actions/ActionRegistry.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/sprites/ImageDocument.h"
#include "core/sprites/SpriteDocument.h"
#include "core/sprites/SpriteFile.h"
#include "core/widgets/EPanelWidget.h"
#include "core/widgets/GraphicsView.h"
#include "core/widgets/ImageCanvas.h"
#include "core/widgets/PaletteWidget.h"
#include "core/widgets/Rulers.h"
#include "formats/codecs/Png.h"
#include "shell/FileDialogs.h"
#include "shell/ImageEditor.h"
#include "shell/MainWindow.h"
#include "shell/PalettesPanel.h"
#include "shell/ProjectView.h"
#include "shell/SpritePage.h"
#include "shell/SpritesEditor.h"
#include "shell/StatusBar.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>

using namespace OFS;
using namespace OFS::GuiTest;

namespace {

// 256 colours with slot 0 transparent. Slot i holds i in the given channel and 0 in the others.
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

SpriteEntry indexedSprite(int group, int number, int width, int height, std::uint8_t fill,
                          const SpritePalettePtr& palette, QPoint axis = {})
{
    SpriteEntry entry;
    entry.header.group = std::uint16_t(group);
    entry.header.number = std::uint16_t(number);
    entry.header.axisX = std::int16_t(axis.x());
    entry.header.axisY = std::int16_t(axis.y());
    entry.pixels = SpriteDocument::makePixels(
        Formats::Image::indexed(width, height, std::vector<std::uint8_t>(std::size_t(width * height), fill)));
    entry.palette = palette;
    return entry;
}

SpriteEntry rgbaSprite(int group, int number, int width, int height, std::uint8_t alpha)
{
    SpriteEntry entry;
    entry.header.group = std::uint16_t(group);
    entry.header.number = std::uint16_t(number);
    std::vector<std::uint8_t> pixels(std::size_t(width * height * 4), 200);
    for (std::size_t i = 3; i < pixels.size(); i += 4)
        pixels[i] = 255;
    pixels[3] = alpha; // the first pixel decides whether the sprite is 24 or 32-bit
    entry.pixels = SpriteDocument::makePixels(Formats::Image::rgba(width, height, std::move(pixels)));
    return entry;
}

QByteArray toBytes(const Formats::Bytes& bytes)
{
    return QByteArray(reinterpret_cast<const char*>(bytes.data()), qsizetype(bytes.size()));
}

void writeSff(const QString& path, const SpriteDocument& doc, Formats::SffVersion version = Formats::SffVersion::V201)
{
    Formats::SffDocument sff = doc.toSff();
    if (version != sff.version)
        QVERIFY(sff.convertTo(version).ok());
    const auto bytes = sff.write();
    QVERIFY(bytes.ok());
    writeFile(path, toBytes(bytes.value()));
}

void writeProject(const QTemporaryDir& dir, const QString& name, const SpriteDocument& doc, const QByteArray& air = {},
                  Formats::SffVersion version = Formats::SffVersion::V201)
{
    writeSff(dir.filePath(name + ".sff"), doc, version);
    QByteArray def = "[Info]\nname = \"" + name.toUtf8() + "\"\nmugenversion = 1.0\n[Files]\ncns = " + name.toUtf8() +
                     ".cns\nsprite = " + name.toUtf8() + ".sff\n";
    if (!air.isEmpty()) {
        def += "anim = " + name.toUtf8() + ".air\n";
        writeFile(dir.filePath(name + ".air"), air);
    }
    writeFile(dir.filePath(name + ".def"), def);
    writeFile(dir.filePath(name + ".cns"), "[Data]\nlife = 1000\n");
}

// In storage order: 9000,1 (8-bit, own palette 9000,1), 0,0 (24-bit), 0,1 (32-bit), then 0,2, 5,0,
// 5,1 (linked to 5,0), 5,2 and 6,0, all 8-bit on palette 1,1.
SpriteDocument mixedSprites()
{
    SpriteDocument doc;
    doc.palettes() << SpriteDocument::makePalette(1, 1, ramp(0)) << SpriteDocument::makePalette(9000, 1, ramp(1));
    doc.sprites() << indexedSprite(9000, 1, 6, 4, 3, doc.palettes()[1], {3, 4});
    doc.sprites() << rgbaSprite(0, 0, 5, 3, 255) << rgbaSprite(0, 1, 4, 2, 0);
    doc.sprites() << indexedSprite(0, 2, 3, 3, 5, doc.palettes()[0], {1, 2});
    doc.sprites() << indexedSprite(5, 0, 4, 4, 6, doc.palettes()[0], {2, 4});
    SpriteEntry linked = doc.sprites().last();
    linked.header.number = 1;
    doc.sprites() << linked;
    doc.sprites() << indexedSprite(5, 2, 2, 2, 7, doc.palettes()[0]);
    doc.sprites() << indexedSprite(6, 0, 2, 2, 8, doc.palettes()[0]);
    return doc;
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

} // namespace

class SpritesParityTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_context = makeContext();
        Config& config = m_context->config();
        config.setValue("confirm_delete", false);
        config.setValue("switch_to_imgpal", true);
        config.setValue("show_trans_color", false);
        config.setValue("show_addsprite_dialog", true);
        config.setValue("sort_file_names", true);
        config.setValue("show_rulers", true);
        config.setValue("show_cross1", true);
        config.setValue("show_guides", false);
        config.setValue("show_organizer", false);
        config.setValue("reverse_spr_axis", false);
    }

    void cleanupTestCase() { delete m_context; }

    void panelControls()
    {
        QTemporaryDir dir;
        writeProject(dir, "mix", mixedSprites());
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        QVERIFY(window.openProject(dir.filePath("mix.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        ProjectView* view = window.currentProjectView();
        auto* page = view->findChild<SpritePage*>();
        QVERIFY(page);
        SpriteFile* file = page->file();

        const struct
        {
            const char* id;
            const char* caption;
            const char* keys;
            const char* tip;
        } buttons[] = {
            {"spr_add", "Add", "Ctrl+A", "Add one or more sprites"},
            {"spr_delete", "Delete", "Ctrl+Del", "Remove the current sprite"},
            {"spr_duplicate", "Duplicate", "Ctrl+D", "Duplicate the current sprite"},
            {"spr_delete_grp", "Delete group", nullptr, "Delete a group of sprites"},
            {"spr_rename_grp", "Rename group", nullptr, "Rename a group of sprites"},
            {"spr_openimg", "Change image", "Ctrl+Shift+C", "Change the image of the sprite from a file..."},
            {"spr_saveimgas", "Save image", "Ctrl+Alt+S", "Save one or more sprites as image files..."},
            {"spr_edit_img", "Image editor", "Alt+I", "Open the Image editor"},
            {"spr_prev_grp", "Previous group", "Ctrl+Shift+Left", "Previous group of sprites from the current"},
            {"spr_next_grp", "Next group", "Ctrl+Shift+Right", "Next group of sprites from the current"},
            {"spr_pos_up", "Move up", nullptr, "Move the sprite 1px up"},
            {"spr_pos_left", "Move left", nullptr, "Move the sprite 1px to the left"},
            {"spr_pos_right", "Move right", nullptr, "Move the sprite 1px to the right"},
            {"spr_pos_down", "Move down", nullptr, "Move the sprite 1px down"},
        };
        for (const auto& b : buttons) {
            auto* button = visibleChild<QToolButton>(view, b.id);
            QVERIFY2(button, b.id);
            QCOMPARE(button->toolTip(), QLatin1String(b.tip));
            QCOMPARE(button->defaultAction()->statusTip(), QLatin1String(b.tip));
        }
        // Check boxes show the caption, the status bar a longer text.
        const struct
        {
            const char* id;
            const char* caption;
            const char* tip;
        } checks[] = {
            {"spr_dec_load", "Decompress on load", "Decompress on load (M.U.G.E.N. 1.x only)"},
            {"spr_trans_color", "Show transparency color", "Display the current sprite without transparency"},
            {"spr_onion_skin", "OnionSkin", "Enable OnionSkin"},
            {"spr_switch_pal", "Switch to the sprite palette",
             "Switch the current palette to the palette of the sprite"},
        };
        for (const auto& c : checks) {
            auto* box = visibleChild<QCheckBox>(view, c.id);
            QVERIFY2(box, c.id);
            QCOMPARE(box->toolTip(), QLatin1String(c.caption));
            QCOMPARE(box->statusTip(), QLatin1String(c.tip));
        }
        QVERIFY(!view->findChild<QToolButton*>("spr_prev"));
        QVERIFY(!view->findChild<QToolButton*>("spr_next"));
        QCOMPARE(actions.action("spr_prev")->shortcut(), QKeySequence("Ctrl+Left"));
        QCOMPARE(actions.action("spr_next")->shortcut(), QKeySequence("Ctrl+Right"));
        auto* tabs = view->findChild<QTabBar*>("leftPanelTabs");
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 2);
        QCOMPARE(tabs->tabText(0), QStringLiteral("Editor"));
        QCOMPARE(tabs->tabText(1), QStringLiteral("Project explorer"));

        auto* label = view->findChild<QLabel*>("spr_sel_lb");
        auto* info = view->findChild<QLabel*>("spr_info_lb");
        auto* palettes = view->findChild<QLabel*>("pal_sel_lb");
        QVERIFY(label && info && palettes);
        page->setCurrent(0);
        QCOMPARE(label->text(), QStringLiteral("1 / 8"));
        QCOMPARE(info->text(), QStringLiteral("6 x 4 px (8 bit) - unique"));
        QCOMPARE(palettes->text(), QStringLiteral("2 / 2"));
        actions.action("spr_next")->trigger();
        QCOMPARE(label->text(), QStringLiteral("2 / 8"));
        QCOMPARE(info->text(), QStringLiteral("5 x 3 px (24 bit) - unique"));
        QCOMPARE(palettes->text(), QStringLiteral("2 / 2"));
        actions.action("spr_next")->trigger();
        QCOMPARE(info->text(), QStringLiteral("4 x 2 px (32 bit) - unique"));
        QCOMPARE(palettes->text(), QStringLiteral("2 / 2"));
        actions.action("spr_next")->trigger();
        QCOMPARE(palettes->text(), QStringLiteral("1 / 2"));
        page->setCurrent(4);
        QCOMPARE(info->text(), QStringLiteral("4 x 4 px (8 bit) - shared"));
        actions.action("spr_next")->trigger();
        QCOMPARE(info->text(), QStringLiteral("4 x 4 px (8 bit) - shared"));
        QCOMPARE(label->text(), QStringLiteral("6 / 8"));

        actions.action("spr_prev_grp")->trigger();
        QCOMPARE(page->current(), 1);
        actions.action("spr_prev_grp")->trigger();
        QCOMPARE(page->current(), 0);
        actions.action("spr_prev_grp")->trigger();
        QCOMPARE(page->current(), 0);

        auto* fileTabs = view->findChild<QTabWidget*>("fileTabs_sprites");
        QVERIFY(fileTabs);
        QCOMPARE(fileTabs->tabText(fileTabs->currentIndex()), QStringLiteral("mix.sff"));
        const QString title = window.windowTitle();
        page->setCurrent(3);
        auto* group = view->findChild<QSpinBox*>("spr_grp");
        auto* number = view->findChild<QSpinBox*>("spr_num");
        QVERIFY(group && number);
        QTest::keyClick(group, Qt::Key_Up);
        QCOMPARE(file->document().sprites()[3].group(), 1);
        QCOMPARE(file->undoStack()->count(), 1);
        number->setValue(9);
        QCOMPARE(file->document().sprites()[3].number(), 9);
        QCOMPARE(file->document().sprites()[3].group(), 1);
        QCOMPARE(page->current(), 3);
        QCOMPARE(file->undoStack()->count(), 2);
        QCOMPARE(fileTabs->tabText(fileTabs->currentIndex()), QStringLiteral("mix.sff*"));
        QCOMPARE(window.windowTitle(), title);
        file->undoStack()->undo();
        QCOMPARE(number->value(), 2);
        file->undoStack()->undo();
        QCOMPARE(group->value(), 0);
        QCOMPARE(fileTabs->tabText(fileTabs->currentIndex()), QStringLiteral("mix.sff"));

        auto* onion = view->findChild<QCheckBox*>("spr_onion_skin");
        auto* onionSelector = view->findChild<QScrollBar*>("spr_onion_sel");
        auto* onionLabel = view->findChild<QLabel*>("spr_onion_lb");
        QVERIFY(onion && onionSelector && onionLabel);
        page->setCurrent(4);
        onion->setChecked(true);
        QCOMPARE(onionSelector->value(), 4);
        QCOMPARE(onionLabel->text(), QStringLiteral("5 / 8 (5, 0)"));
        page->setCurrent(1);
        actions.action("spr_delete")->trigger();
        QCOMPARE(onionLabel->text(), QStringLiteral("5 / 7 (5, 1)"));
        onion->setChecked(false);
        QVERIFY(onionSelector->isEnabled());
        QVERIFY(onionLabel->isEnabled());
        file->undoStack()->undo();
        actions.action("file_close")->trigger();
    }

    // The name is the SFF v1 comment. Only v1 files can edit it, up to 13 characters.
    void spriteName()
    {
        QTemporaryDir dir;
        SpriteDocument doc;
        doc.palettes() << SpriteDocument::makePalette(1, 1, ramp(0));
        doc.sprites() << indexedSprite(0, 0, 2, 2, 3, doc.palettes()[0]);
        writeProject(dir, "nm", doc);
        SpriteDocument named = doc;
        named.sprites()[0].name = QStringLiteral("abc");
        writeSff(dir.filePath("v1.sff"), named, Formats::SffVersion::V1);
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("nm.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        ProjectView* view = window.currentProjectView();
        auto* name = view->findChild<QLineEdit*>("spr_name");
        QVERIFY(name);
        QVERIFY(!name->isEnabled());
        actions.action("spr_split_channels")->trigger();
        QCOMPARE(name->text(), QStringLiteral(" R"));
        QVERIFY(!name->isEnabled());

        FileDialogs::setOpenPromptOverride(
            [&dir](const QString&, const QString&, const QString&) { return dir.filePath("v1.sff"); });
        actions.action("spr_open")->trigger();
        FileDialogs::setOpenPromptOverride({});
        auto* tabs = view->findChild<QTabWidget*>("fileTabs_sprites");
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 2);
        auto* page = qobject_cast<SpritePage*>(tabs->currentWidget());
        QVERIFY(page);
        QCOMPARE(view->spritesEditor()->page(), page);
        QCOMPARE(page->file()->document().version(), Formats::SffVersion::V1);
        QCOMPARE(name->text(), QStringLiteral("abc"));
        QVERIFY(name->isEnabled());
        QCOMPARE(name->maxLength(), 13);
        name->selectAll();
        QTest::keyClicks(name, "abcdefghijklmnopqrstuvwxyz");
        QTest::keyClick(name, Qt::Key_Return);
        QCOMPARE(page->file()->document().sprites()[0].name, QStringLiteral("abcdefghijklm"));
        QCOMPARE(page->file()->undoStack()->count(), 1);
        page->file()->undoStack()->undo();
        QCOMPARE(name->text(), QStringLiteral("abc"));
        for (ProjectFile* f : view->project()->files()) {
            if (auto* sprites = qobject_cast<SpriteFile*>(f))
                sprites->undoStack()->setClean();
        }
        actions.action("file_close")->trigger();
    }

    void menusInAlignMode()
    {
        QTemporaryDir dir;
        writeProject(dir, "mn", mixedSprites(), "[Begin Action 0]\n0,2, 0,0, 10\n");
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("mn.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        if (!actions.action("spr_frame_spr")->isChecked())
            actions.action("spr_frame_spr")->trigger();

        // "caption[ keys][ (off)][ [x]]" per item, "|" for separators, "<title>" for submenus.
        auto describe = [](const QMenu* menu) {
            QStringList items;
            for (QAction* a : menu->actions()) {
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
        };
        QCOMPARE(describe(menu(window, "menu_sprites")), (QStringList{"New",
                                                                      "Open",
                                                                      "Save",
                                                                      "Save as...",
                                                                      "|",
                                                                      "Add Ctrl+A",
                                                                      "Delete Ctrl+Del",
                                                                      "Duplicate Ctrl+D",
                                                                      "Change image... Ctrl+Shift+C",
                                                                      "Save image changes Alt+S (off)",
                                                                      "Save image... Ctrl+Alt+S",
                                                                      "Save image (aligned)...",
                                                                      "Resize",
                                                                      "Crop image",
                                                                      "Delete group",
                                                                      "Rename group",
                                                                      "Alignment Tool Ctrl+Alt+A",
                                                                      "Image editor Alt+I [ ]",
                                                                      "Split channels",
                                                                      "<Layers>",
                                                                      "<Image>",
                                                                      "<Select>",
                                                                      "<Rotation>",
                                                                      "|",
                                                                      "Export... Ctrl+E",
                                                                      "Import... Ctrl+I",
                                                                      "Import from another project... Ctrl+Shift+I",
                                                                      "|",
                                                                      "Switch to the frame's sprite [x]",
                                                                      "|",
                                                                      "Sprite explorer (off) [ ]"}));
        QCOMPARE(describe(menu(window, "menu_spr_layers")),
                 (QStringList{"Add (off)", "Delete (off)", "Duplicate (off)", "|", "Merge visible (off)",
                              "Merge all (off)", "|", "Save visible as sprites (off)", "Play (off)"}));
        QCOMPARE(describe(menu(window, "menu_spr_image")), (QStringList{"Original (off) [ ]", "Properties (off)"}));
        QCOMPARE(describe(menu(window, "menu_spr_select")),
                 (QStringList{"All (off)", "Deselect (off)", "Invert (off)"}));
        QCOMPARE(describe(menu(window, "menu_spr_rotation")),
                 (QStringList{"Rotate (off)", "Flip horizontally (off)", "Flip vertically (off)"}));
        for (const char* name : {"menu_spr_layers", "menu_spr_image", "menu_spr_select", "menu_spr_rotation"})
            QVERIFY2(menu(window, name)->menuAction()->isEnabled(), name);

        actions.action("view_secondary_cross")->setChecked(true);
        actions.action("view_extra_cross")->setChecked(false);
        QCOMPARE(describe(menu(window, "menu_zoom")), (QStringList{"Actual size Ctrl+0", "Fit screen size",
                                                                   "Decrease zoom Ctrl+-", "Increase zoom Ctrl++"}));
        const QStringList viewItems = describe(menu(window, "menu_view"));
        const QStringList tail = viewItems.mid(viewItems.indexOf(QStringLiteral("Centralize origin Ctrl+?")));
        QCOMPARE(tail, (QStringList{"Centralize origin Ctrl+?", "Main cross [x]", "Secondary cross (off) [x]",
                                    "Extra cross (off) [ ]", "Rulers [x]", "Guide lines [ ]", "Attack Box [x]",
                                    "Collision Box [x]"}));
        QVERIFY(viewItems.contains(QStringLiteral("Code toolbar (off) [x]")) ||
                viewItems.contains(QStringLiteral("Code toolbar (off) [ ]")));
        actions.action("editor_anim")->trigger();
        QVERIFY(actions.action("view_secondary_cross")->isEnabled());
        QVERIFY(actions.action("view_extra_cross")->isEnabled());
        actions.action("editor_spr")->trigger();

        QString caption;
        QString filter;
        FileDialogs::setOpenPromptOverride([&](const QString& c, const QString&, const QString& f) {
            caption = c;
            filter = f;
            return QString();
        });
        auto* page = window.currentProjectView()->findChild<SpritePage*>();
        actions.action("spr_import")->trigger();
        FileDialogs::setOpenPromptOverride({});
        QCOMPARE(caption, QStringLiteral("Import..."));
        QCOMPARE(filter, QStringLiteral("Fighter Factory Export (*.ffe)"));
        QCOMPARE(page->file()->document().spriteCount(), 8);
        QVERIFY(!page->file()->isModified());
        actions.action("file_close")->trigger();
    }

    void deletingAndSplitting()
    {
        QTemporaryDir dir;
        writeProject(dir, "dl", mixedSprites());
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("dl.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        auto* page = window.currentProjectView()->findChild<SpritePage*>();
        QVERIFY(page);
        SpriteFile* file = page->file();
        Config& config = m_context->config();
        config.setValue("confirm_delete", true);
        page->setCurrent(3);

        QString seen;
        whenModal<QMessageBox>([&seen](QMessageBox* box) {
            seen = box->windowTitle() + "|" + box->text() + "|" + (box->checkBox() ? box->checkBox()->text() : "") +
                   "|" + (box->defaultButton() == box->button(QMessageBox::Yes) ? "Yes" : "?");
            box->button(QMessageBox::No)->click();
        });
        actions.action("spr_delete")->trigger();
        QCOMPARE(seen, boxTitle("Delete") + "|Do you really want to delete the sprite?|Don't show this again|Yes");
        QCOMPARE(file->document().spriteCount(), 8);
        answerBoxes({QMessageBox::Yes}, nullptr);
        actions.action("spr_delete")->trigger();
        QCOMPARE(file->document().spriteCount(), 7);
        QCOMPARE(page->current(), 3);
        QCOMPARE(file->document().sprites()[3].group(), 5);
        QCOMPARE(view(window)->findChild<QSpinBox*>("spr_grp")->value(), 5);
        QCOMPARE(file->undoStack()->count(), 1);
        file->undoStack()->undo();
        QCOMPARE(file->document().spriteCount(), 8);
        QCOMPARE(file->document().sprites()[3].number(), 2);
        QCOMPARE(file->document().sprites()[3].group(), 0);
        config.setValue("confirm_delete", false);
        page->setCurrent(7);
        actions.action("spr_delete")->trigger();
        QCOMPARE(page->current(), 6);
        QCOMPARE(view(window)->findChild<QLabel*>("spr_sel_lb")->text(), QStringLiteral("7 / 7"));
        file->undoStack()->undo();

        page->setCurrent(1);
        actions.action("spr_split_channels")->trigger();
        QCOMPARE(page->current(), 1);
        QCOMPARE(file->document().sprites()[1].name, QStringLiteral(" R"));
        QCOMPARE(file->document().sprites()[2].name, QStringLiteral(" G"));
        QCOMPARE(file->document().sprites()[3].name, QStringLiteral(" B"));
        while (file->undoStack()->canUndo())
            file->undoStack()->undo();
        actions.action("file_close")->trigger();
    }

    // Back to front the workspace draws the background, onion skin, sprite, cross, guides and rulers.
    void workspaceView()
    {
        QTemporaryDir dir;
        Formats::Palette colors = ramp(0);
        colors[1] = {255, 0, 0, 255};
        colors[2] = {0, 0, 255, 255};
        SpriteDocument doc;
        doc.palettes() << SpriteDocument::makePalette(1, 1, colors);
        doc.sprites() << indexedSprite(0, 0, 20, 20, 1, doc.palettes()[0], {10, 10});
        doc.sprites() << indexedSprite(0, 1, 20, 20, 2, doc.palettes()[0]);
        writeProject(dir, "ws", doc);
        Config& config = m_context->config();
        config.setValue("onion_effect", 0);
        config.setValue("onion_opacity", "0.74");
        config.setValue("background_type", 1);
        config.setValue("background_x", -426);
        config.setValue("background_y", -416);
        config.setValue("show_guides", true);
        config.setValue("sprite_over_guides", false);
        config.setValue("zoom_mode", 1);
        config.setValue("zoom_center", 0);
        config.setValue("crosshair_type", 0);
        MainWindow window(*m_context);
        window.resize(1200, 900);
        window.show();
        QVERIFY(window.openProject(dir.filePath("ws.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        if (!actions.action("view_main_cross")->isChecked())
            actions.action("view_main_cross")->trigger();
        if (!actions.action("view_rulers")->isChecked())
            actions.action("view_rulers")->trigger();
        auto* page = view(window)->findChild<SpritePage*>();
        QVERIFY(page);
        GraphicsView* workspace = page->view();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(workspace->width() > 500 && workspace->height() > 400);
        page->setCurrent(0);
        auto* onion = view(window)->findChild<QCheckBox*>("spr_onion_skin");
        auto* onionSelector = view(window)->findChild<QScrollBar*>("spr_onion_sel");
        onion->setChecked(true);
        onionSelector->setValue(1);
        workspace->setZoomIndex(2); // 1x
        workspace->setOrigin(QPoint(300, 300));
        workspace->setGuides(QPoint(5, -5));
        auto shown = [workspace] {
            return workspace->grab().toImage();
        };
        auto at = [workspace](const QImage& image, double x, double y) {
            return image.pixelColor(workspace->toWidget(QPointF(x, y)).toPoint());
        };
        const QImage backdrop = GraphicsView::defaultBackground();
        QImage image = shown();
        QCOMPARE(at(image, -200, -100), backdrop.pixelColor(226, 316));
        QCOMPARE(at(image, -150, 0), backdrop.pixelColor(276, 416));
        QCOMPARE(at(image, 3, 3), QColor(255, 0, 0));
        const QColor behind = backdrop.pixelColor(426 + 15, 416 + 15);
        const QColor mixed = at(image, 15, 15);
        QVERIFY2(std::abs(mixed.blue() - int(std::lround(0.74 * 255 + 0.26 * behind.blue()))) <= 2,
                 qPrintable(mixed.name()));
        QVERIFY(std::abs(mixed.red() - int(std::lround(0.26 * behind.red()))) <= 2);
        QCOMPARE(at(image, 3, 0), QColor(Qt::black));
        QCOMPARE(at(image, 7, 0), QColor(Qt::white));
        QCOMPARE(at(image, 5, 0), QColor(0, 255, 255));
        QCOMPARE(at(image, 5, -8), QColor(0, 255, 255));
        QCOMPARE(at(image, -8, -5), QColor(0, 255, 255));
        workspace->setOrigin(QPoint(300, 25));
        image = shown();
        const Rulers::Colours rulers = Rulers::Colours::from(workspace->palette());
        QCOMPARE(image.pixelColor(295, 10).rgb(), rulers.band.rgb());
        QCOMPARE(image.pixelColor(295, 30), QColor(255, 0, 0));
        QCOMPARE(image.pixelColor(300, 19).rgb(), rulers.ink.rgb());
        QCOMPARE(image.pixelColor(310, 19).rgb(), rulers.ink.rgb());
        QCOMPARE(image.pixelColor(305, 19).rgb(), rulers.strip.rgb());
        QCOMPARE(image.pixelColor(300, 15).rgb(), rulers.ink.rgb());
        QCOMPARE(image.pixelColor(310, 15).rgb(), rulers.strip.rgb());
        // The numbered step is the largest multiple of 5 up to 50 / zoom, at least 5. Five ticks per number.
        const int majors[] = {200, 100, 50, 25, 15, 10, 10, 5, 5, 5};
        for (int i = 0; i < 10; ++i) {
            int minor = 0;
            int major = 0;
            Rulers::steps(GraphicsView::largeZoomRange()[i], &minor, &major);
            QCOMPARE(major, majors[i]);
            QCOMPARE(minor, std::max(1, majors[i] / 5));
        }

        workspace->setOrigin(QPoint(300, 300));
        workspace->setGuides(QPoint(-100, -100));
        workspace->setZoomIndex(4);
        QCOMPARE(workspace->zoom(), 3.0);
        image = shown();
        const QPoint arm = workspace->toWidget(QPointF(3, 0)).toPoint();
        for (const QPoint d : {QPoint(0, 0), QPoint(2, 0), QPoint(0, 2), QPoint(2, 2)})
            QCOMPARE(image.pixelColor(arm + d), QColor(Qt::black));
        QCOMPARE(image.pixelColor(arm + QPoint(0, 3)), QColor(Qt::white));
        QCOMPARE(image.pixelColor(arm + QPoint(0, 5)), QColor(Qt::white));
        QCOMPARE(image.pixelColor(arm + QPoint(0, 6)), QColor(255, 0, 0));
        actions.action("view_main_cross")->trigger();
        QVERIFY(!config.boolValue("show_cross1", true));
        QCOMPARE(shown().pixelColor(arm), QColor(255, 0, 0));
        actions.action("view_main_cross")->trigger();
        QCOMPARE(shown().pixelColor(arm), QColor(Qt::black));
        actions.action("view_rulers")->trigger();
        QVERIFY(!workspace->rulersVisible());
        QCOMPARE(workspace->origin(), QPoint(300, 300));
        actions.action("view_rulers")->trigger();
        QVERIFY(workspace->rulersVisible());
        actions.action("view_guide_lines")->trigger();
        QVERIFY(!workspace->guidesVisible());
        actions.action("view_guide_lines")->trigger();

        actions.action("view_zoomin")->trigger();
        QCOMPARE(config.intValue("zoom_level"), 5);
        workspace->setOrigin(QPoint(123, 234));
        QCOMPARE(config.intValue("origin_x"), 123);
        QCOMPARE(config.intValue("origin_y"), 234);
        actions.action("view_center_origin")->trigger();
        QCOMPARE(workspace->origin(), workspace->rect().center());

        const QPoint origin = workspace->origin();
        const QPoint onSprite = workspace->toWidget(QPointF(2.5, 2.5)).toPoint();
        for (Qt::MouseButton button : {Qt::RightButton, Qt::MiddleButton}) {
            QMouseEvent press(QEvent::MouseButtonPress, onSprite, workspace->mapToGlobal(onSprite), button, button,
                              Qt::NoModifier);
            QCoreApplication::sendEvent(workspace, &press);
            const QPoint to = onSprite + QPoint(40, 30);
            QMouseEvent move(QEvent::MouseMove, to, workspace->mapToGlobal(to), Qt::NoButton, button, Qt::NoModifier);
            QCoreApplication::sendEvent(workspace, &move);
            QMouseEvent release(QEvent::MouseButtonRelease, to, workspace->mapToGlobal(to), button, Qt::NoButton,
                                Qt::NoModifier);
            QCoreApplication::sendEvent(workspace, &release);
            QCOMPARE(workspace->origin(), origin);
            QVERIFY(!QApplication::activePopupWidget());
        }
        QVERIFY(!page->file()->isModified());
        onion->setChecked(false);
        config.setValue("onion_effect", 2);
        config.setValue("show_guides", false);
        actions.action("file_close")->trigger();
    }

    void storedZoomAndOrigin()
    {
        QTemporaryDir dir;
        writeProject(dir, "zo", mixedSprites());
        Config& config = m_context->config();
        config.setValue("zoom_level", 3);
        config.setValue("origin_x", 210);
        config.setValue("origin_y", 160);
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("zo.def")));
        m_context->actions().action("editor_spr")->trigger();
        GraphicsView* workspace = view(window)->findChild<SpritePage*>()->view();
        QCOMPARE(workspace->zoomIndex(), 3);
        QCOMPARE(workspace->zoom(), 2.0);
        QCOMPARE(workspace->origin(), QPoint(210, 160));
        m_context->actions().action("file_close")->trigger();
    }

    void rangeGroupAndAlignmentDialogs()
    {
        QTemporaryDir dir;
        SpriteDocument doc;
        doc.palettes() << SpriteDocument::makePalette(1, 1, ramp(0));
        std::vector<std::uint8_t> bordered(16, 0);
        for (int i : {5, 6, 9, 10})
            bordered[std::size_t(i)] = 4;
        for (int n = 0; n < 7; ++n) {
            SpriteEntry entry = indexedSprite(n < 6 ? 20 : 30, n < 6 ? n : 0, 4, 4, 0, doc.palettes()[0], {2, 4});
            entry.pixels = SpriteDocument::makePixels(Formats::Image::indexed(4, 4, bordered));
            doc.sprites() << entry;
        }
        writeProject(dir, "rg", doc);
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("rg.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        auto* page = view(window)->findChild<SpritePage*>();
        SpriteFile* file = page->file();
        page->setCurrent(2);

        whenModal<QDialog>([](QDialog* range) {
            QCOMPARE(range->objectName(), QStringLiteral("SaveItem"));
            auto* group = range->findChild<QSpinBox*>("grpNum");
            QCOMPARE(group->value(), 20);
            QCOMPARE(group->maximum(), 65535);
            for (const char* name : {"grpNum", "interval", "start", "end"})
                QVERIFY2(range->findChild<QWidget*>(name)->isEnabled(), name);
            QCOMPARE(range->findChild<QSpinBox*>("start")->value(), 0);
            QCOMPARE(range->findChild<QSpinBox*>("end")->value(), 0);
            range->findChild<QRadioButton*>("grp")->setChecked(true);
            range->findChild<QCheckBox*>("interval")->setChecked(true);
            range->findChild<QSpinBox*>("start")->setValue(3);
            range->findChild<QSpinBox*>("end")->setValue(5);
            range->accept();
        });
        actions.action("spr_crop")->trigger();
        for (int i = 0; i < 7; ++i)
            QCOMPARE(file->document().sprites()[i].width(), i >= 3 && i <= 5 ? 2 : 4);
        QCOMPARE(file->undoStack()->count(), 1);
        file->undoStack()->undo();

        whenModal<QDialog>([](QDialog* dialog) {
            QCOMPARE(dialog->objectName(), QStringLiteral("RenGrp"));
            QCOMPARE(dialog->windowTitle(), QStringLiteral("Rename group"));
            QCOMPARE(dialog->findChild<QSpinBox*>("Grp")->value(), 20);
            QVERIFY(dialog->findChild<QRadioButton*>("AllGroup")->isChecked());
            QCOMPARE(dialog->findChild<QSpinBox*>("Grp_2")->value(), 0);
            QVERIFY(dialog->findChild<QSpinBox*>("Start")->isEnabled());
            dialog->findChild<QRadioButton*>("Intervall")->setChecked(true);
            dialog->findChild<QSpinBox*>("Start")->setValue(1);
            dialog->findChild<QSpinBox*>("End")->setValue(2);
            dialog->findChild<QSpinBox*>("Grp_2")->setValue(30);
            dialog->accept();
        });
        actions.action("spr_rename_grp")->trigger();
        QVERIFY(!QApplication::activeModalWidget());
        QCOMPARE(file->document().sprites()[1].group(), 30);
        QCOMPARE(file->document().sprites()[2].group(), 30);
        QCOMPARE(file->document().sprites()[3].group(), 20);
        QCOMPARE(file->document().sprites()[2].number(), 2);
        QCOMPARE(file->undoStack()->count(), 1);
        file->undoStack()->undo();

        whenModal<QDialog>([](QDialog* dialog) {
            QCOMPARE(dialog->objectName(), QStringLiteral("SprAlign"));
            auto* list = dialog->findChild<QListWidget*>("list");
            QCOMPARE(list->count(), 7);
            QCOMPARE(list->item(0)->text(), QStringLiteral("20, 0"));
            QCOMPARE(list->item(6)->text(), QStringLiteral("30, 0"));
            QCOMPARE(list->selectedItems().size(), 1);
            QVERIFY(list->item(2)->isSelected());
            dialog->findChild<QPushButton*>("pushButton_3")->click();
            QCOMPARE(list->selectedItems().size(), 6);
            QVERIFY(!list->item(2)->isSelected());
            dialog->findChild<QPushButton*>("pushButton_4")->click();
            QVERIFY(list->selectedItems().isEmpty());
            dialog->findChild<QPushButton*>("pushButton_5")->click();
            QCOMPARE(list->selectedItems().size(), 7);
            QCOMPARE(dialog->findChild<QSpinBox*>("xVal")->minimum(), -30000);
            QCOMPARE(dialog->findChild<QSpinBox*>("yVal")->maximum(), 30000);
            dialog->findChild<QSpinBox*>("yVal")->setValue(-3);
            dialog->findChild<QPushButton*>("pushButton")->click();
            dialog->findChild<QPushButton*>("pushButton_2")->click();
        });
        actions.action("spr_align")->trigger();
        for (int i = 0; i < 7; ++i)
            QCOMPARE(int(file->document().sprites()[i].header.axisY), 1);
        while (file->undoStack()->canUndo())
            file->undoStack()->undo();
        actions.action("file_close")->trigger();
    }

    void imageFilesAndExport()
    {
        QTemporaryDir dir;
        SpriteDocument doc;
        doc.palettes() << SpriteDocument::makePalette(1, 1, ramp(0));
        doc.sprites() << indexedSprite(11, 0, 49, 92, 3, doc.palettes()[0], {17, 91});
        doc.sprites() << indexedSprite(11, 1, 49, 80, 4, doc.palettes()[0], {17, 79});
        doc.sprites() << indexedSprite(11, 2, 50, 75, 5, doc.palettes()[0], {18, 73});
        doc.sprites() << indexedSprite(0, 0, 2, 2, 6, doc.palettes()[0]);
        writeProject(dir, "im", doc);
        QVERIFY(QDir(dir.path()).mkpath("pictures"));
        QVERIFY(QDir(dir.path()).mkpath("out"));
        Config& config = m_context->config();
        config.setValue("spr_path", dir.filePath("pictures"));
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("im.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        auto* page = view(window)->findChild<SpritePage*>();
        SpriteFile* file = page->file();
        page->setCurrent(0);

        QString caption;
        QString folder;
        QString filter;
        auto record = [&](const QString& answer) {
            return [&, answer](const QString& c, const QString& d, const QString& f) {
                caption = c;
                folder = d;
                filter = f;
                return answer;
            };
        };
        FileDialogs::setOpenPromptOverride(record(QString()));
        actions.action("spr_openimg")->trigger();
        QCOMPARE(caption, QStringLiteral("Change image..."));
        QCOMPARE(filter, QStringLiteral("All supported (*.pcx *.png *.gif *.bmp);;Personal Computer eXchange (*.pcx);;"
                                        "Portable Network Graphic (*.png);;Graphics Interchange Format (*.gif);;"
                                        "Bitmap (*.bmp)"));
        QVERIFY(!file->isModified());

        FileDialogs::setSavePromptOverride(record(QString()));
        actions.action("spr_saveimgas")->trigger();
        QCOMPARE(caption, QStringLiteral("Save image..."));
        QCOMPARE(QDir(folder), QDir(dir.filePath("pictures")));
        QVERIFY(filter.startsWith(QStringLiteral("Portable Network Graphic (*.png);;")));

        whenModal<QDialog>([](QDialog* range) {
            QCOMPARE(range->objectName(), QStringLiteral("SaveItem"));
            range->findChild<QRadioButton*>("grp")->setChecked(true);
            range->accept();
        });
        FileDialogs::setSavePromptOverride(record(dir.filePath("out/al.png")));
        QString asked;
        whenModal<QMessageBox>([&asked](QMessageBox* box) {
            asked = box->windowTitle() + "|" + box->text() + "|" +
                    (box->defaultButton() == box->button(QMessageBox::No) ? "No" : "?");
            box->button(QMessageBox::Yes)->click();
        });
        actions.action("spr_saveimg_aligned")->trigger();
        FileDialogs::setSavePromptOverride({});
        QCOMPARE(caption, QStringLiteral("Save image (aligned)..."));
        QCOMPARE(asked, boxTitle("Perfect axis") +
                            "|The axis is X: 18, Y: 91. Do you want to save this information to a file?|No");
        for (const char* name : {"out/al_11-0.png", "out/al_11-1.png", "out/al_11-2.png"})
            QCOMPARE(QImage(dir.filePath(name)).size(), QSize(50, 93));
        QCOMPARE(readFile(dir.filePath("out/axis.txt")), QByteArray("X axis: 18\r\nY axis: 91"));

        whenModal<QDialog>([](QDialog* dialog) {
            QCOMPARE(dialog->objectName(), QStringLiteral("ExpDlg"));
            QCOMPARE(dialog->windowTitle(), QStringLiteral("Export"));
            QVERIFY(dialog->findChild<QRadioButton*>("all")->isChecked());
            auto* group = dialog->findChild<QSpinBox*>("grp");
            QCOMPARE(group->value(), 0);
            QVERIFY(group->isEnabled());
            dialog->findChild<QRadioButton*>("allgrp")->setChecked(true);
            dialog->accept();
        });
        FileDialogs::setSavePromptOverride(record(dir.filePath("out/grp0.def")));
        actions.action("spr_export")->trigger();
        QCOMPARE(caption, QStringLiteral("Export..."));
        QCOMPARE(filter, QStringLiteral("Fighter Factory Export (*.ffe);;Elecbyte SprMaker 2 (*.def)"));
        QVERIFY(readFile(dir.filePath("out/grp0.def")).startsWith("[Output]\r\n"));
        QVERIFY(QFileInfo::exists(dir.filePath("out/0-0.png")));
        QVERIFY(!QFileInfo::exists(dir.filePath("out/11-0.png")));
        whenModal<QDialog>([](QDialog* dialog) { dialog->accept(); });
        FileDialogs::setSavePromptOverride(record(dir.filePath("out/all.ffe")));
        actions.action("spr_export")->trigger();
        FileDialogs::setSavePromptOverride({});
        FileDialogs::setOpenPromptOverride({});
        QVERIFY(readFile(dir.filePath("out/all.ffe")).startsWith(";0 always import the palette"));
        QVERIFY(QFileInfo::exists(dir.filePath("out/11-0.png")));
        QVERIFY(!file->isModified());
        actions.action("file_close")->trigger();
    }

    void addSprites()
    {
        QTemporaryDir dir;
        SpriteDocument doc;
        doc.palettes() << SpriteDocument::makePalette(1, 1, ramp(0));
        doc.sprites() << indexedSprite(0, 0, 2, 2, 3, doc.palettes()[0], {17, 91});
        writeProject(dir, "ad", doc);
        QVERIFY(QDir(dir.path()).mkpath("pictures"));
        for (const char* name : {"b.png", "a.png"}) {
            Formats::Palette colors = ramp(2);
            colors[1] = {name[0] == 'a' ? std::uint8_t(10) : std::uint8_t(20), 0, 0, 255};
            const auto png = Formats::encodePng(Formats::Image::indexed(3, 2, {1, 1, 1, 1, 1, 1}, colors));
            QVERIFY(png.ok());
            writeFile(QDir(dir.filePath("pictures")).filePath(QLatin1String(name)), toBytes(png.value()));
        }
        Config& config = m_context->config();
        config.setValue("spr_path", dir.filePath("pictures"));
        config.setValue("show_addsprite_dialog", true);
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("ad.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        auto* page = view(window)->findChild<SpritePage*>();
        SpriteFile* file = page->file();
        QString folder;
        const QString files =
            QDir(dir.filePath("pictures")).filePath("b.png") + "\n" + QDir(dir.filePath("pictures")).filePath("a.png");
        FileDialogs::setOpenPromptOverride([&](const QString& caption, const QString& d, const QString&) {
            folder = d;
            return caption == QLatin1String("Add") ? files : QString();
        });

        whenModal<QDialog>([](QDialog* add) {
            QCOMPARE(add->objectName(), QStringLiteral("AddSpr"));
            QCOMPARE(add->windowTitle(), QStringLiteral("Add sprites..."));
            auto* detect = add->findChild<QCheckBox*>("detect");
            auto* group = add->findChild<QSpinBox*>("Grp");
            auto* index = add->findChild<QSpinBox*>("Img");
            auto* ones = add->findChild<QRadioButton*>("OneInc");
            auto* tens = add->findChild<QRadioButton*>("TenInc");
            auto* x = add->findChild<QSpinBox*>("AxisX");
            auto* y = add->findChild<QSpinBox*>("AxisY");
            auto* palette = add->findChild<QComboBox*>("pal");
            auto* paletteGroup = add->findChild<QSpinBox*>("pGrp");
            auto* paletteIndex = add->findChild<QSpinBox*>("pIdx");
            auto* crop = add->findChild<QComboBox*>("cropImg");
            auto* share = add->findChild<QComboBox*>("shareMode");
            QVERIFY(!detect->isChecked());
            QCOMPARE(group->value(), 0);
            QCOMPARE(group->maximum(), 65535);
            QCOMPARE(index->value(), 0);
            QVERIFY(ones->isChecked());
            QCOMPARE(x->value(), 17);
            QCOMPARE(y->value(), 91);
            QCOMPARE(x->minimum(), -32767);
            QVERIFY(!add->findChild<QCheckBox*>("reversed")->isChecked());
            QCOMPARE(palette->currentIndex(), 0);
            QCOMPARE(palette->count(), 3);
            QCOMPARE(palette->itemText(0), QStringLiteral("Image's palette, if not already added"));
            QCOMPARE(paletteGroup->value(), 1);
            QCOMPARE(paletteIndex->value(), 1);
            QVERIFY(!paletteGroup->isEnabled() && !paletteIndex->isEnabled());
            palette->setCurrentIndex(1);
            QVERIFY(paletteGroup->isEnabled() && paletteIndex->isEnabled());
            palette->setCurrentIndex(2);
            QVERIFY(paletteGroup->isEnabled());
            palette->setCurrentIndex(0);
            QVERIFY(!paletteGroup->isEnabled());
            QCOMPARE(crop->currentText(), QStringLiteral("Don't crop the image"));
            QCOMPARE(crop->count(), 3);
            QCOMPARE(share->currentText(), QStringLiteral("Share identical sprites"));
            QCOMPARE(share->count(), 3);
            for (const char* name :
                 {"AxisNO", "AxisN", "AxisNL", "AxisO", "AxisC", "AxisL", "AxisSO", "AxisS", "AxisSL"})
                QVERIFY2(!add->findChild<QToolButton*>(name)->isChecked(), name);
            detect->setChecked(true);
            QVERIFY(!group->isEnabled() && !index->isEnabled() && !ones->isEnabled() && !tens->isEnabled());
            detect->setChecked(false);
            QVERIFY(group->isEnabled() && index->isEnabled() && tens->isEnabled());
            auto* bottom = add->findChild<QToolButton*>("AxisS");
            auto* topLeft = add->findChild<QToolButton*>("AxisNO");
            bottom->click();
            QVERIFY(bottom->isChecked());
            QVERIFY(!x->isEnabled() && !y->isEnabled());
            topLeft->click();
            QVERIFY(topLeft->isChecked() && !bottom->isChecked());
            topLeft->click();
            QVERIFY(!topLeft->isChecked());
            QVERIFY(x->isEnabled() && y->isEnabled());
            group->setValue(55);
            add->reject();
        });
        actions.action("spr_add")->trigger();
        QCOMPARE(QDir(folder), QDir(dir.filePath("pictures")));
        QCOMPARE(file->document().spriteCount(), 1);

        whenModal<QDialog>([](QDialog* add) {
            QCOMPARE(add->findChild<QSpinBox*>("Grp")->value(), 0);
            add->findChild<QSpinBox*>("Grp")->setValue(9100);
            add->accept();
        });
        actions.action("spr_add")->trigger();
        QCOMPARE(file->document().spriteCount(), 3);
        QCOMPARE(file->document().sprites()[1].number(), 0);
        QCOMPARE(file->document().image(1).palette()[1].r, std::uint8_t(10));
        QCOMPARE(file->document().image(2).palette()[1].r, std::uint8_t(20));
        QCOMPARE(page->current(), 2);
        file->undoStack()->undo();
        config.setValue("sort_file_names", false);
        page->setCurrent(0);
        whenModal<QDialog>([](QDialog* add) { add->accept(); });
        actions.action("spr_add")->trigger();
        QCOMPARE(file->document().image(1).palette()[1].r, std::uint8_t(20));
        config.setValue("sort_file_names", true);
        FileDialogs::setOpenPromptOverride({});
        while (file->undoStack()->canUndo())
            file->undoStack()->undo();
        actions.action("file_close")->trigger();
    }

    // A click in the Organizer only selects. A double-click makes the sprite current.
    void organizer()
    {
        QTemporaryDir dir;
        writeProject(dir, "og", mixedSprites());
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        QVERIFY(window.openProject(dir.filePath("og.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        auto* page = view(window)->findChild<SpritePage*>();
        auto* right = view(window)->findChild<EPanelWidget*>("rightPanel");
        QVERIFY(page && right);
        QCOMPARE(right->title(), QStringLiteral("Palettes"));
        actions.action("view_organizer")->setChecked(true);
        QCOMPARE(right->title(), QStringLiteral("Organizer"));
        auto* list = view(window)->findChild<QListView*>("org_list");
        QVERIFY(list && list->isVisible());
        QCOMPARE(list->model()->rowCount(), 8);
        QCOMPARE(list->model()->index(0, 0).data().toString(), QStringLiteral("9000, 1"));
        QVERIFY(!list->model()->index(0, 0).data(Qt::DecorationRole).isValid());
        page->setCurrent(1);
        actions.action("spr_split_channels")->trigger();
        QCOMPARE(list->model()->index(1, 0).data().toString(), QStringLiteral("0, 0 -  R"));

        page->setCurrent(0);
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const QRect row = list->visualRect(list->model()->index(5, 0));
        QVERIFY(row.isValid());
        QWidget* viewport = list->viewport();
        auto send = [viewport](QEvent::Type type, QPoint at) {
            QMouseEvent event(type, at, viewport->mapToGlobal(at), Qt::LeftButton,
                              type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(viewport, &event);
        };
        send(QEvent::MouseButtonPress, row.center());
        send(QEvent::MouseButtonRelease, row.center());
        QCOMPARE(list->selectionModel()->selectedRows().value(0).row(), 5);
        QCOMPARE(page->current(), 0);
        list->setFocus();
        QTest::keyClick(list, Qt::Key_Down);
        QCOMPARE(list->selectionModel()->selectedRows().value(0).row(), 6);
        QCOMPARE(page->current(), 0);
        send(QEvent::MouseButtonPress, row.center());
        send(QEvent::MouseButtonRelease, row.center());
        send(QEvent::MouseButtonDblClick, row.center());
        send(QEvent::MouseButtonRelease, row.center());
        QCOMPARE(page->current(), 5);
        list->selectionModel()->select(list->model()->index(0, 0), QItemSelectionModel::ClearAndSelect);
        actions.action("org_bottom")->trigger();
        QCOMPARE(page->file()->document().sprites().last().group(), 9000);
        page->file()->undoStack()->undo();

        auto* palettes = view(window)->findChild<QToolButton*>("org_palette");
        QVERIFY(palettes);
        palettes->click();
        QVERIFY(palettes->isChecked());
        QStringList rows;
        for (int r = 0; r < list->model()->rowCount(); ++r)
            rows << list->model()->index(r, 0).data().toString();
        const SpriteDocument& doc = page->file()->document();
        QStringList expected;
        for (const SpritePalettePtr& palette : doc.palettes())
            expected << QStringLiteral("%1, %2").arg(palette->palette.group).arg(palette->palette.number);
        QCOMPARE(rows, expected);
        QCOMPARE(rows.mid(0, 2), (QStringList{"1, 1", "9000, 1"}));
        palettes->click();
        actions.action("view_organizer")->setChecked(false);
        QCOMPARE(right->title(), QStringLiteral("Palettes"));
        while (page->file()->undoStack()->canUndo())
            page->file()->undoStack()->undo();
        actions.action("file_close")->trigger();
    }

    void imageModePanel()
    {
        QTemporaryDir dir;
        writeProject(dir, "ip", mixedSprites());
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        QVERIFY(window.openProject(dir.filePath("ip.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        ProjectView* projectView = view(window);
        auto* page = projectView->findChild<SpritePage*>();
        page->setCurrent(3);
        auto shownButtons = [projectView] {
            QStringList ids;
            for (const char* id :
                 {"spr_add", "spr_delete", "spr_duplicate", "spr_delete_grp", "spr_rename_grp", "spr_openimg",
                  "spr_saveimgas", "spr_saveimg", "spr_layer_save", "spr_layer_play", "spr_explorer", "spr_edit_img"}) {
                if (visibleChild<QToolButton>(projectView, id))
                    ids << QLatin1String(id);
            }
            return ids;
        };
        QCOMPARE(shownButtons(), (QStringList{"spr_add", "spr_delete", "spr_duplicate", "spr_delete_grp",
                                              "spr_rename_grp", "spr_openimg", "spr_saveimgas", "spr_edit_img"}));
        actions.action("spr_edit_img")->trigger();
        QVERIFY(page->imageMode());
        QCOMPARE(shownButtons(), (QStringList{"spr_add", "spr_delete", "spr_duplicate", "spr_saveimg", "spr_layer_save",
                                              "spr_layer_play", "spr_explorer", "spr_edit_img"}));
        QVERIFY(visibleChild<QToolButton>(projectView, "spr_edit_img")->isChecked());
        QVERIFY(visibleChild<QScrollBar>(projectView, "spr_sel"));
        auto* label = visibleChild<QLabel>(projectView, "spr_sel_lb");
        QVERIFY(label);
        QCOMPARE(label->text(), QStringLiteral("4 / 8"));

        const struct
        {
            const char* id;
            const char* caption;
            const char* keys;
            const char* tip;
        } controls[] = {
            {"spr_saveimg", "Save image changes", "Alt+S", "Save changes on current sprite"},
            {"spr_layer_save", "Save visible as sprites", nullptr, "Save visible as sprites"},
            {"spr_layer_play", "Play", nullptr, "Play"},
            {"spr_explorer", "Sprite explorer", nullptr, "Sprite explorer"},
            {"spr_tool_canvasmove", "Move canvas", "Alt+1", "Move canvas"},
            {"spr_tool_selmove", "Move selection", "Alt+2", "Move selection"},
            {"spr_tool_rectsel", "Rectangle selection", "Alt+3", "Rectangle selection"},
            {"spr_tool_freesel", "Free selection", "Alt+4", "Free selection"},
            {"spr_tool_magicwand", "Magic Wand", nullptr, "Magic Wand"},
            {"spr_tool_picker", "Color picker", "Alt+5", "Color picker"},
            {"spr_tool_pen", "Pen", "Alt+6", "Pen"},
            {"spr_tool_eraser", "Eraser", "Alt+7", "Eraser"},
            {"spr_tool_remap_pen", "Remap pen", "Alt+8", "Remap pen"},
            {"spr_tool_paintbucket", "Paint bucket", "Alt+9", "Paint bucket"},
        };
        for (const auto& c : controls) {
            auto* button = visibleChild<QToolButton>(projectView, c.id);
            QVERIFY2(button, c.id);
            QCOMPARE(button->toolTip(), QLatin1String(c.tip));
            QCOMPARE(button->defaultAction()->statusTip(), QLatin1String(c.tip));
        }
        QCOMPARE(visibleChild<QWidget>(projectView, "spr_foreground")->toolTip(), QStringLiteral("Foreground color"));
        QCOMPARE(visibleChild<QWidget>(projectView, "spr_background")->toolTip(), QStringLiteral("Background color"));
        auto* layersButton = visibleChild<QToolButton>(projectView, "spr_layers_menu");
        QVERIFY(layersButton && layersButton->menu());
        QCOMPARE(layersButton->popupMode(), QToolButton::InstantPopup);
        QCOMPARE(layersButton->defaultAction()->objectName(), QStringLiteral("spr_layer_duplicate"));
        QCOMPARE(layersButton->defaultAction()->statusTip(), QStringLiteral("Duplicate the current layer"));
        QCOMPARE(menuIds(layersButton->menu()),
                 (QStringList{"spr_layer_add", "spr_layer_delete", "spr_layer_duplicate", "|", "spr_layer_mergevis",
                              "spr_layer_mergeall", "|", "spr_layer_save", "spr_layer_play"}));
        auto* blend = visibleChild<QComboBox>(projectView, "spr_layers_blendmode");
        auto* opacity = visibleChild<QSpinBox>(projectView, "spr_layers_opacity");
        QVERIFY(blend && opacity);
        QCOMPARE(blend->count(), 13);
        QStringList modes;
        for (int i = 0; i < blend->count(); ++i)
            modes << blend->itemText(i);
        QCOMPARE(modes,
                 (QStringList{"Normal", "Add", "Multiply", "Screen", "Overlay", "Darken", "Lighten", "Color Dodge",
                              "Color Burn", "Hard Light", "Soft Light", "Difference", "Exclusion"}));
        QCOMPARE(blend->currentText(), QStringLiteral("Normal"));
        QCOMPARE(opacity->minimum(), 0);
        QCOMPARE(opacity->maximum(), 100);
        QCOMPARE(opacity->value(), 100);
        QVERIFY(!blend->isEnabled());
        QVERIFY(!opacity->isEnabled());

        ImageEditor* editor = page->imageEditor();
        auto* slots = editor->findChild<QWidget*>("remapSlots");
        auto* size = editor->findChild<QSlider*>("toolSize");
        QVERIFY(slots && size);
        auto shownInBar = [](QWidget* widget) {
            auto* bar = qobject_cast<QToolBar*>(widget->parentWidget());
            for (QAction* a : bar ? bar->actions() : QList<QAction*>()) {
                if (bar->widgetForAction(a) == widget)
                    return a->isVisible();
            }
            return false;
        };
        QVERIFY(!shownInBar(slots));
        actions.action("spr_tool_remap_pen")->trigger();
        QVERIFY(shownInBar(slots));
        QVERIFY(shownInBar(size));
        QCOMPARE(editor->currentSlot(), 0);
        auto clickSlot = [slots](int slot, Qt::MouseButton button, QEvent::Type type = QEvent::MouseButtonPress) {
            const QPoint at(slot * 12 + 6, 8);
            QMouseEvent event(type, at, slots->mapToGlobal(at), button, button, Qt::NoModifier);
            QCoreApplication::sendEvent(slots, &event);
        };
        clickSlot(5, Qt::LeftButton);
        QCOMPARE(editor->currentSlot(), 5);
        editor->setSlotColor(false, PaintColor{3, {}});
        clickSlot(9, Qt::RightButton);
        QCOMPARE(editor->currentSlot(), 5);
        clickSlot(5, Qt::LeftButton, QEvent::MouseButtonDblClick);
        QCOMPARE(editor->currentSlot(), 5);
        QVERIFY(editor->remapSlots()[5].source.has_value());
        actions.action("spr_tool_pen")->trigger();
        QVERIFY(!shownInBar(slots));
        actions.action("spr_edit_img")->trigger();
        QVERIFY(!page->imageMode());
        actions.action("file_close")->trigger();
    }

    // The Remap pen takes its colours from the palette on the right: a left click sets the colour to replace, a right
    // click the colour that replaces it. Several pairs apply at once.
    void remapPenFromPalette()
    {
        QTemporaryDir dir;
        SpriteDocument doc;
        doc.palettes() << SpriteDocument::makePalette(1, 1, ramp(0)) << SpriteDocument::makePalette(1, 2, ramp(1));
        SpriteEntry sprite = indexedSprite(0, 0, 4, 1, 1, doc.palettes()[0]);
        sprite.pixels = SpriteDocument::makePixels(Formats::Image::indexed(4, 1, {1, 2, 3, 4}));
        doc.sprites() << sprite;
        writeProject(dir, "rp", doc);
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        QVERIFY(window.openProject(dir.filePath("rp.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        ProjectView* view = window.currentProjectView();
        auto* page = view->findChild<SpritePage*>();
        QVERIFY(page);
        actions.action("spr_edit_img")->trigger();
        QVERIFY(page->imageMode());
        actions.action("spr_tool_remap_pen")->trigger();
        ImageEditor* editor = page->imageEditor();
        QCOMPARE(editor->tool(), ImageEditor::Tool::RemapPen);
        auto* grid = visibleChild<PaletteWidget>(view, "pal_view");
        QVERIFY(grid);
        // The canvas is drawn with the palette chosen in the panel.
        QCOMPARE(editor->document().palette, ramp(0));
        actions.action("pal_next")->trigger();
        QCOMPARE(editor->document().palette, ramp(1));
        auto clickColor = [grid](int slot, Qt::MouseButton button) {
            QTest::mouseClick(grid, button, {}, grid->cellRect(slot).center());
        };
        clickColor(1, Qt::LeftButton);
        clickColor(3, Qt::RightButton);
        QVERIFY(editor->remapSlots()[0].source && editor->remapSlots()[0].replacement);
        QCOMPARE(editor->remapSlots()[0].source->index, 1);
        QCOMPARE(editor->remapSlots()[0].replacement->index, 3);
        editor->setCurrentSlot(1);
        clickColor(2, Qt::LeftButton);
        clickColor(4, Qt::RightButton);

        editor->setBrushSize(ImageEditor::Tool::RemapPen, 4);
        ImageCanvas* canvas = editor->canvas();
        const QPoint start = canvas->toWidget(QPointF(0.5, 0.5)).toPoint();
        const ImageDocument& image = editor->document();
        auto row = [&image] {
            QList<int> indexes;
            for (int x = 0; x < 4; ++x)
                indexes << image.pixel(x, 0).index;
            return indexes;
        };
        QTest::mouseClick(canvas, Qt::LeftButton, {}, start);
        QCOMPARE(row(), (QList<int>{3, 4, 3, 4}));
        // The right button paints the other way round.
        QTest::mouseClick(canvas, Qt::RightButton, {}, start);
        QCOMPARE(row(), (QList<int>{1, 2, 1, 2}));
        // A slot with only the colour to replace makes it transparent. It comes after slot 0, so it wins.
        editor->setCurrentSlot(2);
        clickColor(1, Qt::LeftButton);
        QTest::mouseClick(canvas, Qt::LeftButton, {}, start);
        QCOMPARE(row(), (QList<int>{0, 4, 0, 4}));

        // Saving keeps the sprite's own palette, whatever the panel shows.
        actions.action("spr_saveimg")->trigger();
        const SpriteDocument& saved = page->file()->document();
        QCOMPARE(saved.sprites()[0].palette, saved.palettes()[0]);
        const Formats::Image savedImage = saved.image(0);
        const std::vector<std::uint8_t> pixels(savedImage.pixels().begin(), savedImage.pixels().end());
        QCOMPARE(pixels, (std::vector<std::uint8_t>{0, 4, 0, 4}));
    }

    void imageLayersAndSaving()
    {
        QTemporaryDir dir;
        SpriteDocument doc;
        Formats::Palette colors = ramp(0);
        doc.palettes() << SpriteDocument::makePalette(1, 1, colors);
        std::vector<std::uint8_t> base(12, 5);
        for (int y = 0; y < 3; ++y)
            base[std::size_t(y * 4)] = 1;
        SpriteEntry first = indexedSprite(0, 0, 4, 3, 5, doc.palettes()[0], {1, 2});
        first.pixels = SpriteDocument::makePixels(Formats::Image::indexed(4, 3, base));
        doc.sprites() << first << indexedSprite(0, 1, 4, 3, 6, doc.palettes()[0]);
        doc.sprites() << indexedSprite(10, 0, 3, 3, 7, doc.palettes()[0]) << rgbaSprite(0, 2, 4, 3, 0);
        writeProject(dir, "il", doc);
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        QVERIFY(window.openProject(dir.filePath("il.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        ProjectView* projectView = view(window);
        auto* page = projectView->findChild<SpritePage*>();
        SpriteFile* file = page->file();
        ImageEditor* editor = page->imageEditor();
        page->setCurrent(0);
        actions.action("spr_edit_img")->trigger();
        auto* layers = projectView->findChild<QListView*>("spr_layers_list");
        QVERIFY(layers);
        auto names = [editor] {
            QStringList list;
            for (const ImageLayer& layer : editor->document().layers)
                list << layer.name;
            return list;
        };

        actions.action("spr_layer_add")->trigger();
        actions.action("spr_layer_add")->trigger();
        QCOMPARE(names(), (QStringList{"0,0", "Layer 2", "Layer 3"}));
        layers->setCurrentIndex(layers->model()->index(1, 0)); // Layer 2, rows start at the top layer
        QCOMPARE(editor->document().current, 1);
        actions.action("spr_layer_delete")->trigger();
        QVERIFY(!QApplication::activeModalWidget());
        QCOMPARE(names(), (QStringList{"0,0", "Layer 3"}));
        QCOMPARE(editor->document().current, 1);
        editor->undoStack()->undo();
        QCOMPARE(names(), (QStringList{"0,0", "Layer 2", "Layer 3"}));
        actions.action("spr_layer_mergevis")->trigger();
        QCOMPARE(names(), QStringList{"0,0"});
        editor->undoStack()->undo();
        QCOMPARE(names(), (QStringList{"0,0", "Layer 2", "Layer 3"}));
        actions.action("spr_layer_mergeall")->trigger();
        editor->undoStack()->undo();
        QCOMPARE(names(), (QStringList{"0,0", "Layer 2", "Layer 3"}));

        layers->setCurrentIndex(layers->model()->index(0, 0));
        ImageDocument painted = editor->document();
        painted.layers[2].image.setIndex(0, 0, 9);
        editor->apply(QStringLiteral("Pen"), painted);
        whenModal<QDialog>([](QDialog* dialog) {
            QCOMPARE(dialog->objectName(), QStringLiteral("RotImgDlg"));
            auto* angle = dialog->findChild<QDoubleSpinBox*>("angle");
            QCOMPARE(angle->value(), 0.0);
            QCOMPARE(angle->minimum(), -360.0);
            QCOMPARE(angle->maximum(), 360.0);
            QCOMPARE(angle->decimals(), 2);
            QCOMPARE(dialog->findChild<QComboBox*>("filter")->count(), 4);
            angle->setValue(90);
            dialog->accept();
        });
        actions.action("spr_image_rotate")->trigger();
        const ImageDocument& turned = editor->document();
        QCOMPARE(turned.width, 3);
        QCOMPARE(turned.height, 4);
        QCOMPARE(turned.layers[2].image.indexAt(2, 0), 9);
        // The base layer is only moved, by floor((3 - 4) / 2) = -1 and 0.
        QCOMPARE(turned.layers[0].image.indexAt(0, 0), 5);
        QCOMPARE(turned.layers[0].image.indexAt(2, 2), 5);
        QCOMPARE(turned.layers[0].image.indexAt(1, 3), 0);
        editor->undoStack()->undo();
        editor->undoStack()->undo();

        // Options > Messages > Discard file modifications does not answer the image editor's question.
        Config& config = m_context->config();
        const QString discard = config.value("showmsg_discard");
        config.setValue("showmsg_discard", "1"); // Always
        QVERIFY(editor->isModified());
        answerDiscard(QMessageBox::No);
        actions.action("spr_next")->trigger();
        QCOMPARE(page->current(), 0);
        config.setValue("showmsg_discard", discard);
        answerDiscard(QMessageBox::Yes);
        page->setCurrent(3);

        auto* blend = projectView->findChild<QComboBox*>("spr_layers_blendmode");
        auto* opacity = projectView->findChild<QSpinBox*>("spr_layers_opacity");
        QVERIFY(blend->isEnabled() && opacity->isEnabled());
        actions.action("spr_layer_add")->trigger();
        const int steps = editor->undoStack()->count();
        blend->setCurrentIndex(2);
        QCOMPARE(editor->document().layer().blend, 2);
        opacity->setValue(40);
        QCOMPARE(editor->document().layer().opacity, 40);
        QCOMPARE(editor->undoStack()->count(), steps + 2);
        layers->setCurrentIndex(layers->model()->index(1, 0));
        QCOMPARE(blend->currentIndex(), 0);
        QCOMPARE(opacity->value(), 100);
        layers->setCurrentIndex(layers->model()->index(0, 0));
        QCOMPARE(blend->currentIndex(), 2);
        QCOMPARE(opacity->value(), 40);
        editor->undoStack()->undo();
        QCOMPARE(opacity->value(), 100);
        editor->undoStack()->undo();
        QCOMPARE(blend->currentIndex(), 0);

        auto* model = qobject_cast<QStandardItemModel*>(layers->model());
        model->item(0)->setText("0,0");
        actions.action("spr_layer_add")->trigger();
        model->item(0)->setText("10,0");
        model->item(0)->setCheckState(Qt::Unchecked);
        const SpritePixelsPtr hidden = file->document().sprites()[2].pixels;
        QStringList texts;
        answerBoxes({QMessageBox::Yes}, &texts);
        actions.action("spr_layer_save")->trigger();
        QCOMPARE(texts, QStringList{"The sprite 0,0 already exists. Do you want to replace it?"});
        QVERIFY(!file->document().image(0).isIndexed());
        QCOMPARE(file->document().sprites()[2].pixels, hidden);
        QCOMPARE(file->document().spriteCount(), 4);
        file->undoStack()->undo();

        answerDiscard(QMessageBox::Yes);
        page->setCurrent(1);
        whenModal<QDialog>([](QDialog* dialog) {
            QCOMPARE(dialog->objectName(), QStringLiteral("ImgInfoDlg"));
            QCOMPARE(dialog->findChild<QSpinBox*>("wid")->maximum(), 65535);
            QCOMPARE(dialog->findChild<QSpinBox*>("hei")->minimum(), 1);
            dialog->findChild<QComboBox*>("fmt")->setCurrentIndex(1); // RGBA
            dialog->accept();
        });
        actions.action("spr_image_properties")->trigger();
        QVERIFY(!editor->document().indexed);
        actions.action("spr_saveimg")->trigger();
        QVERIFY(!file->document().image(1).isIndexed());
        QVERIFY(!editor->isModified());
        while (file->undoStack()->canUndo())
            file->undoStack()->undo();
        actions.action("spr_edit_img")->trigger();
        QVERIFY(!page->imageMode());
        actions.action("file_close")->trigger();
    }

    void imageCanvasView()
    {
        QTemporaryDir dir;
        Formats::Palette colors = ramp(0);
        colors[1] = {255, 0, 0, 255};
        SpriteDocument doc;
        doc.palettes() << SpriteDocument::makePalette(1, 1, colors);
        std::vector<std::uint8_t> pixels(256, 0);
        for (int y = 0; y < 16; ++y)
            for (int x = 12; x < 16; ++x)
                pixels[std::size_t(y * 16 + x)] = 1;
        for (int n = 0; n < 2; ++n) {
            SpriteEntry entry = indexedSprite(0, n, 16, 16, 0, doc.palettes()[0]);
            entry.pixels = SpriteDocument::makePixels(Formats::Image::indexed(16, 16, pixels));
            doc.sprites() << entry;
        }
        doc.sprites() << indexedSprite(1, 0, 30, 20, 1, doc.palettes()[0]);
        writeProject(dir, "ic", doc);
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        QVERIFY(window.openProject(dir.filePath("ic.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        auto* page = view(window)->findChild<SpritePage*>();
        ImageEditor* editor = page->imageEditor();
        ImageCanvas* canvas = editor->canvas();
        auto* status = window.findChild<StatusBar*>();
        QVERIFY(status);
        page->setCurrent(0);
        page->view()->setGuides(QPoint(3, 3));
        const QString alignStatus = status->section(0);
        actions.action("spr_edit_img")->trigger();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        canvas->setZoom(4);
        auto pixel = [canvas](const QImage& image, double x, double y) {
            return image.pixelColor(canvas->toWidget(QPointF(x, y)).toPoint());
        };

        QImage shown = canvas->grab().toImage();
        QCOMPARE(pixel(shown, 0.5, 0.5), QColor(192, 192, 192));
        QCOMPARE(pixel(shown, 7.5, 7.5), QColor(192, 192, 192));
        QCOMPARE(pixel(shown, 8.5, 0.5), QColor(Qt::white));
        QCOMPARE(pixel(shown, 0.5, 8.5), QColor(Qt::white));
        QCOMPARE(pixel(shown, 8.5, 8.5), QColor(192, 192, 192));
        QCOMPARE(pixel(shown, 13.5, 2.5), QColor(255, 0, 0));
        const QPoint corner = canvas->toWidget(QPointF(0, 0)).toPoint();
        QCOMPARE(shown.pixelColor(corner + QPoint(-1, 10)), QColor(Qt::black));
        QCOMPARE(shown.pixelColor(corner + QPoint(10, -1)), QColor(Qt::black));
        const QColor outside = shown.pixelColor(corner + QPoint(-20, 10));
        QVERIFY(outside != QColor(192, 192, 192) && outside != QColor(Qt::white) && outside != QColor(Qt::black));

        for (const char* id : {"view_rulers", "view_main_cross", "view_guide_lines"}) {
            actions.action(id)->trigger();
            QCOMPARE(canvas->grab().toImage(), shown);
            actions.action(id)->trigger();
        }
        for (const QPointF at : {QPointF(2.5, 2.5), QPointF(13.5, 9.5)}) {
            const QPoint p = canvas->toWidget(at).toPoint();
            QMouseEvent move(QEvent::MouseMove, p, canvas->mapToGlobal(p), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(canvas, &move);
        }
        QCOMPARE(status->section(0), alignStatus);

        ImageDocument selected = editor->document();
        ImageEdits::selectRect(selected, QRect(12, 0, 2, 16));
        editor->change(selected);
        shown = canvas->grab().toImage();
        QCOMPARE(pixel(shown, 12.5, 4.5), QColor(255, 0, 0));
        QCOMPARE(pixel(shown, 15.5, 4.5), QColor(255 * 61 / 100, 0, 0));
        actions.action("spr_select_all")->trigger();
        QCOMPARE(pixel(canvas->grab().toImage(), 15.5, 4.5), QColor(255, 0, 0));

        actions.action("spr_view_original")->trigger();
        QVERIFY(canvas->originalShown());
        shown = canvas->grab().toImage();
        const QPoint middle(canvas->width() / 4, canvas->height() / 2);
        QCOMPARE(shown.pixelColor(middle + QPoint(5, 0)), QColor(255, 0, 0));
        QCOMPARE(shown.pixelColor(middle + QPoint(-5, -3)).red(), 192);
        QVERIFY(canvas->toWidget(QPointF(0, 0)).x() >= canvas->width() / 2);
        actions.action("spr_view_original")->trigger();
        QVERIFY(!canvas->originalShown());

        QVERIFY(actions.action("spr_tool_canvasmove")->isChecked());
        for (Qt::MouseButton button : {Qt::LeftButton, Qt::RightButton}) {
            const QPointF before = canvas->toWidget(QPointF(0, 0));
            const QPoint from = canvas->toWidget(QPointF(8, 8)).toPoint();
            const QPoint to = from + QPoint(30, 20);
            QMouseEvent press(QEvent::MouseButtonPress, from, canvas->mapToGlobal(from), button, button,
                              Qt::NoModifier);
            QCoreApplication::sendEvent(canvas, &press);
            QMouseEvent move(QEvent::MouseMove, to, canvas->mapToGlobal(to), Qt::NoButton, button, Qt::NoModifier);
            QCoreApplication::sendEvent(canvas, &move);
            QMouseEvent release(QEvent::MouseButtonRelease, to, canvas->mapToGlobal(to), button, Qt::NoButton,
                                Qt::NoModifier);
            QCoreApplication::sendEvent(canvas, &release);
            QCOMPARE(canvas->toWidget(QPointF(0, 0)), before + QPointF(30, 20));
        }
        QVERIFY(!editor->isModified());

        canvas->setZoom(6);
        const QPointF place = canvas->toWidget(QPointF(0, 0));
        actions.action("spr_next")->trigger();
        QCOMPARE(page->current(), 1);
        QCOMPARE(canvas->zoom(), 6.0);
        QCOMPARE(canvas->toWidget(QPointF(0, 0)), place);
        actions.action("spr_edit_img")->trigger();
        QVERIFY(!page->imageMode());
        actions.action("spr_edit_img")->trigger();
        QCOMPARE(canvas->zoom(), 6.0);
        QCOMPARE(canvas->toWidget(QPointF(0, 0)), place);
        actions.action("spr_edit_img")->trigger();
        actions.action("file_close")->trigger();
    }

    // The explorer's group list ignores the current sprite, but its thumbnails follow sprite changes.
    void spriteExplorer()
    {
        QTemporaryDir dir;
        SpriteDocument doc;
        doc.palettes() << SpriteDocument::makePalette(1, 1, ramp(0));
        doc.sprites() << indexedSprite(0, 0, 4, 3, 5, doc.palettes()[0], {1, 2});
        doc.sprites() << indexedSprite(0, 1, 2, 2, 6, doc.palettes()[0]);
        doc.sprites() << indexedSprite(10, 0, 3, 3, 7, doc.palettes()[0]);
        writeProject(dir, "ex", doc);
        Formats::Palette colors = ramp(0);
        const auto png = Formats::encodePng(Formats::Image::indexed(2, 2, {1, 1, 1, 1}, colors));
        QVERIFY(png.ok());
        writeFile(dir.filePath("new.png"), toBytes(png.value()));
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        QVERIFY(window.openProject(dir.filePath("ex.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        auto* page = view(window)->findChild<SpritePage*>();
        SpriteFile* file = page->file();
        ImageEditor* editor = page->imageEditor();
        page->setCurrent(2);
        actions.action("spr_edit_img")->trigger();
        actions.action("spr_explorer")->trigger();
        auto* groups = view(window)->findChild<QComboBox*>("thumb_grp_list");
        auto* thumbnails = view(window)->findChild<QListView*>("thumb_view");
        QVERIFY(groups && thumbnails);
        QCOMPARE(groups->currentText(), QStringLiteral("0"));
        actions.action("spr_prev")->trigger();
        actions.action("spr_prev")->trigger();
        QCOMPARE(page->current(), 0);
        groups->setCurrentIndex(1);
        actions.action("spr_next")->trigger();
        QCOMPARE(groups->currentText(), QStringLiteral("10"));
        groups->setCurrentIndex(0);
        QCOMPARE(thumbnails->model()->rowCount(), 2);

        page->setCurrent(0);
        m_context->config().setValue("show_addsprite_dialog", false);
        FileDialogs::setOpenPromptOverride(
            [&dir](const QString&, const QString&, const QString&) { return dir.filePath("new.png"); });
        actions.action("spr_add")->trigger();
        FileDialogs::setOpenPromptOverride({});
        m_context->config().setValue("show_addsprite_dialog", true);
        QCOMPARE(file->document().spriteCount(), 4);
        QCOMPARE(thumbnails->model()->rowCount(), 3);
        file->undoStack()->undo();
        QCOMPARE(thumbnails->model()->rowCount(), 2);

        // After the canvas grows, an aligned 0,1 (axis 0,0) lands on 0,0's axis (1,2), moved with the canvas.
        page->setCurrent(0);
        auto resize = [&actions](int width, int height) {
            whenModal<QDialog>([width, height](QDialog* dialog) {
                dialog->findChild<QSpinBox*>("wid")->setValue(width);
                dialog->findChild<QSpinBox*>("hei")->setValue(height);
                dialog->accept();
            });
            actions.action("spr_image_properties")->trigger();
        };
        resize(6, 5);
        QCOMPARE(editor->document().origin, QPoint(1, 1));
        thumbnails->selectionModel()->select(thumbnails->model()->index(1, 0), QItemSelectionModel::ClearAndSelect);
        view(window)->findChild<QToolButton*>("thumb_action3")->click();
        const ImageDocument& image = editor->document();
        QCOMPARE(image.width, 6);
        QCOMPARE(image.layers.first().name, QStringLiteral("0,1"));
        QCOMPARE(image.layers.first().image.indexAt(2, 3), 6);
        QCOMPARE(image.layers.first().image.indexAt(3, 4), 6);
        QCOMPARE(image.layers.first().image.indexAt(1, 3), 0);
        resize(8, 7);
        view(window)->findChild<QToolButton*>("thumb_action3")->click();
        QCOMPARE(editor->document().layers.first().image.indexAt(3, 4), 6);
        QCOMPARE(editor->document().layers.first().image.indexAt(2, 4), 0);
        QCOMPARE(editor->document().layers[1].image.indexAt(3, 4), 6);
        answerDiscard(QMessageBox::Yes);
        actions.action("spr_edit_img")->trigger();
        actions.action("file_close")->trigger();
    }

    // The palette mode and target of Import from another project are always enabled.
    void importPaletteGroup()
    {
        QTemporaryDir mine;
        QTemporaryDir other;
        writeProject(mine, "mine", mixedSprites());
        writeProject(other, "other", mixedSprites());
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(other.filePath("other.def")));
        QVERIFY(window.openProject(mine.filePath("mine.def")));
        m_context->actions().action("editor_spr")->trigger();
        whenModal<QDialog>([](QDialog* dialog) {
            QCOMPARE(dialog->objectName(), QStringLiteral("ProjImportDlg"));
            QCOMPARE(dialog->windowTitle(), QStringLiteral("Import from another project"));
            QComboBox* mode = nullptr;
            QComboBox* target = nullptr;
            for (QComboBox* combo : dialog->findChildren<QComboBox*>()) {
                if (combo->objectName() == QLatin1String("projList"))
                    continue;
                (combo->count() == 3 && combo->itemText(1).startsWith("Force") ? mode : target) = combo;
            }
            QVERIFY(mode && target);
            QCOMPARE(mode->currentIndex(), 0);
            QCOMPARE(target->currentText(), QStringLiteral("1, 1"));
            for (int i = 0; i < 3; ++i) {
                mode->setCurrentIndex(i);
                QVERIFY(mode->isEnabled() && target->isEnabled());
            }
            dialog->reject();
        });
        m_context->actions().action("spr_import_proj")->trigger();
        m_context->actions().action("file_close_all")->trigger();
    }

private:
    static ProjectView* view(MainWindow& window) { return window.currentProjectView(); }

    AppContext* m_context = nullptr;
};

QTEST_MAIN(SpritesParityTest)
#include "SpritesParityTest.moc"
