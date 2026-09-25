// Script editor, Tools and Debug menus, Options settings and main window tests.

#include "GuiTestSupport.h"
#include "core/actions/ActionRegistry.h"
#include "core/app/Paths.h"
#include "core/app/TaskList.h"
#include "core/code/CodeEditor.h"
#include "core/engine/Engine.h"
#include "core/engine/EngineManager.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/sprites/SpriteDocument.h"
#include "core/sprites/SpriteFile.h"
#include "core/theme/ThemeManager.h"
#include "core/widgets/GraphicsView.h"
#include "core/widgets/PaletteWidget.h"
#include "shell/AdvancedPaletteEditor.h"
#include "shell/AnimationPage.h"
#include "shell/AnimationsEditor.h"
#include "shell/BackgroundsEditor.h"
#include "shell/BackgroundsPage.h"
#include "shell/BottomDock.h"
#include "shell/FileDialogs.h"
#include "shell/MainWindow.h"
#include "shell/ProjectView.h"
#include "shell/SpritePage.h"
#include "shell/SpritesEditor.h"
#include "shell/StatusBar.h"
#include "shell/TaskbarTabs.h"
#include "shell/UpdateChecker.h"
#include "shell/dialogs/BreakpointDialog.h"
#include "shell/dialogs/ScriptEditorDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextLayout>
#include <QTreeView>
#include <QUndoStack>

using namespace OFS;
using namespace OFS::GuiTest;

namespace {

void writeIn(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    writeFile(path, data);
}

// SFF with one 4 px high sprite per (group, number, width), filled with palette colour 1.
QByteArray spriteFile(const QList<std::array<int, 3>>& sprites)
{
    SpriteDocument doc;
    Formats::Palette colors(256);
    colors[1] = Formats::Rgba{200, 100, 50, 255};
    doc.palettes() << SpriteDocument::makePalette(1, 1, colors);
    for (const auto& [group, number, width] : sprites) {
        SpriteEntry entry;
        entry.header.group = std::uint16_t(group);
        entry.header.number = std::uint16_t(number);
        entry.pixels = SpriteDocument::makePixels(
            Formats::Image::indexed(width, 4, std::vector<std::uint8_t>(std::size_t(width) * 4, 1), colors));
        entry.palette = doc.palettes().first();
        doc.sprites() << entry;
    }
    const auto bytes = doc.toSff().write();
    return QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size()));
}

// Visible menu items as object names. "|" is a separator and "<text>" an item with no name.
QStringList visibleIds(const QMenu* menu)
{
    QStringList ids;
    for (QAction* a : menu->actions()) {
        if (!a->isVisible())
            continue;
        if (a->isSeparator())
            ids << QStringLiteral("|");
        else if (!a->objectName().isEmpty())
            ids << a->objectName();
        else
            ids << QLatin1Char('<') + a->text() + QLatin1Char('>');
    }
    return ids;
}

void whenPopup(std::function<void(QMenu*)> look)
{
    auto* timer = new QTimer(qApp);
    timer->setInterval(5);
    QObject::connect(timer, &QTimer::timeout, qApp, [timer, look] {
        auto* popup = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!popup || !popup->isVisible())
            return;
        timer->stop();
        timer->deleteLater();
        look(popup);
        popup->close();
    });
    timer->start();
}

void click(QWidget* widget, const QPoint& at, bool press = true)
{
    if (press) {
        QMouseEvent down(QEvent::MouseButtonPress, at, widget->mapToGlobal(at), Qt::LeftButton, Qt::LeftButton,
                         Qt::NoModifier);
        QCoreApplication::sendEvent(widget, &down);
    }
    QMouseEvent up(QEvent::MouseButtonRelease, at, widget->mapToGlobal(at), Qt::LeftButton, Qt::NoButton,
                   Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &up);
}

QStringList headers(const QTreeView* view)
{
    QStringList texts;
    for (int c = 0; c < view->model()->columnCount(); ++c)
        texts << view->model()->headerData(c, Qt::Horizontal).toString();
    return texts;
}

QImage iconImage(const QIcon& icon)
{
    return icon.pixmap(16, 16).toImage();
}

} // namespace

class ShellOptionsParityTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_context = makeContext();
        QVERIFY(m_dir.isValid());
        QVERIFY(m_data.isValid());
        Config& config = m_context->config();
        for (const char* key : kKeys)
            m_saved.insert(QString::fromLatin1(key), config.value(QString::fromLatin1(key)));
        config.setValue("confirm_close", false);
        config.setValue("auto_update", false);
        config.setValue("show_addsprite_dialog", false);

        // A character (sprite 9000,1 is its portrait) and a stage with one background element.
        writeIn(m_dir.filePath("c/c.def"), "[Info]\nname = \"Shell\"\nmugenversion = 1.0\n[Files]\ncmd = c.cmd\n"
                                           "cns = c.cns\nst = c.cns\nsprite = c.sff\nanim = c.air\n");
        writeIn(m_dir.filePath("c/c.cmd"), "[Command]\nname = \"x\"\ncommand = x\n");
        writeIn(m_dir.filePath("c/c.cns"), "[Data]\nlife = 1000\n\n[Statedef 0]\ntype = S\n");
        writeIn(m_dir.filePath("c/c.sff"), spriteFile({{0, 0, 4}, {0, 1, 6}, {9000, 1, 8}}));
        writeIn(m_dir.filePath("c/c.air"), "; Stand\n[Begin Action 0]\nClsn2: 1\n Clsn2[0] = -5,-5, 5,0\n"
                                           "0,0, 0,0, 5\n0,1, 0,0, 5\n");
        writeIn(m_dir.filePath("s/s.def"), "[Info]\nname = \"Stage\"\nmugenversion = 1.0\n[Camera]\nstartx = 0\n"
                                           "[StageInfo]\nlocalcoord = 320,240\n[BGdef]\nspr = s.sff\n"
                                           "[BG Floor]\ntype = normal\nspriteno = 0,0\n");
        writeIn(m_dir.filePath("s/s.sff"), spriteFile({{0, 0, 4}}));
        writeIn(m_data.filePath("scripts/tool/Quick action script.lua"), "function main()\nend\n");
        writeIn(m_data.filePath("engines/mugen/scripts/tool/.keep"), "");
    }

    void cleanup()
    {
        FileDialogs::setOpenPromptOverride({});
        FileDialogs::setSavePromptOverride({});
        UpdateChecker::setReplyOverride({});
        // Options > Apply saves the settings file, so put the old values back.
        Config& config = m_context->config();
        for (auto it = m_saved.cbegin(); it != m_saved.cend(); ++it)
            config.setValue(it.key(), it.value());
        config.setValue("confirm_close", false);
        config.setValue("auto_update", false);
        config.setValue("show_addsprite_dialog", false);
        config.save();
    }

    void cleanupTestCase() { delete m_context; }

    void scriptEditorModal()
    {
        MainWindow window(*m_context);
        window.show();
        bool checked = false;
        whenModal<QDialog>([&](QDialog* dialog) {
            QCOMPARE(dialog->objectName(), QStringLiteral("ScriptEd"));
            QCOMPARE(dialog->windowModality(), Qt::ApplicationModal);
            QCOMPARE(dialog->parentWidget(), &window);
            QCOMPARE(QApplication::activeModalWidget(), dialog);
            checked = true;
            dialog->reject();
        });
        m_context->actions().action("tools_script_editor")->trigger();
        QVERIFY(checked);
    }

    void scriptEditorButtons()
    {
        ScriptEditorDialog dialog(*m_context, environment(), m_data.path());
        QVERIFY(dialog.editor()->toPlainText().isEmpty());
        QCOMPARE(dialog.toolBar()->actions().size(), qsizetype(6));
        for (QAction* action : dialog.toolBar()->actions())
            QVERIFY2(action->isEnabled(), qPrintable(action->objectName()));
    }

    // The script gets Lua highlighting but no completion or automatic indentation.
    void scriptEditorEditing()
    {
        ScriptEditorDialog dialog(*m_context, environment(), m_data.path());
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        CodeEditor* editor = dialog.editor();
        editor->setFocus();
        QTest::keyClicks(editor, "local t = FF.");
        QVERIFY(!editor->isCompleting());
        QVERIFY(!editor->completer() || !editor->completer()->popup()->isVisible());
        editor->clear();
        QTest::keyClicks(editor, "function main()");
        QTest::keyClick(editor, Qt::Key_Return);
        QTest::keyClicks(editor, "  x = 1");
        QTest::keyClick(editor, Qt::Key_Return);
        QTest::keyClicks(editor, "end");
        QCOMPARE(editor->toPlainText(), QStringLiteral("function main()\n  x = 1\nend"));
        const QList<QTextLayout::FormatRange> formats = editor->document()->firstBlock().layout()->formats();
        const bool keyword = std::any_of(formats.cbegin(), formats.cend(), [](const QTextLayout::FormatRange& r) {
            return r.start == 0 && r.length == 8 && r.format.hasProperty(QTextFormat::ForegroundBrush);
        });
        QVERIFY(keyword);
        editor->document()->setModified(false);
    }

    void changeBarsAndCodeMenu()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("one.lua"), "-- one\nx = 1\ny = 2\n");
        ScriptEditorDialog dialog(*m_context, environment(), m_data.path());
        dialog.show();
        CodeEditor* editor = dialog.editor();
        QVERIFY(dialog.openFile(dir.filePath("one.lua")));
        for (int line = 0; line < 4; ++line)
            QVERIFY(!editor->isLineChanged(line));
        QTest::keyClicks(editor, "z = 3");
        QTest::keyClick(editor, Qt::Key_Return);
        QTest::keyClicks(editor, "w = 4");
        QCOMPARE(editor->blockCount(), 5);
        QVERIFY(!editor->isLineChanged(0) && !editor->isLineChanged(1) && !editor->isLineChanged(2));
        QVERIFY(editor->isLineChanged(3) && editor->isLineChanged(4));
        const QImage gutter = editor->findChild<QWidget*>("codeGutter")->grab().toImage();
        const int lineTop = int(editor->cursorRect(QTextCursor(editor->document()->findBlockByNumber(4))).top());
        bool red = false;
        for (int x = 0; x < gutter.width(); ++x) {
            const QColor c = gutter.pixelColor(x, lineTop + 2);
            red = red || (c.red() > 180 && c.green() < 60 && c.blue() < 60);
        }
        QVERIFY(red);
        while (editor->document()->isUndoAvailable())
            editor->undo();
        for (int line = 0; line < editor->blockCount(); ++line)
            QVERIFY(!editor->isLineChanged(line));

        QMenu* menu = editor->createContextMenu(QPoint(5, 5));
        QMenu* code = menu->actions().last()->menu();
        QVERIFY(code);
        QCOMPARE(code->title(), QStringLiteral("Code"));
        QCOMPARE(visibleIds(code),
                 (QStringList{"code_goto", "code_prev_error", "code_next_error", "code_prev_deprecation",
                              "code_next_deprecation", "|", "code_comment", "code_fold_all", "code_unfold_all", "|",
                              "code_autoindent", "code_autoformat"}));
        editor->releaseContextMenu(menu);
        QTextCursor lines(editor->document()->findBlockByNumber(1));
        lines.setPosition(editor->document()->findBlockByNumber(2).position() + 5, QTextCursor::KeepAnchor);
        editor->setTextCursor(lines);
        QAction* comment = nullptr;
        for (QAction* a : dialog.codeMenu()->actions()) {
            if (a->objectName() == QLatin1String("code_comment"))
                comment = a;
        }
        QVERIFY(comment);
        comment->trigger();
        QCOMPARE(editor->toPlainText(), QStringLiteral("-- one\n--x = 1\n--y = 2\n"));
        comment->trigger();
        QCOMPARE(editor->toPlainText(), QStringLiteral("-- one\nx = 1\ny = 2\n"));
        editor->document()->setModified(false);

        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("c/c.def")));
        window.switchEditor("commands");
        CodeEditor* commands = window.currentProjectView()->currentCodeEditor();
        QVERIFY(commands);
        QVERIFY(!commands->isLineChanged(0) && !commands->isLineChanged(1));
        commands->moveCursor(QTextCursor::End);
        commands->insertPlainText("; mine");
        QVERIFY(!commands->isLineChanged(0) && !commands->isLineChanged(1));
        QVERIFY(commands->isLineChanged(commands->blockCount() - 1));
        auto* file = qobject_cast<CodeFile*>(window.currentProjectView()->currentFile());
        QVERIFY(file && file->load(nullptr));
        for (int line = 0; line < commands->blockCount(); ++line)
            QVERIFY(!commands->isLineChanged(line));
    }

    void scriptEditorUnsavedText()
    {
        ScriptEditorDialog dialog(*m_context, environment(), m_data.path());
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        dialog.editor()->insertPlainText("x = 1\n");
        QStringList texts;
        answerBoxes({QMessageBox::Cancel}, &texts);
        QTest::keyClick(&dialog, Qt::Key_Escape);
        QVERIFY(dialog.isVisible());
        answerBoxes({QMessageBox::Cancel}, &texts);
        QVERIFY(!dialog.close());
        QVERIFY(dialog.isVisible());
        answerBoxes({QMessageBox::No}, &texts);
        dialog.reject();
        QVERIFY(!dialog.isVisible());
        QCOMPARE(texts, QStringList(3, "The script was changed. Do you want to save it?"));
    }

    void scriptEditorOpen()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("two.lua"), "-- two\nreturn 2\n");
        ScriptEditorDialog dialog(*m_context, environment(), m_data.path());
        QString asked;
        FileDialogs::setOpenPromptOverride([&](const QString& caption, const QString& folder, const QString& filter) {
            asked = caption + '|' + filter + '|' + QDir::cleanPath(folder);
            return dir.filePath("two.lua");
        });
        dialog.toolBar()->actions().at(1)->trigger();
        QCOMPARE(asked, "Open script|Lua Script (*.lua)|" + QDir::cleanPath(dialog.selectedFolder()));
        QCOMPARE(dialog.editor()->toPlainText(), QStringLiteral("-- two\nreturn 2\n"));
        QVERIFY(dialog.editor()->textCursor().atEnd());
        QCOMPARE(dialog.fileName(), QFileInfo(dir.filePath("two.lua")).absoluteFilePath());
    }

    void scriptEditorSaveWithoutFile()
    {
        ScriptEditorDialog dialog(*m_context, environment(), m_data.path());
        dialog.editor()->setPlainText("return 1\n");
        QTemporaryDir dir;
        QStringList asked;
        FileDialogs::setSavePromptOverride([&](const QString& caption, const QString& folder, const QString& filter) {
            asked << caption + '|' + filter + '|' + QDir::cleanPath(folder);
            return dir.filePath(asked.size() == 1 ? "named" : "other");
        });
        dialog.toolBar()->actions().at(2)->trigger(); // Save
        QCOMPARE(asked.value(0), "Save script|Lua Script (*.lua)|" + QDir::cleanPath(m_data.filePath("scripts/tool")));
        QVERIFY(QFile::exists(dir.filePath("named.lua")));
        QCOMPARE(dialog.fileName(), QFileInfo(dir.filePath("named.lua")).absoluteFilePath());
        QVERIFY(dialog.windowTitle().contains("named.lua"));
        dialog.engineFilter()->setCurrentIndex(1);
        dialog.toolBar()->actions().at(3)->trigger(); // Save as
        QCOMPARE(asked.value(1),
                 "Save script|Lua Script (*.lua)|" + QDir::cleanPath(m_data.filePath("engines/mugen/scripts/tool")));
        QCOMPARE(dialog.fileName(), QFileInfo(dir.filePath("other.lua")).absoluteFilePath());
    }

    void scriptEditorRunClearsOutput()
    {
        ScriptEditorDialog dialog(*m_context, environment(), m_data.path());
        dialog.editor()->setPlainText("debugOutput('once')\n");
        dialog.run();
        dialog.run();
        QCOMPARE(dialog.output()->toPlainText(), QStringLiteral("once"));
        dialog.editor()->document()->setModified(false);
    }

    // Files added later only show up once the list is read again, for example after a save.
    void scriptList()
    {
        ScriptEditorDialog dialog(*m_context, environment(), m_data.path());
        QCOMPARE(dialog.scriptNames()->stringList(), QStringList{"Quick action script"});
        writeIn(m_data.filePath("scripts/tool/Later.lua"), "return 0\n");
        QCOMPARE(dialog.scriptNames()->stringList(), QStringList{"Quick action script"});
        Q_EMIT dialog.scriptList()->doubleClicked(dialog.scriptNames()->index(0));
        QCOMPARE(dialog.fileName(),
                 QFileInfo(m_data.filePath("scripts/tool/Quick action script.lua")).absoluteFilePath());
        QCOMPARE(dialog.editor()->toPlainText(), QStringLiteral("function main()\nend\n"));
        dialog.editor()->setPlainText("return 'mine'\n");
        QVERIFY(dialog.saveAs(m_data.filePath("scripts/tool/Mine.lua")));
        QCOMPARE(dialog.scriptNames()->stringList(), (QStringList{"Later", "Mine", "Quick action script"}));
        QFile::remove(m_data.filePath("scripts/tool/Later.lua"));
        QFile::remove(m_data.filePath("scripts/tool/Mine.lua"));
    }

    // Scripts in the user folder are not listed. A failing tool script shows its error.
    void toolsMenu()
    {
        const QString user = Paths::userDir();
        writeIn(user + "/scripts/tool/UserToolA.lua", "function main() end\n");
        writeIn(user + "/engines/mugen/scripts/tool/UserToolB.lua", "function main() end\n");
        MainWindow window(*m_context);
        window.show();
        QMenu* tools = menu(window, "menu_tools");
        Q_EMIT tools->aboutToShow();
        QCOMPARE(visibleIds(tools), (QStringList{"tools_options", "|", "tools_pal_editor", "tools_script_editor", "|",
                                                 "<Quick action script>"}));
        QVERIFY(window.openProject(m_dir.filePath("c/c.def")));
        Q_EMIT tools->aboutToShow();
        QCOMPARE(visibleIds(tools), (QStringList{"tools_options", "|", "tools_pal_editor", "tools_script_editor", "|",
                                                 "tools_export_html", "<Quick action script>"}));
        QFile::remove(user + "/scripts/tool/UserToolA.lua");
        QFile::remove(user + "/engines/mugen/scripts/tool/UserToolB.lua");

        QTemporaryDir dir;
        writeFile(dir.filePath("Broken.lua"), "error('tool failed')\n");
        QStringList boxes;
        whenModal<QMessageBox>([&boxes](QMessageBox* box) {
            boxes << box->windowTitle() + '|' + box->text();
            QCOMPARE(box->icon(), QMessageBox::Critical);
            box->accept();
        });
        window.runToolScript(dir.filePath("Broken.lua"));
        QCOMPARE(boxes.size(), qsizetype(1));
        QVERIFY2(boxes[0].startsWith(boxTitle("Script error!") + "|sol: runtime error:"), qPrintable(boxes[0]));
        QVERIFY(boxes[0].contains("tool failed"));
    }

    // Debug actions have no default keys. The bottom panel items of the View menu need a project.
    void debugActions()
    {
        const QList<QPair<const char*, const char*>> captions = {
            {"debug_start_engine", "Start debugger on Engine"},
            {"debug_start_emul", "Start debugger on built-in emulator"},
            {"debug_interrupt", "Interrupt"},
            {"debug_continue", "Continue"},
            {"debug_stop", "Stop"},
            {"debug_restart", "Restart"},
            {"debug_step_over", "Step to the next instruction without entering into functions"},
            {"debug_step_into", "Step to the next instruction entering into functions"},
            {"debug_step_out", "Step to the next instruction out of current function"},
            {"debug_step_frame", "Step to the next frame"},
            {"debug_breakpoint", "Toggle breakpoint"},
            {"breakpoint_add", "Add breakpoint..."},
            {"debug_start", "Start/Resume"},
            {"breakpoint_edit", "Edit selected breakpoint"},
            {"breakpoint_enable", "Enable selected breakpoint"},
            {"breakpoint_disable", "Disable selected breakpoint"},
            {"breakpoint_remove", "Remove selected breakpoint"},
            {"breakpoint_remove_all", "Remove all breakpoints"},
            {"watch_remove", "Remove selected"},
            {"watch_remove_all", "Remove all"},
            {"debug_data_watch1", "Watch variable (1)"},
            {"debug_data_watch2", "Watch variable (2)"},
            {"debug_data_breakpoint", "Add data breakpoint..."},
        };
        ActionRegistry& actions = m_context->actions();
        for (const auto& [id, caption] : captions) {
            QAction* action = actions.action(QString::fromLatin1(id));
            QVERIFY2(action, id);
            QVERIFY2(action->shortcuts().isEmpty(), id);
            QCOMPARE(actions.description(QString::fromLatin1(id)), QString::fromLatin1(caption));
        }

        MainWindow window(*m_context);
        window.show();
        const QStringList panels{"view_issues", "view_advanced_search", "view_engine_output", "view_debugger",
                                 "view_watch"};
        for (const QString& id : panels)
            QVERIFY2(!actions.action(id)->isEnabled(), qPrintable(id));
        QVERIFY(actions.action("view_code_bar")->isCheckable());
        QVERIFY(window.openProject(m_dir.filePath("c/c.def")));
        for (const QString& id : panels)
            QVERIFY2(actions.action(id)->isEnabled(), qPrintable(id));
        QVERIFY(actions.action("view_code_bar")->isChecked());
    }

    void breakpointBrowse()
    {
        BreakpointDialog dialog;
        QString asked;
        FileDialogs::setOpenPromptOverride([&](const QString& caption, const QString&, const QString& filter) {
            asked = caption + '|' + filter;
            return m_dir.filePath("c/c.cns");
        });
        dialog.findChild<QPushButton*>("brkBrowse")->click();
        QCOMPARE(asked, QStringLiteral("Choose file...|All files (*.*)"));
        QCOMPARE(dialog.findChild<QLineEdit*>("brkFile")->text(), QDir::toNativeSeparators(m_dir.filePath("c/c.cns")));
    }

    void debuggerPanel()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("c/c.def")));
        BottomDock* dock = window.bottomDock();
        QCOMPARE(headers(dock->callStackView()), (QStringList{"Level", "Function", "File", "Line"}));
        QCOMPARE(headers(dock->breakpointsView()),
                 (QStringList{"Number", "Function", "File", "Line", "Address", "Condition", "Ignore"}));
        window.switchEditor("states");
        CodeEditor* editor = window.currentProjectView()->currentCodeEditor();
        ActionRegistry& actions = m_context->actions();
        editor->goToLine(1);
        actions.action("debug_breakpoint")->trigger();
        editor->goToLine(3);
        actions.action("debug_breakpoint")->trigger();
        dock->showPage(BottomDock::Debugger);
        QTreeView* list = dock->breakpointsView();
        list->resize(600, 300);
        auto* model = list->model();
        QCOMPARE(model->rowCount(), 2);
        const IconSet& icons = m_context->theme().iconSet();
        QCOMPARE(iconImage(model->index(0, 0).data(Qt::DecorationRole).value<QIcon>()),
                 iconImage(icons.icon("breakpoint_pending")));

        auto menuOn = [&](const QPoint& at) {
            QStringList items;
            whenPopup([&items](QMenu* menu) {
                for (QAction* a : menu->actions())
                    items << a->objectName() + (a->isEnabled() ? "" : "(off)");
            });
            Q_EMIT list->customContextMenuRequested(at);
            return items;
        };
        const QPoint row0 = list->visualRect(model->index(0, 0)).center();
        list->setCurrentIndex(model->index(0, 0));
        QCOMPARE(menuOn(row0),
                 (QStringList{"breakpoint_edit", "breakpoint_disable", "breakpoint_remove", "breakpoint_remove_all"}));
        actions.action("breakpoint_disable")->trigger();
        QCOMPARE(iconImage(model->index(0, 0).data(Qt::DecorationRole).value<QIcon>()),
                 iconImage(icons.icon("breakpoint_disabled")));
        QVERIFY(iconImage(icons.icon("breakpoint_disabled")) != iconImage(icons.icon("breakpoint_pending")));
        list->setCurrentIndex(model->index(0, 0));
        QCOMPARE(menuOn(row0),
                 (QStringList{"breakpoint_edit", "breakpoint_enable", "breakpoint_remove", "breakpoint_remove_all"}));
        list->setCurrentIndex(model->index(1, 0));
        const QPoint empty(list->viewport()->width() / 2, list->viewport()->height() - 3);
        QVERIFY(!list->indexAt(empty).isValid());
        QCOMPARE(menuOn(empty),
                 (QStringList{"breakpoint_edit(off)", "breakpoint_remove(off)", "breakpoint_remove_all"}));
        QVERIFY(!list->selectionModel()->hasSelection());
        actions.action("breakpoint_remove_all")->trigger();
    }

    void nativeOpenDialog()
    {
        MainWindow window(*m_context);
        window.show();
        applyOptions([](QDialog* dialog) { dialog->findChild<QCheckBox*>("use_native_open")->setChecked(false); });
        QVERIFY(FileDialogs::openDialogOptions().testFlag(QFileDialog::DontUseNativeDialog));
        applyOptions([](QDialog* dialog) { dialog->findChild<QCheckBox*>("use_native_open")->setChecked(true); });
        QVERIFY(!FileDialogs::openDialogOptions().testFlag(QFileDialog::DontUseNativeDialog));
    }

    void sortFileNames()
    {
        QTemporaryDir dir;
        auto image = [&dir](const QString& name, int width) {
            QImage picture(width, 3, QImage::Format_Indexed8);
            picture.setColorTable({qRgb(0, 0, 0), qRgb(10, 200, 30)});
            picture.fill(1);
            QVERIFY(picture.save(dir.filePath(name)));
        };
        image("a.png", 2);
        image("b.png", 5);
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("c/c.def")));
        window.switchEditor("sprites");
        SpritePage* page = window.currentProjectView()->spritesEditor()->page();
        FileDialogs::setOpenPromptOverride([&](const QString&, const QString&, const QString&) {
            return dir.filePath("b.png") + '\n' + dir.filePath("a.png");
        });
        auto addedWidths = [&] {
            page->setCurrent(page->file()->document().spriteCount() - 1);
            m_context->actions().action("spr_add")->trigger();
            const SpriteDocument& doc = page->file()->document();
            const int last = doc.spriteCount() - 1;
            return QList<int>{int(doc.image(last - 1).width()), int(doc.image(last).width())};
        };
        QCOMPARE(addedWidths(), (QList<int>{2, 5}));
        applyOptions([](QDialog* dialog) { dialog->findChild<QCheckBox*>("sort_file_names")->setChecked(false); });
        QCOMPARE(addedWidths(), (QList<int>{5, 2}));
        page->file()->undoStack()->setClean();
    }

    // The startup check stays quiet unless a newer version exists.
    void updateChecks()
    {
        QList<bool> checks;
        UpdateChecker::Reply answer;
        answer.error = QStringLiteral("Host not found");
        UpdateChecker::setReplyOverride([&](bool interactive) {
            checks << interactive;
            return answer;
        });
        {
            MainWindow window(*m_context);
            window.show();
            QTest::qWait(20);
            QVERIFY(checks.isEmpty());
            applyOptions([](QDialog* dialog) { dialog->findChild<QCheckBox*>("auto_update")->setChecked(true); });
            QVERIFY(m_context->config().boolValue("auto_update"));
        }
        {
            MainWindow window(*m_context);
            window.show();
            QTest::qWait(20);
            QCOMPARE(checks, QList<bool>{false});
            QVERIFY(!QApplication::activeModalWidget());
        }
        answer = UpdateChecker::Reply{"{\"tag_name\": \"v99.0.0\", \"html_url\": \"https://example.com/r\"}", {}, 200};
        QStringList boxes;
        auto record = [&boxes](QMessageBox::StandardButton button) {
            whenModal<QMessageBox>([&boxes, button](QMessageBox* box) {
                boxes << box->windowTitle() + '|' + box->text();
                box->button(button)->click();
            });
        };
        {
            record(QMessageBox::No);
            MainWindow window(*m_context);
            window.show();
            QTRY_COMPARE(boxes.size(), qsizetype(1));
            QCOMPARE(boxes[0], boxTitle("Check updates") +
                                   "|Version v99.0.0 is available. Do you want to open the download page?");
        }
        m_context->config().setValue("auto_update", false);
        MainWindow window(*m_context);
        window.show();
        checks.clear();
        boxes.clear();
        answer = UpdateChecker::Reply{{}, QStringLiteral("Host not found"), 0};
        record(QMessageBox::Ok);
        m_context->actions().action("check_updates")->trigger();
        QTRY_COMPARE(boxes.size(), qsizetype(1));
        QCOMPARE(checks, QList<bool>{true});
        QCOMPARE(boxes[0], boxTitle("Check updates") + "|Could not check for updates: Host not found");
        answer = UpdateChecker::Reply{{}, QStringLiteral("Not found"), 404};
        record(QMessageBox::Ok);
        m_context->actions().action("check_updates")->trigger();
        QTRY_COMPARE(boxes.size(), qsizetype(2));
        QCOMPARE(boxes[1],
                 boxTitle("Check updates") + "|Could not check for updates: No release has been published yet.");
        answer = UpdateChecker::Reply{"{\"tag_name\": \"v0.0.1\"}", {}, 200};
        record(QMessageBox::Ok);
        m_context->actions().action("check_updates")->trigger();
        QTRY_COMPARE(boxes.size(), qsizetype(3));
        QCOMPARE(boxes[2], boxTitle("Check updates") + "|You are using the latest version.");
    }

    void interfaceScale()
    {
        MainWindow window(*m_context);
        window.show();
        applyOptions([](QDialog* dialog) {
            dialog->findChild<QCheckBox*>("dpi_auto")->setChecked(false);
            dialog->findChild<QDoubleSpinBox*>("dpi_scale")->setValue(1.0);
        });
        QCOMPARE(m_context->theme().scale(), 1.0);
        const QString normal = qApp->styleSheet();
        applyOptions([](QDialog* dialog) { dialog->findChild<QDoubleSpinBox*>("dpi_scale")->setValue(1.5); });
        QCOMPARE(m_context->theme().scale(), 1.5);
        QVERIFY(qApp->styleSheet() != normal);
        applyOptions([](QDialog* dialog) { dialog->findChild<QCheckBox*>("dpi_auto")->setChecked(true); });
        QCOMPARE(m_context->theme().scale(), 1.0);
        QCOMPARE(qApp->styleSheet(), normal);
    }

    void paletteFitsSpace()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("c/c.def")));
        auto columns = [&] {
            QList<int> result;
            whenModal<AdvancedPaletteEditor>([&result](AdvancedPaletteEditor* editor) {
                editor->resize(2200, 900);
                QTest::qWait(20);
                for (PaletteWidget* grid : editor->findChildren<PaletteWidget*>()) {
                    if (grid->isVisible())
                        result << grid->columns();
                }
                editor->reject();
            });
            m_context->actions().action("tools_pal_editor")->trigger();
            return result;
        };
        const QList<int> off = columns();
        QVERIFY(!off.isEmpty());
        for (int c : off)
            QCOMPARE(c, 16);
        applyOptions([](QDialog* dialog) { dialog->findChild<QCheckBox*>("pal_fit_space")->setChecked(true); });
        const QList<int> on = columns();
        QCOMPARE(on.size(), off.size());
        for (int c : on)
            QVERIFY2(c > 16, qPrintable(QString::number(c)));
    }

    void graphicsOptions()
    {
        QTemporaryDir dir;
        QImage cross(5, 5, QImage::Format_ARGB32);
        cross.fill(Qt::red);
        QVERIFY(cross.save(dir.filePath("cross.png")));
        QImage back(10, 12, QImage::Format_ARGB32);
        back.fill(Qt::blue);
        QVERIFY(back.save(dir.filePath("back.png")));

        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("s/s.def")));
        BackgroundsPage* stage = window.currentProjectView()->backgroundsEditor()->page();
        QVERIFY(window.openProject(m_dir.filePath("c/c.def")));
        ProjectView* view = window.currentProjectView();
        window.switchEditor("animations");
        AnimationPage* animation = view->animationsEditor()->page();
        window.switchEditor("sprites");
        SpritePage* sprites = view->spritesEditor()->page();
        QVERIFY(stage && animation && sprites);
        QList<GraphicsView*> views{sprites->view(), animation->view()};

        applyOptions([&](QDialog* dialog) {
            dialog->findChild<QRadioButton*>("crosshair_custom")->setChecked(true);
            dialog->findChild<QLineEdit*>("crosshair_main")->setText(dir.filePath("cross.png"));
            dialog->findChild<QRadioButton*>("background_custom")->setChecked(true);
            dialog->findChild<QLineEdit*>("background_file")->setText(dir.filePath("back.png"));
            dialog->findChild<QSpinBox*>("background_x")->setValue(5);
            dialog->findChild<QSpinBox*>("background_y")->setValue(-6);
            dialog->findChild<QComboBox*>("zoom_mode")->setCurrentIndex(0);
            dialog->findChild<QComboBox*>("zoom_center")->setCurrentIndex(1);
            dialog->findChild<QSlider*>("box_opacity")->setValue(30);
            dialog->findChild<QCheckBox*>("sprite_over_guides")->setChecked(true);
            dialog->findChild<QCheckBox*>("contact_over_cross")->setChecked(true);
        });
        for (GraphicsView* v : views) {
            QCOMPARE(v->crossStyle(), GraphicsView::CrossStyle::Custom);
            QCOMPARE(v->crossPicture(0).size(), QSize(5, 5));
            QCOMPARE(v->background(), GraphicsView::Background::Picture);
            QCOMPARE(v->backgroundPicture().size(), QSize(10, 12));
            QCOMPARE(v->backgroundOffset(), QPoint(5, -6));
            QCOMPARE(v->zoomLevels(), GraphicsView::smallZoomRange());
            QCOMPARE(v->zoomCenter(), GraphicsView::ZoomCenter::Workspace);
            QCOMPARE(v->boxOpacity(), 0.3);
            QVERIFY(v->imagesOverGuides());
        }
        QVERIFY(animation->view()->boxesOverCrosses());
        QVERIFY(stage->view()->imagesOverGuides());

        applyOptions([&](QDialog* dialog) {
            dialog->findChild<QRadioButton*>("crosshair_modern")->setChecked(true);
            dialog->findChild<QRadioButton*>("background_none")->setChecked(true);
            dialog->findChild<QComboBox*>("zoom_mode")->setCurrentIndex(1);
            dialog->findChild<QComboBox*>("zoom_center")->setCurrentIndex(0);
            dialog->findChild<QCheckBox*>("sprite_over_guides")->setChecked(false);
            dialog->findChild<QCheckBox*>("contact_over_cross")->setChecked(false);
        });
        for (GraphicsView* v : views) {
            QCOMPARE(v->crossStyle(), GraphicsView::CrossStyle::Modern);
            QCOMPARE(v->background(), GraphicsView::Background::Color);
            QCOMPARE(v->zoomLevels(), GraphicsView::largeZoomRange());
            QCOMPARE(v->zoomCenter(), GraphicsView::ZoomCenter::Axis);
            QVERIFY(!v->imagesOverGuides());
        }
        QVERIFY(!animation->view()->boxesOverCrosses());
        QVERIFY(!stage->view()->imagesOverGuides());

        applyOptions([&](QDialog* dialog) {
            dialog->findChild<QRadioButton*>("crosshair_default")->setChecked(true);
            dialog->findChild<QRadioButton*>("background_default")->setChecked(true);
            dialog->findChild<QSpinBox*>("background_x")->setValue(-426);
            dialog->findChild<QSpinBox*>("background_y")->setValue(-416);
        });
        for (GraphicsView* v : views) {
            QCOMPARE(v->crossStyle(), GraphicsView::CrossStyle::Default);
            QCOMPARE(v->background(), GraphicsView::Background::Picture);
            QCOMPARE(v->backgroundPicture().size(), GraphicsView::defaultBackground().size());
            QCOMPARE(v->backgroundOffset(), QPoint(-426, -416));
        }
    }

    void onionSkinOptions()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("c/c.def")));
        ProjectView* view = window.currentProjectView();
        window.switchEditor("sprites");
        SpritePage* sprites = view->spritesEditor()->page();
        sprites->setCurrent(0);
        view->findChild<QCheckBox*>("spr_onion_skin")->setChecked(true);
        view->findChild<QScrollBar*>("spr_onion_sel")->setValue(1);
        window.switchEditor("animations");
        AnimationPage* animation = view->animationsEditor()->page();
        view->findChild<QCheckBox*>("anim_onion_skin")->setChecked(true);
        view->findChild<QScrollBar*>("anim_onion_framesel")->setValue(1);
        auto onion = [](GraphicsView* v) {
            return v->images().isEmpty() ? GraphicsView::Image{} : v->images().first();
        };
        auto colour = [&onion](GraphicsView* v) {
            const QImage image = onion(v).image;
            return image.isNull() ? QColor() : image.pixelColor(image.width() / 2, image.height() / 2);
        };
        for (GraphicsView* v : {sprites->view(), animation->view()}) {
            QCOMPARE(v->images().size(), qsizetype(2));
            QCOMPARE(colour(v), QColor(227, 177, 152));
        }
        applyOptions([](QDialog* dialog) {
            dialog->findChild<QComboBox*>("onion_effect")->setCurrentIndex(1); // Monochrome
            dialog->findChild<QSlider*>("onion_opacity")->setValue(50);
        });
        QVERIFY(view->findChild<QCheckBox*>("spr_onion_skin")->isChecked());
        QVERIFY(view->findChild<QCheckBox*>("anim_onion_skin")->isChecked());
        const int grey = qGray(200, 100, 50);
        for (GraphicsView* v : {sprites->view(), animation->view()}) {
            QCOMPARE(colour(v), QColor(grey, grey, grey));
            QCOMPARE(onion(v).opacity, 0.5);
        }
        applyOptions([](QDialog* dialog) {
            dialog->findChild<QComboBox*>("onion_effect")->setCurrentIndex(0); // None
            dialog->findChild<QSlider*>("onion_opacity")->setValue(74);
        });
        for (GraphicsView* v : {sprites->view(), animation->view()}) {
            QCOMPARE(colour(v), QColor(200, 100, 50));
            QCOMPARE(onion(v).opacity, 0.74);
        }
    }

    void codeOptions()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("t.def"), "[Info]\nname = \"Tabs\"\nmugenversion = 1.0\n[Files]\ncmd = t.cmd\n");
        writeFile(dir.filePath("t.cmd"), "[Command]\n\tname = \"x\"\n\tcommand = x\n");
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("c/c.def")));
        window.switchEditor("commands");
        CodeEditor* editor = window.currentProjectView()->currentCodeEditor();
        const int wide = editor->gutterWidth();
        applyOptions([](QDialog* dialog) {
            dialog->findChild<QCheckBox*>("code_line_numbers")->setChecked(false);
            dialog->findChild<QCheckBox*>("code_folding_markers")->setChecked(false);
            dialog->findChild<QCheckBox*>("code_highlight_line")->setChecked(false);
            dialog->findChild<QCheckBox*>("code_highlight_blocks")->setChecked(true);
            dialog->findChild<QCheckBox*>("code_paren_matching")->setChecked(false);
            dialog->findChild<QCheckBox*>("open_tabToSpace")->setChecked(true);
            dialog->findChild<QCheckBox*>("save_spaceToTab")->setChecked(true);
        });
        CodeEditor::Options options = editor->options();
        QVERIFY(!options.lineNumbers && !options.foldingMarkers && !options.highlightLine && options.highlightBlocks &&
                !options.parenMatching);
        QVERIFY(editor->gutterWidth() < wide);
        QVERIFY(window.openProject(dir.filePath("t.def")));
        window.switchEditor("commands");
        CodeEditor* tabs = window.currentProjectView()->currentCodeEditor();
        QCOMPARE(tabs->toPlainText(), QStringLiteral("[Command]\n    name = \"x\"\n    command = x\n"));
        tabs->moveCursor(QTextCursor::End);
        tabs->insertPlainText("    time = 15\n");
        m_context->actions().action("file_save")->trigger();
        window.waitForTasks();
        QCOMPARE(readFile(dir.filePath("t.cmd")),
                 QByteArray("[Command]\n\tname = \"x\"\n\tcommand = x\n\ttime = 15\n"));
        applyOptions([](QDialog* dialog) {
            for (const char* key :
                 {"code_line_numbers", "code_folding_markers", "code_highlight_line", "code_paren_matching"})
                dialog->findChild<QCheckBox*>(key)->setChecked(true);
            for (const char* key : {"code_highlight_blocks", "open_tabToSpace", "save_spaceToTab"})
                dialog->findChild<QCheckBox*>(key)->setChecked(false);
        });
        options = editor->options();
        QVERIFY(options.lineNumbers && options.foldingMarkers && options.highlightLine && !options.highlightBlocks &&
                options.parenMatching);
        QCOMPARE(editor->gutterWidth(), wide);
    }

    // The window comes back where it was left, but always on the screen.
    void windowGeometry()
    {
        Config& config = m_context->config();
        config.setValue("window_left", -20000);
        config.setValue("window_top", -20000);
        config.setValue("window_width", 50000);
        config.setValue("window_height", 40000);
        config.setValue("window_state", 8); // normal, active
        const QRect available = QGuiApplication::primaryScreen()->availableGeometry();
        QRect placed;
        {
            MainWindow window(*m_context);
            window.show();
            QVERIFY(!window.isMaximized());
            QVERIFY(available.contains(window.geometry().topLeft()));
            QVERIFY(window.width() <= available.width() && window.height() <= available.height());
            window.setGeometry(available.x() + 30, available.y() + 40, 500, 400);
            placed = window.geometry();
            QCOMPARE(placed.topLeft(), QPoint(available.x() + 30, available.y() + 40));
            QVERIFY(window.close());
        }
        QCOMPARE(config.intValue("window_left", 0), placed.x());
        QCOMPARE(config.intValue("window_top", 0), placed.y());
        QCOMPARE(config.intValue("window_width", 0), placed.width());
        QCOMPARE(config.intValue("window_height", 0), placed.height());
        MainWindow again(*m_context);
        again.show();
        QCOMPARE(again.geometry(), placed);
    }

    void optionsAlwaysAvailable()
    {
        QAction* options = m_context->actions().action("tools_options");
        QCOMPARE(options->shortcut(), QKeySequence(QStringLiteral("Ctrl+K")));
        MainWindow window(*m_context);
        window.show();
        QVERIFY(options->isEnabled());
        QVERIFY(window.openProject(m_dir.filePath("c/c.def")));
        QVERIFY(options->isEnabled());
        m_context->actions().action("file_welcome")->trigger();
        QVERIFY(window.isWelcomeCurrent());
        QVERIFY(options->isEnabled());
    }

    void projectTabs()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("c/c.def")));
        ProjectView* character = window.currentProjectView();
        QVERIFY(window.openProject(m_dir.filePath("s/s.def")));
        ProjectView* stage = window.currentProjectView();
        auto* tabs = window.findChild<QTabBar*>("projectTabs");
        QVERIFY(tabs->isMovable());
        QCOMPARE(tabs->tabText(0), QStringLiteral("Shell"));
        QCOMPARE(tabs->tabText(1), QStringLiteral("Stage"));
        QCOMPARE(tabs->tabToolTip(0), QDir::toNativeSeparators(QDir::cleanPath(m_dir.filePath("c/c.def"))));
        QCOMPARE(tabs->tabToolTip(1), QDir::toNativeSeparators(QDir::cleanPath(m_dir.filePath("s/s.def"))));
        tabs->moveTab(1, 0);
        QCOMPARE(window.projectViews(), (QList<ProjectView*>{stage, character}));
        tabs->setCurrentIndex(1);
        QCOMPARE(window.currentProjectView(), character);
        QCOMPARE(window.windowTitle(), QStringLiteral("Shell - Open Factory Studio"));
        tabs->setCurrentIndex(0);
        QCOMPARE(window.currentProjectView(), stage);
    }

    void taskbarThumbnails()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("c/c.def")));
        ProjectView* character = window.currentProjectView();
        QVERIFY(window.openProject(m_dir.filePath("s/s.def")));
        ProjectView* stage = window.currentProjectView();
        auto* thumbnails = window.findChild<TaskbarTabs*>();
        QVERIFY(thumbnails);
        Q_EMIT thumbnails->activated(character);
        QCOMPARE(window.currentProjectView(), character);
        Q_EMIT thumbnails->closeRequested(stage);
        QCOMPARE(window.projectViews(), QList<ProjectView*>{character});
        const QImage portrait = character->engine()->portrait(*character->project());
        QCOMPARE(portrait.size(), QSize(8, 4));
    }

    // A click on the current tab collapses or expands the panels. Another tab always expands them.
    void bottomPanels()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("c/c.def")));
        BottomDock* dock = window.bottomDock();
        auto* tabs = dock->findChild<QTabBar*>("bottomTabs");
        QStringList captions;
        for (int i = 0; i < tabs->count(); ++i)
            captions << tabs->tabText(i);
        QCOMPARE(captions, (QStringList{"Issues", "Advanced Search", "Engine Output", "Debugger", "Watch"}));
        dock->setExpanded(false);
        tabs->setCurrentIndex(0);
        auto clickTab = [tabs](int index) {
            click(tabs, tabs->tabRect(index).center());
        };
        clickTab(0);
        QVERIFY(dock->isExpanded());
        clickTab(0);
        QVERIFY(!dock->isExpanded());
        clickTab(2);
        QCOMPARE(dock->currentPage(), int(BottomDock::EngineOutput));
        QVERIFY(dock->isExpanded());
        clickTab(3);
        QCOMPARE(dock->currentPage(), int(BottomDock::Debugger));
        QVERIFY(dock->isExpanded());

        const QList<QPair<const char*, int>> items{{"view_issues", BottomDock::Issues},
                                                   {"view_advanced_search", BottomDock::AdvancedSearch},
                                                   {"view_engine_output", BottomDock::EngineOutput},
                                                   {"view_debugger", BottomDock::Debugger},
                                                   {"view_watch", BottomDock::Watch}};
        for (const auto& [id, page] : items) {
            dock->setExpanded(false);
            m_context->actions().action(QString::fromLatin1(id))->trigger();
            QCOMPARE(dock->currentPage(), page);
            QVERIFY(dock->isExpanded());
            QVERIFY(dock->isVisible());
        }
    }

    void statusBarSections()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("c/c.def")));
        window.switchEditor("commands");
        StatusBar* status = window.ofsStatusBar();
        QVERIFY(status->section(2).startsWith("Encoding: "));
        QMenu* encodings = menu(window, "menu_encoding");
        QLabel* section = status->sectionLabel(2);
        click(section, section->rect().center(), false);
        QTRY_VERIFY(encodings->isVisible());
        encodings->close();

        TaskList& tasks = m_context->tasks();
        const int first = tasks.begin(QStringLiteral("Saving a.sff"));
        const int second = tasks.begin(QStringLiteral("Saving b.snd"));
        QProgressBar* progress = status->progressBar();
        QCOMPARE(progress->maximum(), 0); // busy
        QStringList listed;
        whenPopup([&listed](QMenu* menu) {
            for (QAction* a : menu->actions())
                listed << a->text() + (a->isEnabled() ? "" : "(off)");
        });
        click(progress, progress->rect().center(), false);
        QCOMPARE(listed, (QStringList{"Saving a.sff(off)", "Saving b.snd(off)"}));
        tasks.end(first);
        tasks.end(second);
        QCOMPARE(progress->maximum(), 100);
        QCOMPARE(progress->value(), -1);
    }

    void actionList()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("c/c.def")));
        window.switchEditor("sprites");
        QAction* toggle = m_context->actions().action("view_actions");
        toggle->setChecked(true);
        auto* dock = window.findChild<QDockWidget*>("actionListDock");
        QVERIFY(dock->isVisible());
        auto* list = window.findChild<QListWidget*>("actionList");
        QStringList ids;
        for (int i = 0; i < list->count(); ++i)
            ids << list->item(i)->data(Qt::UserRole).toString();
        QStringList expected;
        for (QAction* a : m_context->actions().actionsInScope(QStringLiteral("editor:sprites")))
            expected << a->objectName();
        QCOMPARE(ids, expected);
        SpritePage* page = window.currentProjectView()->spritesEditor()->page();
        page->setCurrent(0);
        QListWidgetItem* next = list->item(int(ids.indexOf("spr_next")));
        QVERIFY(next);
        Q_EMIT list->itemClicked(next);
        QCOMPARE(page->current(), 1);
        window.switchEditor("states");
        QVERIFY(list->count() == 0 || list->item(0)->data(Qt::UserRole).toString().startsWith("st_"));
        toggle->setChecked(false);
    }

