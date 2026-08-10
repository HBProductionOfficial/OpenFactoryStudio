#include "shell/dialogs/OptionsDialog.h"
#include "core/actions/ActionRegistry.h"
#include "core/app/AppContext.h"
#include "core/app/Paths.h"
#include "core/engine/RunConfiguration.h"
#include "core/settings/Config.h"
#include "core/settings/SettingsBundle.h"
#include "core/settings/ShortcutSettings.h"
#include "core/theme/ColorSet.h"
#include "core/theme/ThemeManager.h"
#include "shell/dialogs/ColorsPage.h"
#include "shell/dialogs/MugenOptionsPage.h"
#include "shell/dialogs/RunConfigDialog.h"
#include "shell/dialogs/ShortcutEditor.h"

#include <QAction>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QGroupBox>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace OFS;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray readFile(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

QByteArray configXml(const QList<QPair<QString, QString>>& values)
{
    QByteArray data = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<configuration>\n";
    for (const auto& [name, value] : values)
        data += "    <config name=\"" + name.toUtf8() + "\" value=\"" + value.toUtf8() + "\"/>\n";
    return data + "</configuration>\n";
}

QStringList comboItems(const QComboBox* combo)
{
    QStringList items;
    for (int i = 0; i < combo->count(); ++i)
        items << combo->itemText(i);
    return items;
}

template <typename T>
T* child(QObject* parent, const QString& name)
{
    T* widget = parent->findChild<T*>(name);
    if (!widget)
        qWarning("missing child %s", qPrintable(name));
    return widget;
}

// An app context with its settings files in a temporary folder and a few themes and actions.
struct Fixture
{
    QTemporaryDir dir;
    std::unique_ptr<AppContext> context;

    explicit Fixture(const QList<QPair<QString, QString>>& user = {})
    {
        // Every test in this executable shares the user folder, so start clean.
        QDir(Paths::userDir() + "/colorsets").removeRecursively();
        QFile::remove(Paths::userDir() + "/mugen_config.xml");

        const QString data = dir.filePath("data");
        writeFile(dir.filePath("defaults.xml"), configXml({{"cache_type", "1"},
                                                           {"colorset", "Dark"},
                                                           {"font_name", "Courier New"},
                                                           {"font_size", "10"},
                                                           {"iconset", "Nuvola"},
                                                           {"preset", "Neo"},
                                                           {"showmsg_reload_file", ""},
                                                           {"stylesheet", "Dark"},
                                                           {"undo_limit", "64"}}));
        if (!user.isEmpty())
            writeFile(dir.filePath("user/config.xml"), configXml(user));
        writeFile(dir.filePath("shortcuts_default.xml"), "<shortcuts>\n"
                                                         "    <shortcut action=\"anim_add\" keys=\"Ctrl+A\"/>\n"
                                                         "    <shortcut action=\"file_open\" keys=\"Ctrl+O\"/>\n"
                                                         "    <shortcut action=\"spr_add\" keys=\"Ctrl+A\"/>\n"
                                                         "</shortcuts>\n");
        writeFile(data + "/colorsets/Dark.xml", "<colors>\n"
                                                "    <color name=\"background\" value=\"34,34,34\"/>\n"
                                                "    <color name=\"keyword\" value=\"255,255,255\" bold=\"true\"/>\n"
                                                "</colors>\n");
        writeFile(data + "/colorsets/Classic.xml", "<colors>\n"
                                                   "    <color name=\"background\" value=\"255,255,255\"/>\n"
                                                   "    <color name=\"keyword\" value=\"0,128,128\" bold=\"true\"/>\n"
                                                   "</colors>\n");
        writeFile(data + "/presets/Classic.lua", "");
        writeFile(data + "/presets/Neo.lua", "");
        writeFile(data + "/styles/Dark/stylesheet.css", "");
        writeFile(data + "/iconsets/Nuvola/iconset.xml", "<iconset/>");

        context = std::make_unique<AppContext>();
        context->config().load(dir.filePath("defaults.xml"), dir.filePath("user/config.xml"));
        context->shortcuts().load(dir.filePath("shortcuts_default.xml"), dir.filePath("user/shortcuts.xml"));
        context->theme().setSearchDirs({Paths::userDir(), data});

        ActionRegistry& actions = context->actions();
        actions.add({"file_open", "Open...", "Open a project", {}, false, {}});
        actions.add({"edit_select_all", "Select all", "Select everything", {}, false, {}});
        actions.add({"spr_add", "Add", "Add sprites", "editor:sprites", false, {}});
        actions.add({"anim_add", "Add", "Add an animation", "editor:animations", false, {}});
        actions.add({"spr_tool_pen", "Pencil", "Pencil tool", "window:imgedit", true, {}});
        actions.applyShortcuts(context->shortcuts());
    }

