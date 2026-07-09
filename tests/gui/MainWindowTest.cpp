#include "shell/MainWindow.h"
#include "core/actions/ActionRegistry.h"
#include "core/app/AppContext.h"
#include "core/project/Project.h"
#include "core/settings/History.h"
#include "core/widgets/EPanelWidget.h"
#include "shell/ProjectView.h"
#include "shell/StatusBar.h"

#include <QAction>
#include <QFile>
#include <QLabel>
#include <QMenuBar>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTest>
#include <QToolBar>
#include <QToolButton>

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

private:
    AppContext* m_context = nullptr;
    QTemporaryDir m_dir;
};

QTEST_MAIN(MainWindowTest)
#include "MainWindowTest.moc"