private:
    static constexpr const char* kKeys[] = {
        "use_native_open",
        "sort_file_names",
        "auto_update",
        "dpi_auto",
        "dpi_scale",
        "pal_fit_space",
        "crosshair_type",
        "crosshair_main",
        "background_type",
        "background_file",
        "background_x",
        "background_y",
        "zoom_mode",
        "zoom_center",
        "box_opacity",
        "sprite_over_guides",
        "contact_over_cross",
        "onion_effect",
        "onion_opacity",
        "code_line_numbers",
        "code_folding_markers",
        "code_highlight_line",
        "code_highlight_blocks",
        "code_paren_matching",
        "open_tabToSpace",
        "save_spaceToTab",
        "window_left",
        "window_top",
        "window_width",
        "window_height",
        "window_state",
        "show_action_list",
    };

    Script::Environment environment()
    {
        Script::Environment environment;
        environment.engines = &m_context->engines();
        return environment;
    }

    void applyOptions(const std::function<void(QDialog*)>& change)
    {
        bool applied = false;
        whenModal<QDialog>([&](QDialog* dialog) {
            QCOMPARE(dialog->objectName(), QStringLiteral("Options"));
            change(dialog);
            dialog->findChild<QPushButton*>("applyBut")->click();
            dialog->findChild<QPushButton*>("closeBut")->click();
            applied = true;
        });
        m_context->actions().action("tools_options")->trigger();
        QVERIFY(applied);
    }

    AppContext* m_context = nullptr;
    QTemporaryDir m_dir;
    QTemporaryDir m_data;
    QHash<QString, QString> m_saved;
};

QTEST_MAIN(ShellOptionsParityTest)
#include "ShellOptionsParityTest.moc"
