#include "shell/MainWindow.h"
#include "core/actions/ActionRegistry.h"
#include "core/app/AppContext.h"
#include "core/code/CodeEditor.h"
#include "core/debug/Breakpoints.h"
#include "core/engine/Engine.h"
#include "core/engine/EngineWorkspace.h"
#include "core/engine/RunConfiguration.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/project/ZipArchive.h"
#include "core/settings/Config.h"
#include "core/settings/History.h"
#include "core/sounds/SoundFile.h"
#include "core/sprites/SpriteDocument.h"
#include "core/sprites/SpriteFile.h"
#include "core/widgets/EPanelWidget.h"
#include "core/widgets/GraphicsView.h"
#include "core/widgets/ImageCanvas.h"
#include "core/widgets/PaletteWidget.h"
#include "core/widgets/TimelineWidget.h"
#include "core/widgets/WaveformView.h"
#include "formats/codecs/Png.h"
#include "formats/snd/SndDocument.h"
#include "formats/wav/WavFile.h"
#include "shell/AdvancedPaletteEditor.h"
#include "shell/AdvancedSearchPanel.h"
#include "shell/AnimationPage.h"
#include "shell/AnimationsEditor.h"
#include "shell/BackgroundsEditor.h"
#include "shell/BackgroundsPage.h"
#include "shell/CodePage.h"
#include "shell/ControllerTemplatesWidget.h"
#include "shell/DebuggerPanels.h"
#include "shell/FileDialogs.h"
#include "shell/ImageEditor.h"
#include "shell/OrganizerBinding.h"
#include "shell/ProjectView.h"
#include "shell/QuickFindBar.h"
#include "shell/SoundPage.h"
#include "shell/SoundsEditor.h"
#include "shell/SpritePage.h"
#include "shell/SpritesEditor.h"
#include "shell/StatusBar.h"
#include "shell/dialogs/PaletteDialogs.h"
#include "shell/dialogs/ScriptEditorDialog.h"
#include "shell/dialogs/SnippetEditorDialog.h"
#include "shell/dialogs/StateControllerDialog.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QDropEvent>
#include <QFile>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QUndoStack>
#include <QWizard>

using namespace OFS;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray readFile(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

QString keyText(const char* keys)
{
    return QKeySequence(QString::fromLatin1(keys)).toString(QKeySequence::NativeText);
}

QString boxTitle(const QString& title)
{
#ifdef Q_OS_MACOS
    Q_UNUSED(title);
    return {};
#else
    return title;
#endif
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

void answerDiscard(QMessageBox::StandardButton button)
{
    auto* timer = new QTimer(qApp);
    timer->setInterval(5);
    QObject::connect(timer, &QTimer::timeout, qApp, [timer, button] {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!box || !box->isVisible())
            return;
        timer->stop();
        timer->deleteLater();
        QCOMPARE(box->windowTitle(), boxTitle("Discard changes?"));
        box->button(button)->click();
    });
    timer->start();
}

template <typename T>
void whenModal(std::function<void(T*)> answer)
{
    auto* timer = new QTimer(qApp);
    timer->setInterval(5);
    QObject::connect(timer, &QTimer::timeout, qApp, [timer, answer] {
        if (auto* window = qobject_cast<T*>(QApplication::activeModalWidget()); window && window->isVisible()) {
            timer->stop();
            timer->deleteLater();
            answer(window);
        }
    });
    timer->start();
}

// Answers Project > New with the template at `row` of the first tab, or with none when row is -1.
void chooseTemplate(int row)
{
    whenModal<QDialog>([row](QDialog* dialog) {
        QCOMPARE(dialog->objectName(), QStringLiteral("ProjDlg"));
        auto* tabs = dialog->findChild<QTabWidget*>("tabs");
        QVERIFY(tabs);
        if (row >= 0)
            qobject_cast<QListWidget*>(tabs->widget(0))->setCurrentRow(row);
        dialog->findChild<QDialogButtonBox*>("buttonBox")->button(QDialogButtonBox::Ok)->click();
    });
}

void answerBoxes(QList<QMessageBox::StandardButton> buttons, QStringList* texts)
{
    auto* timer = new QTimer(qApp);
    timer->setInterval(5);
    QObject::connect(timer, &QTimer::timeout, qApp, [timer, buttons, texts]() mutable {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!box || !box->isVisible())
            return;
        if (texts)
            *texts << box->text();
        box->button(buttons.takeFirst())->click();
        if (buttons.isEmpty()) {
            timer->stop();
            timer->deleteLater();
        }
    });
    timer->start();
}

void answerEmptyWizard(const QString& choice)
{
    whenModal<QWizard>([choice](QWizard* wizard) {
        QCOMPARE(wizard->windowTitle(), QStringLiteral("Empty project"));
        for (QRadioButton* radio : wizard->currentPage()->findChildren<QRadioButton*>()) {
            if (radio->text() == choice)
                radio->setChecked(true);
        }
        wizard->button(QWizard::FinishButton)->click();
    });
}

} // namespace

class UrlRecorder : public QObject
{
    Q_OBJECT

public:
    QList<QUrl> urls;

public Q_SLOTS:
    void open(const QUrl& url) { urls << url; }
};

class MainWindowTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        qputenv("OFS_AUDIO_BACKEND", "null");
        m_context = new AppContext;
        m_context->loadSettings();
        m_context->history().clear();
        m_context->config().setValue("lastEditor", QString());
        m_context->config().setValue("show_quicksearch", false);
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
        QCOMPARE(actions.action("file_new")->text(), "New\t (" + keyText("Ctrl+N") + ')');
        QCOMPARE(actions.action("file_open")->text(), "Open...\t (" + keyText("Ctrl+O") + ')');
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
        QCOMPARE(message, boxTitle("Find") + "|Nothing found!");
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

    void runInEngine()
    {
        QTemporaryDir engineDir;
#ifdef Q_OS_WIN
        const QString suffix = QStringLiteral(".exe");
#else
        const QString suffix;
#endif
        const QString exe = engineDir.filePath("mugen" + suffix);
        QVERIFY(QFile::copy(QCoreApplication::applicationDirPath() + "/fake_mugen" + suffix, exe));
        QDir().mkpath(engineDir.filePath("chars"));

        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("fighter.def")));
        Config& engineConfig = m_context->engineConfig("mugen");
        auto expectMessage = [this](const QString& title, const QString& text) {
            QTimer::singleShot(0, this, [title, text] {
                auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                QVERIFY(box);
                QCOMPARE(box->windowTitle(), boxTitle(title));
                QCOMPARE(box->text(), text);
                box->accept();
            });
        };

        RunConfiguration::save(engineConfig, {});
        expectMessage("Warning!", "Oops, you need to add an engine first. Do it in Tools / Options.");
        window.runInEngine();
        RunConfiguration::save(engineConfig, {{"Missing", engineDir.filePath("nothing.exe"), "", "%0 kfm"}});
        expectMessage("Error!", "Executable not found!");
        window.runInEngine();

        RunConfiguration::save(engineConfig, {{"Test engine", QDir::toNativeSeparators(exe), "", "%0 kfm"}});
        window.runInEngine();
        QVERIFY(window.isEngineRunning() || QFileInfo::exists(engineDir.filePath("mugen.log")));
        QTRY_VERIFY_WITH_TIMEOUT(!window.isEngineRunning(), 15000);
        auto* output = window.findChild<QTextEdit*>("engineOutput");
        QVERIFY(output);
        const QString log = output->toPlainText();
        QVERIFY2(log.contains("Arguments: ofs_0 kfm"), qPrintable(log));
        QVERIFY2(log.contains("Character definition: found"), qPrintable(log));
        // Compare canonical paths so short names (RUNNER~1) and symlinks (/private/var) match.
        const QString folderLine = log.section("Working folder: ", 1).section('\n', 0, 0);
        QVERIFY2(!folderLine.isEmpty(), qPrintable(log));
        QCOMPARE(QFileInfo(folderLine).canonicalFilePath(), QFileInfo(engineDir.path()).canonicalFilePath());
        QVERIFY(!QFileInfo::exists(engineDir.filePath("chars/ofs_0")));
        QVERIFY(QFileInfo::exists(engineDir.filePath("chars")));

        QTemporaryDir ikemenDir;
        const QString ikemen = ikemenDir.filePath("Ikemen_GO" + suffix);
        QVERIFY(QFile::copy(QCoreApplication::applicationDirPath() + "/fake_mugen" + suffix, ikemen));
        QDir().mkpath(ikemenDir.filePath("chars"));
        RunConfiguration::save(engineConfig, {{"Ikemen GO", QDir::toNativeSeparators(ikemen), "", "%0 kfm"}});
        output->clear();
        window.runInEngine();
        QTRY_VERIFY_WITH_TIMEOUT(!window.isEngineRunning(), 15000);
        QVERIFY2(output->toPlainText().contains("Arguments: ofs_0 kfm"), qPrintable(output->toPlainText()));
        QVERIFY(!QFileInfo::exists(ikemenDir.filePath("mugen.log")));
        QVERIFY(!QFileInfo::exists(ikemenDir.filePath("chars/ofs_0")));

        RunConfiguration::save(engineConfig, {});
    }

    void closingWithChanges()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("fighter.def")));
        m_context->actions().action("editor_st")->trigger();
        CodeEditor* editor = window.currentProjectView()->currentCodeEditor();
        editor->insertPlainText("; edited\n");
        m_context->config().setValue("showmsg_discard", QString());

        QString asked;
        auto answer = [this, &asked](QMessageBox::StandardButton button) {
            QTimer::singleShot(0, this, [&asked, button] {
                auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                QVERIFY(box);
                asked = box->windowTitle() + "|" + box->text() + "|" + box->checkBox()->text();
                box->button(button)->click();
            });
        };
        answer(QMessageBox::No);
        m_context->actions().action("file_close")->trigger();
        QCOMPARE(asked, boxTitle("Discard changes?") +
                            "|Do you want to discard the changes made in states?|Don't show this again");
        QVERIFY(window.currentProjectView());

        m_context->config().setValue("confirm_close", true);
        answer(QMessageBox::No);
        window.close();
        QCOMPARE(asked.section('|', 0, 1), boxTitle("Close") + "|Do you really want to close Open Factory Studio?");
        QVERIFY(window.isVisible());

        editor->document()->setModified(false);
        m_context->config().setValue("confirm_close", false);
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

    void variableUsage()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("vars.def"), "[Info]\nname = \"Vars\"\nmugenversion = 1.0\n"
                                            "[Files]\ncns = vars.cns\nsprite = v.sff\nanim = v.air\n");
        QByteArray cns = "[Data]\nlife = 1000\n";
        for (int i = 0; i < 100; ++i)
            cns += "; filler\n";
        cns += "; Throw\n[Statedef 810]\ntype = S\n[State 810, 1]\ntype = VarSet\ntrigger1 = 1\nv = 2\n"
               "[State 810, 2]\ntype = Null\ntrigger1 = (fvar(5) := 1.5)\n";
        for (int i = 0; i < 100; ++i)
            cns += "; filler\n";
        writeFile(dir.filePath("vars.cns"), cns);
        MainWindow window(*m_context);
        window.resize(1000, 700);
        window.show();
        QVERIFY(window.openProject(dir.filePath("vars.def")));
        m_context->actions().action("editor_st")->trigger();
        ProjectView* view = window.currentProjectView();
        QTreeView* tree = view->outlineTree("states");
        QVERIFY(tree);
        QTRY_COMPARE(tree->model() ? tree->model()->rowCount() : 0, 2);
        QCOMPARE(tree->model()->index(1, 0).data().toString(), QStringLiteral("810 - Throw"));
        tree->expand(tree->model()->index(0, 0)); // [Data], checked again later

        QAction* toggle = m_context->actions().action("st_var_list");
        QVERIFY(toggle->isCheckable());
        toggle->trigger();
        QVERIFY(toggle->isChecked());
        QAbstractItemModel* model = tree->model();
        QCOMPARE(model->rowCount(), 2);
        const QModelIndex integers = model->index(0, 0);
        const QModelIndex floats = model->index(1, 0);
        QCOMPARE(integers.data().toString(), QStringLiteral("Integer"));
        QCOMPARE(floats.data().toString(), QStringLiteral("Float"));
        QVERIFY(!tree->isExpanded(integers));
        QCOMPARE(model->rowCount(integers), 60);
        QCOMPARE(model->rowCount(floats), 40);
        QCOMPARE(model->index(2, 0, integers).data().toString(), QStringLiteral("2"));
        const QModelIndex throwState = model->index(0, 0, model->index(2, 0, integers));
        QCOMPARE(throwState.data().toString(), QStringLiteral("810 - Throw"));
        QCOMPARE(model->index(0, 0, model->index(5, 0, floats)).data().toString(), QStringLiteral("810 - Throw"));
        QCOMPARE(model->rowCount(model->index(1, 0, integers)), 0);

        CodeEditor* editor = view->currentCodeEditor();
        tree->expand(integers);
        tree->expand(model->index(2, 0, integers));
        Q_EMIT tree->clicked(model->index(3, 0, integers));
        QCOMPARE(editor->cursorLine(), 1);
        Q_EMIT tree->clicked(throwState);
        QCOMPARE(editor->cursorLine(), 104);
        QCOMPARE(editor->textCursor().positionInBlock(), 0);
        QVERIFY2(editor->cursorRect().top() < editor->fontMetrics().height() * 2,
                 qPrintable(QString::number(editor->cursorRect().top())));

        QTextCursor end(editor->document());
        end.movePosition(QTextCursor::End);
        end.insertText("[State 810, 3]\ntype = VarAdd\ntrigger1 = 1\nv = 7\n");
        QTRY_COMPARE(tree->model()->rowCount(tree->model()->index(7, 0, tree->model()->index(0, 0))), 1);

        toggle->trigger();
        QCOMPARE(tree->model()->index(1, 0).data().toString(), QStringLiteral("810 - Throw"));
        QVERIFY(tree->isExpanded(tree->model()->index(0, 0)));
        toggle->trigger();
        QCOMPARE(tree->model()->index(0, 0).data().toString(), QStringLiteral("Integer"));
        QVERIFY(tree->isExpanded(tree->model()->index(0, 0)));
        QVERIFY(tree->isExpanded(tree->model()->index(2, 0, tree->model()->index(0, 0))));

        editor->goToLine(1);
        const QString before = editor->toPlainText();
        QTimer::singleShot(0, this, [] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->objectName(), QStringLiteral("VarDlg"));
            QCOMPARE(dialog->windowTitle(), QStringLiteral("Options"));
            QVERIFY(dialog->findChild<QRadioButton*>("radioButton")->isChecked());
            QVERIFY(dialog->findChild<QCheckBox*>("checkBox_3")->isChecked());
            QVERIFY(!dialog->findChild<QCheckBox*>("checkBox_4")->isChecked());
            dialog->findChild<QRadioButton*>("radioButton_3")->setChecked(true);
            dialog->accept();
        });
        m_context->actions().action("st_var_log")->trigger();
        const QString log = QStringLiteral(";---------------------- Integer variable list ----------------------\n\n"
                                           "; var(2):= 810 - Throw\n"
                                           "; var(7):= 810 - Throw\n\n"
                                           ";---------------------- Float variable list ----------------------\n\n"
                                           "; fvar(5):= 810 - Throw\n\n");
        QCOMPARE(editor->toPlainText(), log + before);
        QCOMPARE(editor->textCursor().position(), log.size());
        editor->undo();
        QCOMPARE(editor->toPlainText(), before);

        toggle->trigger();
        QVERIFY(!toggle->isChecked());
        editor->document()->setModified(false);
    }

    void throwCreator()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("thrower.def"), "[Info]\nname = \"Thrower\"\nmugenversion = 1.0\n"
                                               "[Files]\ncns = thrower.cns\nsprite = t.sff\nanim = thrower.air\n");
        writeFile(dir.filePath("thrower.cns"), "[Data]\nlife = 1000\n");
        writeFile(dir.filePath("thrower.air"), "; Standing\n[Begin Action 0]\n0,0, 0,0, -1\n\n"
                                               "; Throw Attempt\n[Begin Action 800]\n800,0, 0,0, 3\n800,1, 0,0, 3\n"
                                               "800,2, 0,0, 3\n\n"
                                               "; Throw\n[Begin Action 810]\n810,0, 0,0, 5\n810,1, 0,0, 5\n"
                                               "810,2, 0,0, 5\n810,3, 0,0, 5\n\n"
                                               "; Thrown\n[Begin Action 820]\n5000,0, 0,0, 5\n5010,0, 4,0, 5, H\n");
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("thrower.def")));
        m_context->actions().action("editor_st")->trigger();
        CodeEditor* editor = window.currentProjectView()->currentCodeEditor();
        QVERIFY(editor);
        editor->moveCursor(QTextCursor::End);
        const QString before = editor->toPlainText();

        bool checked = false;
        QTimer::singleShot(0, this, [&checked] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->objectName(), QStringLiteral("ThrowDlg"));
            QCOMPARE(dialog->windowTitle(), QStringLiteral("Throw Creator"));
            auto combo = [dialog](const char* name) {
                return dialog->findChild<QComboBox*>(name);
            };
            auto spin = [dialog](const char* name) {
                return dialog->findChild<QSpinBox*>(name);
            };
            auto real = [dialog](const char* name) {
                return dialog->findChild<QDoubleSpinBox*>(name);
            };
            QCOMPARE(combo("p1AnimStart")->currentText(), QStringLiteral("800 - Throw Attempt"));
            QCOMPARE(combo("p1Anim")->currentText(), QStringLiteral("810 - Throw"));
            QCOMPARE(combo("p1Anim")->count(), 4);
            QCOMPARE(spin("p1SDef")->value(), 0);
            QCOMPARE(spin("elem")->value(), 4);
            auto* slider = dialog->findChild<QSlider*>("horizontalSlider");
            QCOMPARE(slider->maximum(), 3);
            QCOMPARE(real("bindX")->value(), 75.0);
            QCOMPARE(real("bindX")->decimals(), 3);
            QCOMPARE(combo("res")->currentText(), QStringLiteral("640x480"));
            QCOMPARE(combo("res")->count(), 6);
            QCOMPARE(combo("projs")->currentText(), QStringLiteral("Thrower"));
            QCOMPARE(combo("p2Anim")->currentText(), QStringLiteral("820 - Thrown"));
            QCOMPARE(spin("grp")->value(), 5000);
            QCOMPARE(combo("flipList")->currentText(), QStringLiteral("No Flip"));

            QVERIFY(dialog->findChild<QWidget*>("render"));
            slider->setValue(1);
            QCOMPARE(spin("grp")->value(), 5010);
            QCOMPARE(combo("flipList")->currentText(), QStringLiteral("Flip H"));
            real("bindX")->setValue(60);
            spin("bindPriority")->setValue(2);
            slider->setValue(0);
            QCOMPARE(real("bindX")->value(), 75.0);
            QCOMPARE(spin("bindPriority")->value(), 0);
            slider->setValue(1);
            QCOMPARE(real("bindX")->value(), 60.0);

            spin("elem")->setValue(2);
            QCOMPARE(slider->maximum(), 1);
            dialog->findChild<QPushButton*>("ins")->click();
            checked = true;
        });
        m_context->actions().action("st_throw")->trigger();
        QVERIFY(checked);
        const QString inserted = editor->toPlainText().mid(before.size());
        QVERIFY2(inserted.startsWith("[Statedef 0]\ntype = S\nmovetype= A\nphysics = S\njuggle = 0\nvelset = 0, 0\n"
                                     "anim = 800\n"),
                 qPrintable(inserted.left(200)));
        QVERIFY(inserted.contains("[Statedef 10]\ntype = S\nmovetype= A\nphysics = N\nanim = 810\n"));
        QVERIFY(inserted.contains("[State 10, Bind 1]\ntype = TargetBind\ntrigger1 = AnimElem = 1\npos = 75.0, 0.0\n"));
        QVERIFY(inserted.contains("[State 10, SprPriority]\ntype = SprPriority\ntrigger1 = AnimElem = 2\nvalue = 2\n\n"
                                  "[State 10, Bind 2]\ntype = TargetBind\ntrigger1 = AnimElem = 2\npos = 60.0, 0.0\n"));
        QVERIFY(!inserted.contains("Bind 3"));
        QVERIFY(inserted.contains("[State 10, Hurt]\ntype = TargetLifeAdd\ntrigger1 = AnimElem = 2\n"));
        QVERIFY(inserted.contains("[State 20, 1]\ntype = ChangeAnim2\ntrigger1 = Time = 0\nvalue = 820\n"));
        QCOMPARE(editor->textCursor().position(), editor->document()->characterCount() - 1);

        const QString after = editor->toPlainText();
        QTimer::singleShot(0, this, [] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->findChild<QPushButton*>("close")->click();
        });
        m_context->actions().action("st_throw")->trigger();
        QCOMPARE(editor->toPlainText(), after);
        editor->document()->setModified(false);
    }

    void offsetViewer()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("fighter.def")));
        m_context->actions().action("editor_st")->trigger();
        CodeEditor* editor = window.currentProjectView()->currentCodeEditor();
        const QString before = editor->toPlainText();
        bool shown = false;
        QTimer::singleShot(0, this, [&shown] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->objectName(), QStringLiteral("OffsetDlg"));
            QVERIFY(dialog->findChild<QWidget*>("render"));
            QVERIFY(!dialog->findChild<QPushButton*>());
            shown = true;
            dialog->reject();
        });
        m_context->actions().action("st_offset")->trigger();
        QVERIFY(shown);
        QCOMPARE(editor->toPlainText(), before);
    }

    void editorFiles()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("files.def"), "[Info]\nname = \"Files\"\nmugenversion = 1.0\n"
                                             "[Files]\ncmd = files.cmd\ncns = files.cns\n");
        writeFile(dir.filePath("files.cmd"), "[Command]\nname = \"x\"\ncommand = x\n");
        writeFile(dir.filePath("files.cns"), "[Data]\nlife = 1000\n");
        writeFile(dir.filePath("other.st"), "[Statedef 5]\ntype = S\n");
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("files.def")));
        ProjectView* view = window.currentProjectView();
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_st")->trigger();
        auto* tabs = view->findChild<QTabWidget*>("fileTabs_states");
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 1);

        actions.action("st_new")->trigger();
        QCOMPARE(tabs->count(), 2);
        QCOMPARE(tabs->currentIndex(), 1);
        QCOMPARE(tabs->tabText(1), QStringLiteral("untitled1.cns"));
        ProjectFile* created = view->currentFile();
        QVERIFY(created && created->isUntitled());
        CodeEditor* editor = view->currentCodeEditor();
        editor->insertPlainText("[Statedef 0]\ntype = S\n");

        QStringList prompts;
        QString answer;
        FileDialogs::setSavePromptOverride([&](const QString& caption, const QString& path, const QString& filter) {
            prompts << caption + "|" + QFileInfo(path).fileName() + "|" + filter;
            return answer;
        });
        actions.action("st_save")->trigger();
        QCOMPARE(prompts.size(), 1);
        QCOMPARE(prompts[0], QStringLiteral("Save as|untitled1.cns|States (*.cns *.st);;All files (*)"));
        QVERIFY(created->isUntitled());
        answer = dir.filePath("brand.cns");
        actions.action("st_save")->trigger();
        QVERIFY(!created->isUntitled());
        QVERIFY(QFileInfo::exists(dir.filePath("brand.cns")));
        QCOMPARE(tabs->tabText(1), QStringLiteral("brand.cns"));
        QCOMPARE(tabs->tabToolTip(1), QDir::toNativeSeparators(QDir::fromNativeSeparators(dir.filePath("brand.cns"))));
        QVERIFY(view->project()->editorFiles("states").contains(QDir::fromNativeSeparators(dir.filePath("brand.cns"))));

        editor->moveCursor(QTextCursor::End);
        editor->insertPlainText("[State 0, 1]\ntype = NoSuchController\ntrigger1 = 1\n");
        const QString brand = QDir::fromNativeSeparators(dir.filePath("brand.cns"));
        QTRY_VERIFY(!view->issues().value(brand).isEmpty());

        QString openDir;
        FileDialogs::setOpenPromptOverride([&](const QString&, const QString& path, const QString&) {
            openDir = path;
            return dir.filePath("other.st");
        });
        actions.action("st_open")->trigger();
        QCOMPARE(tabs->count(), 3);
        QCOMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("other.st"));
        QCOMPARE(QDir(openDir), QDir(dir.path()));
        actions.action("st_open")->trigger();
        QCOMPARE(tabs->count(), 3);

        answer = dir.filePath("other2.st");
        actions.action("st_saveas")->trigger();
        QVERIFY(QFileInfo::exists(dir.filePath("other2.st")));
        QVERIFY(QFileInfo::exists(dir.filePath("other.st")));
        QCOMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("other2.st"));

        actions.action("cmd_new")->trigger();
        QCOMPARE(view->currentEditor(), QStringLiteral("commands"));
        view->currentCodeEditor()->insertPlainText("[Command]\n");
        answer = dir.filePath("more.cmd");
        actions.action("file_save")->trigger();
        QVERIFY(QFileInfo::exists(dir.filePath("more.cmd")));
        QVERIFY(!view->project()->isModified());

        FileDialogs::setSavePromptOverride({});
        FileDialogs::setOpenPromptOverride({});
    }

    void spritesWorkspace()
    {
        QTemporaryDir dir;
        // Two sprites: 0,0 (6x4, axis 3,4) and 0,1.
        SpriteDocument doc;
        doc.palettes() << SpriteDocument::makePalette(1, 1, Formats::grayscalePalette(256));
        for (int n = 0; n < 2; ++n) {
            SpriteEntry entry;
            entry.header.number = std::uint16_t(n);
            entry.header.axisX = 3;
            entry.header.axisY = 4;
            entry.pixels = SpriteDocument::makePixels(
                Formats::Image::indexed(6, 4, std::vector<std::uint8_t>(24, std::uint8_t(n + 5))));
            entry.palette = doc.palettes()[0];
            doc.sprites() << entry;
        }
        Formats::SffDocument sff = doc.toSff();
        sff.version = Formats::SffVersion::V2;
        const auto bytes = sff.write();
        QVERIFY(bytes.ok());
        writeFile(dir.filePath("spr.sff"),
                  QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size())));
        writeFile(dir.filePath("spr.def"), "[Info]\nname = \"Spr\"\nmugenversion = 1.0\n"
                                           "[Files]\ncns = spr.cns\nsprite = spr.sff\n");
        writeFile(dir.filePath("spr.cns"), "[Data]\nlife = 1000\n");

        MainWindow window(*m_context);
        window.resize(1000, 700);
        window.show();
        QVERIFY(window.openProject(dir.filePath("spr.def")));
        m_context->actions().action("editor_spr")->trigger();
        ProjectView* view = window.currentProjectView();
        auto* page = view->findChild<SpritePage*>();
        QVERIFY(page);
        QCOMPARE(page->file()->document().spriteCount(), 2);
        GraphicsView* workspace = page->view();
        QCOMPARE(workspace->images().size(), 1);
        QCOMPARE(workspace->images()[0].topLeft, QPoint(-3, -4));
        QCOMPARE(workspace->images()[0].image.size(), QSize(6, 4));

        QVERIFY(QTest::qWaitForWindowExposed(&window));
        // Center the axis and scale the drag by the zoom, so distances are in sprite pixels.
        workspace->centerOrigin();
        const QPoint on = workspace->toWidget(QPointF(0.5, -1.5)).toPoint();
        const auto by = [workspace, on](double dx, double dy) {
            return on + QPointF(dx * workspace->zoom(), dy * workspace->zoom()).toPoint();
        };
        QTest::mousePress(workspace, Qt::LeftButton, {}, on);
        QTest::mouseMove(workspace, by(2, 0));
        QTest::mouseMove(workspace, by(4, 1));
        QTest::mouseRelease(workspace, Qt::LeftButton, {}, by(4, 1));
        const auto& moved = page->file()->document().sprites()[0].header;
        QCOMPARE(int(moved.axisX), -1);
        QCOMPARE(int(moved.axisY), 3);
        QVERIFY(page->file()->isModified());
        workspace->setFocus();
        m_context->actions().action("edit_undo")->trigger();
        QCOMPARE(int(page->file()->document().sprites()[0].header.axisX), 3);
        QVERIFY(!page->file()->isModified());
        m_context->actions().action("edit_redo")->trigger();
        QCOMPARE(int(page->file()->document().sprites()[0].header.axisX), -1);
        page->file()->undoStack()->undo();

        page->setCurrent(1);
        page->setOnionSkin(0, 0.5);
        QCOMPARE(workspace->images().size(), 2);
        QCOMPARE(workspace->images()[0].opacity, 0.5);
        page->setCurrent(7); // clamped
        QCOMPARE(page->current(), 1);
        page->setOnionSkin(-1, 1.0);

        auto* status = window.findChild<StatusBar*>();
        QVERIFY(status);
        workspace->setGuides(QPoint(7, -3));
        QTRY_COMPARE(status->section(1), QStringLiteral("Horizontal guide: -3, Vertical guide: 7"));
        QCOMPARE(status->section(2), QStringLiteral("Version: 2.0.1.0"));
        QVERIFY(status->section(0).startsWith(QStringLiteral("Position: ")));

        auto* group = view->findChild<QSpinBox*>("spr_grp");
        auto* number = view->findChild<QSpinBox*>("spr_num");
        auto* axisX = view->findChild<QSpinBox*>("spr_xaxis");
        auto* selector = view->findChild<QScrollBar*>("spr_sel");
        QVERIFY(group && number && axisX && selector);
        QCOMPARE(number->value(), 1);
        QCOMPARE(selector->maximum(), 1);
        QCOMPARE(selector->value(), 1);
        selector->setValue(0);
        QCOMPARE(page->current(), 0);
        QCOMPARE(number->value(), 0);
        axisX->setValue(10);
        QCOMPARE(int(page->file()->document().sprites()[0].header.axisX), 10);
        QCOMPARE(workspace->images().last().topLeft, QPoint(-10, -4));

        ActionRegistry& actions = m_context->actions();
        actions.action("spr_next")->trigger();
        QCOMPARE(page->current(), 1);
        actions.action("spr_prev_grp")->trigger(); // already in the first group
        QCOMPARE(page->current(), 1);
        actions.action("spr_next_grp")->trigger(); // wraps to the first sprite
        QCOMPARE(page->current(), 0);
        actions.action("spr_pos_left")->trigger(); // moving the sprite left grows the axis
        QCOMPARE(int(page->file()->document().sprites()[0].header.axisX), 11);
        QCOMPARE(axisX->value(), 11);
        actions.action("spr_duplicate")->trigger();
        QCOMPARE(page->file()->document().spriteCount(), 3);
        QCOMPARE(page->current(), 1);
        QCOMPARE(number->value(), 0);
        const QString sffPath = QDir::fromNativeSeparators(dir.filePath("spr.sff"));
        QTRY_COMPARE(view->issues().value(sffPath).size(), 1);
        QCOMPARE(view->issues().value(sffPath).first().line, 1);
        page->setCurrent(0);
        QVERIFY(view->showFileLine(sffPath, 2));
        QCOMPARE(page->current(), 1);

        auto* organizer = view->findChild<QListView*>("org_list");
        QVERIFY(organizer);
        QCOMPARE(organizer->model()->rowCount(), 3);
        QCOMPARE(organizer->model()->index(2, 0).data().toString(), QStringLiteral("0, 1"));
        organizer->selectionModel()->select(organizer->model()->index(2, 0), QItemSelectionModel::ClearAndSelect);
        actions.action("org_top")->trigger();
        QCOMPARE(page->file()->document().sprites()[0].number(), 1);
        page->file()->undoStack()->undo();
        QCOMPARE(page->file()->document().sprites()[2].number(), 1);
        QVERIFY(organizer->viewport()->acceptDrops());
        organizer->selectionModel()->select(organizer->model()->index(2, 0), QItemSelectionModel::ClearAndSelect);
        view->spritesEditor()->dropOrganizerRows(0);
        QCOMPARE(page->file()->document().sprites()[0].number(), 1);
        QCOMPARE(organizer->selectionModel()->selectedRows().first().row(), 0);
        page->file()->undoStack()->undo();

        m_context->config().setValue("confirm_delete", false);
        actions.action("spr_delete")->trigger();
        m_context->config().setValue("confirm_delete", true);
        QCOMPARE(page->file()->document().spriteCount(), 2);
        while (page->file()->isModified())
            page->file()->undoStack()->undo();
    }

    void spritesImageEditor()
    {
        QTemporaryDir dir;
        // 0,0 and 0,1: 4x3, palette index 5 everywhere, axis 1,2.
        SpriteDocument doc;
        doc.palettes() << SpriteDocument::makePalette(1, 1, Formats::grayscalePalette(256));
        for (int n = 0; n < 2; ++n) {
            SpriteEntry entry;
            entry.header.number = std::uint16_t(n);
            entry.header.axisX = 1;
            entry.header.axisY = 2;
            entry.pixels = SpriteDocument::makePixels(Formats::Image::indexed(4, 3, std::vector<std::uint8_t>(12, 5)));
            entry.palette = doc.palettes()[0];
            doc.sprites() << entry;
        }
        const auto bytes = doc.toSff().write();
        QVERIFY(bytes.ok());
        writeFile(dir.filePath("img.sff"),
                  QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size())));
        writeFile(dir.filePath("img.def"), "[Info]\nname = \"Img\"\nmugenversion = 1.0\n"
                                           "[Files]\ncns = img.cns\nsprite = img.sff\n");
        writeFile(dir.filePath("img.cns"), "[Data]\nlife = 1000\n");

        MainWindow window(*m_context);
        window.resize(1000, 700);
        window.show();
        QVERIFY(window.openProject(dir.filePath("img.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        ProjectView* view = window.currentProjectView();
        auto* page = view->findChild<SpritePage*>();
        QVERIFY(page);
        SpriteFile* file = page->file();
        ImageEditor* editor = page->imageEditor();
        auto* layers = view->findChild<QListView*>("spr_layers_list");
        auto* blend = view->findChild<QComboBox*>("spr_layers_blendmode");
        auto* group = view->findChild<QSpinBox*>("spr_grp");
        QVERIFY(layers && blend && group);
        QVERIFY(layers->isHidden());
        QVERIFY(!group->isHidden());
        QVERIFY(!actions.action("spr_saveimg")->isEnabled());
        auto answer = [this](QMessageBox::StandardButton button, QString* asked = nullptr) {
            QTimer::singleShot(0, this, [button, asked] {
                auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                QVERIFY(box);
                if (asked)
                    *asked = box->windowTitle() + "|" + box->text();
                box->button(button)->click();
            });
        };

        actions.action("spr_edit_img")->trigger();
        QVERIFY(page->imageMode());
        QVERIFY(actions.action("spr_edit_img")->isChecked());
        QVERIFY(actions.action("spr_saveimg")->isEnabled());
        QVERIFY(!actions.action("spr_align")->isEnabled());
        QVERIFY(!layers->isHidden());
        QVERIFY(group->isHidden());
        QVERIFY(actions.action("spr_tool_canvasmove")->isChecked());
        QCOMPARE(layers->model()->rowCount(), 1);
        QCOMPARE(layers->model()->index(0, 0).data().toString(), QStringLiteral("0,0"));
        QVERIFY(!blend->isEnabled()); // 8-bit canvas

        QVERIFY(QTest::qWaitForWindowExposed(&window));
        actions.action("spr_tool_pen")->trigger();
        QCOMPARE(editor->tool(), ImageEditor::Tool::Pen);
        editor->setForeground(PaintColor{7, {}});
        ImageCanvas* canvas = editor->canvas();
        canvas->setZoom(20);
        const QPoint pixel11 = canvas->toWidget(QPointF(1.5, 1.5)).toPoint();
        QTest::mouseClick(canvas, Qt::LeftButton, {}, pixel11);
        QCOMPARE(editor->document().pixel(1, 1).index, 7);
        QCOMPARE(editor->undoStack()->count(), 1);
        QVERIFY(editor->isModified());
        QVERIFY(!file->isModified());
        canvas->setFocus();
        actions.action("edit_undo")->trigger();
        QCOMPARE(editor->document().pixel(1, 1).index, 5);
        actions.action("edit_redo")->trigger();
        QCOMPARE(editor->document().pixel(1, 1).index, 7);
        actions.action("view_zoomin")->trigger();
        QCOMPARE(canvas->zoom(), 21.0);

        auto* model = qobject_cast<QStandardItemModel*>(layers->model());
        QVERIFY(model);
        actions.action("spr_layer_add")->trigger();
        QCOMPARE(model->rowCount(), 2);
        QCOMPARE(model->item(0)->text(), QStringLiteral("Layer 2"));
        QCOMPARE(editor->document().current, 1);
        model->item(0)->setText("0,1");
        QCOMPARE(editor->document().layers[1].name, QStringLiteral("0,1"));
        const int steps = editor->undoStack()->count();
        model->item(0)->setCheckState(Qt::Unchecked);
        QVERIFY(!editor->document().layers[1].visible);
        QCOMPARE(editor->undoStack()->count(), steps);
        model->item(0)->setCheckState(Qt::Checked);
        actions.action("spr_layer_add")->trigger();
        model->item(0)->setText("5,3");
        editor->setForeground(PaintColor{9, {}});
        QTest::mouseClick(canvas, Qt::LeftButton, {}, canvas->toWidget(QPointF(0.5, 0.5)).toPoint());
        QCOMPARE(editor->document().layers[2].image.indexAt(0, 0), 9);
        QCOMPARE(editor->document().layers[0].image.indexAt(0, 0), 5);

        QString asked;
        answer(QMessageBox::Yes, &asked);
        actions.action("spr_layer_save")->trigger();
        QCOMPARE(asked, boxTitle("Replace sprite") + "|The sprite 0,1 already exists. Do you want to replace it?");
        const SpriteDocument& saved = file->document();
        QCOMPARE(saved.spriteCount(), 3);
        QCOMPARE(saved.sprites()[1].group(), 5);
        QCOMPARE(saved.sprites()[1].number(), 3);
        QCOMPARE(saved.image(1).indexAt(0, 0), 9);
        QCOMPARE(saved.image(0).indexAt(1, 1), 7);
        QCOMPARE(saved.image(2).indexAt(1, 1), 0); // 0,1 took the empty layer
        QCOMPARE(model->rowCount(), 3);
        QCOMPARE(page->current(), 0);

        QTimer::singleShot(0, this, [] {
            QWidget* dialog = QApplication::activeModalWidget();
            QVERIFY(dialog);
            QCOMPARE(dialog->objectName(), QStringLiteral("ImgInfoDlg"));
            QCOMPARE(dialog->findChild<QComboBox*>("fmt")->currentText(), QStringLiteral("RGBA (indexed color)"));
            dialog->findChild<QSpinBox*>("wid")->setValue(6);
            dialog->findChild<QSpinBox*>("hei")->setValue(5);
            qobject_cast<QDialog*>(dialog)->accept();
        });
        actions.action("spr_image_properties")->trigger();
        QCOMPARE(editor->document().width, 6);
        QCOMPARE(editor->document().origin, QPoint(1, 1));
        actions.action("spr_saveimg")->trigger();
        QCOMPARE(file->document().sprites()[0].width(), 6);
        QCOMPARE(file->document().sprites()[0].height(), 5);
        QCOMPARE(int(file->document().sprites()[0].header.axisX), 2);
        QCOMPARE(int(file->document().sprites()[0].header.axisY), 3);
        QCOMPARE(file->document().image(0).indexAt(2, 2), 7);
        QVERIFY(!editor->isModified());
        QCOMPARE(model->rowCount(), 3);

        QTest::mouseClick(canvas, Qt::LeftButton, {}, canvas->toWidget(QPointF(3.5, 3.5)).toPoint());
        QVERIFY(editor->isModified());
        answer(QMessageBox::No, &asked);
        actions.action("spr_next")->trigger();
        QCOMPARE(asked,
                 boxTitle("Discard changes?") + "|Do you want to discard the changes made in the current sprite?");
        QCOMPARE(page->current(), 0);
        answer(QMessageBox::No);
        actions.action("editor_def")->trigger();
        QCOMPARE(view->currentEditor(), QStringLiteral("sprites"));
        QVERIFY(actions.action("editor_spr")->isChecked());
        answer(QMessageBox::No);
        actions.action("spr_edit_img")->trigger();
        QVERIFY(page->imageMode());
        QVERIFY(actions.action("spr_edit_img")->isChecked());
        answer(QMessageBox::Yes);
        actions.action("spr_next")->trigger();
        QCOMPARE(page->current(), 1);
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(model->item(0)->text(), QStringLiteral("5,3"));
        QVERIFY(!editor->isModified());

        actions.action("spr_select_all")->trigger();
        actions.action("edit_copy")->trigger();
        actions.action("edit_paste")->trigger();
        QCOMPARE(model->rowCount(), 2);
        QCOMPARE(editor->document().layers[1].image.indexAt(0, 0), 9);

        QTimer::singleShot(0, this, [] {
            QWidget* dialog = QApplication::activeModalWidget();
            QVERIFY(dialog);
            QCOMPARE(dialog->objectName(), QStringLiteral("RotImgDlg"));
            dialog->findChild<QDoubleSpinBox*>("angle")->setValue(90);
            qobject_cast<QDialog*>(dialog)->accept();
        });
        actions.action("spr_image_rotate")->trigger();
        QCOMPARE(editor->document().width, 3);
        QCOMPARE(editor->document().height, 4);

        answer(QMessageBox::Yes);
        actions.action("spr_edit_img")->trigger();
        QVERIFY(!page->imageMode());
        QVERIFY(!actions.action("spr_edit_img")->isChecked());
        QVERIFY(layers->isHidden());
        QVERIFY(!group->isHidden());
        QVERIFY(actions.action("spr_align")->isEnabled());
        while (file->isModified())
            file->undoStack()->undo();
    }

    void spritesExplorer()
    {
        QTemporaryDir dir;
        // 0,0 is 4x3 with axis 1,2. 0,1 is 2x2 with axis 0,0. 10,0 is 3x3.
        SpriteDocument doc;
        doc.palettes() << SpriteDocument::makePalette(1, 1, Formats::grayscalePalette(256));
        const int sprites[][5] = {{0, 0, 4, 3, 1}, {0, 1, 2, 2, 0}, {10, 0, 3, 3, 0}};
        for (const auto& s : sprites) {
            SpriteEntry entry;
            entry.header.group = std::uint16_t(s[0]);
            entry.header.number = std::uint16_t(s[1]);
            entry.header.axisX = std::int16_t(s[4]);
            entry.header.axisY = std::int16_t(s[4] * 2);
            entry.pixels = SpriteDocument::makePixels(
                Formats::Image::indexed(s[2], s[3], std::vector<std::uint8_t>(std::size_t(s[2] * s[3]), 6)));
            entry.palette = doc.palettes()[0];
            doc.sprites() << entry;
        }
        const auto bytes = doc.toSff().write();
        QVERIFY(bytes.ok());
        writeFile(dir.filePath("ex.sff"),
                  QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size())));
        writeFile(dir.filePath("ex.def"), "[Info]\nname = \"Ex\"\nmugenversion = 1.0\n"
                                          "[Files]\ncns = ex.cns\nsprite = ex.sff\n");
        writeFile(dir.filePath("ex.cns"), "[Data]\nlife = 1000\n");

        MainWindow window(*m_context);
        window.resize(1000, 700);
        window.show();
        QVERIFY(window.openProject(dir.filePath("ex.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        ProjectView* view = window.currentProjectView();
        auto* page = view->findChild<SpritePage*>();
        auto* right = view->findChild<EPanelWidget*>("rightPanel");
        QVERIFY(page && right);
        ImageEditor* editor = page->imageEditor();

        QVERIFY(!actions.action("spr_explorer")->isEnabled());
        actions.action("spr_edit_img")->trigger();
        QVERIFY(actions.action("spr_explorer")->isEnabled());
        actions.action("spr_explorer")->trigger();
        QVERIFY(actions.action("spr_explorer")->isChecked());
        QCOMPARE(right->title(), QStringLiteral("Sprite explorer"));
        auto* groups = view->findChild<QComboBox*>("thumb_grp_list");
        auto* thumbnails = view->findChild<QListView*>("thumb_view");
        auto* addSelected = view->findChild<QToolButton*>("thumb_action1");
        auto* addAllAligned = view->findChild<QToolButton*>("thumb_action4");
        QVERIFY(groups && thumbnails && addSelected && addAllAligned);
        QCOMPARE(groups->count(), 2);
        QCOMPARE(groups->itemText(1), QStringLiteral("10"));
        QCOMPARE(groups->currentText(), QStringLiteral("0"));
        QCOMPARE(thumbnails->model()->rowCount(), 2);
        QCOMPARE(thumbnails->model()->index(1, 0).data().toString(), QStringLiteral("1"));
        QCOMPARE(addSelected->text(), QStringLiteral("Add selected as layers"));

        thumbnails->selectionModel()->select(thumbnails->model()->index(1, 0), QItemSelectionModel::ClearAndSelect);
        addSelected->click();
        QCOMPARE(editor->document().layers.size(), 2);
        QCOMPARE(editor->document().layers[0].name, QStringLiteral("0,1"));
        QCOMPARE(editor->document().current, 1);
        QCOMPARE(editor->document().width, 4);
        QCOMPARE(editor->document().height, 3);
        QCOMPARE(editor->undoStack()->count(), 1);
        addAllAligned->click();
        QCOMPARE(editor->document().layers.size(), 4);
        QCOMPARE(editor->document().height, 4); // 0,1 sits at the axis (1,2)
        QCOMPARE(editor->document().layers[1].image.indexAt(1, 3), 6);
        QCOMPARE(editor->undoStack()->count(), 2);

        FileDialogs::setSavePromptOverride([&dir](const QString& caption, const QString&, const QString& filter) {
            return caption == QLatin1String("Save sprites") && filter.startsWith("Portable Network Graphic")
                       ? dir.filePath("thumbs.png")
                       : QString();
        });
        actions.action("thumb_save")->trigger();
        FileDialogs::setSavePromptOverride({});
        QVERIFY(QFileInfo::exists(dir.filePath("thumbs_1.png")));

        groups->setCurrentIndex(1);
        QCOMPARE(thumbnails->model()->rowCount(), 1);

        answerDiscard(QMessageBox::Yes);
        actions.action("spr_edit_img")->trigger();
        QVERIFY(!page->imageMode());
        QCOMPARE(right->title(), QStringLiteral("Palettes"));
        QVERIFY(!actions.action("spr_explorer")->isChecked());
    }

    void spritesImport()
    {
        // Project a has 0,0 and 1,0. Project b ("Other") has 9,0, 0,0 and 9,1.
        QTemporaryDir a;
        QTemporaryDir b;
        auto writeProject = [](const QTemporaryDir& dir, const QString& name, const QList<QPair<int, int>>& ids,
                               int fill) {
            SpriteDocument doc;
            doc.palettes() << SpriteDocument::makePalette(1, 1, Formats::grayscalePalette(256));
            for (const auto& id : ids) {
                SpriteEntry entry;
                entry.header.group = std::uint16_t(id.first);
                entry.header.number = std::uint16_t(id.second);
                entry.pixels = SpriteDocument::makePixels(
                    Formats::Image::indexed(2, 2, std::vector<std::uint8_t>(4, std::uint8_t(id.first + fill))));
                entry.palette = doc.palettes()[0];
                doc.sprites() << entry;
            }
            const auto bytes = doc.toSff().write();
            QVERIFY(bytes.ok());
            writeFile(dir.filePath("p.sff"),
                      QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size())));
            writeFile(dir.filePath("p.def"), "[Info]\nname = \"" + name.toUtf8() +
                                                 "\"\nmugenversion = 1.0\n[Files]\ncns = p.cns\nsprite = p.sff\n");
            writeFile(dir.filePath("p.cns"), "[Data]\nlife = 1000\n");
        };
        writeProject(a, "Mine", {{0, 0}, {1, 0}}, 1);
        writeProject(b, "Other", {{9, 0}, {0, 0}, {9, 1}}, 20);

        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(b.filePath("p.def")));
        m_context->actions().action("editor_spr")->trigger();
        QVERIFY(window.openProject(a.filePath("p.def")));
        m_context->actions().action("editor_spr")->trigger();
        auto* page = window.currentProjectView()->findChild<SpritePage*>();
        QVERIFY(page);

        QStringList asked;
        QTimer::singleShot(0, this, [this, &asked] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->objectName(), QStringLiteral("ProjImportDlg"));
            auto* projects = dialog->findChild<QComboBox*>("projList");
            QCOMPARE(projects->count(), 1); // the current project is not listed
            QCOMPARE(projects->currentText(), QStringLiteral("Other"));
            auto* source = dialog->findChild<QListWidget*>("list");
            auto* destination = dialog->findChild<QListWidget*>("destList");
            QCOMPARE(source->count(), 3);
            QCOMPARE(source->item(0)->text(), QStringLiteral("9, 0"));
            dialog->findChild<QCheckBox*>("sorted")->setChecked(true);
            QCOMPARE(source->item(0)->text(), QStringLiteral("0, 0"));
            source->item(1)->setSelected(true); // 9, 0
            source->item(0)->setSelected(true); // 0, 0
            dialog->findChild<QPushButton*>("addBut")->click();
            QCOMPARE(destination->count(), 2); // in the order of selection
            QCOMPARE(destination->item(0)->text(), QStringLiteral("9,0"));
            destination->item(0)->setText("7,7");
            QVERIFY(dialog->findChild<QRadioButton*>("ask")->isChecked());
            QTimer::singleShot(0, this, [this, &asked] {
                auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                QVERIFY(box);
                asked << box->windowTitle() + "|" + box->text();
                box->button(QMessageBox::Yes)->click();
                QTimer::singleShot(0, this, [&asked] {
                    auto* info = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                    QVERIFY(info);
                    asked << info->windowTitle() + "|" + info->text();
                    info->accept();
                });
            });
            dialog->accept();
        });
        m_context->actions().action("spr_import_proj")->trigger();
        QCOMPARE(asked, (QStringList{boxTitle("Conflict resolution!") +
                                         "|Sprite '0,0' already exists, do you want to replace it?",
                                     boxTitle("Import from another project...") + "|1 new sprite(s) were imported!"}));
        const SpriteDocument& doc = page->file()->document();
        QCOMPARE(doc.spriteCount(), 3);
        QCOMPARE(doc.sprites()[1].group(), 7);
        QCOMPARE(page->current(), 1);
        QCOMPARE(doc.image(0).indexAt(0, 0), 20); // replaced in place by Other's 0,0
        QCOMPARE(doc.image(1).indexAt(0, 0), 29);
        page->file()->undoStack()->undo();
        QCOMPARE(page->file()->document().spriteCount(), 2);
    }

    void spritesNewOpen()
    {
        QTemporaryDir dir;
        auto writeSff = [](const QString& path, int count) {
            SpriteDocument doc;
            doc.palettes() << SpriteDocument::makePalette(1, 1, Formats::grayscalePalette(256));
            for (int n = 0; n < count; ++n) {
                SpriteEntry entry;
                entry.header.number = std::uint16_t(n);
                entry.pixels =
                    SpriteDocument::makePixels(Formats::Image::indexed(2, 2, std::vector<std::uint8_t>(4, 3)));
                entry.palette = doc.palettes()[0];
                doc.sprites() << entry;
            }
            const auto bytes = doc.toSff().write();
            QVERIFY(bytes.ok());
            writeFile(path,
                      QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size())));
        };
        writeSff(dir.filePath("n.sff"), 1);
        writeSff(dir.filePath("other.sff"), 3);
        writeFile(dir.filePath("n.def"), "[Info]\nname = \"N\"\nmugenversion = 1.0\n"
                                         "[Files]\ncns = n.cns\nsprite = n.sff\n");
        writeFile(dir.filePath("n.cns"), "[Data]\nlife = 1000\n");

        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("n.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        ProjectView* view = window.currentProjectView();
        auto* tabs = view->findChild<QTabWidget*>("fileTabs_sprites");
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 1);

        actions.action("spr_new")->trigger();
        QCOMPARE(tabs->count(), 2);
        QCOMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("untitled1.sff"));
        auto* page = qobject_cast<SpritePage*>(tabs->currentWidget());
        QVERIFY(page);
        QVERIFY(page->file()->isUntitled());
        QCOMPARE(page->file()->document().spriteCount(), 0);
        QCOMPARE(page->file()->document().version(), Formats::SffVersion::V2);
        QCOMPARE(view->spritesEditor()->page(), page);
        actions.action("spr_edit_img")->trigger();
        actions.action("spr_layer_add")->trigger();
        actions.action("spr_saveimg")->trigger();
        QCOMPARE(page->file()->document().spriteCount(), 0);
        actions.action("spr_edit_img")->trigger();
        QVERIFY(!page->imageMode());

        FileDialogs::setOpenPromptOverride([&dir](const QString&, const QString&, const QString& filter) {
            return filter.startsWith("Sprites (*.sff)") ? dir.filePath("other.sff") : QString();
        });
        actions.action("spr_open")->trigger();
        QCOMPARE(tabs->count(), 3);
        QCOMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("other.sff"));
        page = qobject_cast<SpritePage*>(tabs->currentWidget());
        QVERIFY(page);
        QCOMPARE(page->file()->document().spriteCount(), 3);
        tabs->setCurrentIndex(0);
        actions.action("spr_open")->trigger();
        QCOMPARE(tabs->count(), 3);
        QCOMPARE(tabs->currentIndex(), 2);
        FileDialogs::setOpenPromptOverride({});
    }

    void spritesClipboard()
    {
        QTemporaryDir dir;
        // 3,0 (2x2, axis 1,1) and 3,1.
        SpriteDocument doc;
        doc.palettes() << SpriteDocument::makePalette(1, 1, Formats::grayscalePalette(256));
        for (int n = 0; n < 2; ++n) {
            SpriteEntry entry;
            entry.header.group = 3;
            entry.header.number = std::uint16_t(n);
            entry.header.axisX = 1;
            entry.header.axisY = 1;
            entry.pixels = SpriteDocument::makePixels(
                Formats::Image::indexed(2, 2, std::vector<std::uint8_t>(4, std::uint8_t(n + 4))));
            entry.palette = doc.palettes()[0];
            doc.sprites() << entry;
        }
        const auto bytes = doc.toSff().write();
        QVERIFY(bytes.ok());
        writeFile(dir.filePath("c.sff"),
                  QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size())));
        writeFile(dir.filePath("c.def"), "[Info]\nname = \"C\"\nmugenversion = 1.0\n"
                                         "[Files]\ncns = c.cns\nsprite = c.sff\n");
        writeFile(dir.filePath("c.cns"), "[Data]\nlife = 1000\n");
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("c.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        auto* page = window.currentProjectView()->findChild<SpritePage*>();
        QVERIFY(page);
        page->view()->setFocus();
        SpriteFile* file = page->file();

        actions.action("edit_copy")->trigger();
        QVERIFY(QApplication::clipboard()->mimeData()->hasImage());
        page->setCurrent(1);
        actions.action("edit_paste")->trigger();
        QCOMPARE(file->document().spriteCount(), 3);
        QCOMPARE(page->current(), 2);
        QCOMPARE(file->document().sprites()[2].number(), 0);
        QCOMPARE(int(file->document().sprites()[2].header.axisX), 1);
        QCOMPARE(file->document().image(2).indexAt(0, 0), 4);
        QCOMPARE(file->document().sprites()[2].palette, file->document().palettes()[0]);
        m_context->config().setValue("confirm_delete", false);
        actions.action("edit_cut")->trigger();
        m_context->config().setValue("confirm_delete", true);
        QCOMPARE(file->document().spriteCount(), 2);
        QImage red(3, 1, QImage::Format_RGBA8888);
        red.fill(Qt::red);
        QApplication::clipboard()->setImage(red);
        page->setCurrent(0);
        actions.action("edit_paste")->trigger();
        QCOMPARE(file->document().spriteCount(), 3);
        QCOMPARE(page->current(), 1);
        QCOMPARE(file->document().sprites()[1].group(), 3);
        QCOMPARE(file->document().sprites()[1].number(), 2);
        QCOMPARE(file->document().sprites()[1].width(), 3);
        QVERIFY(!file->document().image(1).isIndexed());
        while (file->isModified())
            file->undoStack()->undo();
    }

    void animationsEditor()
    {
        QTemporaryDir dir;
        // Sprites 0,0, 0,1 and 5,0 (4x4, axis 2,4). Animations 0 "Stand" and 20 "Walk".
        SpriteDocument sprites;
        sprites.palettes() << SpriteDocument::makePalette(1, 1, Formats::grayscalePalette(256));
        const int ids[][2] = {{0, 0}, {0, 1}, {5, 0}};
        for (const auto& id : ids) {
            SpriteEntry entry;
            entry.header.group = std::uint16_t(id[0]);
            entry.header.number = std::uint16_t(id[1]);
            entry.header.axisX = 2;
            entry.header.axisY = 4;
            entry.pixels = SpriteDocument::makePixels(
                Formats::Image::indexed(4, 4, std::vector<std::uint8_t>(16, std::uint8_t(id[0] + id[1] + 3))));
            entry.palette = sprites.palettes()[0];
            sprites.sprites() << entry;
        }
        const auto bytes = sprites.toSff().write();
        QVERIFY(bytes.ok());
        writeFile(dir.filePath("a.sff"),
                  QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size())));
        writeFile(dir.filePath("a.air"), "; Stand\n[Begin Action 0]\nClsn2Default: 1\n Clsn2[0] = -2, -4, 2, 0\n"
                                         "0,0, 0,0, 4\n0,1, 0,0, 3\n\n; Walk\n[Begin Action 20]\n0,1, 0,0, 5\n"
                                         "5,0, 0,0, -1\n");
        writeFile(dir.filePath("a.def"), "[Info]\nname = \"A\"\nmugenversion = 1.0\n"
                                         "[Files]\ncns = a.cns\nsprite = a.sff\nanim = a.air\n");
        writeFile(dir.filePath("a.cns"), "[Data]\nlife = 1000\n");

        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        QVERIFY(window.openProject(dir.filePath("a.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_anim")->trigger();
        ProjectView* view = window.currentProjectView();
        QCOMPARE(view->currentEditor(), QStringLiteral("animations"));
        auto* page = view->findChild<AnimationPage*>();
        QVERIFY(page);
        QTextDocument* text = page->file()->document();
        auto line = [text](int n) {
            return text->findBlockByNumber(n).text();
        };
        auto* label = view->findChild<QLabel*>("anim_sel_lb");
        auto* frameLabel = view->findChild<QLabel*>("anim_frame_sel_lb");
        auto* time = view->findChild<QSpinBox*>("anim_frame_time");
        auto* flip = view->findChild<QComboBox*>("anim_frame_flip");
        auto* transparency = view->findChild<QComboBox*>("anim_frame_trans");
        auto* name = view->findChild<QLineEdit*>("anim_name");
        auto* loop = view->findChild<QCheckBox*>("anim_frame_loop");
        QVERIFY(label && frameLabel && time && flip && transparency && name && loop);
        QCOMPARE(label->text(), QStringLiteral("1 / 2"));
        QCOMPARE(frameLabel->text(), QStringLiteral("1 / 2 - 4 ticks"));
        QCOMPARE(page->view()->images().size(), 1);
        QCOMPARE(page->view()->boxes().size(), 1);
        QVERIFY(page->view()->boxes().first().editable);

        time->setValue(6);
        QCOMPARE(line(4), QStringLiteral("0,0, 0,0, 6"));
        QCOMPARE(frameLabel->text(), QStringLiteral("1 / 2 - 6 ticks"));
        flip->setCurrentIndex(1);
        QCOMPARE(line(4), QStringLiteral("0,0, 0,0, 6, H"));
        transparency->setCurrentIndex(2); // Addition 50%
        QCOMPARE(line(4), QStringLiteral("0,0, 0,0, 6, H, A1"));
        name->setFocus();
        name->end(false);
        QTest::keyClicks(name, "X");
        QCOMPARE(line(0), QStringLiteral("; StandX"));
        page->view()->setFocus();
        actions.action("edit_undo")->trigger();
        QCOMPARE(line(0), QStringLiteral("; Stand"));
        actions.action("anim_next_frame")->trigger();
        QCOMPARE(page->currentFrame(), 1);
        loop->setChecked(true);
        QCOMPARE(line(5), QStringLiteral("LoopStart"));
        QCOMPARE(line(6), QStringLiteral("0,1, 0,0, 3"));

        actions.action("anim_next_anim")->trigger();
        QCOMPARE(page->current(), 1);
        QCOMPARE(page->currentFrame(), 0);
        actions.action("anim_prev_anim")->trigger();
        QCOMPARE(page->currentFrame(), 1);
        actions.action("anim_frame_add")->trigger();
        QCOMPARE(page->currentFrame(), 2);
        QCOMPARE(line(7), QStringLiteral("-1,0, 0,0, 10"));

        QVERIFY(QTest::qWaitForWindowExposed(&window));
        actions.action("anim_box_add_attack")->trigger();
        QVERIFY(actions.action("anim_box_add_attack")->isChecked());
        GraphicsView* canvas = page->view();
        canvas->centerOrigin();
        const QPoint from = canvas->toWidget(QPointF(-5.5, -9.5)).toPoint();
        const QPoint to = canvas->toWidget(QPointF(3.5, -2.5)).toPoint();
        QTest::mousePress(canvas, Qt::LeftButton, {}, from);
        QTest::mouseMove(canvas, (from + to) / 2);
        QTest::mouseMove(canvas, to);
        QTest::mouseRelease(canvas, Qt::LeftButton, {}, to);
        QCOMPARE(line(7), QStringLiteral("Clsn1: 1"));
        QCOMPARE(line(8), QStringLiteral("  Clsn1[0] = -6, -10, 3, -3"));
        QCOMPARE(line(9), QStringLiteral("-1,0, 0,0, 10"));
        QVERIFY(page->selectedBox().isValid());
        QTest::mouseClick(canvas, Qt::RightButton, {}, to);
        QVERIFY(!actions.action("anim_box_add_attack")->isChecked());
        actions.action("anim_box_type")->trigger();
        QCOMPARE(line(7), QStringLiteral("Clsn2: 1"));
        QVERIFY(page->selectedBox().isValid());

        actions.action("anim_editcode")->trigger();
        QVERIFY(page->textMode());
        QVERIFY(actions.action("anim_editcode")->isChecked());
        QVERIFY(!actions.action("anim_box_add_attack")->isEnabled());
        QCOMPARE(view->currentCodeEditor(), page->codePage()->editor());
        QTRY_VERIFY(window.ofsStatusBar()->section(0).startsWith(QStringLiteral("Line: ")));
        page->codePage()->editor()->goToLine(15); // "5,0, 0,0, -1"
        QCOMPARE(page->current(), 1);
        QCOMPARE(page->currentFrame(), 1);
        QCOMPARE(frameLabel->text(), QStringLiteral("2 / 2 - -1 ticks"));
        actions.action("anim_editcode")->trigger();
        QVERIFY(!page->textMode());

        actions.action("editor_spr")->trigger();
        auto* spritePage = view->findChild<SpritePage*>();
        QVERIFY(spritePage);
        QCOMPARE(spritePage->current(), 2); // 5,0
        actions.action("editor_anim")->trigger();

        QTimer::singleShot(0, this, [] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->objectName(), QStringLiteral("AddAnim"));
            dialog->findChild<QLineEdit*>("name")->setText("Jump");
            dialog->findChild<QSpinBox*>("number")->setValue(40);
            dialog->findChild<QSpinBox*>("time")->setValue(3);
            dialog->accept();
        });
        actions.action("anim_add")->trigger();
        QCOMPARE(page->document().count(), 3);
        QCOMPARE(page->current(), 2);
        QCOMPARE(page->animation()->number, 40);
        QCOMPARE(page->animation()->name, QStringLiteral("Jump"));
        QCOMPARE(page->animation()->frames.first().time, 3);
        // showmsg_anim_clone 2 answers No, so the copy keeps number 40.
        m_context->config().setValue("showmsg_anim_clone", "2");
        actions.action("anim_duplicate")->trigger();
        QCOMPARE(page->document().count(), 4);
        QCOMPARE(page->document().animations().at(3).number, 40);
        const QString airPath = QDir::fromNativeSeparators(dir.filePath("a.air"));
        auto reportsTwice = [view, airPath] {
            const IssueList issues = view->issues().value(airPath);
            return std::any_of(issues.cbegin(), issues.cend(), [](const Issue& issue) {
                return issue.message == QStringLiteral("Animation 40 is already defined at line 18");
            });
        };
        QTRY_VERIFY(reportsTwice());
        m_context->config().setValue("showmsg_anim_clone", QString());
        m_context->config().setValue("confirm_delete", false);
        actions.action("anim_delete")->trigger();
        m_context->config().setValue("confirm_delete", true);
        QCOMPARE(page->document().count(), 3);

        page->setCurrent(0);
        actions.action("anim_play")->trigger();
        QVERIFY(page->isPlaying());
        QVERIFY(actions.action("anim_play")->isChecked());
        QVERIFY(!actions.action("anim_frame_delete")->isEnabled());
        QVERIFY(!time->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(page->currentFrame() > 0, 2000);
        actions.action("anim_stop")->trigger();
        QVERIFY(!page->isPlaying());
        QVERIFY(actions.action("anim_frame_delete")->isEnabled());

        FileDialogs::setSavePromptOverride([&dir](const QString& caption, const QString&, const QString&) {
            return caption == QLatin1String("Save as animated GIF...") ? dir.filePath("walk.gif") : QString();
        });
        QTimer::singleShot(0, this, [] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->objectName(), QStringLiteral("GIFDialog"));
            dialog->accept();
        });
        actions.action("anim_save_gif")->trigger();
        FileDialogs::setSavePromptOverride({});
        QFile gif(dir.filePath("walk.gif"));
        QVERIFY(gif.open(QIODevice::ReadOnly));
        QCOMPARE(gif.read(6), QByteArray("GIF89a"));
        gif.close();

        QString asked;
        QTimer::singleShot(0, this, [&asked] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            QVERIFY(box);
            asked = box->windowTitle() + "|" + box->text();
            box->accept();
        });
        actions.action("anim_optimize")->trigger();
        QCOMPARE(asked, boxTitle("Optimize done!") + "|Code clean, and 0 line(s) were removed in the optimization!");

        text->setModified(false);
        page->file()->document()->clearUndoRedoStacks();
    }

    void animationsFilesAndExplorer()
    {
        QTemporaryDir dir;
        // Sprites 0,0, 0,1 and 3,0. One animation uses 0,0.
        SpriteDocument sprites;
        sprites.palettes() << SpriteDocument::makePalette(1, 1, Formats::grayscalePalette(256));
        const int ids[][2] = {{0, 0}, {0, 1}, {3, 0}};
        for (const auto& id : ids) {
            SpriteEntry entry;
            entry.header.group = std::uint16_t(id[0]);
            entry.header.number = std::uint16_t(id[1]);
            entry.pixels = SpriteDocument::makePixels(Formats::Image::indexed(2, 2, std::vector<std::uint8_t>(4, 5)));
            entry.palette = sprites.palettes()[0];
            sprites.sprites() << entry;
        }
        const auto bytes = sprites.toSff().write();
        QVERIFY(bytes.ok());
        writeFile(dir.filePath("e.sff"),
                  QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size())));
        writeFile(dir.filePath("e.air"), "[Begin Action 0]\n0,0, 0,0, 5\n");
        writeFile(dir.filePath("other.air"), "[Begin Action 7]\n3,0, 0,0, 2\n[Begin Action 8]\n3,0, 0,0, 2\n");
        writeFile(dir.filePath("e.def"), "[Info]\nname = \"E\"\nmugenversion = 1.0\n"
                                         "[Files]\ncns = e.cns\nsprite = e.sff\nanim = e.air\n");
        writeFile(dir.filePath("e.cns"), "[Data]\nlife = 1000\n");
        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        QVERIFY(window.openProject(dir.filePath("e.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_anim")->trigger();
        ProjectView* view = window.currentProjectView();
        auto* tabs = view->findChild<QTabWidget*>("fileTabs_animations");
        QVERIFY(tabs);
        auto* page = qobject_cast<AnimationPage*>(tabs->currentWidget());
        QVERIFY(page);

        auto* right = view->findChild<EPanelWidget*>("rightPanel");
        QCOMPARE(right->title(), QStringLiteral("Sprite explorer"));
        auto* thumbnails = view->findChild<QListView*>("thumb_view");
        auto* addSelected = view->findChild<QToolButton*>("thumb_action1");
        QVERIFY(thumbnails && addSelected);
        QCOMPARE(addSelected->text(), QStringLiteral("Add selected to the current animation"));
        QTRY_COMPARE(thumbnails->model()->rowCount(), 2);
        thumbnails->selectionModel()->select(thumbnails->model()->index(1, 0), QItemSelectionModel::ClearAndSelect);
        addSelected->click();
        QCOMPARE(page->animation()->frames.size(), 2);
        QCOMPARE(page->animation()->frames.at(1).number, 1);
        QCOMPARE(page->animation()->frames.at(1).time, 10);
        QCOMPARE(page->currentFrame(), 1);
        QTimer::singleShot(0, this, [] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->findChild<QSpinBox*>("number")->setValue(9);
            dialog->findChild<QSpinBox*>("time")->setValue(4);
            dialog->accept();
        });
        view->findChild<QToolButton*>("thumb_action4")->click();
        QCOMPARE(page->document().count(), 2);
        QCOMPARE(page->animation()->number, 9);
        QCOMPARE(page->animation()->frames.size(), 2);
        QCOMPARE(page->animation()->frames.at(0).time, 4);

        actions.action("anim_new")->trigger();
        QCOMPARE(tabs->count(), 2);
        QCOMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("untitled1.air"));
        auto* fresh = qobject_cast<AnimationPage*>(tabs->currentWidget());
        QVERIFY(fresh);
        QCOMPARE(fresh->document().count(), 0);
        QVERIFY(fresh->file()->isUntitled());
        FileDialogs::setOpenPromptOverride([&dir](const QString&, const QString&, const QString& filter) {
            return filter.startsWith("Animations (*.air)") ? dir.filePath("other.air") : QString();
        });
        actions.action("anim_open")->trigger();
        FileDialogs::setOpenPromptOverride({});
        QCOMPARE(tabs->count(), 3);
        auto* opened = qobject_cast<AnimationPage*>(tabs->currentWidget());
        QVERIFY(opened);
        QCOMPARE(opened->document().count(), 2);

        for (int i = 0; i < tabs->count(); ++i) {
            if (auto* p = qobject_cast<AnimationPage*>(tabs->widget(i)))
                p->file()->document()->setModified(false);
        }
    }

    void soundsEditor()
    {
        QTemporaryDir dir;
        // Sounds 0,0 (0.2 seconds), 0,1 (1000 frames) and 5,2 (2 seconds), mono 16-bit at 11025 Hz.
        auto wav = [](std::size_t frames, std::int16_t value) {
            auto bytes = Formats::encodeAudio(
                Formats::SampleBuffer::fromInt16(std::vector<std::int16_t>(frames, value), 1, 11025));
            return bytes ? bytes.value() : Formats::Bytes();
        };
        auto toBytes = [](const Formats::Bytes& bytes) {
            return QByteArray(reinterpret_cast<const char*>(bytes.data()), qsizetype(bytes.size()));
        };
        Formats::SndDocument snd;
        snd.sounds.push_back(Formats::SndSound{0, 0, wav(2205, 1000)});
        snd.sounds.push_back(Formats::SndSound{0, 1, wav(1000, -2000)});
        snd.sounds.push_back(Formats::SndSound{5, 2, wav(22050, 500)});
        writeFile(dir.filePath("s.snd"), toBytes(snd.write()));
        writeFile(dir.filePath("s.def"), "[Info]\nname = \"S\"\nmugenversion = 1.0\n"
                                         "[Files]\ncns = s.cns\nsound = s.snd\n");
        writeFile(dir.filePath("s.cns"), "[Data]\nlife = 1000\n");

        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        QVERIFY(window.openProject(dir.filePath("s.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_snd")->trigger();
        ProjectView* view = window.currentProjectView();
        QCOMPARE(view->currentEditor(), QStringLiteral("sounds"));
        auto* page = view->findChild<SoundPage*>();
        QVERIFY(page);
        SoundFile* file = page->file();
        auto* label = view->findChild<QLabel*>("snd_sel_lb");
        auto* group = view->findChild<QSpinBox*>("snd_grp");
        auto* number = view->findChild<QSpinBox*>("snd_num");
        auto* info = view->findChild<QLabel*>("snd_info_lb");
        auto* start = view->findChild<QSpinBox*>("snd_sel_start");
        auto* end = view->findChild<QSpinBox*>("snd_sel_end");
        auto* volume = view->findChild<QSlider*>("snd_volume");
        QVERIFY(label && group && number && info && start && end && volume);
        StatusBar* status = window.ofsStatusBar();
        QCOMPARE(label->text(), QStringLiteral("1 / 3"));
        QCOMPARE(info->text(), QStringLiteral("Channels: 1 (Mono)\nSample rate: 11025 Hz\nBits per sample: 16 bits\n"
                                              "Duration: 0.2 seconds\nTick count: 12 at 60Hz"));
        QCOMPARE(volume->value(), 55);
        QCOMPARE(status->section(0), QStringLiteral("Sample: 0/2205, Selection: 0/0"));
        QCOMPARE(status->section(1), QStringLiteral("Time: 0.000/0.200s, Selection: 0.000/0.000s"));
        QCOMPARE(status->section(2), QStringLiteral("Status: Stopped"));
        QVERIFY(actions.action("snd_stop")->isChecked());
        QVERIFY(actions.action("snd_tool_select")->isChecked());

        actions.action("snd_next")->trigger();
        QCOMPARE(page->current(), 1);
        QCOMPARE(label->text(), QStringLiteral("2 / 3"));
        actions.action("snd_next_grp")->trigger();
        QCOMPARE(page->current(), 2);
        QCOMPARE(group->value(), 5);
        QCOMPARE(number->value(), 2);
        actions.action("snd_next")->trigger();
        QCOMPARE(page->current(), 2);
        actions.action("snd_prev_grp")->trigger();
        QCOMPARE(page->current(), 0);
        actions.action("snd_prev")->trigger();
        QCOMPARE(page->current(), 0);

        number->setValue(3);
        QCOMPARE(file->document().sounds().at(0).number, 3);
        QVERIFY(file->isModified());
        actions.action("edit_undo")->trigger();
        QCOMPARE(file->document().sounds().at(0).number, 0);
        QCOMPARE(number->value(), 0);
        QVERIFY(!file->isModified());

        actions.action("snd_select_all")->trigger();
        QCOMPARE(end->value(), 2205);
        QCOMPARE(status->section(0), QStringLiteral("Sample: 0/2205, Selection: 0/2205"));
        start->setValue(100);
        end->setValue(300);
        QCOMPARE(page->view()->selection().start, std::size_t(100));
        QCOMPARE(page->view()->selection().count, std::size_t(200));
        QCOMPARE(status->section(1), QStringLiteral("Time: 0.000/0.200s, Selection: 0.009/0.027s"));

        actions.action("snd_remove_selected")->trigger();
        QVERIFY(page->hasChanges());
        QCOMPARE(page->samples().frameCount(), std::size_t(2005));
        QVERIFY(info->text().contains(QStringLiteral("Duration: 0.181859 seconds")));
        QVERIFY(!file->isModified());
        actions.action("snd_discard_changes")->trigger();
        QVERIFY(!page->hasChanges());
        QCOMPARE(page->samples().frameCount(), std::size_t(2205));
        end->setValue(205);
        actions.action("snd_remove_selected")->trigger();
        QCOMPARE(page->samples().frameCount(), std::size_t(2000));
        answerDiscard(QMessageBox::No);
        actions.action("snd_next")->trigger();
        QCOMPARE(page->current(), 0);
        QVERIFY(page->hasChanges());
        answerDiscard(QMessageBox::No);
        actions.action("editor_def")->trigger();
        QCOMPARE(view->currentEditor(), QStringLiteral("sounds"));
        QVERIFY(actions.action("editor_snd")->isChecked());
        actions.action("snd_save_changes")->trigger();
        QVERIFY(!page->hasChanges());
        QVERIFY(file->isModified());
        QCOMPARE(SoundEdits::decode(file->document().sounds().at(0)).value().buffer.frameCount(), std::size_t(2000));
        actions.action("edit_undo")->trigger();
        QCOMPARE(page->samples().frameCount(), std::size_t(2205));
        actions.action("edit_redo")->trigger();
        QCOMPARE(page->samples().frameCount(), std::size_t(2000));

        actions.action("snd_next")->trigger();
        QCOMPARE(page->current(), 1);
        page->view()->setFocus();
        end->setValue(10);
        actions.action("edit_copy")->trigger();
        actions.action("snd_select_none")->trigger();
        actions.action("edit_paste")->trigger();
        QCOMPARE(page->samples().frameCount(), std::size_t(1010));
        QCOMPARE(page->view()->selection().count, std::size_t(10));
        actions.action("edit_cut")->trigger();
        QCOMPARE(page->samples().frameCount(), std::size_t(1000));
        end->setValue(100);
        actions.action("snd_fill_silence")->trigger();
        QCOMPARE(page->samples().sample(50, 0), 0.0f);
        QVERIFY(page->samples().sample(150, 0) < 0.0f);
        actions.action("snd_discard_changes")->trigger();
        end->setValue(100);
        actions.action("snd_fade_in")->trigger();
        QVERIFY(std::abs(page->samples().sample(0, 0)) < std::abs(page->samples().sample(99, 0)));
        actions.action("snd_discard_changes")->trigger();

        actions.action("snd_next")->trigger();
        QCOMPARE(page->current(), 2);
        page->view()->setCursor(1000);
        actions.action("snd_play")->trigger();
        QCOMPARE(page->state(), SoundPage::State::Playing);
        QVERIFY(actions.action("snd_play")->isChecked());
        QCOMPARE(status->section(2), QStringLiteral("Status: Playing"));
        actions.action("snd_pause")->trigger();
        QCOMPARE(page->state(), SoundPage::State::Paused);
        QVERIFY(actions.action("snd_pause")->isChecked());
        QCOMPARE(status->section(2), QStringLiteral("Status: Paused"));
        actions.action("snd_play")->trigger();
        QCOMPARE(page->state(), SoundPage::State::Playing);
        actions.action("snd_stop")->trigger();
        QCOMPARE(page->state(), SoundPage::State::Stopped);
        QVERIFY(actions.action("snd_stop")->isChecked());
        QCOMPARE(page->view()->cursor(), 0);
        actions.action("snd_play_loop")->trigger();
        QVERIFY(actions.action("snd_play_loop")->isChecked());
        actions.action("snd_stop")->trigger();
        actions.action("snd_prev")->trigger();
        page->view()->setCursor(500);
        actions.action("snd_play")->trigger();
        QTRY_COMPARE(page->state(), SoundPage::State::Stopped);
        QCOMPARE(page->view()->cursor(), 0);
        QVERIFY(actions.action("snd_stop")->isChecked());

        actions.action("snd_tool_zoom")->trigger();
        QCOMPARE(page->view()->tool(), WaveformView::Tool::Zoom);
        QVERIFY(!actions.action("snd_tool_select")->isChecked());
        actions.action("snd_tool_select")->trigger();
        QCOMPARE(page->view()->tool(), WaveformView::Tool::Select);

        auto* issues = window.findChild<QTableView*>("issuesView");
        QVERIFY(issues);
        auto issueMessages = [issues] {
            QStringList messages;
            for (int row = 0; row < issues->model()->rowCount(); ++row)
                messages << issues->model()->index(row, 0).data().toString();
            return messages;
        };
        const QString duplicateIssue = QStringLiteral("The group 0, index 1 is already in use by sound #2!");
        actions.action("snd_duplicate")->trigger();
        QCOMPARE(file->document().count(), 4);
        QCOMPARE(page->current(), 2);
        QTRY_VERIFY(issueMessages().contains(duplicateIssue));
        number->setValue(7);
        QTRY_VERIFY(!issueMessages().contains(duplicateIssue));
        m_context->config().setValue(QStringLiteral("confirm_delete"), false);
        actions.action("snd_delete")->trigger();
        QCOMPARE(file->document().count(), 3);

        writeFile(dir.filePath("9-4.wav"), toBytes(wav(300, 100)));
        FileDialogs::setOpenPromptOverride(
            [&dir](const QString&, const QString&, const QString&) { return dir.filePath("9-4.wav"); });
        QTimer::singleShot(0, this, [] {
            auto* add = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(add);
            QCOMPARE(add->objectName(), QStringLiteral("AddSom"));
            add->findChild<QCheckBox*>("detect")->setChecked(true);
            add->accept();
        });
        actions.action("snd_add")->trigger();
        QCOMPARE(file->document().count(), 4);
        QCOMPARE(group->value(), 9);
        QCOMPARE(number->value(), 4);

        QTimer::singleShot(0, this, [] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->objectName(), QStringLiteral("ResampleDlg"));
            auto* rate = dialog->findChild<QComboBox*>("rate");
            QCOMPARE(rate->currentData().toInt(), 11025);
            rate->setCurrentIndex(rate->findData(22050));
            dialog->accept();
        });
        actions.action("snd_resample")->trigger();
        QCOMPARE(page->samples().sampleRate(), 22050u);
        QVERIFY(std::abs(int(page->samples().frameCount()) - 600) <= 2);

        QVERIFY(QDir(dir.path()).mkdir("out"));
        FileDialogs::setSavePromptOverride(
            [&dir](const QString&, const QString&, const QString&) { return dir.filePath("out/list.ffe"); });
        QTimer::singleShot(0, this, [] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->objectName(), QStringLiteral("ExpDlg"));
            dialog->accept();
        });
        actions.action("snd_export")->trigger();
        QVERIFY(QFile::exists(dir.filePath("out/list.ffe")));
        QVERIFY(QFile::exists(dir.filePath("out/9-4.wav")));
        actions.action("snd_delete")->trigger();
        QCOMPARE(file->document().count(), 3);
        FileDialogs::setOpenPromptOverride(
            [&dir](const QString&, const QString&, const QString&) { return dir.filePath("out/list.ffe"); });
        QTimer::singleShot(0, this, [] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            QVERIFY(box);
            QCOMPARE(box->windowTitle(), boxTitle("Conflict!"));
            box->button(QMessageBox::NoToAll)->click();
        });
        actions.action("snd_import")->trigger();
        FileDialogs::setOpenPromptOverride({});
        QCOMPARE(file->document().count(), 4);
        QCOMPARE(group->value(), 9);
        QCOMPARE(page->samples().sampleRate(), 22050u);

        QTemporaryDir outside;
        FileDialogs::setSavePromptOverride(
            [&outside](const QString&, const QString&, const QString&) { return outside.filePath("copy.snd"); });
        QTimer::singleShot(0, this, [] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            QVERIFY(box);
            QCOMPARE(box->windowTitle(), boxTitle("External file!"));
            box->button(QMessageBox::Yes)->click();
        });
        actions.action("editor_saveas")->trigger();
        FileDialogs::setSavePromptOverride({});
        QVERIFY(QFile::exists(outside.filePath("copy.snd")));
        QCOMPARE(file->path(), dir.filePath("s.snd"));
        QVERIFY(file->isModified());

        // Save runs in the background.
        actions.action("file_save")->trigger();
        QTRY_VERIFY(!file->isModified());
        SoundFile reread(dir.filePath("s.snd"), "sounds");
        QString error;
        QVERIFY2(reread.load(&error), qPrintable(error));
        QCOMPARE(reread.document().count(), 4);
        QCOMPARE(reread.document().sounds().at(3).group, 9);
    }

    void palettesPanel()
    {
        QTemporaryDir dir;
        // Palettes 1,1 (slot i = i,0,0) and 1,2 (slot i = 0,i,0). Sprites 0,0 and 0,1 use 1,1 and 5,0
        // uses 1,2. Pixel (x, y) of every sprite is palette index x + 4y.
        auto ramp = [](int channel) {
            Formats::Palette colors(256);
            for (int i = 0; i < 256; ++i) {
                std::uint8_t c[3] = {0, 0, 0};
                c[channel] = std::uint8_t(i);
                colors[std::size_t(i)] = Formats::Rgba{c[0], c[1], c[2], std::uint8_t(i == 0 ? 0 : 255)};
            }
            return colors;
        };
        SpriteDocument sprites;
        sprites.palettes() << SpriteDocument::makePalette(1, 1, ramp(0)) << SpriteDocument::makePalette(1, 2, ramp(1));
        std::vector<std::uint8_t> pixels(16);
        for (int i = 0; i < 16; ++i)
            pixels[std::size_t(i)] = std::uint8_t(i);
        const int ids[][3] = {{0, 0, 0}, {0, 1, 0}, {5, 0, 1}};
        for (const auto& id : ids) {
            SpriteEntry entry;
            entry.header.group = std::uint16_t(id[0]);
            entry.header.number = std::uint16_t(id[1]);
            entry.pixels = SpriteDocument::makePixels(Formats::Image::indexed(4, 4, pixels));
            entry.palette = sprites.palettes()[id[2]];
            sprites.sprites() << entry;
        }
        const auto bytes = sprites.toSff().write();
        QVERIFY(bytes.ok());
        writeFile(dir.filePath("p.sff"),
                  QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size())));
        writeFile(dir.filePath("p.def"), "[Info]\nname = \"P\"\nmugenversion = 1.0\n"
                                         "[Files]\ncns = p.cns\nsprite = p.sff\n");
        writeFile(dir.filePath("p.cns"), "[Data]\nlife = 1000\n");

        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        QVERIFY(window.openProject(dir.filePath("p.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        ProjectView* view = window.currentProjectView();
        auto* page = view->findChild<SpritePage*>();
        QVERIFY(page);
        SpriteFile* file = page->file();
        auto* label = view->findChild<QLabel*>("pal_sel_lb");
        auto* group = view->findChild<QSpinBox*>("pal_grp");
        auto* number = view->findChild<QSpinBox*>("pal_num");
        auto* grid = view->findChild<PaletteWidget*>("pal_view");
        auto* follow = view->findChild<QCheckBox*>("spr_switch_pal");
        QVERIFY(label && group && number && grid && follow);
        auto colors = [file](int palette) {
            return file->document().palettes()[palette]->palette.colors;
        };
        // The color the current sprite shows at pixel (x, 0).
        auto shown = [page](int x) {
            return page->view()->images().last().image.pixelColor(x, 0);
        };
        auto modal = [] {
            return QApplication::activeModalWidget();
        };
        auto answerColor = [this, modal](QColor color) {
            QTimer::singleShot(0, this, [modal, color] {
                auto* dialog = qobject_cast<QColorDialog*>(modal());
                QVERIFY(dialog);
                QCOMPARE(dialog->windowTitle(), QStringLiteral("Color"));
                QVERIFY(dialog->testOption(QColorDialog::ShowAlphaChannel));
                dialog->setCurrentColor(color);
                dialog->accept();
            });
        };
        auto answerBox = [this, modal](const QString& title, QMessageBox::StandardButton button) {
            QTimer::singleShot(0, this, [modal, title, button] {
                auto* box = qobject_cast<QMessageBox*>(modal());
                QVERIFY(box);
                QCOMPARE(box->windowTitle(), boxTitle(title));
                box->button(button)->click();
            });
        };
        QCOMPARE(label->text(), QStringLiteral("1 / 2"));
        QCOMPARE(group->value(), 1);
        QCOMPARE(number->value(), 1);
        QCOMPARE(grid->colors(), ramp(0));
        QVERIFY(follow->isChecked());
        QCOMPARE(shown(1), QColor(1, 0, 0));

        actions.action("spr_next")->trigger();
        actions.action("spr_next")->trigger();
        QCOMPARE(label->text(), QStringLiteral("2 / 2"));
        actions.action("spr_prev")->trigger();
        QCOMPARE(label->text(), QStringLiteral("1 / 2"));
        follow->setChecked(false);
        actions.action("pal_next")->trigger();
        QCOMPARE(label->text(), QStringLiteral("2 / 2"));
        QCOMPARE(shown(1), QColor(0, 1, 0));
        actions.action("pal_next")->trigger();
        QCOMPARE(label->text(), QStringLiteral("2 / 2"));
        follow->setChecked(true);
        QCOMPARE(label->text(), QStringLiteral("1 / 2"));
        QCOMPARE(shown(1), QColor(1, 0, 0));

        auto* issues = window.findChild<QTableView*>("issuesView");
        auto issueMessages = [issues] {
            QStringList messages;
            for (int row = 0; row < issues->model()->rowCount(); ++row)
                messages << issues->model()->index(row, 0).data().toString();
            return messages;
        };
        number->setValue(2);
        QCOMPARE(file->document().palettes()[0]->palette.number, std::uint16_t(2));
        QTRY_VERIFY(issueMessages().contains(QStringLiteral("The group 1, index 2 is already in use by palette #1!")));
        actions.action("edit_undo")->trigger();
        QCOMPARE(number->value(), 1);
        QTRY_VERIFY(issueMessages().isEmpty());

        answerBox("Add palette", QMessageBox::No);
        actions.action("pal_add")->trigger();
        QCOMPARE(file->document().palettes().size(), 3);
        QCOMPARE(label->text(), QStringLiteral("2 / 3"));
        QCOMPARE(group->value(), 0);
        QCOMPARE(grid->slotCount(), 32);
        QCOMPARE(grid->colors().at(5), (Formats::Rgba{0, 0, 0, 0}));
        actions.action("edit_undo")->trigger();
        QCOMPARE(file->document().palettes().size(), 2);
        QCOMPARE(label->text(), QStringLiteral("2 / 2"));
        actions.action("pal_prev")->trigger();

        grid->setSelection({1, 5});
        actions.action("pal_switch_color")->trigger();
        QCOMPARE(colors(0)[1], ramp(0)[5]);
        QCOMPARE(colors(0)[5], ramp(0)[1]);
        QCOMPARE(grid->selection(), (QList<int>{1, 5}));
        QCOMPARE(shown(1), QColor(5, 0, 0));
        actions.action("pal_undo")->trigger();
        QCOMPARE(colors(0), ramp(0));
        actions.action("pal_redo")->trigger();
        QCOMPARE(colors(0)[1], ramp(0)[5]);
        actions.action("edit_undo")->trigger();
        QCOMPARE(colors(0), ramp(0));
        QVERIFY(grid->selection().isEmpty());

        grid->setSelection({2, 3});
        answerColor(QColor(200, 100, 0, 128));
        actions.action("pal_set_color")->trigger();
        QCOMPARE(colors(0)[2], (Formats::Rgba{200, 100, 0, 128}));
        QCOMPARE(colors(0)[3], (Formats::Rgba{200, 100, 0, 128}));
        QCOMPARE(shown(2).red(), 200);
        answerColor(QColor(1, 2, 3));
        QTest::mouseDClick(grid, Qt::LeftButton, {}, grid->cellRect(4).center());
        QCOMPARE(colors(0)[4], (Formats::Rgba{1, 2, 3, 255}));
        // Dragging slot 4 to 9 moves its color there and shifts the slots between down by one.
        Q_EMIT grid->slotMoved(4, 9);
        QCOMPARE(colors(0)[9], (Formats::Rgba{1, 2, 3, 255}));
        QCOMPARE(colors(0)[4], ramp(0)[5]);
        actions.action("pal_undo")->trigger();
        QCOMPARE(colors(0)[4], (Formats::Rgba{1, 2, 3, 255}));
        actions.action("pal_undo")->trigger();
        actions.action("pal_undo")->trigger();
        QCOMPARE(colors(0), ramp(0));

        // Change... loads an ACT file, which is read in reverse.
        QByteArray act;
        for (int i = 0; i < 256; ++i) {
            act += char(0);
            act += char(0);
            act += char(i);
        }
        writeFile(dir.filePath("blue.act"), act);
        FileDialogs::setOpenPromptOverride([&dir](const QString& caption, const QString& folder, const QString&) {
            [&] {
                QCOMPARE(caption, QStringLiteral("Open palette"));
                QCOMPARE(QDir(folder), QDir(dir.path()));
            }();
            return dir.filePath("blue.act");
        });
        actions.action("pal_open")->trigger();
        FileDialogs::setOpenPromptOverride({});
        QCOMPARE(colors(0)[255], (Formats::Rgba{0, 0, 0, 255}));
        QCOMPARE(colors(0)[1], (Formats::Rgba{0, 0, 254, 255}));
        QCOMPARE(colors(0)[0].a, std::uint8_t(0));
        actions.action("edit_undo")->trigger();

        QTimer::singleShot(0, this, [modal] {
            QWidget* items = modal();
            QVERIFY(items);
            QCOMPARE(items->objectName(), QStringLiteral("ItemsDlg"));
            QCOMPARE(items->findChild<QListWidget*>("list")->item(1)->text(), QStringLiteral("1, 2"));
            items->findChild<QPushButton*>("allBut")->click();
            qobject_cast<QDialog*>(items)->accept();
        });
        FileDialogs::setSavePromptOverride([&dir](const QString& caption, const QString&, const QString& filter) {
            [&] {
                QCOMPARE(caption, QStringLiteral("Save palette"));
                QCOMPARE(filter, QStringLiteral("Adobe Color Table (*.act)"));
            }();
            return dir.filePath("multi.act");
        });
        actions.action("pal_save")->trigger();
        FileDialogs::setSavePromptOverride({});
        QVERIFY(QFile::exists(dir.filePath("multi_1-1.act")));
        QVERIFY(QFile::exists(dir.filePath("multi_1-2.act")));

        actions.action("pal_switch_count")->trigger();
        QCOMPARE(grid->slotCount(), 32);
        actions.action("pal_switch_count")->trigger();
        QCOMPARE(colors(0), ramp(0));

        answerBox("Delete", QMessageBox::Yes);
        actions.action("pal_delete")->trigger();
        QCOMPARE(file->document().palettes().size(), 1);
        QCOMPARE(label->text(), QStringLiteral("1 / 1"));
        QCOMPARE(file->document().sprites()[0].palette, file->document().palettes()[0]);
        QCOMPARE(shown(1), QColor(0, 1, 0));
        actions.action("edit_undo")->trigger();
        QCOMPARE(file->document().palettes().size(), 2);

        actions.action("pal_next")->trigger();
        FileDialogs::setOpenPromptOverride(
            [&dir](const QString&, const QString&, const QString&) { return dir.filePath("blue.act"); });
        actions.action("pal_open")->trigger();
        actions.action("pal_prev")->trigger();
        actions.action("pal_open")->trigger();
        FileDialogs::setOpenPromptOverride({});
        QCOMPARE(colors(0), colors(1));
        actions.action("pal_delete_duplicates")->trigger();
        QCOMPARE(file->document().palettes().size(), 1);
        QCOMPARE(file->document().sprites()[2].palette, file->document().palettes()[0]);
        for (int i = 0; i < 3; ++i)
            actions.action("edit_undo")->trigger();
        QCOMPARE(colors(1), ramp(1));

        // Clear all unused keeps the character palettes (group 1).
        answerBox("Add palette", QMessageBox::No);
        actions.action("pal_add")->trigger();
        number->setValue(9);
        QCOMPARE(file->document().palettes().size(), 3);
        actions.action("pal_clear_unused")->trigger();
        QCOMPARE(file->document().palettes().size(), 2);

        m_context->config().setValue(QStringLiteral("confirm_delete"), true);
        answerBox("Clear", QMessageBox::Yes);
        actions.action("pal_clear")->trigger();
        QVERIFY(file->document().palettes().isEmpty());
        QCOMPARE(label->text(), QStringLiteral("0 / 0"));
        QCOMPARE(grid->slotCount(), 256);
        QVERIFY(!group->isEnabled());
        QVERIFY(!actions.action("pal_switch_count")->isEnabled());
        QTRY_VERIFY(issueMessages().contains(QStringLiteral("Indexed sprite without palette!")));
        actions.action("edit_undo")->trigger();
        QCOMPARE(label->text(), QStringLiteral("1 / 2"));
        QVERIFY(actions.action("pal_switch_count")->isEnabled());
        m_context->config().setValue(QStringLiteral("confirm_delete"), false);

        actions.action("pal_remap")->trigger();
        auto* remap = window.findChild<RemapDialog*>("RemapDlg");
        QVERIFY(remap && remap->isVisible() && !remap->isModal());
        QCOMPARE(remap->table()->model()->rowCount(), 1);
        remap->table()->model()->setData(remap->table()->model()->index(0, 3), 2);
        QCOMPARE(shown(1), QColor(0, 1, 0));
        QCOMPARE(label->text(), QStringLiteral("1 / 2"));
        remap->findChild<QPushButton*>("pushButton_3")->click(); // Clear
        QCOMPARE(shown(1), QColor(1, 0, 0));
        remap->findChild<QPushButton*>("pushButton")->click();
        QCOMPARE(remap->rows().size(), 1);
        remap->close();

        grid->setSelection({7});
        actions.action("pal_pick_color")->trigger();
        QVERIFY(page->colorPicker());
        Q_EMIT page->view()->pressed(QPointF(1.5, 2.5)); // pixel (1, 2): index 9
        Q_EMIT page->view()->pressed(QPointF(3.2, 0.1)); // index 3
        QCOMPARE(grid->selection(), (QList<int>{3, 7, 9}));
        actions.action("pal_pick_color")->trigger();
        QVERIFY(!page->colorPicker());

        QTimer::singleShot(0, this, [modal] {
            QWidget* items = modal();
            QVERIFY(items);
            auto* list = items->findChild<QListWidget*>("list");
            list->clearSelection();
            list->item(2)->setSelected(true); // 5,0
            qobject_cast<QDialog*>(items)->accept();
        });
        actions.action("pal_apply_sprite")->trigger();
        QCOMPARE(file->document().sprites()[2].palette, file->document().palettes()[0]);
    }

    void advancedPaletteEditor()
    {
        QTemporaryDir dir;
        // Palettes 1,1 (slot i = i,0,0) and 1,2 (slot i = 0,i,0). Sprite 0,0 uses 1,1 and 5,0 uses
        // 1,2. Pixel (x, y) is palette index x + 4y.
        auto ramp = [](int channel) {
            Formats::Palette colors(256);
            for (int i = 0; i < 256; ++i) {
                std::uint8_t c[3] = {0, 0, 0};
                c[channel] = std::uint8_t(i);
                colors[std::size_t(i)] = Formats::Rgba{c[0], c[1], c[2], std::uint8_t(i == 0 ? 0 : 255)};
            }
            return colors;
        };
        SpriteDocument sprites;
        sprites.palettes() << SpriteDocument::makePalette(1, 1, ramp(0)) << SpriteDocument::makePalette(1, 2, ramp(1));
        std::vector<std::uint8_t> pixels(16);
        for (int i = 0; i < 16; ++i)
            pixels[std::size_t(i)] = std::uint8_t(i);
        const int ids[][3] = {{0, 0, 0}, {5, 0, 1}};
        for (const auto& id : ids) {
            SpriteEntry entry;
            entry.header.group = std::uint16_t(id[0]);
            entry.header.number = std::uint16_t(id[1]);
            entry.pixels = SpriteDocument::makePixels(Formats::Image::indexed(4, 4, pixels));
            entry.palette = sprites.palettes()[id[2]];
            sprites.sprites() << entry;
        }
        const auto bytes = sprites.toSff().write();
        QVERIFY(bytes.ok());
        writeFile(dir.filePath("e.sff"),
                  QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size())));
        writeFile(dir.filePath("e.def"), "[Info]\nname = \"E\"\nmugenversion = 1.0\n"
                                         "[Files]\ncns = e.cns\nsprite = e.sff\n");
        writeFile(dir.filePath("e.cns"), "[Data]\nlife = 1000\n");

        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("e.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        auto* page = window.currentProjectView()->findChild<SpritePage*>();
        QVERIFY(page);
        SpriteFile* file = page->file();
        auto modal = [] {
            return QApplication::activeModalWidget();
        };

        QTimer::singleShot(0, this, [modal] {
            auto* editor = qobject_cast<AdvancedPaletteEditor*>(modal());
            QVERIFY(editor);
            QCOMPARE(editor->windowTitle(), QStringLiteral("Advanced palette editor"));
            QCOMPARE(editor->findChild<QLabel*>("imgSelLb")->text(), QStringLiteral("0,0"));
            QCOMPARE(editor->findChild<QLabel*>("palSelLb")->text(), QStringLiteral("1,1"));
            editor->reject();
        });
        actions.action("tools_pal_editor")->trigger();

        AdvancedPaletteEditor editor(*m_context, file, 1, 0, &window); // sprite 5,0
        editor.show();
        auto button = [&editor](const char* name) {
            auto* b = editor.findChild<QToolButton*>(name);
            [&] {
                QVERIFY2(b, name);
            }();
            return b;
        };
        auto left = [&editor] {
            return editor.column(AdvancedPaletteEditor::Character).palette;
        };
        QCOMPARE(editor.findChild<QLabel*>("imgSelLb")->text(), QStringLiteral("5,0"));
        QCOMPARE(editor.findChild<QLabel*>("palSelLb")->text(), QStringLiteral("1,2"));
        QCOMPARE(left(), ramp(1));
        QCOMPARE(editor.checkedColumn(), AdvancedPaletteEditor::Character);
        QCOMPARE(editor.findChild<QComboBox*>("effects")->currentText(), QStringLiteral("Blue to Red"));
        QVERIFY(!editor.hasChanges());

        PaletteWidget* grid = editor.grid(AdvancedPaletteEditor::Character);
        grid->setSelection({1, 5});
        button("swPos")->click();
        QCOMPARE(left()[1], ramp(1)[5]);
        QCOMPARE(grid->selection(), (QList<int>{1, 5}));
        QVERIFY(editor.hasChanges());
        QVERIFY(!file->isModified());
        button("undo")->click();
        QCOMPARE(left(), ramp(1));
        button("redo")->click();
        QCOMPARE(left()[1], ramp(1)[5]);
        editor.setCheckedColumn(AdvancedPaletteEditor::ImageOrPalette);
        button("undo")->click(); // the other column's history is empty
        QCOMPARE(left()[1], ramp(1)[5]);
        editor.setCheckedColumn(AdvancedPaletteEditor::Character);

        button("zoomIn")->click();
        QCOMPARE(editor.zoom(AdvancedPaletteEditor::Character), 2.0);
        for (int i = 0; i < 6; ++i)
            button("zoomOut")->click();
        QCOMPARE(editor.zoom(AdvancedPaletteEditor::Character), 0.25);

        grid->setSelection({2});
        editor.findChild<QComboBox*>("effects")->setCurrentIndex(8); // Monochrome: (0+2+0)/3 = 0
        button("effApply")->click();
        QCOMPARE(left()[2], (Formats::Rgba{0, 0, 0, 255}));
        grid->setSelection({3});
        auto* red = editor.findChild<QSlider*>("rgbR");
        red->setValue(50);
        QCOMPARE(left()[3], (Formats::Rgba{50, 3, 0, 255}));
        QCOMPARE(editor.findChild<QLabel*>("rLb")->text(), QStringLiteral("50"));
        button("rgbNo")->click();
        QCOMPARE(left()[3], ramp(1)[3]);
        QCOMPARE(red->value(), 0);
        const int steps = editor.undoStack(AdvancedPaletteEditor::Character)->count();
        editor.findChild<QSlider*>("hueHue")->setValue(120); // green -> blue
        button("hueYes")->click();
        QCOMPARE(left()[3], (Formats::Rgba{0, 0, 3, 255}));
        QCOMPARE(editor.findChild<QSlider*>("hueHue")->value(), 0);
        QCOMPARE(editor.undoStack(AdvancedPaletteEditor::Character)->count(), steps + 1);

        grid->setSelection({10, 11, 12});
        QTimer::singleShot(0, this, [modal, this] {
            auto* start = qobject_cast<QColorDialog*>(modal());
            QVERIFY(start);
            QCOMPARE(start->windowTitle(), QStringLiteral("Start color"));
            start->setCurrentColor(QColor(0, 0, 255));
            QTimer::singleShot(0, this, [modal] {
                auto* end = qobject_cast<QColorDialog*>(modal());
                QVERIFY(end);
                QCOMPARE(end->windowTitle(), QStringLiteral("End color"));
                QCOMPARE(end->currentColor(), QColor(255, 255, 255));
                end->setCurrentColor(QColor(255, 0, 0));
                end->accept();
            });
            start->accept();
        });
        button("corGrad")->click();
        QCOMPARE(left()[10], (Formats::Rgba{0, 0, 255, 255}));
        QCOMPARE(left()[11], (Formats::Rgba{127, 0, 128, 255}));
        QCOMPARE(left()[12], (Formats::Rgba{255, 0, 0, 255}));

        auto* paletteSelector = editor.findChild<QScrollBar*>("palSel");
        answerDiscard(QMessageBox::No);
        paletteSelector->setValue(0);
        QCOMPARE(editor.findChild<QLabel*>("palSelLb")->text(), QStringLiteral("1,2"));
        QCOMPARE(paletteSelector->value(), 1);

        button("commChanges")->click();
        QCOMPARE(file->document().palettes()[1]->palette.colors, left());
        QVERIFY(!editor.hasChanges());
        actions.action("edit_undo")->trigger();
        QCOMPARE(file->document().palettes()[1]->palette.colors, ramp(1));
        paletteSelector->setValue(0); // nothing uncommitted, so no question
        QCOMPARE(left(), ramp(0));

        QByteArray act;
        for (int i = 0; i < 256; ++i) {
            act += char(i);
            act += char(i);
            act += char(i);
        }
        writeFile(dir.filePath("gray.act"), act);
        editor.setCheckedColumn(AdvancedPaletteEditor::ImageOrPalette);
        FileDialogs::setOpenPromptOverride(
            [&dir](const QString&, const QString&, const QString&) { return dir.filePath("gray.act"); });
        button("openPal")->click();
        FileDialogs::setOpenPromptOverride({});
        const Formats::Palette right = editor.column(AdvancedPaletteEditor::ImageOrPalette).palette;
        QCOMPARE(right[1], (Formats::Rgba{254, 254, 254, 255})); // ACT read in reverse
        editor.grid(AdvancedPaletteEditor::ImageOrPalette)->setSelection({1, 2});
        button("selRL")->click();
        QCOMPARE(grid->selection(), (QList<int>{1, 2}));
        grid->setSelection({7, 8, 9});
        button("corRL")->click(); // two colors into the first two target slots
        QCOMPARE(left()[7], right[1]);
        QCOMPARE(left()[8], right[2]);
        QCOMPARE(left()[9], ramp(0)[9]);
        editor.setCheckedColumn(AdvancedPaletteEditor::Character);

        QTimer::singleShot(0, this, [modal] {
            auto* dialog = qobject_cast<CommitSpriteDialog*>(modal());
            QVERIFY(dialog);
            QCOMPARE(dialog->windowTitle(), QStringLiteral("Palette"));
            QCOMPARE(dialog->palettes(), QList<int>{0});
            dialog->findChild<QPushButton*>("okBut")->click();
        });
        button("commSprChanges")->click();
        QCOMPARE(file->document().palettes().size(), 3);
        QCOMPARE(file->document().sprites()[1].palette, file->document().palettes()[2]);
        QCOMPARE(file->document().palettes()[2]->palette.colors, left());
        actions.action("edit_undo")->trigger();
        QCOMPARE(file->document().palettes().size(), 2);

        answerDiscard(QMessageBox::No);
        editor.reject();
        QVERIFY(editor.isVisible());
        answerDiscard(QMessageBox::Yes);
        editor.reject();
        QVERIFY(!editor.isVisible());
    }

    void backgroundsEditor()
    {
        QTemporaryDir dir;
        // Sprites 0,0 (8x4) and 0,1 (4x4). The stage has two elements and one controller.
        SpriteDocument sprites;
        sprites.palettes() << SpriteDocument::makePalette(1, 1, Formats::grayscalePalette(256));
        const int ids[][3] = {{0, 0, 8}, {0, 1, 4}};
        for (const auto& id : ids) {
            SpriteEntry entry;
            entry.header.group = std::uint16_t(id[0]);
            entry.header.number = std::uint16_t(id[1]);
            entry.pixels = SpriteDocument::makePixels(
                Formats::Image::indexed(id[2], 4, std::vector<std::uint8_t>(std::size_t(id[2] * 4), 5)));
            entry.palette = sprites.palettes()[0];
            sprites.sprites() << entry;
        }
        const auto bytes = sprites.toSff().write();
        QVERIFY(bytes.ok());
        writeFile(dir.filePath("b.sff"),
                  QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size())));
        writeFile(dir.filePath("b.def"), "[Info]\nname = \"Bg Test\"\nmugenversion = 1.0\n"
                                         "[Camera]\nstartx = 0\nstarty = 0\nboundleft = -100\nboundright = 100\n"
                                         "boundhigh = -25\nboundlow = 0\n"
                                         "[PlayerInfo]\np1startx = -70\np1starty = 0\np2startx = 70\np2starty = 0\n"
                                         "[StageInfo]\nzoffset = 190\nlocalcoord = 320, 240\n"
                                         "[BGdef]\nspr = b.sff\n\n;----\n[BG 0]\ntype  = normal\nspriteno = 0, 0\n"
                                         "layerno = 0 ; Back\nstart = 10, 20\ntile = 1, 0\n\n"
                                         "[BG Sky]\ntype = normal\nspriteno = 0, 1\nid = 7\n"
                                         "[BGCtrlDef Mover]\nlooptime = 120\nctrlID = 7\n"
                                         "[BGCtrl Shift]\ntype = PosAdd\ntime = 30\nx = 20\n");

        MainWindow window(*m_context);
        window.resize(1100, 800);
        window.show();
        QVERIFY(window.openProject(dir.filePath("b.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_bg")->trigger();
        ProjectView* view = window.currentProjectView();
        QCOMPARE(view->currentEditor(), QStringLiteral("backgrounds"));
        auto* page = view->findChild<BackgroundsPage*>();
        QVERIFY(page);
        QTextDocument* text = page->file()->document();
        auto has = [text](const QString& line) {
            return text->toPlainText().split(QLatin1Char('\n')).contains(line);
        };
        auto* label = view->findChild<QLabel*>("bgs_sel_lb");
        auto* name = view->findChild<QLineEdit*>("bgs_name");
        auto* type = view->findChild<QComboBox*>("bgs_type");
        auto* startX = view->findChild<QDoubleSpinBox*>("bgs_start_x");
        auto* startY = view->findChild<QDoubleSpinBox*>("bgs_start_y");
        auto* deltaX = view->findChild<QDoubleSpinBox*>("bgs_delta_x");
        auto* layer = view->findChild<QSpinBox*>("bgs_layer");
        auto* trans = view->findChild<QComboBox*>("bgs_trans");
        auto* source = view->findChild<QSpinBox*>("bgs_transsrc");
        auto* parallax = view->findChild<QComboBox*>("bgs_parallax_mode");
        auto* windowMode = view->findChild<QComboBox*>("bgs_window_mode");
        QVERIFY(label && name && type && startX && startY && deltaX && layer && trans && source && parallax &&
                windowMode);
        auto choose = [](QComboBox* combo, int index) {
            combo->setCurrentIndex(index);
            Q_EMIT combo->activated(index);
        };
        QCOMPARE(label->text(), QStringLiteral("1 / 2"));
        QCOMPARE(name->text(), QStringLiteral("0"));
        QCOMPARE(type->currentText(), QStringLiteral("Normal"));
        QCOMPARE(startX->value(), 10.0);
        QCOMPARE(trans->currentText(), QStringLiteral("Default"));

        QCOMPARE(page->view()->frames().first().rect, QRectF(-160, 0, 320, 240));
        QVERIFY(page->view()->images().size() > 30); // the tiled floor covers the screen
        QCOMPARE(page->view()->images().first().topLeft.y(), 20);

        startX->setValue(30);
        QVERIFY(has("start = 30, 20"));
        startX->setValue(0);
        startY->setValue(0);
        QVERIFY(!text->toPlainText().contains("start ="));
        page->view()->setFocus();
        actions.action("edit_undo")->trigger();
        QVERIFY(has("start = 0, 20"));
        layer->setValue(1);
        QVERIFY(has("layerno = 1 ; Back"));
        deltaX->setValue(0.5);
        QVERIFY(has("delta = 0.5, 1"));
        QCOMPARE(text->findBlockByNumber(28).text(), QStringLiteral("delta = 0.5, 1"));

        choose(trans, 2);
        QVERIFY(has("trans = Add"));
        choose(trans, 3);
        QVERIFY(has("trans = Add1"));
        choose(trans, 1);
        QVERIFY(!text->toPlainText().contains("trans ="));
        source->setValue(200);
        QVERIFY(has("trans = AddAlpha"));
        QVERIFY(has("alpha = 200, 256"));
        choose(trans, 1);

        choose(type, 3);
        QVERIFY(has("type = Parallax"));
        QVERIFY(has("width = 8, 8"));
        QVERIFY(has("spriteno = 0, 0"));
        choose(parallax, 2);
        QVERIFY(has("xscale = 1, 1"));
        QVERIFY(!text->toPlainText().contains("width ="));
        choose(type, 1);
        QVERIFY(has("type = Normal"));
        // No mask window before 1.1. The window counts from the screen's top-left.
        QCOMPARE(windowMode->count(), 2);
        choose(windowMode, 1);
        QVERIFY(has("window = 0, 0, 319, 239"));
        choose(windowMode, 0);
        QVERIFY(!text->toPlainText().contains("window ="));

        name->setFocus();
        name->selectAll();
        QTest::keyClicks(name, "Floor");
        QVERIFY(has("[BG Floor]"));

        QTimer::singleShot(0, this, [] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->objectName(), QStringLiteral("AddBG"));
            QCOMPARE(dialog->windowTitle(), QStringLiteral("Add background"));
            dialog->findChild<QLineEdit*>("name")->setText("New");
            dialog->findChild<QComboBox*>("type")->setCurrentIndex(1);
            dialog->accept();
        });
        page->view()->setFocus();
        actions.action("bgs_add")->trigger();
        QCOMPARE(label->text(), QStringLiteral("2 / 3"));
        QVERIFY(text->toPlainText().contains("[BG New]\ntype = Anim\nactionno = -1\n\n[BG Sky]"));
        actions.action("bgs_duplicate")->trigger();
        QCOMPARE(label->text(), QStringLiteral("3 / 4"));
        QCOMPARE(text->toPlainText().count("[BG New]"), 2);
        m_context->config().setValue(QStringLiteral("confirm_delete"), true);
        QTimer::singleShot(0, this, [] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            QVERIFY(box);
            QCOMPARE(box->windowTitle(), boxTitle("Remove"));
            QCOMPARE(box->text(), QStringLiteral("Do you really want to delete the background?"));
            box->button(QMessageBox::Yes)->click();
        });
        actions.action("bgs_delete")->trigger();
        m_context->config().setValue(QStringLiteral("confirm_delete"), false);
        QCOMPARE(label->text(), QStringLiteral("3 / 3"));
        page->setCurrent(1);
        actions.action("bgs_delete")->trigger();
        QCOMPARE(label->text(), QStringLiteral("2 / 2"));
        QVERIFY(!text->toPlainText().contains("[BG New]"));

        auto* defLabel = view->findChild<QLabel*>("bgctrls_def_sel_lb");
        auto* defName = view->findChild<QLineEdit*>("bgctrls_def_name");
        auto* loop = view->findChild<QSpinBox*>("bgctrls_def_loop");
        auto* id1 = view->findChild<QSpinBox*>("bgctrls_def_id_1");
        auto* id2 = view->findChild<QSpinBox*>("bgctrls_def_id_2");
        auto* id3 = view->findChild<QSpinBox*>("bgctrls_def_id_3");
        auto* ctrlName = view->findChild<QLineEdit*>("bgctrls_name");
        auto* ctrlType = view->findChild<QComboBox*>("bgctrls_type");
        auto* xOn = view->findChild<QCheckBox*>("bgctrls_x_lb");
        auto* x = view->findChild<QDoubleSpinBox*>("bgctrls_x");
        auto* end = view->findChild<QSpinBox*>("bgctrls_end_time");
        QVERIFY(defLabel && defName && loop && id1 && id2 && id3 && ctrlName && ctrlType && xOn && x && end);
        QCOMPARE(defLabel->text(), QStringLiteral("1 / 1"));
        QCOMPARE(defName->text(), QStringLiteral("Mover"));
        QCOMPARE(loop->value(), 120);
        QCOMPARE(id1->value(), 7);
        QVERIFY(id2->isEnabled());
        QVERIFY(!id3->isEnabled()); // after the first unused slot
        QCOMPARE(ctrlName->text(), QStringLiteral("Shift"));
        QCOMPARE(ctrlType->currentText(), QStringLiteral("Add position"));
        QVERIFY(xOn->isChecked());
        QCOMPARE(x->value(), 20.0);

        // PosAdd x 20 at time 30 shows from tick 31.
        auto skyLeft = [page] {
            for (const GraphicsView::Image& image : page->view()->images()) {
                if (image.image.width() == 4)
                    return image.topLeft.x();
            }
            return -9999;
        };
        const int before = skyLeft();
        page->setTick(30);
        QCOMPARE(skyLeft(), before);
        page->setTick(31);
        QCOMPARE(skyLeft(), before + 20);
        page->setTick(0);

        id2->setValue(3);
        QVERIFY(has("ctrlID = 7, 3"));
        QVERIFY(id3->isEnabled());
        id2->setValue(-1);
        QVERIFY(has("ctrlID = 7"));
        xOn->setChecked(false);
        QVERIFY(!text->toPlainText().contains("x = 20"));
        QVERIFY(!text->toPlainText().contains("nan"));
        end->setValue(40);
        QVERIFY(has("time = 30, 40"));
        end->setValue(30);
        QVERIFY(has("time = 30"));
        defName->setFocus();
        defName->selectAll();
        QTest::keyClicks(defName, "Go");
        QVERIFY(has("[BGCtrlDef Go]"));
        choose(ctrlType, 2);
        QVERIFY(has("type = Visible"));
        QVERIFY(has("value = 1"));

        page->view()->setFocus();
        actions.action("bgs_play")->trigger();
        QCOMPARE(page->mode(), BackgroundsPage::Mode::Playing);
        QVERIFY(actions.action("bgs_play")->isChecked());
        QVERIFY(!actions.action("bgs_delete")->isEnabled());
        QVERIFY(actions.action("bgs_add")->isEnabled());
        QVERIFY(!startX->isEnabled());
        QCOMPARE(window.ofsStatusBar()->section(0), QStringLiteral("60 FPS"));
        QTest::keyClick(page->view(), Qt::Key_Right, Qt::AltModifier);
        QTest::keyClick(page->view(), Qt::Key_Right, Qt::ShiftModifier);
        QCOMPARE(page->camera(), QPointF(12, 0));
        for (int i = 0; i < 20; ++i)
            QTest::keyClick(page->view(), Qt::Key_Right, Qt::AltModifier);
        QCOMPARE(page->camera(), QPointF(100, 0));
        QTRY_VERIFY(page->tick() > 0);
        actions.action("bgs_pause")->trigger();
        QCOMPARE(page->mode(), BackgroundsPage::Mode::Paused);
        QVERIFY(actions.action("bgs_pause")->isChecked());
        actions.action("bgs_stop")->trigger();
        QCOMPARE(page->mode(), BackgroundsPage::Mode::Stopped);
        QCOMPARE(page->tick(), 0);
        QCOMPARE(page->camera(), QPointF());
        QVERIFY(actions.action("bgs_stop")->isChecked());

        actions.action("view_organizer")->setChecked(true);
        Q_EMIT actions.action("view_organizer")->triggered(true);
        auto* organizer = view->findChild<QListView*>("org_list");
        QVERIFY(organizer);
        QCOMPARE(organizer->model()->rowCount(), 2);
        QCOMPARE(organizer->model()->index(0, 0).data().toString(), QStringLiteral("Floor"));
        auto* rows = qobject_cast<QStandardItemModel*>(organizer->model());
        QVERIFY(rows);
        const int shown = page->view()->images().size();
        rows->item(0)->setCheckState(Qt::Unchecked);
        QCOMPARE(page->view()->images().size(), 1); // only the sky
        rows->item(0)->setCheckState(Qt::Checked);
        QCOMPARE(page->view()->images().size(), shown);
        organizer->selectionModel()->select(organizer->model()->index(1, 0), QItemSelectionModel::ClearAndSelect);
        actions.action("org_up")->trigger();
        QVERIFY(text->toPlainText().indexOf("[BG Sky]") < text->toPlainText().indexOf("[BG Floor]"));
        QCOMPARE(organizer->model()->index(0, 0).data().toString(), QStringLiteral("Sky"));
        page->view()->setFocus();
        actions.action("edit_undo")->trigger();
        QVERIFY(text->toPlainText().indexOf("[BG Floor]") < text->toPlainText().indexOf("[BG Sky]"));
        organizer->selectionModel()->select(organizer->model()->index(0, 0), QItemSelectionModel::ClearAndSelect);
        view->backgroundsEditor()->dropElements(2);
        QVERIFY(text->toPlainText().indexOf("[BG Sky]") < text->toPlainText().indexOf("[BG Floor]"));
        actions.action("edit_undo")->trigger();
        QVERIFY(text->toPlainText().indexOf("[BG Floor]") < text->toPlainText().indexOf("[BG Sky]"));
        actions.action("view_organizer")->setChecked(false);
        Q_EMIT actions.action("view_organizer")->triggered(false);

        QMenu* bgMenu = menu(window, "menu_backgrounds");
        QVERIFY(bgMenu);
        Q_EMIT bgMenu->aboutToShow();
        QAction* groupItem = nullptr;
        for (QAction* a : bgMenu->actions()) {
            if (a->text() == QLatin1String("BG"))
                groupItem = a;
        }
        QVERIFY(groupItem && groupItem->isCheckable() && groupItem->isChecked());
    }

    void spritesSaveAs()
    {
        QTemporaryDir dir;
        QTemporaryDir outside;
        // A 1.1 file with one true-color sprite.
        SpriteDocument doc;
        SpriteEntry entry;
        entry.pixels = SpriteDocument::makePixels(Formats::Image::rgba(2, 1, {255, 0, 0, 255, 0, 0, 255, 255}));
        doc.sprites() << entry;
        const auto bytes = doc.toSff().write(); // a new document is version 2.01
        QVERIFY(bytes.ok());
        writeFile(dir.filePath("s.sff"),
                  QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size())));
        writeFile(dir.filePath("s.def"), "[Info]\nname = \"S\"\nmugenversion = 1.0\n"
                                         "[Files]\ncns = s.cns\nsprite = s.sff\n");
        writeFile(dir.filePath("s.cns"), "[Data]\nlife = 1000\n");
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("s.def")));
        m_context->actions().action("editor_spr")->trigger();
        auto* page = window.currentProjectView()->findChild<SpritePage*>();
        QVERIFY(page);
        SpriteFile* file = page->file();
        QCOMPARE(file->document().version(), Formats::SffVersion::V201);
        auto modal = [] {
            return QApplication::activeModalWidget();
        };

        QString answer = dir.filePath("out.sff");
        FileDialogs::setSavePromptOverride([&answer](const QString& caption, const QString&, const QString& filter) {
            return caption == QLatin1String("Save sprites as...") && filter.startsWith("Sprites (*.sff)") ? answer
                                                                                                          : QString();
        });
        QTimer::singleShot(0, this, [this, modal] {
            QWidget* format = modal();
            QVERIFY(format);
            QCOMPARE(format->objectName(), QStringLiteral("SaveSpr"));
            QVERIFY(format->findChild<QRadioButton*>("mugen11")->isChecked());
            format->findChild<QRadioButton*>("mugen10")->setChecked(true);
            QTimer::singleShot(0, this, [modal] {
                auto* box = qobject_cast<QMessageBox*>(modal());
                QVERIFY(box);
                QCOMPARE(box->windowTitle(), boxTitle("Convert sprites to 8-bit"));
                box->button(QMessageBox::Yes)->click();
            });
            qobject_cast<QDialog*>(format)->accept();
        });
        m_context->actions().action("spr_saveas")->trigger();
        QCOMPARE(file->path(), QDir::fromNativeSeparators(dir.filePath("out.sff")));
        QCOMPARE(file->document().version(), Formats::SffVersion::V2);
        QVERIFY(file->document().image(0).isIndexed());
        QVERIFY(!file->isModified());
        auto* tabs = window.currentProjectView()->findChild<QTabWidget*>("fileTabs_sprites");
        QCOMPARE(tabs->tabText(tabs->currentIndex()), QStringLiteral("out.sff"));

        answer = outside.filePath("copy.sff");
        QTimer::singleShot(0, this, [this, modal] {
            QWidget* format = modal();
            QVERIFY(format);
            format->findChild<QRadioButton*>("mugenBT")->setChecked(true);
            QTimer::singleShot(0, this, [modal] {
                auto* box = qobject_cast<QMessageBox*>(modal());
                QVERIFY(box);
                QCOMPARE(box->windowTitle(), boxTitle("External file!"));
                box->button(QMessageBox::Yes)->click();
            });
            qobject_cast<QDialog*>(format)->accept();
        });
        m_context->actions().action("spr_saveas")->trigger();
        FileDialogs::setSavePromptOverride({});
        QVERIFY(QFileInfo::exists(outside.filePath("copy.sff")));
        QCOMPARE(file->path(), QDir::fromNativeSeparators(dir.filePath("out.sff")));
        SpriteFile copy(outside.filePath("copy.sff"), "sprites");
        QString error;
        QVERIFY(copy.load(&error));
        QCOMPARE(copy.document().version(), Formats::SffVersion::V1);
    }

    void spritesDialogs()
    {
        QTemporaryDir dir;
        // Sprites 0,0, 0,1 and 10,0 are 8x6 with a one pixel transparent border. Action 200 shows 0,0.
        SpriteDocument doc;
        doc.palettes() << SpriteDocument::makePalette(1, 1, Formats::grayscalePalette(256));
        std::vector<std::uint8_t> bordered(48, 0);
        for (int y = 1; y <= 4; ++y)
            for (int x = 1; x <= 6; ++x)
                bordered[std::size_t(y * 8 + x)] = 5;
        const int ids[][2] = {{0, 0}, {0, 1}, {10, 0}};
        for (const auto& id : ids) {
            SpriteEntry entry;
            entry.header.group = std::uint16_t(id[0]);
            entry.header.number = std::uint16_t(id[1]);
            entry.header.axisX = 4;
            entry.header.axisY = 6;
            entry.pixels = SpriteDocument::makePixels(Formats::Image::indexed(8, 6, bordered));
            entry.palette = doc.palettes()[0];
            doc.sprites() << entry;
        }
        Formats::SffDocument sff = doc.toSff();
        sff.version = Formats::SffVersion::V2;
        const auto bytes = sff.write();
        QVERIFY(bytes.ok());
        writeFile(dir.filePath("d.sff"),
                  QByteArray(reinterpret_cast<const char*>(bytes.value().data()), qsizetype(bytes.value().size())));
        writeFile(dir.filePath("d.def"), "[Info]\nname = \"D\"\nmugenversion = 1.0\n"
                                         "[Files]\ncns = d.cns\nsprite = d.sff\nanim = d.air\n");
        writeFile(dir.filePath("d.cns"), "[Data]\nlife = 1000\n");
        writeFile(dir.filePath("d.air"), "[Begin Action 200]\nClsn1: 1\n Clsn1[0] = 1, 2, 3, 4\n0,0, 0,0, 5\n");

        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("d.def")));
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_spr")->trigger();
        auto* page = window.currentProjectView()->findChild<SpritePage*>();
        QVERIFY(page);
        SpriteFile* file = page->file();
        auto modal = [] {
            return QApplication::activeModalWidget();
        };

        QTimer::singleShot(0, this, [modal] {
            QWidget* range = modal();
            QVERIFY(range);
            QCOMPARE(range->objectName(), QStringLiteral("SaveItem"));
            QCOMPARE(range->windowTitle(), QStringLiteral("Range"));
            QVERIFY(range->findChild<QRadioButton*>("cur")->isChecked());
            range->findChild<QRadioButton*>("all")->setChecked(true);
            qobject_cast<QDialog*>(range)->accept();
        });
        actions.action("spr_crop")->trigger();
        QCOMPARE(file->document().sprites()[2].width(), 6);
        QCOMPARE(int(file->document().sprites()[2].header.axisX), 3);
        QCOMPARE(file->undoStack()->count(), 1);

        QTimer::singleShot(0, this, [this, modal] {
            QWidget* resize = modal();
            QVERIFY(resize);
            QCOMPARE(resize->objectName(), QStringLiteral("ResizeSpr"));
            QCOMPARE(resize->findChild<QDoubleSpinBox*>("xscale")->value(), 1.0);
            resize->findChild<QDoubleSpinBox*>("xscale")->setValue(2);
            resize->findChild<QDoubleSpinBox*>("yscale")->setValue(2);
            QTimer::singleShot(0, this, [modal] {
                auto* box = qobject_cast<QMessageBox*>(modal());
                QVERIFY(box);
                QCOMPARE(box->windowTitle(), boxTitle("Scale contact boxes"));
                box->button(QMessageBox::Yes)->click();
            });
            qobject_cast<QDialog*>(resize)->accept();
        });
        page->setCurrent(0);
        actions.action("spr_resize")->trigger();
        QCOMPARE(file->document().sprites()[0].width(), 12);
        QCOMPARE(file->document().sprites()[1].width(), 6); // only the current one
        auto* air = qobject_cast<CodeFile*>(window.currentProjectView()->project()->file(dir.filePath("d.air")));
        QVERIFY(air);
        QVERIFY2(air->document()->toPlainText().contains("  Clsn1[0] = 2, 4, 7, 9\n"),
                 qPrintable(air->document()->toPlainText()));

        QTimer::singleShot(0, this, [modal] {
            QWidget* dialog = modal();
            QVERIFY(dialog);
            QCOMPARE(dialog->objectName(), QStringLiteral("DelGrp"));
            QCOMPARE(dialog->findChild<QSpinBox*>("Grp")->value(), 0);
            dialog->findChild<QSpinBox*>("Grp")->setValue(10);
            qobject_cast<QDialog*>(dialog)->accept();
        });
        actions.action("spr_delete_grp")->trigger();
        QCOMPARE(file->document().spriteCount(), 2);

        const int steps = file->undoStack()->count();
        const int axisX = file->document().sprites()[0].header.axisX;
        QTimer::singleShot(0, this, [modal] {
            QWidget* dialog = modal();
            QVERIFY(dialog);
            QCOMPARE(dialog->windowTitle(), QStringLiteral("Alignment tool"));
            dialog->findChild<QSpinBox*>("xVal")->setValue(5);
            dialog->findChild<QPushButton*>("pushButton")->click();
            dialog->findChild<QPushButton*>("pushButton")->click();
            dialog->findChild<QPushButton*>("pushButton_2")->click();
        });
        actions.action("spr_align")->trigger();
        QCOMPARE(int(file->document().sprites()[0].header.axisX), axisX + 10);
        QCOMPARE(file->undoStack()->count(), steps + 2);

        actions.action("spr_split_channels")->trigger();
        QCOMPARE(file->document().spriteCount(), 4);
        QCOMPARE(file->document().sprites()[0].name, QStringLiteral(" R"));

        FileDialogs::setSavePromptOverride([&dir](const QString& caption, const QString&, const QString& filter) {
            return caption == QLatin1String("Save image...") && filter.startsWith("Portable Network Graphic (*.png)")
                       ? dir.filePath("exp.png")
                       : QString();
        });
        QTimer::singleShot(0, this, [modal] {
            QWidget* items = modal();
            QVERIFY(items);
            QCOMPARE(items->objectName(), QStringLiteral("ItemsDlg"));
            qobject_cast<QDialog*>(items)->accept();
        });
        actions.action("spr_saveimgas")->trigger();
        FileDialogs::setSavePromptOverride({});
        QVERIFY(QFileInfo::exists(dir.filePath("exp.png")));

        Formats::Palette colors = Formats::grayscalePalette(256);
        colors[1] = {9, 9, 9, 255};
        const auto png = Formats::encodePng(Formats::Image::indexed(2, 2, {0, 1, 1, 0}, colors));
        QVERIFY(png.ok());
        writeFile(dir.filePath("new.png"),
                  QByteArray(reinterpret_cast<const char*>(png.value().data()), qsizetype(png.value().size())));
        FileDialogs::setOpenPromptOverride([&dir](const QString& caption, const QString&, const QString& filter) {
            return caption == QLatin1String("Add") && filter.startsWith("All supported (*.pcx *.png *.gif *.bmp)")
                       ? dir.filePath("new.png")
                       : QString();
        });
        page->setCurrent(1);
        const int palettes = int(file->document().palettes().size());
        const auto& currentHeader = file->document().sprites()[1].header;
        const QPoint currentAxis(currentHeader.axisX, currentHeader.axisY);
        QTimer::singleShot(0, this, [modal, currentAxis] {
            QWidget* add = modal();
            QVERIFY(add);
            QCOMPARE(add->objectName(), QStringLiteral("AddSpr"));
            QCOMPARE(add->findChild<QSpinBox*>("AxisX")->value(), currentAxis.x());
            add->findChild<QSpinBox*>("Grp")->setValue(9100);
            qobject_cast<QDialog*>(add)->accept();
        });
        actions.action("spr_add")->trigger();
        FileDialogs::setOpenPromptOverride({});
        QCOMPARE(page->current(), 2);
        const SpriteEntry& added = file->document().sprites()[2];
        QCOMPARE(added.group(), 9100);
        QCOMPARE(added.number(), 0);
        QCOMPARE(QPoint(added.header.axisX, added.header.axisY), currentAxis);
        QCOMPARE(int(file->document().palettes().size()), palettes + 1);
        file->undoStack()->undo();
        QCOMPARE(int(file->document().palettes().size()), palettes);

        while (file->undoStack()->canUndo())
            file->undoStack()->undo();
        air->document()->setModified(false);
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

    void newProject()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        ActionRegistry& actions = m_context->actions();
        QVERIFY(actions.action("file_new")->isEnabled());
        const qsizetype history = m_context->history().entries().size();
        QTemporaryDir folder;
        m_context->config().setValue("project_path", folder.path());

        chooseTemplate(-1);
        actions.action("file_new")->trigger();
        QVERIFY(!window.currentProjectView());

        chooseTemplate(0);
        answerEmptyWizard("Character");
        actions.action("file_new")->trigger();
        ProjectView* view = window.currentProjectView();
        QVERIFY(view);
        Project* project = view->project();
        QVERIFY(project->isMemoryOnly());
        QCOMPARE(project->name(), QStringLiteral("Char name"));
        QCOMPARE(view->currentEditor(), QStringLiteral("definitions"));
        QCOMPARE(window.windowTitle(), QStringLiteral("Char name - Open Factory Studio"));
        QCOMPARE(m_context->history().entries().size(), history);
        QVERIFY(actions.action("file_save")->isEnabled());
        auto* tabs = view->findChild<QTabWidget*>("fileTabs_definitions");
        QVERIFY(tabs);
        QCOMPARE(tabs->tabText(0), QString());
        QVERIFY(tabs->tabToolTip(0).isEmpty());
        QCOMPARE(view->findChild<QTabWidget*>("fileTabs_states")->count(), 1);

        QStringList asked;
        FileDialogs::setSavePromptOverride([&](const QString& caption, const QString& path, const QString&) {
            asked << caption + '|' + path;
            return QString();
        });
        actions.action("file_save")->trigger();
        QVERIFY(asked.isEmpty());

        view->currentCodeEditor()->moveCursor(QTextCursor::End);
        view->currentCodeEditor()->insertPlainText("; mine\n");
        QCOMPARE(tabs->tabText(0), QStringLiteral("*"));
        CodeFile* commands = qobject_cast<CodeFile*>(project->file(folder.path() + "/char.cmd"));
        QVERIFY(commands);
        commands->document()->setPlainText(commands->document()->toPlainText() + "; more\n");
        QTemporaryDir target;
        FileDialogs::setSavePromptOverride([&](const QString& caption, const QString& path, const QString&) {
            asked << caption + '|' + path;
            return target.filePath(caption.contains("definitions") ? "mychar.def" : QFileInfo(path).fileName());
        });
        actions.action("file_save")->trigger();
        FileDialogs::setSavePromptOverride({});
        QCOMPARE(asked, (QStringList{"Save definitions as...|" + folder.path() + "/char.def",
                                     "Save commands as...|" + target.path() + "/char.cmd"}));
        QVERIFY(!project->isMemoryOnly());
        QCOMPARE(project->filePath(), target.path() + "/mychar.def");
        QCOMPARE(tabs->tabText(0), QStringLiteral("mychar.def"));
        QVERIFY(QFile::exists(target.filePath("char.cmd")));
        QCOMPARE(m_context->history().entries().size(), history);

        chooseTemplate(0);
        answerEmptyWizard("Font");
        actions.action("file_new")->trigger();
        QVERIFY(window.currentProjectView() != view);
        QVERIFY(window.currentProjectView()->project()->name().startsWith("Project "));
        QCOMPARE(window.currentProjectView()->editorIds(), (QStringList{"definitions", "sprites", "emulator"}));
    }

    void archiveProject()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.filePath("src/fighter"));
        const QByteArray commandsText = "[Command]\nname = \"x\"\ncommand = x\n";
        writeFile(dir.filePath("src/fighter/fighter.def"), "[Info]\nname = \"Zip Fighter\"\nmugenversion = 1.0\n"
                                                           "[Files]\ncmd = fighter.cmd\ncns = fighter.cns\n");
        writeFile(dir.filePath("src/fighter/fighter.cmd"), commandsText);
        writeFile(dir.filePath("src/fighter/fighter.cns"), "[Data]\nlife = 1000\n");
        writeFile(dir.filePath("src/stage.def"), "[Info]\nname = \"Zip Stage\"\n[Camera]\nstartx = 0\n");
        const QString archive = dir.filePath("pack.zip");
        QVERIFY(Zip::create(archive, dir.filePath("src"),
                            {"fighter/fighter.def", "fighter/fighter.cmd", "fighter/fighter.cns", "stage.def"}));

        MainWindow window(*m_context);
        window.show();
        ActionRegistry& actions = m_context->actions();
        QVERIFY(actions.action("file_open_archive")->isEnabled());
        QStringList captions;
        FileDialogs::setOpenPromptOverride([&](const QString& caption, const QString&, const QString& filter) {
            captions << caption << filter;
            return archive;
        });
        actions.action("file_open_archive")->trigger();
        FileDialogs::setOpenPromptOverride({});
        QCOMPARE(captions, (QStringList{"Open archive", "All supported (*.zip);;ZIP Archive (*.zip)"}));
        ProjectView* view = window.currentProjectView();
        QVERIFY(view);
        Project* project = view->project();
        QCOMPARE(project->name(), QStringLiteral("Zip Fighter"));
        QCOMPARE(project->archivePath(), archive);
        QCOMPARE(m_context->history().entries().first().file, archive);
        auto* tabs = view->findChild<QTabWidget*>("fileTabs_definitions");
        QCOMPARE(tabs->tabText(0), QStringLiteral("fighter.def"));
        QCOMPARE(tabs->tabToolTip(0), QDir::toNativeSeparators(archive));
        auto* explorer = view->findChild<QTreeView*>("projectExplorer");
        QCOMPARE(explorer->model()->rowCount(), 3);
        const int tabCount = window.findChild<QTabBar*>()->count();
        QVERIFY(window.openProject(archive));
        QCOMPARE(window.findChild<QTabBar*>()->count(), tabCount);

        actions.action("editor_cmd")->trigger();
        view->currentCodeEditor()->insertPlainText("; mine\n");
        QStringList asked;
        FileDialogs::setSavePromptOverride([&](const QString& caption, const QString& path, const QString&) {
            asked << caption + '|' + path;
            return QString();
        });
        actions.action("file_save")->trigger();
        QCOMPARE(asked, QStringList{"Save as|" + dir.path() + "/fighter.cmd"});
        QVERIFY(view->currentFile()->isModified());
        QCOMPARE(Zip::read(archive, "fighter/fighter.cmd"), commandsText);
        FileDialogs::setSavePromptOverride([&](const QString&, const QString& path, const QString&) { return path; });
        actions.action("file_save")->trigger();
        FileDialogs::setSavePromptOverride({});
        QVERIFY(!view->currentFile()->isModified());
        QVERIFY(readFile(dir.filePath("fighter.cmd")).startsWith("; mine"));
        QCOMPARE(Zip::read(archive, "fighter/fighter.cmd"), commandsText);

        const QString extractedDef = project->filePath();
        QVERIFY(QFile::exists(extractedDef));
        actions.action("file_close")->trigger();
        QTRY_VERIFY(!QFile::exists(extractedDef));

        QVERIFY(Zip::create(dir.filePath("empty.zip"), dir.filePath("src/fighter"), {"fighter.cmd"}));
        QTimer::singleShot(0, this, [] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            QVERIFY(box);
            QVERIFY(box->text().startsWith("There is no project in"));
            box->accept();
        });
        QVERIFY(!window.openProject(dir.filePath("empty.zip")));
    }

    void singleFileProjects()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("one.cmd"), "[Command]\nname = \"x\"\ncommand = x\n");
        writeFile(dir.filePath("two.air"), "[Begin Action 0]\n0,0, 0,0, 10\n");
        MainWindow window(*m_context);
        window.show();
        const qsizetype history = m_context->history().entries().size();
        QString filter;
        FileDialogs::setOpenPromptOverride([&](const QString& caption, const QString&, const QString& f) {
            filter = caption + '|' + f;
            return dir.filePath("one.cmd") + '\n' + dir.filePath("two.air");
        });
        m_context->actions().action("file_open_single")->trigger();
        FileDialogs::setOpenPromptOverride({});
        QCOMPARE(filter, QStringLiteral("Open file|MUGENext (*.def *.sff *.snd *.air *.cns *.st *.cmd *.*)"));
        ProjectView* animations = window.currentProjectView();
        QVERIFY(animations && animations->project()->isSingleFile());
        QCOMPARE(animations->editorIds(), (QStringList{"animations", "emulator"}));
        QCOMPARE(animations->currentEditor(), QStringLiteral("animations"));
        QVERIFY(animations->project()->name().startsWith("Project "));
        QCOMPARE(window.projectViews().size(), qsizetype(2));
        ProjectView* commands = window.projectViews().first();
        QCOMPARE(commands->editorIds(), (QStringList{"commands", "emulator"}));
        QCOMPARE(m_context->history().entries().size(), history);
        QCOMPARE(animations->findChild<QTreeView*>("projectExplorer")->model()->rowCount(), 0);
        QVERIFY(window.windowTitle().startsWith("Project "));

        m_context->actions().action("editor_cmd");
        window.findChild<QTabBar*>()->setCurrentIndex(window.findChild<QTabBar*>()->count() - 2);
        QCOMPARE(window.currentProjectView(), commands);
        commands->currentCodeEditor()->insertPlainText("; mine\n");
        m_context->actions().action("file_save")->trigger();
        QVERIFY(readFile(dir.filePath("one.cmd")).startsWith("; mine"));
    }

    void saveProjectAs()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("hero.def"), "[Info]\nname = \"Hero\"\nmugenversion = 1.0\n"
                                            "[Files]\ncmd = hero.cmd\ncns = hero.cns\n");
        writeFile(dir.filePath("hero.cmd"), "[Command]\nname = \"x\"\ncommand = x\n");
        writeFile(dir.filePath("hero.cns"), "[Data]\nlife = 1000\n");
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("hero.def")));
        const int index = window.findChild<QTabBar*>()->currentIndex();
        ActionRegistry& actions = m_context->actions();
        actions.action("editor_cmd")->trigger();
        window.currentProjectView()->currentCodeEditor()->insertPlainText("; changed\n");

        QTemporaryDir target;
        QString asked;
        FileDialogs::setSavePromptOverride([&](const QString& caption, const QString& path, const QString& f) {
            asked = caption + '|' + QFileInfo(path).fileName() + '|' + f;
            return target.filePath("copy/hero2.def");
        });
        actions.action("file_saveas")->trigger();
        FileDialogs::setSavePromptOverride({});
        QCOMPARE(asked, QStringLiteral("Save project as...|hero.def|MUGENext project (*.def)"));
        ProjectView* view = window.currentProjectView();
        QVERIFY(view);
        QCOMPARE(view->project()->filePath(), target.filePath("copy/hero2.def"));
        QCOMPARE(window.findChild<QTabBar*>()->currentIndex(), index);
        QCOMPARE(view->currentEditor(), QStringLiteral("commands"));
        QVERIFY(!view->project()->isModified());
        QVERIFY(readFile(target.filePath("copy/hero.cmd")).startsWith("; changed"));
        QVERIFY(QFile::exists(target.filePath("copy/hero.cns")));
        QCOMPARE(readFile(dir.filePath("hero.cmd")), QByteArray("[Command]\nname = \"x\"\ncommand = x\n"));
        QCOMPARE(m_context->history().entries().first().file, target.filePath("copy/hero2.def"));
        QCOMPARE(m_context->config().value("proj_save_path"), target.filePath("copy"));
    }

    void closePrompts()
    {
        QTemporaryDir dir;
        for (const char* name : {"a", "b"}) {
            const QString base = dir.filePath(name);
            writeFile(base + ".def", QByteArray("[Info]\nname = \"") + name +
                                         "\"\nmugenversion = 1.0\n[Files]\ncmd = " + name + ".cmd\ncns = " + name +
                                         ".cns\n");
            writeFile(base + ".cmd", "[Command]\nname = \"x\"\ncommand = x\n");
            writeFile(base + ".cns", "[Data]\nlife = 1000\n");
        }
        MainWindow window(*m_context);
        window.show();
        m_context->config().setValue("showmsg_discard", QString());
        ActionRegistry& actions = m_context->actions();
        QVERIFY(window.openProject(dir.filePath("a.def")));
        ProjectView* a = window.currentProjectView();
        for (const char* editor : {"editor_st", "editor_cmd", "editor_def"}) {
            actions.action(editor)->trigger();
            a->currentCodeEditor()->insertPlainText("; edited\n");
        }

        QStringList texts;
        answerBoxes({QMessageBox::No, QMessageBox::Yes, QMessageBox::Yes}, &texts);
        actions.action("file_close")->trigger();
        QCOMPARE(texts, (QStringList{"Do you want to discard the changes made in commands?",
                                     "Do you want to discard the changes made in definitions?",
                                     "Do you want to discard the changes made in states?"}));
        QCOMPARE(window.currentProjectView(), a);
        QVERIFY(a->project()->isModified());

        QVERIFY(window.openProject(dir.filePath("b.def")));
        ProjectView* b = window.currentProjectView();
        actions.action("editor_st")->trigger();
        b->currentCodeEditor()->insertPlainText("; edited\n");
        texts.clear();
        answerBoxes({QMessageBox::No}, &texts);
        actions.action("file_close_all")->trigger();
        QCOMPARE(texts, QStringList{"Do you want to discard the changes made in states?"});
        QCOMPARE(window.projectViews().size(), qsizetype(2));

        texts.clear();
        answerBoxes({QMessageBox::Yes}, &texts);
        actions.action("file_close")->trigger();
        QCOMPARE(window.currentProjectView(), a);

        m_context->config().setValue("confirm_close", true);
        QTimer::singleShot(0, this, [] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            QVERIFY(box);
            QCOMPARE(box->defaultButton(), box->button(QMessageBox::No));
            box->button(QMessageBox::No)->click();
        });
        window.close();
        QVERIFY(window.isVisible());
        m_context->config().setValue("confirm_close", false);
        for (ProjectFile* file : a->project()->files()) {
            if (auto* code = qobject_cast<CodeFile*>(file))
                code->document()->setModified(false);
        }
    }

    void backups()
    {
        QTemporaryDir dir;
        const QByteArray original = "[Command]\nname = \"x\"\ncommand = x\n";
        writeFile(dir.filePath("bk.def"), "[Info]\nname = \"Backed\"\nmugenversion = 1.0\n[Files]\ncmd = bk.cmd\n");
        writeFile(dir.filePath("bk.cmd"), original);
        MainWindow window(*m_context);
        window.show();
        ActionRegistry& actions = m_context->actions();

        m_context->config().setValue("backup_onload", true);
        QVERIFY(window.openProject(dir.filePath("bk.def")));
        m_context->config().setValue("backup_onload", false);
        QStringList zips = QDir(dir.filePath("backup")).entryList({"*.zip"});
        QCOMPARE(zips.size(), qsizetype(1));
        QVERIFY(zips.first().endsWith("_Auto.zip"));

        QVERIFY(actions.action("file_backup")->isEnabled());
        actions.action("file_backup")->trigger();
        QVERIFY(!QApplication::activeModalWidget());
        QCOMPARE(QDir(dir.filePath("backup")).entryList({"*.zip"}).size(), qsizetype(2));

        ProjectView* view = window.currentProjectView();
        actions.action("editor_cmd")->trigger();
        view->currentCodeEditor()->insertPlainText("; unsaved\n");
        writeFile(dir.filePath("bk.cmd"), "; changed on disk\n");
        QStringList rows;
        QTimer::singleShot(0, this, [&rows] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->objectName(), QStringLiteral("BackupDlg"));
            QCOMPARE(dialog->windowTitle(), QStringLiteral("Restore backup"));
            auto* list = dialog->findChild<QListWidget*>("listView");
            for (int i = 0; i < list->count(); ++i)
                rows << list->item(i)->text();
            QVERIFY(list->currentItem() == nullptr || !list->currentItem()->isSelected());
            list->setCurrentRow(1);
            dialog->findChild<QDialogButtonBox*>("buttonBox")->button(QDialogButtonBox::Ok)->click();
        });
        actions.action("file_load_backup")->trigger();
        QCOMPARE(rows.size(), qsizetype(2));
        QCOMPARE(rows.filter("(automatic)").size(), qsizetype(1));
        QCOMPARE(readFile(dir.filePath("bk.cmd")), original);
        ProjectView* reloaded = window.currentProjectView();
        QVERIFY(reloaded && reloaded != view);
        QVERIFY(!reloaded->project()->isModified());
        QCOMPARE(reloaded->currentEditor(), QStringLiteral("commands"));

        chooseTemplate(0);
        answerEmptyWizard("Stage");
        actions.action("file_new")->trigger();
        QVERIFY(window.currentProjectView()->project()->isMemoryOnly());
        QVERIFY(!actions.action("file_backup")->isEnabled());
        QVERIFY(!actions.action("file_load_backup")->isEnabled());
    }

    void autosave()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("au.def"), "[Info]\nname = \"Auto\"\nmugenversion = 1.0\n[Files]\ncmd = au.cmd\n");
        writeFile(dir.filePath("au.cmd"), "[Command]\nname = \"x\"\ncommand = x\n");
        m_context->config().setValue("autosave_interval", 2);
        MainWindow window(*m_context);
        window.show();
        auto* timer = window.findChild<QTimer*>("autosaveTimer");
        QVERIFY(timer);
        QVERIFY(timer->isActive());
        QCOMPARE(timer->interval(), 2 * 60 * 1000);
        QVERIFY(window.openProject(dir.filePath("au.def")));
        m_context->actions().action("editor_cmd")->trigger();
        window.currentProjectView()->currentCodeEditor()->insertPlainText("; auto\n");
        chooseTemplate(0);
        answerEmptyWizard("Character");
        m_context->actions().action("file_new")->trigger();
        ProjectView* blank = window.currentProjectView();
        blank->currentCodeEditor()->insertPlainText("; blank\n");

        timer->start(1);
        QTRY_VERIFY(readFile(dir.filePath("au.cmd")).startsWith("; auto"));
        QVERIFY(!QApplication::activeModalWidget());
        QVERIFY(blank->project()->isModified());
        blank->currentCodeEditor()->document()->setModified(false);

        m_context->config().setValue("autosave_interval", 0);
        MainWindow quiet(*m_context);
        QVERIFY(!quiet.findChild<QTimer*>("autosaveTimer")->isActive());
    }

    void fileWatcher()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("fw.def"), "[Info]\nname = \"Watched\"\nmugenversion = 1.0\n[Files]\ncmd = fw.cmd\n");
        writeFile(dir.filePath("fw.cmd"), "[Command]\nname = \"x\"\ncommand = x\n");
        Config& config = m_context->config();
        config.setValue("enable_file_watcher", true);
        config.setValue("showmsg_reload_file", "1"); // Always
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("fw.def")));
        m_context->actions().action("editor_cmd")->trigger();
        CodeEditor* editor = window.currentProjectView()->currentCodeEditor();
        QTest::qWait(300); // the OS starts watching a moment after a file is added

        writeFile(dir.filePath("fw.cmd"), "; first\n");
        QTRY_COMPARE(editor->document()->toPlainText(), QStringLiteral("; first\n"));
        QVERIFY(!editor->document()->isModified());
        QVERIFY(!editor->document()->isUndoAvailable());

        editor->insertPlainText("; mine\n");
        QString asked;
        whenModal<QMessageBox>([&asked](QMessageBox* box) {
            asked = box->windowTitle() + '|' + box->text();
            box->button(QMessageBox::No)->click();
        });
        writeFile(dir.filePath("fw.cmd"), "; second\n");
        QTRY_VERIFY(!asked.isEmpty());
        QCOMPARE(asked, boxTitle("File modified") + "|The file '" + dir.filePath("fw.cmd") +
                            "' has been modified. Do you want to reload all other instances of this file now?");
        QVERIFY(editor->document()->toPlainText().contains("; mine"));
        editor->document()->setModified(false);

        config.setValue("showmsg_reload_file", QString()); // Ask
        whenModal<QMessageBox>([](QMessageBox* box) {
            QVERIFY(box->checkBox());
            box->button(QMessageBox::Yes)->click();
        });
        writeFile(dir.filePath("fw.cmd"), "; third\n");
        QTRY_COMPARE(editor->document()->toPlainText(), QStringLiteral("; third\n"));

        editor->insertPlainText("; saved\n");
        m_context->actions().action("file_save")->trigger();
        QTest::qWait(500);
        QVERIFY(!QApplication::activeModalWidget());

        config.setValue("showmsg_reload_file", "2"); // Never
        writeFile(dir.filePath("fw.cmd"), "; fourth\n");
        QTest::qWait(500);
        QVERIFY(!QApplication::activeModalWidget());
        QVERIFY(editor->document()->toPlainText().contains("; saved"));
        QVERIFY(!editor->document()->toPlainText().contains("; fourth"));
        config.setValue("showmsg_reload_file", "1");
    }

    void organizerInOtherEditors()
    {
        QTemporaryDir dir;
        auto wav = [](std::size_t frames, std::int16_t value) {
            auto bytes = Formats::encodeAudio(
                Formats::SampleBuffer::fromInt16(std::vector<std::int16_t>(frames, value), 1, 11025));
            return bytes ? bytes.value() : Formats::Bytes();
        };
        Formats::SndDocument snd;
        snd.sounds.push_back(Formats::SndSound{5, 2, wav(100, 1)});
        snd.sounds.push_back(Formats::SndSound{0, 1, wav(100, 2)});
        snd.sounds.push_back(Formats::SndSound{0, 0, wav(100, 3)});
        const Formats::Bytes sndBytes = snd.write();
        writeFile(dir.filePath("o.snd"),
                  QByteArray(reinterpret_cast<const char*>(sndBytes.data()), qsizetype(sndBytes.size())));
        writeFile(dir.filePath("o.def"), "[Info]\nname = \"Org\"\nmugenversion = 1.0\n"
                                         "[Files]\ncmd = o.cmd\ncns = o.cns\nanim = o.air\nsound = o.snd\n");
        writeFile(dir.filePath("o.cns"), "[Data]\nlife = 1000\n");
        writeFile(dir.filePath("o.air"), "; Standing\n[Begin Action 0]\n0,0, 0,0, 10\n\n"
                                         "; Turning\n[Begin Action 5]\n5,0, 0,0, 7\n\n"
                                         "; Crouching\n[Begin Action 2]\n2,0, 0,0, 3\n");
        writeFile(dir.filePath("o.cmd"), "[Remap]\nx = x\n\n[Command]\nname = \"FF\"\ncommand = F, F\n\n"
                                         "[Command]\nname = \"BB\"\ncommand = B, B\n\n"
                                         "[Command]\nname = \"x\"\ncommand = x\n\n[Statedef -1]\n");
        MainWindow window(*m_context);
        window.show();
        ActionRegistry& actions = m_context->actions();
        QVERIFY(window.openProject(dir.filePath("o.def")));
        ProjectView* view = window.currentProjectView();
        actions.action("view_organizer")->setChecked(true);
        auto* list = view->findChild<QListView*>("org_list");
        QVERIFY(list);
        auto rows = [list] {
            QStringList captions;
            for (int r = 0; r < list->model()->rowCount(); ++r)
                captions << list->model()->index(r, 0).data().toString();
            return captions;
        };
        auto selectRow = [list](int row) {
            list->selectionModel()->clearSelection();
            list->selectionModel()->select(list->model()->index(row, 0), QItemSelectionModel::Select);
        };

        actions.action("editor_anim")->trigger();
        QCOMPARE(rows(), (QStringList{"0 - Standing", "5 - Turning", "2 - Crouching"}));
        selectRow(2);
        actions.action("org_top")->trigger();
        QCOMPARE(rows(), (QStringList{"2 - Crouching", "0 - Standing", "5 - Turning"}));
        QCOMPARE(list->selectionModel()->selectedRows().first().row(), 0);
        auto* anims = qobject_cast<CodeFile*>(view->project()->file(dir.filePath("o.air")));
        QCOMPARE(anims->document()->toPlainText(),
                 QStringLiteral("; Crouching\n[Begin Action 2]\n2,0, 0,0, 3\n\n; Standing\n[Begin Action 0]\n"
                                "0,0, 0,0, 10\n\n; Turning\n[Begin Action 5]\n5,0, 0,0, 7\n"));
        actions.action("org_auto")->trigger();
        QCOMPARE(rows(), (QStringList{"0 - Standing", "2 - Crouching", "5 - Turning"}));
        actions.action("edit_undo")->trigger();
        QCOMPARE(rows(), (QStringList{"2 - Crouching", "0 - Standing", "5 - Turning"}));
        Q_EMIT list->doubleClicked(list->model()->index(2, 0));
        QCOMPARE(view->animationsEditor()->page()->current(), 2);

        actions.action("editor_snd")->trigger();
        QCOMPARE(rows(), (QStringList{"5, 2", "0, 1", "0, 0"}));
        actions.action("org_auto")->trigger();
        QCOMPARE(rows(), (QStringList{"0, 0", "0, 1", "5, 2"}));
        auto* sounds = qobject_cast<SoundFile*>(view->project()->file(dir.filePath("o.snd")));
        QVERIFY(sounds->isModified());
        selectRow(0);
        actions.action("org_down")->trigger();
        QCOMPARE(rows(), (QStringList{"0, 1", "0, 0", "5, 2"}));

        actions.action("editor_cmd")->trigger();
        QCOMPARE(rows(), (QStringList{"FF", "BB", "x"}));
        selectRow(0);
        actions.action("org_down_10")->trigger();
        QCOMPARE(rows(), (QStringList{"BB", "x", "FF"}));
        selectRow(2);
        actions.action("org_top")->trigger();
        QCOMPARE(rows(), (QStringList{"FF", "BB", "x"}));
        auto* commands = qobject_cast<CodeFile*>(view->project()->file(dir.filePath("o.cmd")));
        const QString text = commands->document()->toPlainText();
        actions.action("org_auto")->trigger();
        QCOMPARE(commands->document()->toPlainText(), text);
        QVERIFY(list->viewport()->acceptDrops());
        QCOMPARE(list->dragDropMode(), QAbstractItemView::InternalMove);
        selectRow(0);
        view->commandsOrganizer()->dropSelection(3);
        QCOMPARE(rows(), (QStringList{"BB", "x", "FF"}));
        QCOMPARE(list->selectionModel()->selectedRows().first().row(), 2);
        actions.action("edit_undo")->trigger();
        QTRY_COMPARE(rows(), (QStringList{"FF", "BB", "x"})); // the list catches up with the text shortly after
        QVERIFY(text.startsWith("[Remap]\nx = x\n\n[Command]\nname = \"FF\""));
        QVERIFY(text.endsWith("\n\n[Statedef -1]\n"));

        actions.action("view_organizer")->setChecked(false);
        for (ProjectFile* file : view->project()->files()) {
            if (auto* code = qobject_cast<CodeFile*>(file))
                code->document()->setModified(false);
        }
        sounds->undoStack()->setClean();
    }

    void recentProjects()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("r.def"), "[Info]\nname = \"Recent\"\nmugenversion = 1.0\n[Files]\ncmd = r.cmd\n");
        writeFile(dir.filePath("r.cmd"), "[Command]\nname = \"x\"\ncommand = x\n");
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("r.def")));
        const QByteArray xml = readFile(qEnvironmentVariable("OFS_CONFIG_DIR") + "/history.xml");
        QVERIFY(xml.contains(dir.filePath("r.def").toUtf8()));

        m_context->history().add("Gone", dir.filePath("gone.def"));
        QString asked;
        QTimer::singleShot(0, this, [&asked] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            QVERIFY(box);
            asked = box->windowTitle() + '|' + box->text();
            QCOMPARE(box->defaultButton(), box->button(QMessageBox::Yes));
            box->button(QMessageBox::Yes)->click();
        });
        QVERIFY(!window.openRecentProject(dir.filePath("gone.def")));
        QCOMPARE(asked, boxTitle("File not found!") + "|The file '" + dir.filePath("gone.def") +
                            "' doesn't exist. Do you want to remove it from the history?");
        for (const History::Entry& entry : m_context->history().entries())
            QVERIFY(entry.file != dir.filePath("gone.def"));
    }

    void importFromAnotherProject()
    {
        QTemporaryDir dir;
        auto wav = [](std::size_t frames, std::int16_t value) {
            auto bytes = Formats::encodeAudio(
                Formats::SampleBuffer::fromInt16(std::vector<std::int16_t>(frames, value), 1, 11025));
            return bytes ? bytes.value() : Formats::Bytes();
        };
        auto writeSnd = [&wav](const QString& path, const QList<QPair<int, int>>& ids) {
            Formats::SndDocument snd;
            for (const auto& id : ids)
                snd.sounds.push_back(Formats::SndSound{id.first, id.second, wav(50, std::int16_t(id.second))});
            const Formats::Bytes bytes = snd.write();
            writeFile(path, QByteArray(reinterpret_cast<const char*>(bytes.data()), qsizetype(bytes.size())));
        };
        for (const char* name : {"a", "b"}) {
            writeFile(dir.filePath(QStringLiteral("%1.def").arg(name)),
                      QStringLiteral("[Info]\nname = \"%1\"\nmugenversion = 1.0\n[Files]\ncns = %1.cns\n"
                                     "anim = %1.air\nsound = %1.snd\n")
                          .arg(QString::fromLatin1(name).toUpper())
                          .toUtf8());
            writeFile(dir.filePath(QStringLiteral("%1.cns").arg(name)), "[Data]\nlife = 1000\n");
        }
        writeFile(dir.filePath("a.air"), "; Old\n[Begin Action 0]\n9,9, 0,0, 1\n\n[Begin Action 6]\n6,0, 0,0, 4\n");
        writeFile(dir.filePath("b.air"), "; Standing\n[Begin Action 0]\n0,0, 0,0, 10\n\n"
                                         "; Walking\n[Begin Action 20]\n20,0, 0,0, 5\n");
        writeSnd(dir.filePath("a.snd"), {{0, 0}});
        writeSnd(dir.filePath("b.snd"), {{0, 77}, {0, 1}});

        MainWindow window(*m_context);
        window.show();
        ActionRegistry& actions = m_context->actions();
        QVERIFY(window.openProject(dir.filePath("b.def")));
        QVERIFY(window.openProject(dir.filePath("a.def")));
        ProjectView* a = window.currentProjectView();
        QCOMPARE(a->project()->name(), QStringLiteral("A"));

        actions.action("editor_anim")->trigger();
        QStringList sourceRows;
        whenModal<QDialog>([&sourceRows](QDialog* dialog) {
            QCOMPARE(dialog->objectName(), QStringLiteral("ProjImportDlg"));
            QCOMPARE(dialog->findChild<QComboBox*>("projList")->currentText(), QStringLiteral("B"));
            auto* list = dialog->findChild<QListWidget*>("list");
            for (int i = 0; i < list->count(); ++i)
                sourceRows << list->item(i)->text();
            list->selectAll();
            dialog->findChild<QPushButton*>("addBut")->click();
            auto* destination = dialog->findChild<QListWidget*>("destList");
            QCOMPARE(destination->count(), 2);
            destination->item(1)->setText("7777");
            QVERIFY(dialog->findChild<QRadioButton*>("ask")->isChecked());
            dialog->findChild<QDialogButtonBox*>("buttonBox")->button(QDialogButtonBox::Ok)->click();
        });
        QStringList boxes;
        answerBoxes({QMessageBox::No, QMessageBox::Ok}, &boxes);
        actions.action("anim_import_proj")->trigger();
        QCOMPARE(sourceRows, (QStringList{"0 - Standing", "20 - Walking"}));
        QCOMPARE(boxes, (QStringList{"Animation '0' already exists, do you want to replace it?",
                                     "1 animation(s) were imported!"}));
        auto* anims = qobject_cast<CodeFile*>(a->project()->file(dir.filePath("a.air")));
        QCOMPARE(anims->document()->toPlainText(),
                 QStringLiteral("; Old\n[Begin Action 0]\n9,9, 0,0, 1\n\n; Walking\n[Begin Action 7777]\n"
                                "20,0, 0,0, 5\n\n[Begin Action 6]\n6,0, 0,0, 4\n"));
        QCOMPARE(a->animationsEditor()->page()->current(), 1);

        actions.action("editor_snd")->trigger();
        whenModal<QDialog>([](QDialog* dialog) {
            auto* list = dialog->findChild<QListWidget*>("list");
            QCOMPARE(list->item(0)->text(), QStringLiteral("0, 77"));
            list->selectAll();
            dialog->findChild<QPushButton*>("addBut")->click();
            auto* destination = dialog->findChild<QListWidget*>("destList");
            QCOMPARE(destination->item(0)->text(), QStringLiteral("0,77"));
            destination->item(1)->setText("0,0");
            dialog->findChild<QRadioButton*>("replace")->setChecked(true);
            dialog->findChild<QDialogButtonBox*>("buttonBox")->button(QDialogButtonBox::Ok)->click();
        });
        boxes.clear();
        answerBoxes({QMessageBox::Ok}, &boxes);
        actions.action("snd_import_proj")->trigger();
        QCOMPARE(boxes, QStringList{"2 sound(s) were imported!"});
        auto* sounds = qobject_cast<SoundFile*>(a->project()->file(dir.filePath("a.snd")));
        QCOMPARE(sounds->document().count(), 2);
        QCOMPARE(sounds->document().sounds()[1].number, 77);
        QCOMPARE(a->soundsEditor()->page()->current(), 1);

        actions.action("file_close_others")->trigger();
        boxes.clear();
        answerBoxes({QMessageBox::Ok}, &boxes);
        actions.action("snd_import_proj")->trigger();
        QCOMPARE(boxes, QStringList{"You can't import from the destination project!"});

        anims->document()->setModified(false);
        sounds->undoStack()->setClean();
    }

    void exportToHtml()
    {
        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(m_dir.filePath("fighter.def")));
        QAction* action = m_context->actions().action("tools_export_html");
        QVERIFY(action->isEnabled());
        QTemporaryDir out;
        QString asked;
        FileDialogs::setSavePromptOverride([&](const QString& caption, const QString& path, const QString& filter) {
            asked = caption + '|' + QFileInfo(path).fileName() + '|' + filter;
            return out.filePath("page.html");
        });
        whenModal<QDialog>([](QDialog* dialog) {
            QCOMPARE(dialog->objectName(), QStringLiteral("ExpHtmlDlg"));
            QCOMPARE(dialog->findChild<QComboBox*>("lay")->currentText(), QStringLiteral("Default"));
            dialog->findChild<QLineEdit*>("mail")->setText("me@example.com");
            dialog->findChild<QDialogButtonBox*>("buttonBox")->button(QDialogButtonBox::Ok)->click();
        });
        action->trigger();
        FileDialogs::setSavePromptOverride({});
        QCOMPARE(asked, QStringLiteral("Save As|fighter.html|HyperText Markup Language (*.html)"));
        const QString page = QString::fromUtf8(readFile(out.filePath("page.html")));
        QVERIFY(page.contains("<title>Test Fighter</title>"));
        QVERIFY(page.contains("me@example.com"));
        QVERIFY(page.contains("<img src=\"images/x.png\" alt=\"x\"/>"));
    }

    void scriptsFromTheWindow()
    {
        MainWindow window(*m_context);
        window.show();
        QMenu* tools = menu(window, "menu_tools");
        Q_EMIT tools->aboutToShow();
        QAction* quick = nullptr;
        for (QAction* a : tools->actions()) {
            if (a->text() == QStringLiteral("Quick action script"))
                quick = a;
        }
        QVERIFY(quick);
        quick->trigger();

        QVERIFY(window.openProject(m_dir.filePath("fighter.def")));
        whenModal<QDialog>([](QDialog* dialog) {
            QCOMPARE(dialog->objectName(), QStringLiteral("ScriptEd"));
            auto* editor = qobject_cast<ScriptEditorDialog*>(dialog);
            QVERIFY(editor);
            editor->editor()->setPlainText("debugOutput(FF.projectCount() .. FF.currentProject().name)\n"
                                           "FF.emptyProject('mugen', MUGEN.ProjectType.Font)\n");
            editor->run();
            QCOMPARE(editor->output()->toPlainText(), QStringLiteral("1Test Fighter"));
            editor->reject();
        });
        m_context->actions().action("tools_script_editor")->trigger();
        QCOMPARE(window.projectViews().size(), qsizetype(2));
        QCOMPARE(window.currentProjectView()->project()->typeId(), QStringLiteral("font"));
    }

    void breakpoints()
    {
        MainWindow window(*m_context);
        window.show();
        ActionRegistry& actions = m_context->actions();
        QVERIFY(actions.action("breakpoint_add")->isEnabled());
        QVERIFY(!actions.action("debug_start_engine")->isEnabled());
        QVERIFY(!actions.action("debug_breakpoint")->isEnabled());

        QVERIFY(window.openProject(m_dir.filePath("fighter.def")));
        QVERIFY(actions.action("debug_start_emul")->isEnabled());
        QVERIFY(!actions.action("debug_continue")->isEnabled());
        window.switchEditor("states");
        ProjectView* view = window.currentProjectView();
        CodeEditor* editor = view->currentCodeEditor();
        QVERIFY(editor);
        QVERIFY(editor->options().breakpoints);
        const QString file = view->currentCodePage()->property("filePath").toString();

        editor->goToLine(2);
        actions.action("debug_breakpoint")->trigger();
        auto* panels = window.findChild<DebuggerPanels*>();
        QVERIFY(panels);
        QCOMPARE(panels->breakpointRows(),
                 QStringList{QStringLiteral("1|---|%1|2|0|---|0").arg(QDir::toNativeSeparators(file))});
        QVERIFY(editor->breakpointMarks().contains(1));
        QVERIFY(!editor->document()->isModified());
        Q_EMIT editor->breakpointToggled(0);
        QCOMPARE(panels->breakpointRows().size(), 2);

        whenModal<QDialog>([](QDialog* dialog) {
            QCOMPARE(dialog->objectName(), QStringLiteral("BreakpointDlg"));
            dialog->findChild<QComboBox*>("brkType")->setCurrentIndex(1);
            dialog->findChild<QLineEdit*>("brkFunction")->setText("200");
            dialog->findChild<QDialogButtonBox*>("buttonBox")->button(QDialogButtonBox::Ok)->click();
        });
        actions.action("breakpoint_add")->trigger();
        QCOMPARE(panels->breakpointRows().value(2), QStringLiteral("3|200|---|0|0|---|0"));

        QTreeView* list = window.findChild<QTreeView*>("breakpoints");
        list->setCurrentIndex(list->model()->index(0, 0));
        QVERIFY(actions.action("breakpoint_disable")->isEnabled());
        actions.action("breakpoint_disable")->trigger();
        QCOMPARE(editor->breakpointMarks().value(1), CodeEditor::BreakpointMark::Disabled);
        QVERIFY(actions.action("breakpoint_enable")->isEnabled());
        actions.action("breakpoint_remove_all")->trigger();
        QVERIFY(panels->breakpointRows().isEmpty());
        QVERIFY(editor->breakpointMarks().isEmpty());
        actions.action("debug_breakpoint")->trigger();
        QVERIFY(panels->breakpointRows().value(0).startsWith("1|"));
        actions.action("breakpoint_remove_all")->trigger();
    }

    void referenceDocumentation()
    {
        UrlRecorder recorder;
        QDesktopServices::setUrlHandler("file", &recorder, "open");
        MainWindow window(*m_context);
        m_context->actions().action("ref_docs")->trigger();
        QDesktopServices::unsetUrlHandler("file");
        QCOMPARE(recorder.urls.size(), 1);
        const QString path = recorder.urls.first().toLocalFile();
        QVERIFY2(path.endsWith("/docs/en-us/index.html"), qPrintable(path));
        QVERIFY(QFileInfo::exists(path));
        const QString text = readFile(path);
        static const QRegularExpression link(QStringLiteral("href=\"([^\"#]+\\.html)\""));
        int pages = 0;
        for (auto m = link.globalMatch(text); m.hasNext(); ++pages) {
            const QString target = QFileInfo(path).dir().filePath(m.next().captured(1));
            QVERIFY2(QFileInfo::exists(target), qPrintable(target));
        }
        QVERIFY(pages >= 10);
        // The MUGEN reference index must list every controller and trigger page (tools/gen_docs_index.py).
        const QDir reference(QFileInfo(path).dir().filePath("../../engines/mugen/docs/en-us"));
        const QString index = readFile(reference.filePath("index.html"));
        const QStringList common = QDir(reference.filePath("common")).entryList({"*.html"}, QDir::Files);
        QVERIFY(common.size() > 200);
        for (const QString& page : common)
            QVERIFY2(index.contains("\"common/" + page + "\""), qPrintable(page));
    }

    void graphicQuickFind()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("q.def"), "[Info]\nname = \"Q\"\nmugenversion = 1.0\n[Files]\nsprite = q.sff\n"
                                         "anim = q.air\nsound = q.snd\ncmd = q.cmd\ncns = q.cns\n");
        writeFile(dir.filePath("q.cmd"), "[Command]\nname = \"x\"\ncommand = x\n");
        writeFile(dir.filePath("q.cns"), "[Data]\nlife = 1000\n");
        writeFile(dir.filePath("q.air"), "; Standing Animation\n[Begin Action 0]\n0,0, 0,0, 10\n\n"
                                         "; Stand Light Punch\n[Begin Action 200]\n200,1, 0,0, 10\n");
        SpriteDocument sprites;
        Formats::Palette colors(256);
        sprites.palettes() << SpriteDocument::makePalette(1, 1, colors);
        for (const auto& [group, number] : {std::pair(0, 0), std::pair(200, 1)}) {
            SpriteEntry sprite;
            sprite.header.group = std::uint16_t(group);
            sprite.header.number = std::uint16_t(number);
            sprite.pixels =
                SpriteDocument::makePixels(Formats::Image::indexed(2, 2, std::vector<std::uint8_t>(4, 1), colors));
            sprite.palette = sprites.palettes().first();
            sprites.sprites() << sprite;
        }
        QVERIFY(sprites.toSff().save(dir.filePath("q.sff").toStdWString()).ok());
        Formats::SndDocument snd;
        for (const auto& [group, number] : {std::pair(1, 0), std::pair(5, 0)}) {
            Formats::SndSound sound;
            sound.group = group;
            sound.number = number;
            sound.data = Formats::WavFile::fromSamples(std::vector<double>(50, 0.1), 1, 22050, 16, false)->write();
            snd.sounds.push_back(sound);
        }
        QVERIFY(snd.save(dir.filePath("q.snd").toStdWString()).ok());

        MainWindow window(*m_context);
        window.show();
        QVERIFY(window.openProject(dir.filePath("q.def")));
        ActionRegistry& actions = m_context->actions();
        ProjectView* view = window.currentProjectView();
        auto find = [&](const QString& text) {
            actions.action("edit_find_replace")->trigger();
            auto* bar = window.findChild<QuickFindBar*>();
            bar->setFindText(text);
            Q_EMIT bar->findRequested();
        };
        auto nothingFound = [&](const QString& text) {
            bool asked = false;
            whenModal<QMessageBox>([&asked](QMessageBox* box) {
                asked = box->text() == QStringLiteral("Nothing found!");
                box->accept();
            });
            find(text);
            return asked;
        };
        window.switchEditor("sprites");
        find("200,1");
        QCOMPARE(view->spritesEditor()->page()->current(), 1);
        find("0,0");
        QCOMPARE(view->spritesEditor()->page()->current(), 0);
        QVERIFY(nothingFound("200"));
        QVERIFY(nothingFound("0, 0"));
        window.switchEditor("sounds");
        find("5,0");
        QCOMPARE(view->soundsEditor()->page()->current(), 1);
        QVERIFY(nothingFound("5"));
        window.switchEditor("animations");
        find("200");
        QCOMPARE(view->animationsEditor()->page()->current(), 1);
        find("standing animation");
        QCOMPARE(view->animationsEditor()->page()->current(), 0);
        QVERIFY(nothingFound("Stand"));
        window.findChild<QuickFindBar*>()->closeRequested();
        window.closeAllProjects();
    }

    void toolbarPlaces()
    {
        Config& config = m_context->config();
        const bool confirm = config.boolValue("confirm_close", true);
        config.setValue("confirm_close", false);
        config.setValue("main_toolbar", int(Qt::LeftToolBarArea));
        config.setValue("extra_toolbar", int(Qt::TopToolBarArea));
        {
            MainWindow window(*m_context);
            auto* main = window.findChild<QToolBar*>("mainToolBar");
            auto* extra = window.findChild<QToolBar*>("extraToolBar");
            QCOMPARE(window.toolBarArea(main), Qt::LeftToolBarArea);
            QCOMPARE(window.toolBarArea(extra), Qt::TopToolBarArea);
            window.addToolBar(Qt::BottomToolBarArea, extra);
            window.close();
        }
        QCOMPARE(config.intValue("extra_toolbar", 0), int(Qt::BottomToolBarArea));
        QCOMPARE(config.intValue("main_toolbar", 0), int(Qt::LeftToolBarArea));
        config.setValue("main_toolbar", int(Qt::TopToolBarArea));
        config.setValue("extra_toolbar", int(Qt::TopToolBarArea));
        config.setValue("toolbar_break", true);
        config.setValue("toolbar_first", true);
        config.setValue("confirm_close", confirm);
    }

    void emulatorDebugging()
    {
        // A character in an engine folder. Its -1 state counts up var(0) and line 4 is the trigger.
        QTemporaryDir engine;
        QVERIFY(engine.isValid());
        auto write = [&engine](const QString& name, const QByteArray& text) {
            QDir().mkpath(QFileInfo(engine.filePath(name)).absolutePath());
            writeFile(engine.filePath(name), text);
        };
        write("data/common1.cns", "[Statedef 5900]\ntype = S\n[State 5900]\ntype = ChangeState\ntrigger1 = 1\n"
                                  "value = 0\n[Statedef 0]\ntype = S\nphysics = S\nanim = 0\nctrl = 1\n"
                                  "[State 0]\ntype = Null\ntrigger1 = 1\n");
        write("data/fight.def", "[Round]\nmatch.wins = 1\nstart.waittime = 3\nctrl.time = 3\n");
        write("data/mugen.cfg", "[Options]\ntime = 5\n");
        write("stages/stage0.def", "[StageInfo]\nzoffset = 200\n[Camera]\nboundleft = -100\nboundright = 100\n"
                                   "[PlayerInfo]\np1startx = -50\np2startx = 50\n[BGdef]\nspr = stage0.sff\n");
        SpriteDocument sprites;
        Formats::Palette colors(256);
        colors[1] = Formats::Rgba{200, 100, 50, 255};
        sprites.palettes() << SpriteDocument::makePalette(1, 1, colors);
        SpriteEntry sprite;
        sprite.header.group = 0;
        sprite.header.number = 0;
        sprite.header.axisX = 4;
        sprite.header.axisY = 8;
        sprite.pixels =
            SpriteDocument::makePixels(Formats::Image::indexed(8, 8, std::vector<std::uint8_t>(64, 1), colors));
        sprite.palette = sprites.palettes().first();
        sprites.sprites() << sprite;
        QDir().mkpath(engine.filePath("chars/dbg"));
        QVERIFY(sprites.toSff().save(engine.filePath("chars/dbg/dbg.sff").toStdWString()).ok());
        QVERIFY(sprites.toSff().save(engine.filePath("stages/stage0.sff").toStdWString()).ok());
        write("chars/dbg/dbg.air", "[Begin Action 0]\n0,0, 0,0, -1\n");
        write("chars/dbg/dbg.cns", "[Data]\nlife = 1000\n");
        write("chars/dbg/dbg.cmd", "[Statedef -1]\n[State -1, count]\ntype = VarAdd\ntrigger1 = 1\nv = 0\nvalue = 1\n");
        write("chars/dbg/dbg.def", "[Info]\nname = \"Debugged\"\n[Files]\nsprite = dbg.sff\nanim = dbg.air\n"
                                   "cns = dbg.cns\nst = dbg.cns\ncmd = dbg.cmd\nstcommon = common1.cns\n");
        const QString cmd = QDir::cleanPath(engine.filePath("chars/dbg/dbg.cmd"));

        MainWindow window(*m_context);
        window.show();
        ActionRegistry& actions = m_context->actions();
        QVERIFY(window.openProject(engine.filePath("chars/dbg/dbg.def")));
        ProjectView* view = window.currentProjectView();
        EngineWorkspace* debugger = view->debugWorkspace();
        QVERIFY(debugger);
        QVERIFY(actions.action("debug_start_emul")->isEnabled());
        QVERIFY(!actions.action("debug_interrupt")->isEnabled());
        auto* breakpoints = window.findChild<Debug::BreakpointList*>();
        QVERIFY(breakpoints);
        breakpoints->toggle(cmd, 4);

        QStringList failures;
        auto check = [&failures](bool ok, const char* what) {
            if (!ok)
                failures << QString::fromLatin1(what);
        };
        int phase = 0;
        QTimer driver;
        driver.setInterval(5);
        connect(&driver, &QTimer::timeout, this, [&] {
            if (phase != 0 || debugger->debugState() != Debug::SessionState::Interrupted)
                return;
            phase = 1;
            CodeEditor* editor = view->currentCodeEditor();
            check(editor && editor->currentDebugLine() == 3, "current line marked");
            check(view->currentEditor() == QLatin1String("commands"), "the file's editor shown");
            check(actions.action("debug_continue")->isEnabled(), "continue enabled");
            check(actions.action("debug_step_into")->isEnabled(), "step enabled");
            check(!actions.action("debug_interrupt")->isEnabled(), "interrupt disabled");
            auto* variables = view->debugPanel()->findChild<QTreeView*>("debug_variables");
            check(variables && variables->model() && variables->model()->rowCount() >= 3, "variables listed");
            check(variables && variables->isVisible(), "debug panel shown");
            QTreeView* stack = window.findChild<QTreeView*>("callStack");
            check(stack && stack->model() && stack->model()->rowCount() == 2, "call stack");
            check(editor && editor->breakpointMarks().value(3) == CodeEditor::BreakpointMark::Active, "bound");
            if (variables && variables->model()) {
                const QModelIndexList found =
                    variables->model()->match(variables->model()->index(0, 0), Qt::DisplayRole, "var(0)", 1,
                                              Qt::MatchExactly | Qt::MatchRecursive);
                check(!found.isEmpty(), "var(0) found");
                if (!found.isEmpty())
                    variables->setCurrentIndex(found.first());
                actions.action("debug_data_watch1")->trigger();
                QTreeView* watch = window.findChild<QTreeView*>("watch1");
                check(watch && watch->model() && watch->model()->rowCount() == 1, "watched");
            }
            breakpoints->removeAll();
            actions.action("debug_continue")->trigger();
            phase = 2;
        });
        driver.start();
        actions.action("debug_start_emul")->trigger();
        QCOMPARE(view->currentEditor(), view->debugEditor());
        QTRY_COMPARE_WITH_TIMEOUT(phase, 2, 10000);
        driver.stop();
        QVERIFY2(failures.isEmpty(), qPrintable(failures.join(", ")));
        QTRY_COMPARE(debugger->debugState(), Debug::SessionState::Running);
        QVERIFY(actions.action("debug_interrupt")->isEnabled());
        QCOMPARE(view->currentCodeEditor() ? view->currentCodeEditor()->currentDebugLine() : -1, -1);

        actions.action("debug_stop")->trigger();
        QCOMPARE(debugger->debugState(), Debug::SessionState::None);
        QVERIFY(actions.action("debug_start_emul")->isEnabled());
        QVERIFY(!actions.action("debug_stop")->isEnabled());
        QVERIFY(!view->isDebugging());
        QTreeView* watch = window.findChild<QTreeView*>("watch1");
        QCOMPARE(watch->model()->index(0, 1).data().toString(), tr("N/A"));
        actions.action("watch_remove_all")->trigger();
        QCOMPARE(watch->model()->rowCount(), 0);
        window.closeAllProjects();
    }

private:
    AppContext* m_context = nullptr;
    QTemporaryDir m_dir;
};

QTEST_MAIN(MainWindowTest)
#include "MainWindowTest.moc"