    Config& config() { return context->config(); }
};

} // namespace

class OptionsDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void tabsAndButtons()
    {
        Fixture f;
        OptionsDialog dialog(*f.context);
        QCOMPARE(dialog.windowTitle(), QStringLiteral("Options"));
        QStringList tabs;
        for (int i = 0; i < dialog.tabs()->count(); ++i)
            tabs << dialog.tabs()->tabText(i);
        QCOMPARE(tabs, (QStringList{"General", "Interface", "Messages", "Colors", "Graphics", "Code", "M.U.G.E.N"}));
        QCOMPARE(comboItems(dialog.engineSelector()), QStringList{"M.U.G.E.N"});

        QStringList buttons;
        for (const char* name : {"defaultsBut", "openBut", "saveBut", "applyBut", "closeBut"})
            buttons << child<QPushButton>(&dialog, name)->text();
        QCOMPARE(buttons, (QStringList{"Load defaults", "Open...", "Save...", "Apply", "Close"}));
        QVERIFY(dialog.enginePage("mugen"));
    }

    void generalTab()
    {
        Fixture f;
        OptionsDialog dialog(*f.context);
        const QList<QPair<const char*, const char*>> checks = {
            {"can_close_editor", "Enable closing last open file (closes editor)"},
            {"enable_file_watcher", "Enable the file modification watcher"},
            {"use_native_open", "Use system native open dialog"},
            {"sort_file_names", "Sort file names from the open dialog"},
            {"backup_onload", "Make a backup before loading a project"},
            {"auto_update", "Automatically check updates on startup"},
            {"autosave_async", "Only save if asynchronous mode is supported"}};
        for (const auto& [key, text] : checks)
            QCOMPARE(child<QCheckBox>(&dialog, key)->text(), QString::fromLatin1(text));
        QCOMPARE(comboItems(child<QComboBox>(&dialog, "cache_type")),
                 (QStringList{"Memory (maximum speed)", "Hard disk (low memory usage)"}));
        QCOMPARE(child<QComboBox>(&dialog, "cache_type")->currentIndex(), 0);
        auto* undo = child<QSpinBox>(&dialog, "undo_limit");
        QCOMPARE(undo->maximum(), 256);
        QCOMPARE(undo->value(), 64);
        auto* history = child<QSpinBox>(&dialog, "max_history");
        QCOMPARE(history->minimum(), 10);
        QCOMPARE(history->maximum(), 32);
        QCOMPARE(child<QSpinBox>(&dialog, "thumb_size")->maximum(), 256);
        QCOMPARE(child<QSpinBox>(&dialog, "autosave_interval")->maximum(), 99);
        QCOMPARE(child<QGroupBox>(&dialog, "autoSave")->title(), QStringLiteral("Auto save:"));
        auto* language = child<QComboBox>(&dialog, "language");
        QCOMPARE(language->currentText(), QStringLiteral("English (USA)"));
        QCOMPARE(language->currentData().toString(), QStringLiteral("en-us"));
        QVERIFY(child<QLabel>(&dialog, "langAuthor")->text().startsWith(QLatin1String("Author: ")));
        QVERIFY(child<QLabel>(&dialog, "langContact")->text().startsWith(QLatin1String("Contact: ")));
    }

    void messagesTab()
    {
        Fixture f({{"showmsg_reload_file", "1"}, {"showmsg_crop_spr", "2"}, {"confirm_delete", "false"}});
        OptionsDialog dialog(*f.context);
        const QStringList answers = {"Ask", "Always", "Never"};
        for (const char* key :
             {"showmsg_discard", "showmsg_reload_file", "showmsg_share_spr", "showmsg_share_pal", "showmsg_crop_spr",
              "showmsg_pal_file", "showmsg_anim_clone", "showmsg_cbox_replicate", "showmsg_enc_reload"})
            QCOMPARE(comboItems(child<QComboBox>(&dialog, key)), answers);
        QCOMPARE(child<QComboBox>(&dialog, "showmsg_discard")->currentText(), QStringLiteral("Ask"));
        QCOMPARE(child<QComboBox>(&dialog, "showmsg_reload_file")->currentText(), QStringLiteral("Always"));
        QCOMPARE(child<QComboBox>(&dialog, "showmsg_crop_spr")->currentText(), QStringLiteral("Never"));
        QCOMPARE(child<QCheckBox>(&dialog, "confirm_close")->text(), QStringLiteral("Request confirmation on exit"));
        QVERIFY(child<QCheckBox>(&dialog, "confirm_close")->isChecked());
        QVERIFY(!child<QCheckBox>(&dialog, "confirm_delete")->isChecked());
        QCOMPARE(child<QCheckBox>(&dialog, "showmsg_spr_no_pal")->text(),
                 QStringLiteral("Can't save 8-bit sprites without palette"));
        QVERIFY(!child<QCheckBox>(&dialog, "showmsg_sprs_empty")->isChecked());
    }

    void graphicsAndCodeTabs()
    {
        Fixture f({{"opengl_mode", "0"},
                   {"native_painting", "false"},
                   {"background_type", "1"},
                   {"crosshair_type", "0"},
                   {"onion_opacity", "0.7400000095367432"},
                   {"onion_effect", "2"},
                   {"texUploadMethod", "1"},
                   {"zoom_mode", "1"},
                   {"default_codepage", "51"}});
        OptionsDialog dialog(*f.context);
        auto* renderer = child<QComboBox>(&dialog, "opengl_mode");
        QCOMPARE(comboItems(renderer),
                 (QStringList{"Automatic", "OpenGL (Desktop)", "DirectX (ANGLE)", "Native painter"}));
        QCOMPARE(renderer->currentIndex(), 1);
        QCOMPARE(comboItems(child<QComboBox>(&dialog, "texUploadMethod")),
                 (QStringList{"Immediate (may slow down loading)", "Delayed (on-demand)",
                              "Multi-threaded (in the background)"}));
        QCOMPARE(child<QComboBox>(&dialog, "texUploadMethod")->currentIndex(), 1);
        QCOMPARE(comboItems(child<QComboBox>(&dialog, "onion_effect")),
                 (QStringList{"None", "Monochrome", "Lighter", "Darker"}));
        QCOMPARE(child<QComboBox>(&dialog, "onion_effect")->currentText(), QStringLiteral("Lighter"));
        QCOMPARE(child<QSlider>(&dialog, "onion_opacity")->value(), 74);
        QCOMPARE(child<QSlider>(&dialog, "box_opacity")->value(), 50);
        QCOMPARE(comboItems(child<QComboBox>(&dialog, "zoom_mode")),
                 (QStringList{"Small range (up to 4X)", "Large range (up to 8X)"}));
        QCOMPARE(comboItems(child<QComboBox>(&dialog, "zoom_center")),
                 (QStringList{"Sprite's axis", "Center of workspace"}));
        QVERIFY(child<QRadioButton>(&dialog, "background_default")->isChecked());
        QVERIFY(child<QRadioButton>(&dialog, "crosshair_default")->isChecked());
        QVERIFY(!child<QLineEdit>(&dialog, "crosshair_main")->isEnabled());
        QVERIFY(!child<QLineEdit>(&dialog, "background_file")->isEnabled());
        QCOMPARE(child<QSpinBox>(&dialog, "background_x")->minimum(), -2000);
        QCOMPARE(child<QSpinBox>(&dialog, "background_x")->value(), -426);
        child<QRadioButton>(&dialog, "crosshair_custom")->setChecked(true);
        QVERIFY(child<QLineEdit>(&dialog, "crosshair_main")->isEnabled());

        auto* codePage = child<QComboBox>(&dialog, "default_codepage");
        QCOMPARE(codePage->count(), 60);
        QCOMPARE(codePage->itemText(0), QStringLiteral("System default - System"));
        QCOMPARE(codePage->currentText(), QStringLiteral("Unicode - UTF-8"));
        QCOMPARE(child<QSpinBox>(&dialog, "font_size")->value(), 10);
        QCOMPARE(child<QCheckBox>(&dialog, "auto_expand_tree")->text(), QStringLiteral("Current line navigation"));
        QCOMPARE(child<QCheckBox>(&dialog, "open_tabToSpace")->text(),
                 QStringLiteral("Convert Tab to Spaces while opening"));
    }

    void loadModifyApply()
    {
        Fixture f({{"cache_type", "2"},
                   {"showmsg_reload_file", "1"},
                   {"opengl_mode", "0"},
                   {"font_name", "A Font That Is Not Installed"},
                   {"switch_frame_spr", "false"}});
        Config& config = f.config();
        OptionsDialog dialog(*f.context);
        QSignalSpy applied(&dialog, &OptionsDialog::settingsApplied);

        QCOMPARE(child<QComboBox>(&dialog, "cache_type")->currentIndex(), 1);
        child<QCheckBox>(&dialog, "can_close_editor")->setChecked(false);
        child<QComboBox>(&dialog, "cache_type")->setCurrentIndex(0);
        child<QSpinBox>(&dialog, "undo_limit")->setValue(100);
        child<QComboBox>(&dialog, "showmsg_discard")->setCurrentIndex(2);
        child<QCheckBox>(&dialog, "showmsg_snds_empty")->setChecked(true);
        child<QComboBox>(&dialog, "opengl_mode")->setCurrentIndex(3);
        child<QSlider>(&dialog, "onion_opacity")->setValue(50);
        child<QRadioButton>(&dialog, "crosshair_custom")->setChecked(true);
        child<QLineEdit>(&dialog, "crosshair_main")->setText("C:/cross/main.png");
        child<QRadioButton>(&dialog, "background_none")->setChecked(true);
        child<QSpinBox>(&dialog, "font_size")->setValue(12);
        child<QCheckBox>(&dialog, "dpi_auto")->setChecked(true);
        child<QDoubleSpinBox>(&dialog, "dpi_scale")->setValue(1.5);
        child<QComboBox>(&dialog, "default_codepage")->setCurrentIndex(45);

        QCOMPARE(config.value("undo_limit"), QStringLiteral("64"));

        QTest::mouseClick(child<QPushButton>(&dialog, "applyBut"), Qt::LeftButton);
        QCOMPARE(applied.count(), 1);
        QCOMPARE(config.value("can_close_editor"), QStringLiteral("false"));
        QCOMPARE(config.value("cache_type"), QStringLiteral("1"));
        QCOMPARE(config.value("undo_limit"), QStringLiteral("100"));
        QCOMPARE(config.value("showmsg_discard"), QStringLiteral("2"));
        QCOMPARE(config.value("showmsg_reload_file"), QStringLiteral("1"));
        QCOMPARE(config.value("showmsg_snds_empty"), QStringLiteral("1"));
        QCOMPARE(config.value("native_painting"), QStringLiteral("true"));
        QCOMPARE(config.value("opengl_mode"), QStringLiteral("0"));
        QCOMPARE(config.value("onion_opacity"), QStringLiteral("0.5"));
        QCOMPARE(config.value("crosshair_type"), QStringLiteral("2"));
        QCOMPARE(config.value("crosshair_main"), QStringLiteral("C:/cross/main.png"));
        QCOMPARE(config.value("background_type"), QStringLiteral("0"));
        QCOMPARE(config.value("font_size"), QStringLiteral("12"));
        QCOMPARE(config.value("font_name"), QStringLiteral("A Font That Is Not Installed"));
        QCOMPARE(config.value("dpi_auto"), QStringLiteral("true"));
        QCOMPARE(config.value("dpi_scale"), QStringLiteral("1.5"));
        QCOMPARE(config.value("default_codepage"), QStringLiteral("45"));
        QCOMPARE(config.value("switch_frame_spr"), QStringLiteral("false"));
        QCOMPARE(config.value("preset"), QStringLiteral("Neo"));

        const QByteArray saved = readFile(config.userPath());
        QVERIFY(saved.contains("<config name=\"undo_limit\" value=\"100\"/>"));
        QVERIFY(saved.contains("<config name=\"switch_frame_spr\" value=\"false\"/>"));

        QSignalSpy changed(&config, &Config::changed);
        for (int i = 0; i < 3; ++i)
            QVERIFY(dialog.apply());
        QCOMPARE(applied.count(), 4);
        QCOMPARE(changed.count(), 0);

        OptionsDialog again(*f.context);
        QCOMPARE(child<QSpinBox>(&again, "undo_limit")->value(), 100);
        QCOMPARE(child<QComboBox>(&again, "opengl_mode")->currentIndex(), 3);
        QCOMPARE(child<QComboBox>(&again, "showmsg_discard")->currentText(), QStringLiteral("Never"));
    }

    void loadDefaultsAppliesOnApply()
    {
        Fixture f({{"font_size", "14"}, {"undo_limit", "5"}});
        OptionsDialog dialog(*f.context);
        QCOMPARE(child<QSpinBox>(&dialog, "font_size")->value(), 14);
        QTest::mouseClick(child<QPushButton>(&dialog, "defaultsBut"), Qt::LeftButton);
        QCOMPARE(child<QSpinBox>(&dialog, "font_size")->value(), 10);
        QCOMPARE(child<QSpinBox>(&dialog, "undo_limit")->value(), 64);
        // Built-in default for a key the defaults file lacks.
        QCOMPARE(child<QSpinBox>(&dialog, "thumb_size")->value(), 64);
        QCOMPARE(f.config().value("font_size"), QStringLiteral("14"));
        QVERIFY(dialog.apply());
        QCOMPARE(f.config().value("font_size"), QStringLiteral("10"));
        QCOMPARE(f.config().value("undo_limit"), QStringLiteral("64"));
    }

    void interfaceTabLists()
    {
        Fixture f;
        OptionsDialog dialog(*f.context);
        QCOMPARE(comboItems(child<QComboBox>(&dialog, "preset")), (QStringList{"Classic", "Neo"}));
        QCOMPARE(child<QComboBox>(&dialog, "preset")->currentText(), QStringLiteral("Neo"));
        QCOMPARE(comboItems(child<QComboBox>(&dialog, "iconset")), QStringList{"Nuvola"});
        QCOMPARE(comboItems(child<QComboBox>(&dialog, "colorset")), (QStringList{"Custom", "Classic", "Dark"}));
        QCOMPARE(child<QComboBox>(&dialog, "colorset")->currentText(), QStringLiteral("Dark"));
        QCOMPARE(child<QGroupBox>(&dialog, "dpiBox")->title(), QStringLiteral("DPI Scaling:"));
        QCOMPARE(child<QCheckBox>(&dialog, "dpi_auto")->text(), QStringLiteral("Automatic"));
        auto* scale = child<QDoubleSpinBox>(&dialog, "dpi_scale");
        QCOMPARE(scale->minimum(), 0.2);
        QCOMPARE(scale->maximum(), 10.0);
        QCOMPARE(scale->value(), 1.0);
        QCOMPARE(child<QGroupBox>(&dialog, "shortcut_lb")->title(), QStringLiteral("Keyboard shortcuts:"));

        f.config().setValue("iconset", QStringLiteral("Gone"));
        dialog.load();
        QCOMPARE(child<QComboBox>(&dialog, "iconset")->currentText(), QStringLiteral("Gone"));
    }

    void shortcutTable()
    {
        Fixture f;
        OptionsDialog dialog(*f.context);
        ShortcutEditor* editor = dialog.shortcutEditor();
        QAbstractItemModel* model = editor->view()->model();
        QCOMPARE(model->headerData(0, Qt::Horizontal).toString(), QStringLiteral("Function"));
        QCOMPARE(model->headerData(1, Qt::Horizontal).toString(), QStringLiteral("Keys"));
        QCOMPARE(model->rowCount(), 5);
        QCOMPARE(model->index(0, 0).data().toString(), QStringLiteral("Open a project"));
        QCOMPARE(model->index(0, 1).data().toString(), QKeySequence("Ctrl+O").toString(QKeySequence::NativeText));
        QVERIFY(model->flags(model->index(0, 1)) & Qt::ItemIsEditable);
        QVERIFY(!(model->flags(model->index(0, 0)) & Qt::ItemIsEditable));

        QVERIFY(editor->conflictingActions().isEmpty());

        QStringList asked;
        bool answer = false;
        dialog.setConflictHandler([&](const QString& id, const QKeySequence& keys, const QStringList& others) {
            asked = QStringList{id, keys.toString(QKeySequence::PortableText)} + others;
            return answer;
        });

        QVERIFY(!editor->assign("edit_select_all", QKeySequence("Ctrl+A")));
        QCOMPARE(asked, (QStringList{"edit_select_all", "Ctrl+A", "spr_add", "anim_add"}));
        QVERIFY(editor->keys("edit_select_all").isEmpty());
        QCOMPARE(editor->keys("spr_add"), QKeySequence("Ctrl+A"));
        answer = true;
        QVERIFY(editor->assign("edit_select_all", QKeySequence("Ctrl+A")));
        QCOMPARE(editor->keys("edit_select_all"), QKeySequence("Ctrl+A"));
        QVERIFY(editor->keys("spr_add").isEmpty());
        QVERIFY(editor->keys("anim_add").isEmpty());

        asked.clear();
        QVERIFY(editor->assign("spr_tool_pen", QKeySequence("Ctrl+O")));
        QVERIFY(asked.isEmpty());
        QCOMPARE(editor->conflictsWith("spr_add", QKeySequence("Ctrl+O")), QStringList{"file_open"});

        const QModelIndex penKeys = model->index(4, 1);
        editor->view()->setCurrentIndex(penKeys);
        editor->view()->edit(penKeys);
        auto* keyEdit = editor->view()->findChild<QKeySequenceEdit*>();
        QVERIFY(keyEdit);
        QCOMPARE(keyEdit->keySequence(), QKeySequence("Ctrl+O"));
        keyEdit->setKeySequence(QKeySequence("Ctrl+Shift+K"));
        Q_EMIT keyEdit->editingFinished();
        QTRY_COMPARE(editor->keys("spr_tool_pen"), QKeySequence("Ctrl+Shift+K"));
        QCOMPARE(model->index(4, 1).data().toString(), QKeySequence("Ctrl+Shift+K").toString(QKeySequence::NativeText));

        editor->view()->setCurrentIndex(model->index(0, 0));
        QTest::keyClick(editor->view(), Qt::Key_Delete);
        QVERIFY(editor->keys("file_open").isEmpty());

        QVERIFY(dialog.apply());
        ShortcutSettings& shortcuts = f.context->shortcuts();
        QCOMPARE(shortcuts.keys("edit_select_all"), QKeySequence("Ctrl+A"));
        QVERIFY(shortcuts.hasEntry("spr_add"));
        QVERIFY(shortcuts.keys("spr_add").isEmpty());
        QVERIFY(shortcuts.keys("file_open").isEmpty());
        QCOMPARE(f.context->actions().action("edit_select_all")->shortcut(), QKeySequence("Ctrl+A"));

        dialog.loadDefaults();
        QCOMPARE(editor->keys("file_open"), QKeySequence("Ctrl+O"));
        QVERIFY(dialog.apply());
        QCOMPARE(shortcuts.keys("file_open"), QKeySequence("Ctrl+O"));
        QCOMPARE(shortcuts.keys("spr_add"), QKeySequence("Ctrl+A"));
        QVERIFY(shortcuts.keys("edit_select_all").isEmpty());
    }

    void colorsTab()
    {
        Fixture f;
        OptionsDialog dialog(*f.context);
        ColorsPage* page = dialog.colorsPage();
        auto* colorset = child<QComboBox>(&dialog, "colorset");
        QCOMPARE(page->list()->model()->rowCount(), 20);
        QCOMPARE(page->list()->model()->index(0, 0).data().toString(), QStringLiteral("Text background"));
        QCOMPARE(page->list()->model()->index(19, 0).data().toString(), QStringLiteral("Waveform"));
        QCOMPARE(page->colorSet().color("keyword"), QColor(255, 255, 255));
        QVERIFY(page->colorSet().entry("keyword").bold);

        auto* bold = child<QCheckBox>(page, "bold");
        QCOMPARE(bold->text(), QStringLiteral("Bold"));
        QCOMPARE(child<QCheckBox>(page, "italic")->text(), QStringLiteral("Italic"));
        QCOMPARE(child<QPushButton>(page, "setColor")->text(), QStringLiteral("Color"));
        QCOMPARE(child<QPushButton>(page, "colorSave")->text(), QStringLiteral("Save"));
        QVERIFY(!bold->isEnabled());
        page->setCurrentRole("keyword");
        QVERIFY(bold->isEnabled());
        QVERIFY(bold->isChecked());
        page->setCurrentRole("attackbox");
        QVERIFY(!bold->isEnabled());

        colorset->setCurrentText("Classic");
        QCOMPARE(page->colorSet().color("keyword"), QColor(0, 128, 128));
        colorset->setCurrentText("Dark");

        page->setRoleColor("keyword", QColor(1, 2, 3));
        page->setCurrentRole("comment");
        bold->setChecked(true);
        QCOMPARE(colorset->currentText(), QStringLiteral("Custom"));
        QVERIFY(dialog.apply());
        QCOMPARE(f.config().value("colorset"), QStringLiteral("Custom"));
        ColorSet custom;
        QVERIFY(custom.load(Paths::userDir() + "/colorsets/Custom.xml"));
        QCOMPARE(custom.color("keyword"), QColor(1, 2, 3));
        QVERIFY(custom.entry("comment").bold);

        QVERIFY(!dialog.saveColorSet("bad/name"));
        QVERIFY(dialog.saveColorSet("Mine"));
        QVERIFY(QFile::exists(Paths::userDir() + "/colorsets/Mine.xml"));
        QCOMPARE(colorset->currentText(), QStringLiteral("Mine"));
        QVERIFY(comboItems(colorset).contains("Mine"));
        QVERIFY(dialog.apply());
        QCOMPARE(f.config().value("colorset"), QStringLiteral("Mine"));

        f.config().setValue("colorset", QStringLiteral("Custom"));
        OptionsDialog again(*f.context);
        QCOMPARE(again.colorsPage()->colorSet().color("keyword"), QColor(1, 2, 3));
    }

    void mugenPage()
    {
        Fixture f;
        OptionsDialog dialog(*f.context);
        auto* page = qobject_cast<MugenOptionsPage*>(dialog.enginePage("mugen"));
        QVERIFY(page);
        Config& engine = f.context->engineConfig("mugen");

        QCOMPARE(child<QLabel>(page, "eng_msg_lb")->text(),
                 QStringLiteral("Attention! All settings here auto apply when changed."));
        QStringList groups;
        for (const QGroupBox* group : page->findChildren<QGroupBox*>())
            groups << group->title();
        groups.sort();
        QCOMPARE(groups, (QStringList{"CLSN Default color:", "P1 controls:", "P2 controls:", "Run configurations:"}));
        QStringList buttons;
        for (const char* name : {"engAdd", "engDel", "engUp", "engEdit", "engClear", "engDown"})
            buttons << child<QPushButton>(page, name)->text();
        QCOMPARE(buttons, (QStringList{"Add", "Remove", "\u02C4", "Edit", "Clear", "\u02C5"}));
        QCOMPARE(child<QLabel>(page, "clsnAttack")->text(), QStringLiteral("Attack"));
        QCOMPARE(child<QLabel>(page, "clsnCollision")->text(), QStringLiteral("Collision"));

        QCOMPARE(child<QKeySequenceEdit>(page, "p1_left")->keySequence(), QKeySequence("A"));
        QCOMPARE(child<QKeySequenceEdit>(page, "p1_start")->keySequence(), QKeySequence("U"));
        QCOMPARE(child<QKeySequenceEdit>(page, "p2_up")->keySequence(), QKeySequence("Up"));
        QCOMPARE(child<QKeySequenceEdit>(page, "p2_z")->keySequence(), QKeySequence("9"));
        QCOMPARE(page->clsnColor(true), QColor(255, 128, 0));
        QCOMPARE(page->clsnColor(false), QColor(0, 255, 255));

        child<QKeySequenceEdit>(page, "p1_left")->setKeySequence(QKeySequence("Q"));
        QCOMPARE(engine.value("p1_left"), QStringLiteral("Q"));
        QVERIFY(readFile(Paths::userDir() + "/mugen_config.xml").contains("name=\"p1_left\" value=\"Q\""));
        page->setClsnColor(true, QColor(255, 0, 0));
        QCOMPARE(engine.value("clsn_default_attack"), QStringLiteral("255,0,0"));

        page->addRunConfiguration({"MUGEN 1.1", "C:/mugen/mugen.exe", "", "%0 kfm"});
        page->addRunConfiguration({"MUGEN 1.0", "C:/old/mugen.exe", "", "%0 %0"});
        QCOMPARE(RunConfiguration::load(engine).size(), 2);
        QCOMPARE(page->currentRunConfiguration(), 1);
        page->moveRunConfiguration(1, -1);
        QCOMPARE(RunConfiguration::load(engine).first().name, QStringLiteral("MUGEN 1.0"));
        QVERIFY(!child<QPushButton>(page, "engUp")->isEnabled());
        page->removeRunConfiguration(0);
        QCOMPARE(RunConfiguration::load(engine).size(), 1);

        dialog.loadDefaults();
        QCOMPARE(engine.value("p1_left"), QStringLiteral("A"));
        QCOMPARE(page->clsnColor(true), QColor(255, 128, 0));
        QCOMPARE(page->runConfigurations().size(), 1);
    }

    void runConfigDialog()
    {
        RunConfigDialog add;
        QCOMPARE(add.windowTitle(), QStringLiteral("Add"));
        auto* presets = child<QComboBox>(&add, "eng_params_list");
        QCOMPARE(comboItems(presets),
                 (QStringList{"Current char vs KFM", "Current char vs KFM AI", "Current char vs current char",
                              "Current char vs current char AI", "Current motif", "KFM vs KFM on current Stage",
                              "KFM vs KFM on current motif"}));
        QCOMPARE(child<QLineEdit>(&add, "eng_params")->text(), QStringLiteral("%0 kfm"));
        auto* buttons = add.findChild<QDialogButtonBox*>("buttonBox");
        QVERIFY(buttons->button(QDialogButtonBox::Ok)->isEnabled()); // no field checks, same as Fighter Factory
        child<QLineEdit>(&add, "eng_name")->setText("MUGEN");
        child<QLineEdit>(&add, "eng_exe")->setText("C:/mugen/mugen.exe");
        presets->setCurrentIndex(4);
        Q_EMIT presets->activated(4);
        QCOMPARE(add.value(), (RunConfiguration{"MUGEN", "C:/mugen/mugen.exe", "", "-r %0"}));

        RunConfigDialog edit({"Old", "C:/old/mugen.exe", "C:/old/icon.png", "custom args"}, true);
        QCOMPARE(edit.windowTitle(), QStringLiteral("Edit"));
        QCOMPARE(edit.value().parameters, QStringLiteral("custom args"));
    }

    void saveAndOpenSettingsFile()
    {
        Fixture f;
        OptionsDialog dialog(*f.context);
        child<QSpinBox>(&dialog, "font_size")->setValue(16);
        child<QComboBox>(&dialog, "zoom_center")->setCurrentIndex(1);
        dialog.shortcutEditor()->clear("file_open");
        dialog.colorsPage()->setRoleColor("string", QColor(9, 8, 7));
        f.config().setValue("window_width", 1111);

        QTemporaryDir dir;
        const QString path = dir.filePath("settings.ofsc");
        QVERIFY(dialog.exportSettings(path));
        SettingsBundle bundle;
        QVERIFY(bundle.load(path));
        QCOMPARE(bundle.config.value("font_size"), QStringLiteral("16"));
        QCOMPARE(bundle.config.value("window_width"), QStringLiteral("1111"));
        QCOMPARE(bundle.shortcuts.value("file_open"), QString());
        QCOMPARE(bundle.shortcuts.value("spr_add"), QStringLiteral("Ctrl+A"));
        QCOMPARE(bundle.engines.value("mugen").value("p1_left"), QStringLiteral("A"));
        QCOMPARE(bundle.colorSetName, QStringLiteral("Custom"));

        dialog.load();
        f.config().setValue("window_width", 800);
        QCOMPARE(child<QSpinBox>(&dialog, "font_size")->value(), 10);
        QVERIFY(dialog.importSettings(path));
        QCOMPARE(child<QSpinBox>(&dialog, "font_size")->value(), 16);
        QCOMPARE(child<QComboBox>(&dialog, "zoom_center")->currentIndex(), 1);
        QVERIFY(dialog.shortcutEditor()->keys("file_open").isEmpty());
        QCOMPARE(child<QComboBox>(&dialog, "colorset")->currentText(), QStringLiteral("Custom"));
        QCOMPARE(dialog.colorsPage()->colorSet().color("string"), QColor(9, 8, 7));
        QCOMPARE(f.config().value("font_size"), QStringLiteral("10"));

        QVERIFY(dialog.apply());
        QCOMPARE(f.config().value("font_size"), QStringLiteral("16"));
        QCOMPARE(f.config().value("window_width"), QStringLiteral("1111"));
        QVERIFY(f.context->shortcuts().keys("file_open").isEmpty());

        QVERIFY(!dialog.importSettings(dir.filePath("missing.ofsc")));
    }

    void showOptionsNeedsContext() { QCOMPARE(OptionsDialog::showOptions(), int(QDialog::Rejected)); }
};

QTEST_MAIN(OptionsDialogTest)
#include "OptionsDialogTest.moc"
