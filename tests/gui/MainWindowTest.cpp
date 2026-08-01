#include "shell/MainWindow.h"
#include "core/actions/ActionRegistry.h"
#include "core/app/AppContext.h"
#include "core/code/CodeEditor.h"
#include "core/engine/Engine.h"
#include "core/project/Project.h"
#include "core/settings/Config.h"
#include "core/settings/History.h"
#include "core/widgets/EPanelWidget.h"
#include "shell/AdvancedSearchPanel.h"
#include "shell/CodePage.h"
#include "shell/ControllerTemplatesWidget.h"
#include "shell/FileDialogs.h"
#include "shell/ProjectView.h"
#include "shell/StatusBar.h"
#include "shell/dialogs/SnippetEditorDialog.h"
#include "shell/dialogs/StateControllerDialog.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>

using namespace OFS;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QStringList visibleMenuTitles(QMainWindow& window)
{
    QStringList titles;
    for (QAction* a : window.menuBar()->actions()) {
        if (a->isVisible() && a->menu())
            titles << a->menu()->title();
    }
    return titles;
}

QStringList toolBarIds(QToolBar* bar)
{
    QStringList ids;
    for (QAction* a : bar->actions())
        ids << (a->isSeparator() ? QStringLiteral("|") : a->objectName());
    return ids;
}

QMenu* menu(QMainWindow& window, const QString& objectName)
{
    return window.findChild<QMenu*>(objectName);
}

} // namespace

class MainWindowTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_context = new AppContext;
        m_context->loadSettings();
        m_context->initializeEngines();
        QVERIFY(m_dir.isValid());

        writeFile(m_dir.filePath("fighter.def"), "; test character\n"
                                                 "[Info]\nname = \"Test Fighter\"\nmugenversion = 1.0\n"
                                                 "[Files]\ncmd = fighter.cmd\ncns = fighter.cns\nst = fighter.cns\n"
                                                 "sprite = fighter.sff\nanim = fighter.air\nsound = fighter.snd\n");
        writeFile(m_dir.filePath("fighter.cmd"), "[Command]\nname = \"x\"\ncommand = x\n");
        writeFile(m_dir.filePath("fighter.cns"), "[Data]\nlife = 1000\n");
        writeFile(m_dir.filePath("fighter.air"), "[Begin Action 0]\n0,0, 0,0, 10\n");
        writeFile(m_dir.filePath("stage.def"), "[Info]\nname = \"Test Stage\"\n[Camera]\nstartx = 0\n"
                                               "[BGdef]\nspr = stage.sff\n[BG 1]\ntype = normal\n");
    }

    void cleanupTestCase() { delete m_context; }

    void noProject()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCOMPARE(window.windowTitle(), QStringLiteral("Open Factory Studio"));
        QCOMPARE(visibleMenuTitles(window), (QStringList{"Project", "Edit", "View", "Debug", "Tools", "Help"}));
        QCOMPARE(toolBarIds(window.mainToolBar()),
                 (QStringList{"file_new", "file_close", "file_open", "file_save", "|", "file_load_backup",
                              "file_backup", "|", "file_run", "|", "tools_options"}));
        QVERIFY(!window.extraToolBar()->isVisible());

        ActionRegistry& actions = m_context->actions();
        QCOMPARE(actions.action("file_new")->text(), QStringLiteral("New\t (Ctrl+N)"));
        QCOMPARE(actions.action("file_open")->text(), QStringLiteral("Open...\t (Ctrl+O)"));
        QVERIFY(!actions.action("file_save")->isEnabled());
        QVERIFY(actions.action("edit_find_replace_multi")->isEnabled());
        QVERIFY(!actions.action("edit_undo")->isEnabled());
        QVERIFY(actions.action("breakpoint_add")->isEnabled());
        QVERIFY(!actions.action("debug_step_over")->isEnabled());

        QStringList project;
        for (QAction* a : menu(window, "menu_project")->actions())
            project << (a->isSeparator() ? QStringLiteral("|") : a->text().section(QLatin1Char('\t'), 0, 0));
        QCOMPARE(project, (QStringList{"New",
                                       "Open...",
                                       "Open from archive...",
                                       "Open single file...",
                                       "Open recent",
                                       "Save",
                                       "Save as...",
                                       "|",
                                       "Close",
                                       "Close all",
                                       "Close others",
                                       "|",
                                       "Create backup",
                                       "Load backup",
                                       "|",
                                       "Run in engine...",
                                       "|",
                                       "Welcome screen",
                                       "|",
                                       "Exit"}));
    }

    void characterProject()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("fighter.def")));
        QCOMPARE(window.windowTitle(), QStringLiteral("Test Fighter - Open Factory Studio"));
        QCOMPARE(visibleMenuTitles(window),
                 (QStringList{"Project", "Edit", "View", "Debug", "Palettes", "Backgrounds", "Sprites", "Animations",
                              "Commands", "States", "Sounds", "Tools", "Help"}));

        ActionRegistry& actions = m_context->actions();
        QVERIFY(actions.action("editor_def")->isChecked());
        QVERIFY(actions.action("editor_cmd")->isVisible());
        QVERIFY(!actions.action("editor_bg")->isVisible());
        QVERIFY(actions.action("file_save")->isEnabled());
        QVERIFY(!actions.action("bgs_add")->isEnabled());
        QVERIFY(actions.action("st_throw")->isEnabled());

        QCOMPARE(toolBarIds(window.extraToolBar()).first(), QStringLiteral("editor_new"));
        QVERIFY(toolBarIds(window.extraToolBar()).contains(QStringLiteral("code_comment")));
        QCOMPARE(window.ofsStatusBar()->section(3), QStringLiteral("Project: Character, M.U.G.E.N 1.0"));
        QVERIFY(window.ofsStatusBar()->section(0).startsWith(QStringLiteral("Line: 1, Column: 1, Line count: ")));

        ProjectView* view = window.currentProjectView();
        QVERIFY(view);
        QCOMPARE(view->editorIds(),
                 (QStringList{"definitions", "sprites", "animations", "commands", "states", "sounds", "emulator"}));
        auto* left = view->findChild<EPanelWidget*>("leftPanel");
        auto* right = view->findChild<EPanelWidget*>("rightPanel");
        QCOMPARE(left->title(), QStringLiteral("Definitions"));
        QVERIFY(!right->isVisible());

        actions.action("editor_spr")->trigger();
        QCOMPARE(view->currentEditor(), QStringLiteral("sprites"));
        QCOMPARE(left->title(), QStringLiteral("Sprites"));
        QVERIFY(right->isVisible());
        QCOMPARE(right->title(), QStringLiteral("Palettes"));
        QVERIFY(!actions.action("code_comment")->isEnabled());
        QVERIFY(toolBarIds(window.extraToolBar()).contains(QStringLiteral("view_zoomin")));
        QCOMPARE(window.ofsStatusBar()->section(0), QStringLiteral("Position: 0, 0"));

        QVERIFY(view->findChild<QScrollBar*>("spr_sel"));
        QVERIFY(view->findChild<QToolButton*>("spr_add"));
        QCOMPARE(view->findChild<QLabel*>("spr_grp_lb")->text(), QStringLiteral("Group:"));

        QCOMPARE(actions.action("spr_add")->shortcut(), QKeySequence("Ctrl+A"));
        QVERIFY(actions.action("anim_add")->shortcut().isEmpty());
        actions.action("editor_anim")->trigger();
        QVERIFY(actions.action("spr_add")->shortcut().isEmpty());
        QCOMPARE(actions.action("anim_add")->shortcut(), QKeySequence("Ctrl+A"));
        QCOMPARE(right->title(), QStringLiteral("Sprite explorer"));

        actions.action("file_close")->trigger();
        QCOMPARE(window.windowTitle(), QStringLiteral("Open Factory Studio"));
        QVERIFY(!window.currentProjectView());
    }

    void stageProject()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("stage.def")));
        ActionRegistry& actions = m_context->actions();
        QVERIFY(actions.action("editor_bg")->isVisible());
        QVERIFY(!actions.action("editor_cmd")->isVisible());
        QVERIFY(!actions.action("editor_snd")->isVisible());
        QVERIFY(actions.action("bgs_add")->isEnabled());
        QVERIFY(!actions.action("st_throw")->isEnabled());
        QCOMPARE(window.ofsStatusBar()->section(3), QStringLiteral("Project: Stage, DOS M.U.G.E.N beta"));
    }

    void encodingMenu()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("fighter.def")));
        m_context->actions().action("editor_def")->trigger();
        QMenu* encodings = menu(window, "menu_encoding");
        QVERIFY(encodings);
        QVERIFY(encodings->menuAction()->isEnabled());
        auto find = [encodings](const QString& text) -> QAction* {
            Q_EMIT encodings->aboutToShow();
            for (QAction* a : encodings->actions()) {
                if (a->text() == text)
                    return a;
            }
            return nullptr;
        };
        QAction* utf8 = find("Unicode\tUTF-8");
        QVERIFY(utf8);
        QVERIFY(utf8->isChecked());
        const auto listed = encodings->actions();
        QCOMPARE(std::count_if(listed.begin(), listed.end(), [](QAction* a) { return a->isVisible(); }), 60);

        Config& config = m_context->config();
        config.setValue("showmsg_enc_reload", "1"); // Always
        find("Latin/Western European\twindows-1252, CP1252")->trigger();
        QCOMPARE(window.ofsStatusBar()->section(2), QStringLiteral("Encoding: windows-1252"));
        QVERIFY(find("Latin/Western European\twindows-1252, CP1252")->isChecked());
        QVERIFY(!window.currentProjectView()->project()->isModified());

        config.setValue("showmsg_enc_reload", "2"); // Never
        find("Unicode\tUTF-8")->trigger();
        QCOMPARE(window.ofsStatusBar()->section(2), QStringLiteral("Encoding: UTF-8"));
        QVERIFY(window.currentProjectView()->project()->isModified());

        config.setValue("showmsg_enc_reload", "1");
        find("Unicode\tUTF-8")->trigger();
        QVERIFY(window.currentProjectView()->project()->isModified());
        find("System default\tSystem")->trigger();
        QVERIFY(!window.currentProjectView()->project()->isModified());
        config.setValue("showmsg_enc_reload", "");

        m_context->actions().action("editor_spr")->trigger();
        QVERIFY(!encodings->menuAction()->isEnabled());
    }

    void searchTools()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("fighter.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_def")->trigger();
        CodeEditor* editor = window.currentProjectView()->currentCodeEditor();
        QVERIFY(editor);
        Config& config = m_context->config();

        auto* bar = window.findChild<QWidget*>("quickFind");
        QVERIFY(bar);
        QVERIFY(!bar->isVisible());
        actions.action("edit_find_replace")->trigger();
        QVERIFY(bar->isVisible());
        QVERIFY(config.boolValue("show_quicksearch"));
        auto* find = bar->findChild<QLineEdit*>("quickSearchFind");
        auto* replace = bar->findChild<QLineEdit*>("quickSearchReplace");
        find->setText("fighter");
        bar->findChild<QPushButton*>("quickSearchFindBut")->click();
        QCOMPARE(editor->textCursor().selectedText(), QStringLiteral("Fighter"));
        QCOMPARE(editor->cursorLine(), 3);
        bar->findChild<QPushButton*>("quickSearchFindBut")->click();
        QCOMPARE(editor->cursorLine(), 6); // cmd = fighter.cmd
        bar->findChild<QToolButton*>("quickSearchCase")->setChecked(true);
        QVERIFY(config.boolValue("quicksearch_case"));
        find->setText("Fighter");
        bar->findChild<QPushButton*>("quickSearchFindBut")->click(); // wraps around to line 3
        QCOMPARE(editor->cursorLine(), 3);
        replace->setText("Brawler");
        bar->findChild<QPushButton*>("quickSearchReplaceBut")->click();
        QVERIFY(editor->toPlainText().contains("name = \"Test Brawler\""));
        bar->findChild<QToolButton*>("quickSearchCase")->setChecked(false);
        find->setText("fighter\\.(\\w+)");
        replace->setText("%1.fighter");
        bar->findChild<QToolButton*>("quickSearchRegExp")->setChecked(true);
        bar->findChild<QPushButton*>("quickSearchReplaceAllBut")->click();
        QVERIFY(editor->toPlainText().contains("cmd = cmd.fighter"));
        QVERIFY(editor->toPlainText().contains("anim = air.fighter"));
        bar->findChild<QToolButton*>("quickSearchRegExp")->setChecked(false);
        find->setText("no such text");
        QString message;
        QTimer::singleShot(0, this, [&message] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            QVERIFY(box);
            message = box->windowTitle() + QLatin1Char('|') + box->text();
            box->accept();
        });
        bar->findChild<QPushButton*>("quickSearchFindBut")->click();
        QCOMPARE(message, QStringLiteral("Find|Nothing found!"));
        QVERIFY(find->property("notFound").toBool());
        bar->findChild<QToolButton*>("quickSearchClose")->click();
        QVERIFY(!bar->isVisible());
        QVERIFY(!config.boolValue("show_quicksearch"));

        actions.action("edit_find_replace_multi")->trigger();
        auto* dialog = window.findChild<QDialog*>("FindDlgMlt");
        QVERIFY(dialog);
        QVERIFY(dialog->isVisible());
        QCOMPARE(dialog->windowTitle(), QStringLiteral("Find/Replace (multi-line)"));
        QVERIFY(config.boolValue("show_find_multi"));
        dialog->findChild<QPlainTextEdit*>("findTxt")->setPlainText("[Info]\nname");
        editor->moveCursor(QTextCursor::End);
        dialog->findChild<QPushButton*>("next_but")->click();
        QCOMPARE(editor->textCursor().selectedText(), QString("[Info]") + QChar::ParagraphSeparator + "name");
        dialog->findChild<QPlainTextEdit*>("replaceTxt")->setPlainText("[Info]\n; renamed\nname");
        dialog->findChild<QPushButton*>("all_but")->click();
        QVERIFY(editor->toPlainText().contains("[Info]\n; renamed\nname = "));
        dialog->close();
        QVERIFY(!config.boolValue("show_find_multi"));

        QTimer::singleShot(0, this, [] {
            auto* input = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
            QVERIFY(input);
            QCOMPARE(input->objectName(), QStringLiteral("GoToDlg"));
            QCOMPARE(input->windowTitle(), QStringLiteral("Number"));
            QCOMPARE(input->intMaximum(), 99999999);
            input->setIntValue(5);
            input->accept();
        });
        actions.action("code_goto")->trigger();
        QCOMPARE(editor->cursorLine(), 5);

        editor->document()->setModified(false);
    }

    void welcomeTabAndHistory()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("fighter.def")));
        QVERIFY(window.openProject(m_dir.filePath("stage.def")));
        QCOMPARE(window.projectViews().size(), 2);
        QVERIFY(window.openProject(m_dir.filePath("fighter.def")));
        QCOMPARE(window.projectViews().size(), 2);
        QCOMPARE(window.currentProjectView()->project()->name(), QStringLiteral("Test Fighter"));

        window.showWelcomeTab();
        QVERIFY(window.isWelcomeCurrent());
        QCOMPARE(window.windowTitle(), QStringLiteral("Open Factory Studio"));
        QVERIFY(!window.extraToolBar()->isVisible());

        window.closeAllProjects();
        QVERIFY(window.projectViews().isEmpty());
        QCOMPARE(m_context->history().entries().size(), 2);
        QCOMPARE(m_context->history().entries().first().name, QStringLiteral("Test Stage"));
    }

    void validationIssues()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("bad.def"), "[Info]\nname = \"Bad\"\nmugenversion = 1.0\n"
                                           "[Files]\ncmd = bad.cmd\ncns = bad.cns\nsprite = bad.sff\n"
                                           "anim = bad.air\n");
        writeFile(dir.filePath("bad.cmd"), "[Command]\nname = \"x\"\ncommand = x\n");
        writeFile(dir.filePath("bad.cns"), "[Data]\nlife = 1000\n"
                                           "[Statedef 200]\ntype = Q\n"
                                           "[State 200, 1]\ntype = ChangeState\ntrigger1 = 1\nvalue = 0\nbogus = 1\n");
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("bad.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_st")->trigger();
        ProjectView* view = window.currentProjectView();
        QCOMPARE(view->currentEditor(), QStringLiteral("states"));

        auto* issues = window.findChild<QTableView*>("issuesView");
        QVERIFY(issues);
        QTRY_COMPARE(issues->model()->rowCount(), 2);
        QStringList messages;
        for (int row = 0; row < 2; ++row)
            messages << issues->model()->index(row, 0).data().toString();
        messages.sort();
        QCOMPARE(messages, (QStringList{"Unknown keyword 'Q'", "Unknown parameter 'bogus' for 'ChangeState' block"}));
        QCOMPARE(issues->model()->index(0, 1).data().toString(), QStringLiteral("bad.cns"));
        QCOMPARE(window.ofsStatusBar()->section(1), QStringLiteral("Syntax error count: 2"));

        CodeEditor* editor = view->currentCodeEditor();
        editor->goToLine(1);
        actions.action("code_next_error")->trigger();
        QCOMPARE(editor->cursorLine(), 4);
        actions.action("code_next_error")->trigger();
        QCOMPARE(editor->cursorLine(), 9);
        actions.action("code_prev_error")->trigger();
        QCOMPARE(editor->cursorLine(), 4);
        actions.action("editor_cmd")->trigger();
        int row = 0;
        while (row < 2 && !issues->model()->index(row, 0).data().toString().startsWith("Unknown parameter"))
            ++row;
        Q_EMIT issues->doubleClicked(issues->model()->index(row, 2));
        QCOMPARE(view->currentEditor(), QStringLiteral("states"));
        QCOMPARE(view->currentCodeEditor()->cursorLine(), 9);

        // The issues update after the edit pause.
        QTextCursor cursor(editor->document()->findBlockByNumber(8));
        cursor.select(QTextCursor::LineUnderCursor);
        cursor.insertText("ctrl = 1");
        QTRY_COMPARE(issues->model()->rowCount(), 1);
        QTRY_COMPARE(window.ofsStatusBar()->section(1), QStringLiteral("Syntax error count: 1"));
        editor->document()->setModified(false);
    }

    void documentationPopup()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("doc.def"), "[Info]\nname = \"Doc\"\nmugenversion = 1.1\n"
                                           "[Files]\ncns = doc.cns\nsprite = doc.sff\nanim = doc.air\n");
        writeFile(dir.filePath("doc.cns"),
                  "[Statedef 0]\ntype = S\n"
                  "[State 0, a]\ntype = ChangeState\ntrigger1 = AnimTime = 0\nvalue = 0 ; Time\n");
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("doc.def")));
        m_context->actions().action("editor_st")->trigger();
        ProjectView* view = window.currentProjectView();
        CodeEditor* editor = view->currentCodeEditor();
        QVERIFY(editor);
        const QString text = editor->toPlainText();
        auto target = [&](const char* word, int skip = 0) {
            return editor->documentationTarget(int(text.indexOf(QLatin1String(word))) + skip);
        };
        QCOMPARE(target("ChangeState", 3), (std::pair<QString, QString>{"sctrl", "ChangeState"}));
        QCOMPARE(target("AnimTime"), (std::pair<QString, QString>{"trigger", "AnimTime"}));
        QCOMPARE(target("type = S").first, QString()); // a parameter name
        QCOMPARE(target("Time\n").first, QString());   // inside a comment
        QCOMPARE(target("S\n").first, QString());      // a StateDef type value

        Engine* engine = view->engine();
        QVERIFY(engine->documentationPage(*view->project(), "sctrl", "Explod").endsWith("/1.1/sctrl.explod.html"));
        QVERIFY(engine->documentationPage(*view->project(), "sctrl", "changestate")
                    .endsWith("/common/sctrl.changestate.html"));
        QVERIFY(engine->documentationPage(*view->project(), "sctrl", "NoSuchController").isEmpty());

        QTextCursor cursor(editor->document());
        cursor.setPosition(int(text.indexOf("ChangeState")) + 2);
        const QPoint point = editor->cursorRect(cursor).center();
        QTest::mouseClick(editor->viewport(), Qt::LeftButton, Qt::ControlModifier, point);
        QFrame* popup = nullptr;
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (w->objectName() == QLatin1String("docPopup") && w->isVisible())
                popup = qobject_cast<QFrame*>(w);
        }
        QVERIFY(popup);
        QCOMPARE(popup->size(), QSize(640, 480));
        QVERIFY(popup->windowFlags() & Qt::WindowStaysOnTopHint);
        QVERIFY(popup->findChild<QTextBrowser*>("docBrowser")->toPlainText().contains("ChangeState"));
        QTest::keyClick(popup, Qt::Key_Escape);
        QTRY_VERIFY(!popup->isVisible());

        // Two rounds, because the Code submenu must survive being shown.
        for (int round = 0; round < 2; ++round) {
            QStringList captions;
            QTimer::singleShot(0, this, [&captions] {
                auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                QVERIFY(menu);
                for (QAction* a : menu->actions())
                    captions << (a->isSeparator() ? QStringLiteral("|") : a->text().section('\t', 0, 0));
                menu->close();
            });
            QContextMenuEvent event(QContextMenuEvent::Mouse, QPoint(5, 5),
                                    editor->viewport()->mapToGlobal(QPoint(5, 5)));
            QApplication::sendEvent(editor->viewport(), &event);
            QCOMPARE(captions.last(), QStringLiteral("Code"));
            QCOMPARE(captions.at(captions.size() - 2), QStringLiteral("|"));
        }
        QVERIFY(window.findChild<QMenu*>("menu_code"));
        QVERIFY(window.findChild<QMenu*>("menu_code")->menuAction()->isVisible());
    }

    void codeBlockTree()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("tree.def"), "[Info]\nname = \"Tree\"\nmugenversion = 1.0\n"
                                            "[Files]\ncns = tree.cns\nsprite = t.sff\nanim = t.air\n");
        writeFile(dir.filePath("tree.cns"), "[Data]\nlife = 1000\n; Stand\n[Statedef 0]\ntype = S\n"
                                            "[State 0, 1]\ntype = ChangeAnim\ntrigger1 = 1\nvalue = 0\n");
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("tree.def")));
        m_context->actions().action("editor_st")->trigger();
        ProjectView* view = window.currentProjectView();
        QTreeView* tree = view->outlineTree("states");
        QVERIFY(tree);
        QCOMPARE(tree->objectName(), QStringLiteral("st_tree"));
        QTRY_COMPARE(tree->model() ? tree->model()->rowCount() : 0, 2);
        QCOMPARE(tree->model()->index(1, 0).data().toString(), QStringLiteral("0 - Stand"));

        const QModelIndex stateDef = tree->model()->index(1, 0);
        tree->setExpanded(stateDef, true);
        Q_EMIT tree->clicked(stateDef);
        CodeEditor* editor = view->currentCodeEditor();
        QCOMPARE(editor->cursorLine(), 4);
        QTextCursor cursor(editor->document()->findBlockByNumber(2));
        cursor.select(QTextCursor::LineUnderCursor);
        cursor.insertText("; Standing");
        QTRY_COMPARE(tree->model()->index(1, 0).data().toString(), QStringLiteral("0 - Standing"));
        QVERIFY(tree->isExpanded(tree->model()->index(1, 0)) == false); // new text makes it a new node

        m_context->config().setValue("auto_expand_tree", true);
        tree->expand(tree->model()->index(0, 0));
        editor->goToLine(9); // value = 0, in [State 0, 1]
        const QModelIndex current = tree->currentIndex();
        QCOMPARE(current.data().toString(), QStringLiteral("value"));
        QCOMPARE(current.parent().data().toString(), QStringLiteral("0 - 1"));
        QVERIFY(tree->isExpanded(current.parent().parent()));
        QVERIFY(!tree->isExpanded(tree->model()->index(0, 0))); // [Data]
        m_context->config().setValue("auto_expand_tree", false);
        editor->document()->setModified(false);
    }

    void snippetEditor()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("fighter.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_cmd")->trigger();
        ProjectView* view = window.currentProjectView();
        CodePage* page = view->currentCodePage();
        const QString userDir = view->engine()->userSnippetDir();

        QTimer::singleShot(0, this, [userDir] {
            auto* dialog = qobject_cast<SnippetEditorDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->objectName(), QStringLiteral("SnipDlg"));
            dialog->findChild<QPlainTextEdit*>("snipEdit")->setPlainText("[Command]\nname = \"CURSOR\"");
            QVERIFY(dialog->saveTo(QDir(userDir).filePath("Zz test.xml")));
            dialog->reject();
        });
        actions.action("code_snippet_new")->trigger();
        QCOMPARE(page->currentSnippet(), QStringLiteral("Zz test"));

        CodeEditor* editor = page->editor();
        editor->moveCursor(QTextCursor::End);
        actions.action("code_snippet_insert")->trigger(); // no fields, so no dialog
        QVERIFY(editor->toPlainText().endsWith("[Command]\nname = \"\"\n"));
        QCOMPARE(editor->textCursor().block().text(), QStringLiteral("name = \"\""));

        QFile::remove(QDir(userDir).filePath("Zz test.xml"));
        editor->document()->setModified(false);
    }

    void advancedSearch()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("adv.def"), "[Info]\nname = \"Adv\"\nmugenversion = 1.0\n"
                                           "[Files]\ncmd = adv.cmd\ncns = adv.cns\nsprite = a.sff\nanim = a.air\n");
        writeFile(dir.filePath("adv.cmd"), "[Command]\nname = \"x\"\ncommand = x\n; ChangeState here\n");
        writeFile(dir.filePath("adv.cns"), "[Statedef 0]\ntype = S\n[State 0, 1]\ntype = ChangeState\n"
                                           "trigger1 = 1\nvalue = 0\n[State 0, 2]\ntype = HitDef\ntrigger1 = 1\n"
                                           "attr = S, NA\nanimtype = Med\n[State 0, 3]\ntype = ChangeState\n"
                                           "trigger1 = 1\nvalue = 1\n");
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("adv.def")));
        auto* panel = window.findChild<AdvancedSearchPanel*>("advancedSearch");
        QVERIFY(panel);
        auto* scope = panel->findChild<QComboBox*>("advsearch_scope");
        auto* type = panel->findChild<QComboBox*>("advsearch_type");
        auto* text = panel->findChild<QLineEdit*>("advsearch_text");
        QVERIFY(!panel->findChild<QPushButton*>("advsearch_search")->isEnabled());

        scope->setCurrentIndex(AdvancedSearchPanel::CurrentProject);
        text->setText("changestate");
        panel->search(false);
        QVERIFY(panel->isShowingResults());
        QCOMPARE(panel->hits().size(), 3);
        const QAbstractItemModel* model = panel->resultsView()->model();
        QCOMPARE(model->rowCount(), 2); // grouped by file
        QCOMPARE(panel->findChild<QLabel*>("advsearch_count")->text(), QStringLiteral("3 matches found."));

        ProjectView* view = window.currentProjectView();
        QModelIndex cnsRow;
        for (int row = 0; row < model->rowCount(); ++row) {
            if (model->index(row, 0).data().toString().endsWith("adv.cns"))
                cnsRow = model->index(row, 0);
        }
        QVERIFY(cnsRow.isValid());
        Q_EMIT panel->resultsView()->doubleClicked(model->index(1, 0, cnsRow));
        QCOMPARE(view->currentEditor(), QStringLiteral("states"));
        QCOMPARE(view->currentCodeEditor()->cursorLine(), 13);
        QCOMPARE(view->currentCodeEditor()->textCursor().positionInBlock(), 7);

        panel->search(true);
        auto* replaceModel = qobject_cast<QStandardItemModel*>(panel->resultsView()->model());
        QStandardItem* cnsFile = nullptr;
        for (int row = 0; row < replaceModel->rowCount(); ++row) {
            if (replaceModel->item(row)->text().endsWith("adv.cns"))
                cnsFile = replaceModel->item(row);
        }
        QVERIFY(cnsFile);
        replaceModel->setData(cnsFile->child(1)->index(), Qt::Unchecked, Qt::CheckStateRole); // as a click does
        QCOMPARE(cnsFile->checkState(), Qt::PartiallyChecked);
        panel->findChild<QLineEdit*>("advsearch_replacement")->setText("SelfState");
        QCOMPARE(panel->replaceChecked(), 2);
        QVERIFY(!panel->isShowingResults());
        CodeEditor* cns = view->currentCodeEditor();
        QVERIFY(cns->toPlainText().contains("type = SelfState\ntrigger1 = 1\nvalue = 0"));
        QVERIFY(cns->toPlainText().contains("type = ChangeState\ntrigger1 = 1\nvalue = 1"));
        cns->undo();
        QCOMPARE(cns->toPlainText().count("ChangeState"), 2);

        type->setCurrentIndex(1);
        QVERIFY(!text->isVisible());
        scope->setCurrentIndex(AdvancedSearchPanel::CurrentFile);
        panel->search(true);
        QCOMPARE(panel->hits().size(), 1);
        QCOMPARE(panel->hits().first().matched, QStringLiteral("Med"));
        QCOMPARE(panel->hits().first().replacement, QStringLiteral("Medium"));
        panel->replaceChecked();
        QVERIFY(cns->toPlainText().contains("animtype = Medium"));

        for (ProjectView* v : window.projectViews()) {
            for (CodeEditor* e : v->codeEditors())
                e->document()->setModified(false);
        }
    }

    void projectExplorer()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("fighter.def")));
        ProjectView* view = window.currentProjectView();
        auto* explorer = view->findChild<QTreeView*>("projectExplorer");
        QVERIFY(explorer);
        QStringList names;
        for (int row = 0; row < explorer->model()->rowCount(); ++row)
            names << explorer->model()->index(row, 0).data().toString();
        QCOMPARE(names, (QStringList{"fighter.air", "fighter.cmd", "fighter.cns", "fighter.def"}));
        m_context->actions().action("editor_def")->trigger();
        Q_EMIT explorer->doubleClicked(explorer->model()->index(1, 0));
        QCOMPARE(view->currentEditor(), QStringLiteral("commands"));
        Q_EMIT explorer->doubleClicked(explorer->model()->index(0, 0));
        QCOMPARE(view->currentEditor(), QStringLiteral("animations"));
    }

    void stateControllerList()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("fighter.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_st")->trigger();
        ProjectView* view = window.currentProjectView();
        CodePage* page = view->currentCodePage();
        QVERIFY(page);
        auto* templates = qobject_cast<ControllerTemplatesWidget*>(page->sidePanel());
        QVERIFY(templates);
        QCOMPARE(templates->names().size(), 91);
        QCOMPARE(templates->names().first(), QStringLiteral("AfterImage"));
        QVERIFY(templates->names().contains("StateDef"));
        actions.action("editor_cmd")->trigger();
        QVERIFY(!view->currentCodePage()->sidePanel());
        actions.action("editor_st")->trigger();

        CodeEditor* editor = page->editor();
        editor->moveCursor(QTextCursor::End);
        templates->selectName("ChangeState");
        Q_EMIT templates->list()->itemDoubleClicked(templates->list()->currentItem());
        QVERIFY(editor->toPlainText().contains("[State 0, ChangeState]\ntype = ChangeState\n"));

        auto answerDialog = [](const QString& text) {
            QTimer::singleShot(0, [text] {
                auto* dialog = qobject_cast<StateControllerDialog*>(QApplication::activeModalWidget());
                QVERIFY(dialog);
                QCOMPARE(dialog->objectName(), QStringLiteral("SCtrlEdit"));
                if (dialog->text().startsWith("[State ]"))
                    QCOMPARE(dialog->text(), QStringLiteral("[State ]\ntype = "));
                dialog->setText(text);
                dialog->accept();
            });
        };
        QString promptedDir;
        FileDialogs::setSavePromptOverride([&promptedDir](const QString& caption, const QString& dir, const QString&) {
            promptedDir = dir;
            return caption == QLatin1String("Save State controller") ? QDir(dir).filePath("Mine") : QString();
        });
        answerDialog("[State 0, Mine]\ntype = MyCustom\ntrigger1 = 1\n");
        actions.action("st_sctrl_new")->trigger();
        FileDialogs::setSavePromptOverride({});
        QVERIFY(promptedDir.startsWith(view->engine()->userDataDir()));
        QVERIFY(templates->names().contains("Mine"));
        QCOMPARE(templates->selectedName(), QStringLiteral("Mine"));
        QFile created(templates->templatePath("Mine"));
        QVERIFY(created.open(QIODevice::ReadOnly));
        QCOMPARE(created.readAll(), QByteArray("[State 0, Mine]\ntype = MyCustom\ntrigger1 = 1\n"));
        created.close();

        answerDialog("[State 0, Mine]\ntype = MyCustom\ntrigger1 = Time = 0\n");
        actions.action("st_sctrl_edit")->trigger();
        QCOMPARE(templates->templateText("Mine"),
                 QStringLiteral("[State 0, Mine]\ntype = MyCustom\ntrigger1 = Time = 0\n"));
        templates->selectName("Width");
        const QString shipped = templates->templatePath("Width");
        answerDialog("[State 0, Width]\ntype = Width\ntrigger1 = 1\n");
        actions.action("st_sctrl_edit")->trigger();
        QVERIFY(templates->templatePath("Width") != shipped);
        QFile copy(templates->templatePath("Width"));
        QVERIFY(copy.open(QIODevice::ReadOnly));
        QCOMPARE(copy.readAll(), QByteArray("[State 0, Width]\r\ntype = Width\r\ntrigger1 = 1\r\n"));
        copy.close();
        QVERIFY(QFile::remove(templates->templatePath("Width")));
        templates->reload();

        templates->selectName("Mine");
        m_context->config().setValue("confirm_delete", false);
        actions.action("st_sctrl_delete")->trigger();
        m_context->config().setValue("confirm_delete", true);
        QVERIFY(!templates->names().contains("Mine"));
        QVERIFY(templates->names().contains("Width"));

        editor->document()->setModified(false);
    }

private:
    AppContext* m_context = nullptr;
    QTemporaryDir m_dir;
};

QTEST_MAIN(MainWindowTest)
#include "MainWindowTest.moc"
