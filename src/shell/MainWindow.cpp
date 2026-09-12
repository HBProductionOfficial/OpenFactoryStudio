#include "shell/MainWindow.h"
#include "shell/DebugView.h"
#include "shell/DebuggerPanels.h"
#include "shell/dialogs/BreakpointDialog.h"

#include "core/Version.h"
#include "core/actions/ActionRegistry.h"
#include "core/actions/MenuSpec.h"
#include "core/anims/AirEdits.h"
#include "core/app/AppContext.h"
#include "core/app/Messages.h"
#include "core/app/Paths.h"
#include "core/app/TaskList.h"
#include "core/code/CodeEditor.h"
#include "core/code/Snippet.h"
#include "core/code/TextSearch.h"
#include "core/debug/Breakpoints.h"
#include "core/engine/Engine.h"
#include "core/engine/EngineManager.h"
#include "core/engine/EngineWorkspace.h"
#include "core/engine/RunConfiguration.h"
#include "core/preset/PresetBuilder.h"
#include "core/preset/PresetEngine.h"
#include "core/project/Project.h"
#include "core/project/ProjectBackup.h"
#include "core/project/ProjectFile.h"
#include "core/project/ProjectWatcher.h"
#include "core/project/ZipArchive.h"
#include "core/script/ScriptFolders.h"
#include "core/script/ScriptHost.h"
#include "core/settings/Config.h"
#include "core/settings/History.h"
#include "core/sounds/SoundFile.h"
#include "core/sprites/SpriteFile.h"
#include "core/text/Encodings.h"
#include "core/theme/ThemeManager.h"
#include "core/widgets/GraphicsView.h"
#include "core/widgets/ImageCanvas.h"
#include "core/widgets/WaveformView.h"
#include "shell/AdvancedPaletteEditor.h"
#include "shell/AdvancedSearchPanel.h"
#include "shell/AnimationPage.h"
#include "shell/AnimationsEditor.h"
#include "shell/BackgroundsEditor.h"
#include "shell/BackgroundsPage.h"
#include "shell/BottomDock.h"
#include "shell/CodePage.h"
#include "shell/ControllerTemplatesWidget.h"
#include "shell/FileDialogs.h"
#include "shell/ImageEditor.h"
#include "shell/MenuBuilder.h"
#include "shell/PalettesPanel.h"
#include "shell/ProjectView.h"
#include "shell/QuickFindBar.h"
#include "shell/SoundPage.h"
#include "shell/SoundsEditor.h"
#include "shell/SpritePage.h"
#include "shell/SpritesEditor.h"
#include "shell/StatusBar.h"
#include "shell/UpdateChecker.h"
#include "shell/WelcomePage.h"
#include "shell/dialogs/AboutDialog.h"
#include "shell/dialogs/CalculatorDialog.h"
#include "shell/dialogs/ClipboardContainer.h"
#include "shell/dialogs/OptionsDialog.h"
#include "shell/dialogs/ProjectDialogs.h"
#include "shell/dialogs/RunChooserDialog.h"
#include "shell/dialogs/ScriptEditorDialog.h"
#include "shell/dialogs/SnippetEditorDialog.h"
#include "shell/dialogs/SnippetFieldsDialog.h"
#include "shell/dialogs/StateControllerDialog.h"
#include "shell/dialogs/VariableLogDialog.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QScreen>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QTabBar>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QUndoStack>
#include <QUrl>
#include <QVBoxLayout>

namespace OFS {

namespace {

const QString kWelcomeTab = QStringLiteral("welcome");

QString workspaceToolBarName(WorkspaceType type)
{
    switch (type) {
    case WorkspaceType::Code:
        return QStringLiteral("code");
    case WorkspaceType::Sound:
        return QStringLiteral("sound");
    case WorkspaceType::Graphics:
    case WorkspaceType::Emulator:
        return QStringLiteral("graphics");
    }
    return {};
}

} // namespace

MainWindow::MainWindow(AppContext& context, QWidget* parent)
    : QMainWindow(parent)
    , m_context(context)
    , m_menus(std::make_unique<MenuBuilder>(context.actions()))
{
    setObjectName(QStringLiteral("MainWindow"));
    setWindowIcon(QIcon(Paths::dataDir() + QStringLiteral("/icons/ofs.png")));

    // Add every action to the window so its shortcut works even outside menus and toolbars.
    for (const QString& id : m_context.actions().ids())
        addAction(m_context.actions().action(id));

    buildMenus();
    buildToolBars();
    buildCentralArea();
    m_status = new StatusBar(this);
    setStatusBar(m_status);
    connect(&m_context.tasks(), &TaskList::changed, this, [this] {
        QStringList names;
        int progress = -1;
        for (const TaskList::Task& task : m_context.tasks().tasks()) {
            names << task.name;
            progress = task.progress;
        }
        m_status->setTasks(names, progress);
    });
    connect(m_status->sectionLabel(2), &EClickableLabel::clicked, this, [this] {
        ProjectView* view = currentProjectView();
        QMenu* encodings = m_menus->submenu(QStringLiteral("menu_encoding"));
        if (view && encodings && view->currentEditorInfo().workspace == WorkspaceType::Code)
            encodings->popup(QCursor::pos());
    });
    connectActions();

    applyInterface();
    restoreWindowGeometry();
    restartAutosave();
    refreshForCurrent();
    if (m_context.config().boolValue(QStringLiteral("show_find_multi")))
        QTimer::singleShot(0, this, &MainWindow::showMultiLineFind);
}

MainWindow::~MainWindow()
{
    // Stop a running engine so its copy of the project can be removed.
    if (m_engineProcess) {
        disconnect(m_engineProcess, nullptr, this, nullptr);
        m_engineProcess->kill();
        m_engineProcess->waitForFinished(3000);
        m_runCopy.remove();
    }
    // The dialog must not report being closed while the window goes away.
    if (m_multiFind) {
        disconnect(m_multiFind, nullptr, this, nullptr);
        delete m_multiFind;
    }
}

// ---------------------------------------------------------------------------------------------
// Construction

void MainWindow::buildMenus()
{
    m_menus->setDynamic(QStringLiteral("recent_projects"),
                        [this](QMenu* menu, QAction* before, QList<QAction*>& created) {
                            populateRecentProjects(menu, before, created);
                        });
    m_menus->setDynamic(QStringLiteral("bg_groups"), [this](QMenu* menu, QAction* before, QList<QAction*>& created) {
        ProjectView* view = currentProjectView();
        BackgroundsEditor* backgrounds = view ? view->backgroundsEditor() : nullptr;
        if (!backgrounds || view->currentEditor() != QLatin1String("backgrounds"))
            return;
        const QStringList names = backgrounds->groupNames();
        if (names.isEmpty())
            return;
        auto* separator = new QAction(menu);
        separator->setSeparator(true);
        menu->insertAction(before, separator);
        created << separator;
        auto* group = new QActionGroup(menu);
        for (int i = 0; i < names.size(); ++i) {
            auto* item = new QAction(names.at(i), menu);
            item->setCheckable(true);
            item->setChecked(i == backgrounds->group());
            item->setActionGroup(group);
            connect(item, &QAction::triggered, this, [backgrounds, i] { backgrounds->setGroup(i); });
            menu->insertAction(before, item);
            created << item;
        }
    });
    m_menus->setDynamic(QStringLiteral("encodings"), [this](QMenu* menu, QAction* before, QList<QAction*>& created) {
        populateEncodings(menu, before, created);
    });
    m_menus->setDynamic(QStringLiteral("tool_scripts"), [this](QMenu* menu, QAction* before, QList<QAction*>& created) {
        QStringList engines;
        for (const Engine* engine : m_context.engines().engines())
            engines << engine->name();
        for (const Script::ToolScript& tool : Script::toolScripts(Paths::dataDir(), engines)) {
            auto* item = new QAction(tool.caption, menu);
            const QString path = tool.path;
            connect(item, &QAction::triggered, this, [this, path] { runToolScript(path); });
            menu->insertAction(before, item);
            created << item;
        }
    });
    m_menus->setDynamic(QStringLiteral("engine_tools"), [this](QMenu* menu, QAction* before, QList<QAction*>& created) {
        ProjectView* view = currentProjectView();
        if (!view)
            return;
        for (const MenuItemSpec& item : view->engine()->toolsMenuItems()) {
            if (QAction* a = m_context.actions().action(item.id)) {
                menu->insertAction(before, a);
                created << a;
            }
        }
    });

    for (const MenuSpec& spec : applicationMenusBeforeEngine())
        menuBar()->addMenu(m_menus->build(spec, this));
    for (Engine* engine : m_context.engines().engines()) {
        for (const MenuSpec& spec : engine->menus()) {
            QMenu* menu = m_menus->build(spec, this);
            menu->setProperty("engine", engine->name());
            menu->setProperty("hiddenMenu", spec.hidden);
            menuBar()->addMenu(menu);
            menu->menuAction()->setVisible(false);
            m_engineMenus << menu;
        }
    }
    for (const MenuSpec& spec : applicationMenusAfterEngine())
        menuBar()->addMenu(m_menus->build(spec, this));
}

void MainWindow::buildToolBars()
{
    m_mainBar = new QToolBar(tr("Main"), this);
    m_mainBar->setObjectName(QStringLiteral("mainToolBar"));
    m_mainBar->setIconSize(QSize(32, 32));
    m_extraBar = new QToolBar(tr("Extra"), this);
    m_extraBar->setObjectName(QStringLiteral("extraToolBar"));
    m_extraBar->setIconSize(QSize(22, 22));
    const Config& config = m_context.config();
    auto area = [&config](const char* key) {
        const int value = config.intValue(QLatin1String(key), Qt::TopToolBarArea);
        return value == Qt::LeftToolBarArea || value == Qt::RightToolBarArea || value == Qt::BottomToolBarArea
                   ? Qt::ToolBarArea(value)
                   : Qt::TopToolBarArea;
    };
    const Qt::ToolBarArea mainArea = area("main_toolbar");
    const Qt::ToolBarArea extraArea = area("extra_toolbar");
    const bool mainFirst = config.boolValue(QStringLiteral("toolbar_first"), true);
    QToolBar* first = mainFirst ? m_mainBar : m_extraBar;
    QToolBar* second = mainFirst ? m_extraBar : m_mainBar;
    addToolBar(mainFirst ? mainArea : extraArea, first);
    if (mainArea == extraArea && config.boolValue(QStringLiteral("toolbar_break"), true))
        addToolBarBreak(mainArea);
    addToolBar(mainFirst ? extraArea : mainArea, second);

    m_editorGroup = new QActionGroup(this);
    m_editorGroup->setExclusive(true);
}

void MainWindow::buildCentralArea()
{
    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("centralArea"));
    m_projectTabs = new QTabBar(central);
    m_projectTabs->setObjectName(QStringLiteral("projectTabs"));
    m_projectTabs->setTabsClosable(true);
    m_projectTabs->setSelectionBehaviorOnRemove(QTabBar::SelectLeftTab);
    m_projectTabs->setMovable(true);
    m_projectTabs->setExpanding(false);
    m_projectTabs->setDocumentMode(true);
    m_stack = new QStackedWidget(central);
    m_welcome = new WelcomePage(m_context.history(), m_stack);
    m_stack->addWidget(m_welcome);

    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_projectTabs);
    layout->addWidget(m_stack, 1);
    setCentralWidget(central);

    m_dock = new BottomDock(this);
    m_dock->hide();
    m_breakpoints = new Debug::BreakpointList(this);
    m_debugPanels = new DebuggerPanels(*m_dock, *m_breakpoints, m_context.actions(), m_context.theme().iconSet(), this);
    connect(m_debugPanels, &DebuggerPanels::editRequested, this, &MainWindow::editBreakpoint);
    AdvancedSearchPanel::Context search;
    search.projects = [this] {
        QList<Project*> projects;
        for (ProjectView* view : projectViews())
            projects << view->project();
        return projects;
    };
    search.currentProject = [this]() -> Project* {
        ProjectView* view = currentProjectView();
        return view ? view->project() : nullptr;
    };
    search.currentFile = [this]() -> QString {
        ProjectView* view = currentProjectView();
        auto* file = view ? qobject_cast<CodeFile*>(view->currentFile()) : nullptr;
        return file ? file->path() : QString();
    };
    search.syntaxFor = [this](const QString& path) -> std::pair<const SyntaxDatabase*, QString> {
        for (ProjectView* view : projectViews()) {
            for (const QString& id : view->editorIds()) {
                if (view->project()->editorFiles(id).contains(path, Qt::CaseInsensitive))
                    return {view->engine()->syntax(), view->engine()->syntaxFileType(*view->project(), id)};
            }
            if (view->project()->filePath().compare(path, Qt::CaseInsensitive) == 0)
                return {view->engine()->syntax(),
                        view->engine()->syntaxFileType(*view->project(), QStringLiteral("definitions"))};
        }
        return {nullptr, QString()};
    };
    search.openLocation = [this](const QString& path, int line, int column) {
        for (ProjectView* view : projectViews()) {
            for (int i = 0; i < m_projectTabs->count(); ++i) {
                if (m_projectTabs->tabData(i).value<QObject*>() != view)
                    continue;
                if (!view->showFileLine(path, line))
                    break;
                m_projectTabs->setCurrentIndex(i);
                if (CodeEditor* editor = view->currentCodeEditor()) {
                    QTextCursor cursor = editor->textCursor();
                    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, column);
                    editor->setTextCursor(cursor);
                }
                return;
            }
        }
    };
    m_dock->advancedSearch()->setContext(search);
    m_quickFind = new QuickFindBar(m_context.config(), this);
    m_quickFind->hide();
    connect(m_quickFind, &QuickFindBar::findRequested, this, [this] { quickFind(0); });
    connect(m_quickFind, &QuickFindBar::replaceRequested, this, [this] { quickFind(1); });
    connect(m_quickFind, &QuickFindBar::replaceAllRequested, this, [this] { quickFind(2); });
    connect(m_quickFind, &QuickFindBar::closeRequested, this, [this] { setQuickFindOpen(false); });

    m_actionList = new QListWidget;
    m_actionList->setObjectName(QStringLiteral("actionList"));
    m_actionListDock = new QDockWidget(tr("Action list"), this);
    m_actionListDock->setObjectName(QStringLiteral("actionListDock"));
    m_actionListDock->setWidget(m_actionList);
    addDockWidget(Qt::RightDockWidgetArea, m_actionListDock);
    m_actionListDock->hide();
    connect(m_actionList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (QAction* a = m_context.actions().action(item->data(Qt::UserRole).toString()))
            a->trigger();
    });

    connect(m_projectTabs, &QTabBar::currentChanged, this, &MainWindow::onCurrentTabChanged);
    connect(m_projectTabs, &QTabBar::tabCloseRequested, this, [this](int index) { closeProject(index); });
    connect(m_projectTabs, &QTabBar::tabMoved, this, [this] { refreshForCurrent(); });
    connect(m_welcome, &WelcomePage::openProjectRequested, this, &MainWindow::openRecentProject);
    connect(m_welcome, &WelcomePage::contributeRequested, this,
            [] { QDesktopServices::openUrl(QUrl(QStringLiteral(OFS_HOMEPAGE))); });
}

void MainWindow::connectActions()
{
    ActionRegistry& actions = m_context.actions();
    Config& config = m_context.config();
    auto on = [&actions, this](const char* id, auto slot) {
        if (QAction* a = actions.action(QString::fromLatin1(id)))
            connect(a, &QAction::triggered, this, slot);
    };

    on("file_new", [this] { newProject(); });
    on("file_open", [this] { openProjectDialog(); });
    on("file_open_archive", [this] { openArchiveDialog(); });
    on("file_open_single", [this] { openSingleFileDialog(); });
    on("file_saveas", [this] { saveProjectAs(); });
    on("file_backup", [this] { createBackup(); });
    on("file_load_backup", [this] { loadBackup(); });
    on("file_close", [this] { closeCurrentProject(); });
    on("file_close_all", [this] { closeAllProjects(); });
    on("file_close_others", [this] { closeOtherProjects(); });
    on("file_welcome", [this] { showWelcomeTab(); });
    on("file_exit", [this] { close(); });

    struct Toggle
    {
        const char* action;
        const char* key;
        bool fallback;
    };
    const Toggle toggles[] = {
        {"view_left_panel", "show_left_panel", true},       {"view_right_panel", "show_right_panel", true},
        {"view_organizer", "show_organizer", false},        {"view_actions", "show_action_list", false},
        {"view_code_bar", "show_code_bar", true},           {"view_rulers", "show_rulers", true},
        {"view_guide_lines", "show_guides", false},         {"view_attack_box", "show_attack_box", true},
        {"view_collision_box", "show_collision_box", true}, {"view_main_cross", "show_cross1", true},
        {"view_secondary_cross", "show_cross2", true},      {"view_extra_cross", "show_cross3", false},
        {"spr_frame_spr", "switch_frame_spr", true},
    };
    for (const Toggle& t : toggles) {
        QAction* a = actions.action(QString::fromLatin1(t.action));
        if (!a)
            continue;
        a->setChecked(config.boolValue(QString::fromLatin1(t.key), t.fallback));
        const QString key = QString::fromLatin1(t.key);
        connect(a, &QAction::toggled, this, [this, key](bool checked) {
            m_context.config().setValue(key, checked);
            for (ProjectView* view : projectViews()) {
                view->setLeftPanelVisible(m_context.config().boolValue(QStringLiteral("show_left_panel"), true));
                view->setRightPanelVisible(m_context.config().boolValue(QStringLiteral("show_right_panel"), true));
                view->setOrganizerVisible(m_context.config().boolValue(QStringLiteral("show_organizer"), false));
            }
            m_actionListDock->setVisible(m_context.config().boolValue(QStringLiteral("show_action_list"), false));
            for (ProjectView* view : projectViews())
                view->reconfigureEditors();
        });
    }

    if (QAction* a = actions.action(QStringLiteral("spr_frame_spr"))) {
        connect(a, &QAction::toggled, this, [this](bool on) {
            ProjectView* view = currentProjectView();
            if (on && view && view->currentEditor() == QLatin1String("sprites"))
                view->showFrameSprite();
        });
    }

    const struct
    {
        const char* action;
        int page;
    } panels[] = {{"view_issues", BottomDock::Issues},
                  {"view_advanced_search", BottomDock::AdvancedSearch},
                  {"view_engine_output", BottomDock::EngineOutput},
                  {"view_debugger", BottomDock::Debugger},
                  {"view_watch", BottomDock::Watch}};
    for (const auto& p : panels) {
        const int page = p.page;
        on(p.action, [this, page] { m_dock->showPage(page); });
    }

    for (Engine* engine : m_context.engines().engines()) {
        for (const EditorInfo& info : engine->editors()) {
            QAction* a = actions.action(info.switchAction);
            if (!a)
                continue;
            m_editorGroup->addAction(a);
            const QString id = info.id;
            connect(a, &QAction::triggered, this, [this, id] { switchEditor(id); });
        }
    }

    on("tools_options", [this] {
        OptionsDialog::showOptions(this, [this] {
            m_context.applySettings();
            restartAutosave();
            const bool watching = m_context.config().boolValue(QStringLiteral("enable_file_watcher"), true);
            for (ProjectView* view : projectViews()) {
                if (auto* watcher = view->findChild<ProjectWatcher*>())
                    watcher->setEnabled(watching);
            }
            applyInterface();
            for (ProjectView* view : projectViews())
                view->reconfigureEditors();
        });
    });
    on("tools_pal_editor", [this] {
        ProjectView* view = currentProjectView();
        SpritesEditor* sprites = view ? view->spritesEditor() : nullptr;
        SpritePage* page = sprites ? sprites->page() : nullptr;
        AdvancedPaletteEditor editor(m_context, page ? page->file() : nullptr, page ? page->current() : -1,
                                     page ? page->currentPalette() : -1, this);
        editor.exec();
    });
    on("tools_script_editor", [this] { showScriptEditor(); });
    on("debug_breakpoint", [this] {
        ProjectView* view = currentProjectView();
        CodeEditor* editor = view ? view->currentCodeEditor() : nullptr;
        CodePage* page = view ? view->currentCodePage() : nullptr;
        if (editor && page)
            m_breakpoints->toggle(page->property("filePath").toString(), editor->cursorLine());
    });
    on("breakpoint_add", [this] { editBreakpoint(0); });
    on("debug_start_emul", [this] {
        ProjectView* view = currentProjectView();
        if (!view || view->debugEditor().isEmpty())
            return;
        view->setCurrentEditor(view->debugEditor());
        if (EngineWorkspace* debugger = view->debugWorkspace())
            debugger->perform(QStringLiteral("debug_start_emul"));
    });
    for (const char* id : {"debug_interrupt", "debug_continue", "debug_start", "debug_stop", "debug_restart",
                           "debug_step_over", "debug_step_into", "debug_step_out", "debug_step_frame"}) {
        const QString action = QString::fromLatin1(id);
        on(id, [this, action] {
            ProjectView* view = currentProjectView();
            if (EngineWorkspace* debugger = view ? view->debugWorkspace() : nullptr)
                debugger->perform(action);
        });
    }
    on("debug_data_breakpoint", [this] { addDataBreakpoint(); });
    on("debug_data_watch1", [this] { addWatch(0); });
    on("debug_data_watch2", [this] { addWatch(1); });
    on("watch_remove", [this] { removeWatch(false); });
    on("watch_remove_all", [this] { removeWatch(true); });
    on("breakpoint_edit", [this] {
        if (const int number = m_debugPanels->selectedBreakpoint())
            editBreakpoint(number);
    });
    on("breakpoint_enable", [this] { m_breakpoints->setEnabled(m_debugPanels->selectedBreakpoint(), true); });
    on("breakpoint_disable", [this] { m_breakpoints->setEnabled(m_debugPanels->selectedBreakpoint(), false); });
    on("breakpoint_remove", [this] { m_breakpoints->remove(m_debugPanels->selectedBreakpoint()); });
    on("breakpoint_remove_all", [this] { m_breakpoints->removeAll(); });
    on("help_about", [this] { AboutDialog::showAbout(this); });
    on("file_run", [this] { runInEngine(); });
    on("view_calculator", [this] { CalculatorDialog::showCalculator(this); });
    on("view_clipboard", [this] { ClipboardContainer::showClipboard(this); });
    on("ref_docs", [] { QDesktopServices::openUrl(QUrl(QStringLiteral(OFS_HOMEPAGE))); });
    m_updates = new UpdateChecker(this);
    on("check_updates", [this] { m_updates->check(true); });
    connect(m_updates, &UpdateChecker::updateAvailable, this,
            [this](const QString& version, const QString& url, const QString&) {
                const auto answer = QMessageBox::information(
                    this, tr("Check updates"),
                    tr("Version %1 is available. Do you want to open the download page?").arg(version),
                    QMessageBox::Yes | QMessageBox::No);
                if (answer == QMessageBox::Yes)
                    QDesktopServices::openUrl(QUrl(url));
            });
    connect(m_updates, &UpdateChecker::upToDate, this,
            [this] { QMessageBox::information(this, tr("Check updates"), tr("You are using the latest version.")); });
    connect(m_updates, &UpdateChecker::failed, this, [this](const QString& message) {
        QMessageBox::warning(this, tr("Check updates"), tr("Could not check for updates: %1").arg(message));
    });
    if (config.boolValue(QStringLiteral("auto_update"), false))
        m_updates->check(false);
    connect(m_welcome, &WelcomePage::getStartedRequested, actions.action(QStringLiteral("ref_docs")),
            &QAction::trigger);
    connectEditActions();
}
void MainWindow::connectEditActions()
{
    ActionRegistry& actions = m_context.actions();
    auto on = [&actions, this](const char* id, auto slot) {
        if (QAction* a = actions.action(QString::fromLatin1(id)))
            connect(a, &QAction::triggered, this, slot);
    };
    auto textTarget = [this]() -> QWidget* {
        QWidget* focus = QApplication::focusWidget();
        if (qobject_cast<QPlainTextEdit*>(focus) || qobject_cast<QLineEdit*>(focus))
            return focus;
        ProjectView* view = currentProjectView();
        return view ? view->currentCodeEditor() : nullptr;
    };
    auto forText = [textTarget](auto plain, auto line) {
        return [textTarget, plain, line] {
            QWidget* w = textTarget();
            if (auto* p = qobject_cast<QPlainTextEdit*>(w))
                (p->*plain)();
            else if (auto* l = qobject_cast<QLineEdit*>(w))
                (l->*line)();
        };
    };
    // Undo and Redo also act on the sprite file shown in the Sprites editor.
    auto spriteUndo = [this]() -> QUndoStack* {
        ProjectView* view = currentProjectView();
        QTabWidget* tabs =
            view ? view->findChild<QTabWidget*>(QStringLiteral("fileTabs_") + view->currentEditor()) : nullptr;
        if (auto* sounds = tabs ? qobject_cast<SoundPage*>(tabs->currentWidget()) : nullptr)
            return sounds->file() ? sounds->file()->undoStack() : nullptr;
        auto* page = tabs ? qobject_cast<SpritePage*>(tabs->currentWidget()) : nullptr;
        if (page && page->imageMode())
            return page->imageEditor()->undoStack();
        return page && page->file() ? page->file()->undoStack() : nullptr;
    };
    // The Animations editor's graphic mode edits the text, so it uses the text's history.
    auto animationPage = [this]() -> AnimationPage* {
        ProjectView* view = currentProjectView();
        QTabWidget* tabs =
            view ? view->findChild<QTabWidget*>(QStringLiteral("fileTabs_") + view->currentEditor()) : nullptr;
        return tabs ? qobject_cast<AnimationPage*>(tabs->currentWidget()) : nullptr;
    };
    // Same for the Backgrounds editor and its definitions text.
    auto backgroundsPage = [this]() -> BackgroundsPage* {
        ProjectView* view = currentProjectView();
        QTabWidget* tabs =
            view ? view->findChild<QTabWidget*>(QStringLiteral("fileTabs_") + view->currentEditor()) : nullptr;
        return tabs ? qobject_cast<BackgroundsPage*>(tabs->currentWidget()) : nullptr;
    };
    auto undoRedo = [textTarget, spriteUndo, animationPage, backgroundsPage](bool redo) {
        return [textTarget, spriteUndo, animationPage, backgroundsPage, redo] {
            QWidget* w = textTarget();
            if (auto* p = qobject_cast<QPlainTextEdit*>(w))
                redo ? p->redo() : p->undo();
            else if (auto* l = qobject_cast<QLineEdit*>(w))
                redo ? l->redo() : l->undo();
            else if (QUndoStack* stack = spriteUndo())
                redo ? stack->redo() : stack->undo();
            else if (AnimationPage* page = animationPage(); page && page->file())
                redo ? page->file()->document()->redo() : page->file()->document()->undo();
            else if (BackgroundsPage* bg = backgroundsPage(); bg && bg->file())
                redo ? bg->file()->document()->redo() : bg->file()->document()->undo();
        };
    };
    on("edit_undo", undoRedo(false));
    on("edit_redo", undoRedo(true));
    // Cut, Copy and Paste also act on the sprite or on the image editor's selection.
    auto spritesEditing = [this]() -> SpritesEditor* {
        ProjectView* view = currentProjectView();
        SpritesEditor* sprites = view ? view->spritesEditor() : nullptr;
        if (!sprites || view->currentEditor() != QLatin1String("sprites") || !sprites->page())
            return nullptr;
        return sprites;
    };
    enum class Clip
    {
        Cut,
        Copy,
        Paste,
    };
    auto animationsEditing = [this]() -> AnimationsEditor* {
        ProjectView* view = currentProjectView();
        AnimationsEditor* animations = view ? view->animationsEditor() : nullptr;
        if (!animations || view->currentEditor() != QLatin1String("animations") || !animations->page() ||
            animations->page()->textMode())
            return nullptr;
        return animations;
    };
    auto soundEditing = [this]() -> SoundPage* {
        ProjectView* view = currentProjectView();
        SoundsEditor* sounds = view ? view->soundsEditor() : nullptr;
        if (!sounds || view->currentEditor() != QLatin1String("sounds"))
            return nullptr;
        return sounds->page();
    };
    auto clipboard = [textTarget, spritesEditing, animationsEditing, soundEditing, forText](Clip clip) {
        std::function<void()> text;
        if (clip == Clip::Cut)
            text = forText(&QPlainTextEdit::cut, &QLineEdit::cut);
        else if (clip == Clip::Copy)
            text = forText(&QPlainTextEdit::copy, &QLineEdit::copy);
        else
            text = forText(&QPlainTextEdit::paste, &QLineEdit::paste);
        return [textTarget, spritesEditing, animationsEditing, soundEditing, text, clip] {
            if (textTarget()) {
                text();
            } else if (SoundPage* sound = soundEditing()) {
                if (clip == Clip::Cut)
                    sound->cutSelection();
                else if (clip == Clip::Copy)
                    sound->copySelection();
                else
                    sound->paste();
            } else if (AnimationsEditor* animations = animationsEditing()) {
                if (clip == Clip::Cut)
                    animations->cutBox();
                else if (clip == Clip::Copy)
                    animations->copyBox();
                else
                    animations->pasteBox();
            } else if (SpritesEditor* sprites = spritesEditing()) {
                if (sprites->imageMode()) {
                    if (clip == Clip::Copy)
                        sprites->copyImage();
                    else if (clip == Clip::Paste)
                        sprites->pasteImage();
                } else if (clip == Clip::Cut) {
                    sprites->cutSprite();
                } else if (clip == Clip::Copy) {
                    sprites->copySprite();
                } else {
                    sprites->pasteSprite();
                }
            }
        };
    };
    on("edit_cut", clipboard(Clip::Cut));
    on("edit_copy", clipboard(Clip::Copy));
    on("edit_paste", clipboard(Clip::Paste));

    auto withCode = [this](auto fn) {
        return [this, fn] {
            if (ProjectView* view = currentProjectView()) {
                if (CodeEditor* editor = view->currentCodeEditor())
                    fn(editor);
            }
        };
    };
    on("code_comment", withCode([](CodeEditor* e) { e->toggleComment(); }));
    on("code_autoindent", withCode([](CodeEditor* e) { e->autoIndentSelection(); }));
    on("code_autoformat", withCode([](CodeEditor* e) { e->autoFormatSelection(); }));
    on("code_fold_all", withCode([](CodeEditor* e) { e->foldAll(); }));
    on("code_unfold_all", withCode([](CodeEditor* e) { e->unfoldAll(); }));
    on("code_goto", withCode([this](CodeEditor* e) { goToLineDialog(e); }));
    on("edit_find_replace", [this] {
        if (!currentProjectView())
            return;
        if (QPlainTextEdit* target = searchTarget()) {
            const QString selected = target->textCursor().selectedText();
            if (!selected.isEmpty() && !selected.contains(QChar::ParagraphSeparator))
                m_quickFind->setFindText(selected);
        }
        setQuickFindOpen(true);
    });
    on("edit_find_replace_multi", [this] { showMultiLineFind(); });
    auto currentTemplates = [this]() -> ControllerTemplatesWidget* {
        ProjectView* view = currentProjectView();
        CodePage* page = view ? view->currentCodePage() : nullptr;
        return page ? qobject_cast<ControllerTemplatesWidget*>(page->sidePanel()) : nullptr;
    };
    auto editTemplate = [this, currentTemplates](bool create) {
        ControllerTemplatesWidget* templates = currentTemplates();
        if (!templates || (!create && templates->selectedName().isEmpty()))
            return;
        StateControllerDialog dialog(currentProjectView()->engine()->syntax(), this);
        if (!create)
            dialog.setText(templates->templateText(templates->selectedName()));
        if (dialog.exec() != QDialog::Accepted)
            return;
        QString error;
        bool ok = false;
        if (create) {
            QDir().mkpath(templates->userDir());
            QString path = FileDialogs::getSaveFileName(this, tr("Save State controller"), templates->userDir(),
                                                        tr("State controller (*.sctrl)"));
            if (path.isEmpty())
                return;
            if (QFileInfo(path).suffix().isEmpty())
                path += QStringLiteral(".sctrl");
            ok = templates->saveTemplateAs(path, dialog.text(), &error);
        } else {
            ok = templates->saveTemplate(templates->selectedName(), dialog.text(), &error);
        }
        if (!ok && !error.isEmpty())
            QMessageBox::warning(this, QApplication::applicationDisplayName(), error);
    };
    for (Engine* engine : m_context.engines().engines()) {
        for (const QString& id : engine->toolActions()) {
            if (QAction* a = actions.action(id))
                connect(a, &QAction::triggered, this, [this, engine, id] { runEngineTool(engine, id); });
        }
    }
    // Sprites editor actions.
    for (const char* id : {"spr_prev",
                           "spr_next",
                           "spr_prev_grp",
                           "spr_next_grp",
                           "spr_pos_left",
                           "spr_pos_up",
                           "spr_pos_right",
                           "spr_pos_down",
                           "spr_delete",
                           "spr_duplicate",
                           "spr_crop",
                           "spr_resize",
                           "spr_delete_grp",
                           "spr_rename_grp",
                           "spr_align",
                           "spr_openimg",
                           "spr_saveimgas",
                           "spr_saveimg_aligned",
                           "spr_split_channels",
                           "spr_add",
                           "spr_export",
                           "spr_import",
                           "org_up",
                           "org_down",
                           "org_up_10",
                           "org_down_10",
                           "org_top",
                           "org_bottom",
                           "org_auto",
                           "spr_align_topleft",
                           "spr_align_topcenter",
                           "spr_align_topright",
                           "spr_align_centerleft",
                           "spr_align_centercenter",
                           "spr_align_centerright",
                           "spr_align_bottomleft",
                           "spr_align_bottomcenter",
                           "spr_align_bottomright",
                           "spr_edit_img",
                           "spr_saveimg",
                           "spr_layer_add",
                           "spr_layer_delete",
                           "spr_layer_duplicate",
                           "spr_layer_mergevis",
                           "spr_layer_mergeall",
                           "spr_layer_save",
                           "spr_layer_play",
                           "spr_view_original",
                           "spr_image_properties",
                           "spr_select_all",
                           "spr_select_none",
                           "spr_select_invert",
                           "spr_image_rotate",
                           "spr_image_fliph",
                           "spr_image_flipv",
                           "spr_tool_canvasmove",
                           "spr_tool_selmove",
                           "spr_tool_rectsel",
                           "spr_tool_freesel",
                           "spr_tool_magicwand",
                           "spr_tool_picker",
                           "spr_tool_pen",
                           "spr_tool_eraser",
                           "spr_tool_remap_pen",
                           "spr_tool_paintbucket",
                           "spr_explorer",
                           "thumb_save"}) {
        const QString action = QString::fromLatin1(id);
        on(id, [this, action] {
            ProjectView* view = currentProjectView();
            if (view && action == QLatin1String("thumb_save") && view->currentEditor() == QLatin1String("animations") &&
                view->animationsEditor()) {
                view->animationsEditor()->perform(action);
                return;
            }
            if (view && action.startsWith(QLatin1String("org_")) && view->performOrganizer(action))
                return;
            const bool done = view && view->spritesEditor() && view->spritesEditor()->perform(action);
            if (!done && action == QLatin1String("spr_edit_img"))
                refreshActionStates(); // nothing to edit: the action unchecks again
        });
    }
    // Palettes panel actions.
    for (const char* id :
         {"pal_clear", "pal_clear_unused", "pal_add", "pal_delete", "pal_delete_duplicates", "pal_open", "pal_save",
          "pal_remap", "pal_undo", "pal_redo", "pal_switch_color", "pal_set_color", "pal_apply_sprite", "pal_prev",
          "pal_next", "pal_switch_count", "pal_prev_grp", "pal_next_grp", "pal_pick_color"}) {
        const QString action = QString::fromLatin1(id);
        on(id, [this, action] {
            ProjectView* view = currentProjectView();
            if (view && view->spritesEditor())
                view->spritesEditor()->palettes()->perform(action);
        });
    }
    // Backgrounds editor actions.
    for (const char* id : {"bgs_add", "bgs_delete", "bgs_duplicate", "bgs_play", "bgs_pause", "bgs_stop",
                           "bgctrls_def_add", "bgctrls_def_delete", "bgctrls_add", "bgctrls_delete"}) {
        const QString action = QString::fromLatin1(id);
        on(id, [this, action] {
            ProjectView* view = currentProjectView();
            if (view && view->backgroundsEditor())
                view->backgroundsEditor()->perform(action);
            refreshActionStates();
        });
    }
    // Sounds editor actions.
    for (const char* id :
         {"snd_add",         "snd_delete",       "snd_duplicate",       "snd_open_sample",  "snd_save_sample",
          "snd_resample",    "snd_play",         "snd_play_loop",       "snd_pause",        "snd_stop",
          "snd_select_all",  "snd_select_none",  "snd_remove_selected", "snd_fill_silence", "snd_fade_in",
          "snd_fade_out",    "snd_save_changes", "snd_discard_changes", "snd_tool_offset",  "snd_tool_zoom",
          "snd_tool_select", "snd_tool_move",    "snd_tool_hresize",    "snd_tool_vresize", "snd_prev",
          "snd_next",        "snd_delete_grp",   "snd_rename_grp",      "snd_export",       "snd_import",
          "snd_prev_grp",    "snd_next_grp"}) {
        const QString action = QString::fromLatin1(id);
        on(id, [this, action] {
            ProjectView* view = currentProjectView();
            if (view && view->soundsEditor())
                view->soundsEditor()->perform(action);
            refreshActionStates();
        });
    }
    auto exclusive = [this, &actions](std::initializer_list<const char*> ids) {
        auto* group = new QActionGroup(this);
        group->setExclusive(true);
        for (const char* id : ids) {
            if (QAction* a = actions.action(QString::fromLatin1(id)))
                group->addAction(a);
        }
    };
    exclusive({"snd_tool_offset", "snd_tool_zoom", "snd_tool_select", "snd_tool_move", "snd_tool_hresize",
               "snd_tool_vresize"});
    exclusive({"snd_play", "snd_play_loop", "snd_pause", "snd_stop"});
    exclusive({"bgs_play", "bgs_pause", "bgs_stop"});
    if (QAction* stop = actions.action(QStringLiteral("bgs_stop")))
        stop->setChecked(true);
    if (QAction* select = actions.action(QStringLiteral("snd_tool_select")))
        select->setChecked(true);
    // Animations editor actions.
    for (const char* id : {"anim_add",           "anim_delete",          "anim_duplicate",
                           "anim_code",          "anim_save_gif",        "anim_play",
                           "anim_stop",          "anim_editcode",        "anim_frame_add",
                           "anim_frame_delete",  "anim_frameall_offset", "anim_frameall_time",
                           "anim_frameall_flip", "anim_frameall_trans",  "anim_frameall_scale",
                           "anim_frame_align",   "anim_box_add_attack",  "anim_box_add_collision",
                           "anim_box_del",       "anim_box_delall",      "anim_box_delall_anim",
                           "anim_box_type",      "anim_box_default",     "anim_box_prev",
                           "anim_box_allinst",   "anim_optimize",        "anim_pos_left",
                           "anim_pos_up",        "anim_pos_right",       "anim_pos_down",
                           "anim_onion_find",    "anim_prev_anim",       "anim_next_anim",
                           "anim_prev_frame",    "anim_next_frame"}) {
        const QString action = QString::fromLatin1(id);
        on(id, [this, action] {
            ProjectView* view = currentProjectView();
            if (view && view->animationsEditor())
                view->animationsEditor()->perform(action);
            refreshActionStates();
        });
    }
    on("anim_import_proj", [this] {
        ProjectView* view = currentProjectView();
        if (!view || !view->animationsEditor())
            return;
        QList<AnimationsEditor::ImportSource> sources;
        for (ProjectView* other : projectViews()) {
            AnimationsEditor* animations = other->animationsEditor();
            if (other == view || !animations || !animations->page())
                continue;
            AnimationsEditor::ImportSource source;
            source.name = other->project()->name();
            source.lines = AirEdits::lines(animations->page()->file()->document());
            if (SpritesEditor* sprites = other->spritesEditor(); sprites && sprites->page())
                source.sprites = &sprites->page()->file()->document();
            sources << source;
        }
        view->animationsEditor()->importFromProject(sources);
    });
    on("snd_import_proj", [this] {
        ProjectView* view = currentProjectView();
        if (!view || !view->soundsEditor())
            return;
        QList<QPair<QString, const SoundDocument*>> sources;
        for (ProjectView* other : projectViews()) {
            SoundsEditor* sounds = other->soundsEditor();
            if (other != view && sounds && sounds->page())
                sources << qMakePair(other->project()->name(), &sounds->page()->file()->document());
        }
        view->soundsEditor()->importFromProject(sources);
    });
    on("spr_import_proj", [this] {
        ProjectView* view = currentProjectView();
        if (!view || !view->spritesEditor())
            return;
        QList<QPair<QString, const SpriteDocument*>> sources;
        for (ProjectView* other : projectViews()) {
            SpritesEditor* sprites = other->spritesEditor();
            if (other != view && sprites && sprites->page())
                sources << qMakePair(other->project()->name(), &sprites->page()->file()->document());
        }
        view->spritesEditor()->importFromProject(sources);
    });
    auto* tools = new QActionGroup(this);
    tools->setExclusive(true);
    for (const char* id :
         {"spr_tool_canvasmove", "spr_tool_selmove", "spr_tool_rectsel", "spr_tool_freesel", "spr_tool_magicwand",
          "spr_tool_picker", "spr_tool_pen", "spr_tool_eraser", "spr_tool_remap_pen", "spr_tool_paintbucket"}) {
        if (QAction* a = actions.action(QString::fromLatin1(id)))
            tools->addAction(a);
    }
    on("st_var_list", [this] {
        for (ProjectView* view : projectViews())
            view->refreshOutlines();
    });
    on("st_var_log", [this] {
        ProjectView* view = currentProjectView();
        CodePage* page = view ? view->currentCodePage() : nullptr;
        if (!page || page->editor()->isReadOnly())
            return;
        const VariableUsage usage = view->engine()->variableUsage(
            *view->project(), page->property("editorId").toString(), page->editor()->toPlainText());
        if (usage.isEmpty())
            return;
        VariableLogDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted)
            page->insertText(variableLog(usage, dialog.options()));
    });
    on("st_sctrl_new", [editTemplate] { editTemplate(true); });
    on("st_sctrl_edit", [editTemplate] { editTemplate(false); });
    on("st_sctrl_delete", [this, currentTemplates] {
        ControllerTemplatesWidget* templates = currentTemplates();
        const QString name = templates ? templates->selectedName() : QString();
        if (name.isEmpty())
            return;
        if (m_context.config().boolValue(QStringLiteral("confirm_delete"), true) &&
            QMessageBox::question(this, QApplication::applicationDisplayName(),
                                  tr("Delete the state controller '%1'?").arg(name)) != QMessageBox::Yes)
            return;
        QString error;
        if (!templates->deleteTemplate(name, &error))
            QMessageBox::warning(this, QApplication::applicationDisplayName(), error);
    });
    auto editSnippet = [this](bool create) {
        ProjectView* view = currentProjectView();
        CodePage* page = view ? view->currentCodePage() : nullptr;
        if (!page)
            return;
        Engine* engine = view->engine();
        Snippet snippet;
        if (!create) {
            const Snippet* current = engine->snippets().find(page->currentSnippet());
            if (!current)
                return;
            snippet = *current;
        }
        SnippetEditorDialog dialog(snippet, engine->userSnippetDir(), engine->syntax(), this);
        connect(&dialog, &SnippetEditorDialog::saved, this, [this, engine](const QString& path) {
            engine->reloadSnippets();
            for (ProjectView* v : projectViews()) {
                if (v->engine() == engine)
                    v->refreshSnippets();
            }
            if (ProjectView* current = currentProjectView()) {
                if (CodePage* p = current->currentCodePage())
                    p->setCurrentSnippet(QFileInfo(path).completeBaseName());
            }
        });
        dialog.exec();
    };
    on("code_snippet_new", [editSnippet] { editSnippet(true); });
    on("code_snippet_edit", [editSnippet] { editSnippet(false); });
    on("code_snippet_insert", [this] {
        ProjectView* view = currentProjectView();
        CodePage* page = view ? view->currentCodePage() : nullptr;
        if (!page || page->editor()->isReadOnly())
            return;
        const Snippet* snippet = view->engine()->snippets().find(page->currentSnippet());
        if (!snippet)
            return;
        QHash<QString, QString> values;
        if (!snippet->fields.isEmpty() && !SnippetFieldsDialog::ask(this, *snippet, &values))
            return;
        page->insertSnippet(*snippet, values);
    });
    auto withPage = [this](bool forward, bool deprecations) {
        return [this, forward, deprecations] {
            if (ProjectView* view = currentProjectView()) {
                if (CodePage* page = view->currentCodePage())
                    page->goToIssue(forward, deprecations);
            }
        };
    };
    on("code_next_error", withPage(true, false));
    on("code_prev_error", withPage(false, false));
    on("code_next_deprecation", withPage(true, true));
    on("code_prev_deprecation", withPage(false, true));
    if (QAction* codeBar = actions.action(QStringLiteral("view_code_bar"))) {
        connect(codeBar, &QAction::toggled, this, [this](bool visible) {
            for (ProjectView* view : projectViews())
                view->setCodeToolBarsVisible(visible);
        });
    }
    connect(m_dock->issuesView(), &QTableView::doubleClicked, this, [this](const QModelIndex& index) {
        const QModelIndex first = index.siblingAtColumn(0);
        auto* view = qobject_cast<ProjectView*>(first.data(Qt::UserRole + 1).value<QObject*>());
        if (!view)
            return;
        for (int i = 0; i < m_projectTabs->count(); ++i) {
            if (m_projectTabs->tabData(i).value<QObject*>() == view)
                m_projectTabs->setCurrentIndex(i);
        }
        view->showFileLine(first.data(Qt::UserRole).toString(), first.data(Qt::UserRole + 2).toInt() + 1);
    });
    auto currentWorkspace = [this]() -> SpritePage* {
        ProjectView* view = currentProjectView();
        QTabWidget* tabs =
            view ? view->findChild<QTabWidget*>(QStringLiteral("fileTabs_") + view->currentEditor()) : nullptr;
        return tabs ? qobject_cast<SpritePage*>(tabs->currentWidget()) : nullptr;
    };
    auto zoom = [this, currentWorkspace, animationPage](void (GraphicsView::*graphic)(), void (ImageCanvas::*image)(),
                                                        std::function<void(CodeEditor*)> code) {
        return [this, currentWorkspace, animationPage, graphic, image, code] {
            if (AnimationPage* animation = animationPage(); animation && !animation->textMode()) {
                (animation->view()->*graphic)();
            } else if (SpritePage* page = currentWorkspace()) {
                if (!page->imageMode())
                    (page->view()->*graphic)();
                else if (image)
                    (page->imageEditor()->canvas()->*image)();
            } else if (code) {
                if (ProjectView* view = currentProjectView(); view && view->currentCodeEditor())
                    code(view->currentCodeEditor());
            }
        };
    };
    on("view_zoomin", zoom(&GraphicsView::zoomIn, &ImageCanvas::zoomIn, [](CodeEditor* e) { e->zoomIn(); }));
    on("view_zoomout", zoom(&GraphicsView::zoomOut, &ImageCanvas::zoomOut, [](CodeEditor* e) { e->zoomOut(); }));
    on("view_zoomreset",
       zoom(&GraphicsView::zoomReset, &ImageCanvas::zoomReset, [](CodeEditor* e) { e->resetZoom(); }));
    on("view_zoomfit", zoom(&GraphicsView::zoomFit, &ImageCanvas::zoomFit, {}));
    on("view_center_origin", zoom(&GraphicsView::centerOrigin, nullptr, {}));

    // Saving
    on("file_save", [this] {
        ProjectView* view = currentProjectView();
        if (!view)
            return;
        for (ProjectFile* file : view->project()->files()) {
            if (file->isUntitled() && file->isModified() && !saveFileAs(file))
                return;
        }
        for (ProjectFile* file : view->project()->files()) {
            if (file->isModified() && !file->isUntitled() && !saveNow(file))
                return;
        }
    });
    on("editor_save", [this] {
        if (ProjectView* view = currentProjectView())
            saveFile(view->currentFile(), false);
    });
    auto forCurrentEditor = [this](void (MainWindow::*fn)(const QString&)) {
        return [this, fn] {
            if (ProjectView* view = currentProjectView())
                (this->*fn)(view->currentEditor());
        };
    };
    on("editor_new", forCurrentEditor(&MainWindow::newEditorFile));
    on("editor_open", forCurrentEditor(&MainWindow::openEditorFile));
    on("editor_saveas", forCurrentEditor(&MainWindow::saveEditorFileAs));
    for (Engine* engine : m_context.engines().engines()) {
        for (const EditorInfo& info : engine->editors()) {
            if (info.actionPrefix.isEmpty())
                continue;
            const QString editor = info.id;
            auto action = [&actions, &info](const char* suffix) {
                return actions.action(info.actionPrefix + QLatin1Char('_') + QLatin1String(suffix));
            };
            if (QAction* a = action("new"))
                connect(a, &QAction::triggered, this, [this, editor] { newEditorFile(editor); });
            if (QAction* a = action("open"))
                connect(a, &QAction::triggered, this, [this, editor] { openEditorFile(editor); });
            if (QAction* a = action("save"))
                connect(a, &QAction::triggered, this, [this, editor] { saveEditorFile(editor, false); });
            if (QAction* a = action("saveas"))
                connect(a, &QAction::triggered, this, [this, editor] { saveEditorFileAs(editor); });
            if (QAction* a = action("saveas_utf8"))
                connect(a, &QAction::triggered, this, [this, editor] { saveEditorFile(editor, true); });
        }
    }
}

QPlainTextEdit* MainWindow::searchTarget() const
{
    if (ProjectView* view = currentProjectView()) {
        if (CodeEditor* editor = view->currentCodeEditor())
            return editor;
    }
    return nullptr;
}

void MainWindow::setQuickFindOpen(bool open)
{
    m_context.config().setValue(QStringLiteral("show_quicksearch"), open);
    if (open) {
        m_quickFind->activate();
        return;
    }
    m_quickFind->hide();
    if (QPlainTextEdit* target = searchTarget())
        target->setFocus();
}

// 0 find, 1 replace, 2 replace all
void MainWindow::quickFind(int operation)
{
    QPlainTextEdit* target = searchTarget();
    const TextSearch search(m_quickFind->findText(), m_quickFind->options());
    if (!target || search.isEmpty())
        return;
    if (!search.isValid()) {
        m_quickFind->setNotFound(true);
        m_quickFind->setToolTip(search.errorString());
        return;
    }
    m_quickFind->setToolTip(QString());
    bool found = false;
    switch (operation) {
    case 0:
        found = TextSearchOps::find(target, search, m_quickFind->isBackward(), m_quickFind->isCircular());
        break;
    case 1:
        found = TextSearchOps::replace(target, search, m_quickFind->replaceText(), m_quickFind->isBackward(),
                                       m_quickFind->isCircular());
        break;
    default:
        found = TextSearchOps::replaceAll(target, search, m_quickFind->replaceText()) > 0;
        break;
    }
    m_quickFind->setNotFound(!found);
    if (!found)
        QMessageBox::information(this, tr("Find"), tr("Nothing found!"));
}

void MainWindow::showMultiLineFind()
{
    if (!m_multiFind) {
        m_multiFind = new MultiLineFindDialog(this);
        m_multiFind->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_multiFind, &MultiLineFindDialog::findRequested, this,
                [this](bool backward) { multiLineFind(0, backward); });
        connect(m_multiFind, &MultiLineFindDialog::replaceRequested, this,
                [this](bool backward) { multiLineFind(1, backward); });
        connect(m_multiFind, &MultiLineFindDialog::replaceAllRequested, this, [this] { multiLineFind(2, false); });
        connect(m_multiFind, &QDialog::finished, this,
                [this] { m_context.config().setValue(QStringLiteral("show_find_multi"), false); });
    }
    if (QPlainTextEdit* target = searchTarget()) {
        const QString selected = target->textCursor().selectedText();
        if (!selected.isEmpty())
            m_multiFind->setFindText(QString(selected).replace(QChar::ParagraphSeparator, QLatin1Char('\n')));
    }
    m_context.config().setValue(QStringLiteral("show_find_multi"), true);
    m_multiFind->show();
    m_multiFind->raise();
    m_multiFind->activateWindow();
}

// 0 find, 1 replace, 2 replace all
void MainWindow::multiLineFind(int operation, bool backward)
{
    QPlainTextEdit* target = searchTarget();
    if (!target || !m_multiFind)
        return;
    SearchOptions options;
    options.caseSensitive = m_multiFind->isCaseSensitive();
    const TextSearch search(m_multiFind->findText(), options);
    bool found = false;
    switch (operation) {
    case 0:
        found = TextSearchOps::find(target, search, backward, true);
        break;
    case 1:
        found = TextSearchOps::replace(target, search, m_multiFind->replaceText(), backward, true);
        break;
    default:
        found = TextSearchOps::replaceAll(target, search, m_multiFind->replaceText()) > 0;
        break;
    }
    if (!found)
        QApplication::beep();
}

void MainWindow::goToLineDialog(CodeEditor* editor)
{
    QInputDialog dialog(this);
    dialog.setObjectName(QStringLiteral("GoToDlg"));
    dialog.setWindowTitle(tr("Number"));
    dialog.setInputMode(QInputDialog::IntInput);
    dialog.setIntRange(1, 99999999);
    dialog.setIntValue(editor->cursorLine());
    if (dialog.exec() == QDialog::Accepted)
        editor->goToLine(dialog.intValue());
}

void MainWindow::runInEngine()
{
    ProjectView* view = currentProjectView();
    if (!view)
        return;
    if (m_engineProcess) {
        QMessageBox::information(this, tr("Run in engine"), tr("The engine is already running."));
        return;
    }
    Engine* engine = view->engine();
    const QList<RunConfiguration> runs = RunConfiguration::load(m_context.engineConfig(engine->name()));
    if (runs.isEmpty()) {
        QMessageBox::warning(this, tr("Warning!"),
                             tr("Oops, you need to add an engine first. Do it in Tools / Options."));
        return;
    }
    int index = 0;
    const QString lastKey = QStringLiteral("last_") + engine->displayName();
    if (runs.size() > 1) {
        RunChooserDialog chooser(runs, m_context.config().intValue(lastKey, 0), this);
        if (chooser.exec() != QDialog::Accepted)
            return;
        index = chooser.selectedIndex();
        m_context.config().setValue(lastKey, index);
    }
    const RunConfiguration& run = runs.at(index);
    if (run.executable.trimmed().isEmpty() || !QFileInfo(run.executable).isFile()) {
        QMessageBox::critical(this, tr("Error!"), tr("Executable not found!"));
        return;
    }
    const QString engineDir = QFileInfo(run.executable).absolutePath();
    RunCopy copy = engine->prepareRun(*view->project(), engineDir);
    if (!copy.isValid()) {
        QMessageBox::warning(this, tr("Run in engine"), copy.error);
        return;
    }
    // Run in place: the engine reads the project's own files, so they are saved first.
    if (copy.inPlace) {
        waitForTasks();
        QString error;
        if (!view->project()->save(&error)) {
            QMessageBox::warning(this, tr("Run in engine"), error);
            return;
        }
    }
    QString parameters = run.parameters;
    parameters.replace(QStringLiteral("%0"), copy.name);

    auto* process = new QProcess(this);
    process->setProgram(run.executable);
    process->setArguments(QProcess::splitCommand(parameters));
    process->setWorkingDirectory(engineDir);
    process->setProcessChannelMode(QProcess::MergedChannels);
    m_engineProcess = process;
    m_runCopy = copy;
    m_runLog = engine->runLogFile(engineDir);
    m_runStarted = QDateTime::currentDateTime().addSecs(-1);
    m_runOutput.clear();
    connect(process, &QProcess::readyRead, this,
            [this, process] { m_runOutput += QString::fromLocal8Bit(process->readAll()); });
    connect(process, &QProcess::finished, this, [this, process] { finishEngineRun(process); });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        const QString message = process->errorString();
        finishEngineRun(process);
        QMessageBox::critical(this, tr("Error!"), message);
    });
    process->start();
}

void MainWindow::runEngineTool(Engine* engine, const QString& actionId)
{
    ProjectView* view = currentProjectView();
    if (!view || view->engine() != engine)
        return;
    ToolContext context;
    context.parent = this;
    context.project = view->project();
    for (ProjectView* v : projectViews())
        context.projects << v->project();
    context.icon = [this](const QString& name) {
        return m_context.theme().iconSet().icon(name);
    };
    if (CodePage* page = view->currentCodePage(); page && !page->editor()->isReadOnly()) {
        QPointer<CodePage> target = page;
        context.insertText = [target](const QString& text) {
            if (target)
                target->insertText(text);
        };
    }
    QPointer<ProjectView> target = view;
    context.insertInEditor = [target](const QString& editorId, const QString& text) {
        if (!target)
            return;
        target->setCurrentEditor(editorId);
        if (CodePage* page = target->currentCodePage(); page && !page->editor()->isReadOnly())
            page->insertText(text);
    };
    if (SpritesEditor* sprites = view->spritesEditor(); sprites && sprites->page())
        context.spriteIndex = sprites->page()->current();
    context.saveFileName = [this](const QString& caption, const QString& path, const QString& filter) {
        return FileDialogs::getSaveFileName(this, caption, path, filter);
    };
    engine->runTool(actionId, context);
}

void MainWindow::finishEngineRun(QProcess* process)
{
    if (process != m_engineProcess)
        return;
    m_runOutput += QString::fromLocal8Bit(process->readAll());
    QString logPath = m_runLog;
    if (QFileInfo(logPath).isDir()) {
        const QFileInfoList files = QDir(logPath).entryInfoList(QDir::Files, QDir::Time);
        logPath = files.isEmpty() ? QString() : files.first().absoluteFilePath();
    }
    QString text = m_runOutput;
    QFile log(logPath);
    if (!logPath.isEmpty() && QFileInfo(logPath).lastModified() >= m_runStarted && log.open(QIODevice::ReadOnly))
        text = QString::fromLocal8Bit(log.readAll());
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    if (!text.isEmpty())
        m_dock->engineOutput()->setPlainText(text);
    m_runCopy.remove();
    m_runCopy = RunCopy();
    m_engineProcess = nullptr;
    process->deleteLater();
}

void MainWindow::refreshIssues()
{
    auto* model = qobject_cast<QStandardItemModel*>(m_dock->issuesView()->model());
    if (!model)
        return;
    model->removeRows(0, model->rowCount());
    const IconSet& icons = m_context.theme().iconSet();
    for (ProjectView* view : projectViews()) {
        const auto& all = view->issues();
        for (auto it = all.cbegin(); it != all.cend(); ++it) {
            for (const Issue& issue : it.value()) {
                QString icon = QStringLiteral("error");
                if (issue.severity == Issue::Severity::Warning || issue.severity == Issue::Severity::Deprecation)
                    icon = QStringLiteral("warning");
                else if (issue.severity == Issue::Severity::Information)
                    icon = QStringLiteral("information");
                auto* message = new QStandardItem(icons.icon(icon), issue.message);
                message->setData(it.key(), Qt::UserRole);
                message->setData(QVariant::fromValue<QObject*>(view), Qt::UserRole + 1);
                message->setData(issue.line, Qt::UserRole + 2);
                auto* file = new QStandardItem(QFileInfo(it.key()).fileName());
                file->setToolTip(QDir::toNativeSeparators(it.key()));
                auto* line = new QStandardItem(QString::number(issue.line + 1));
                model->appendRow({message, file, line});
            }
        }
    }
}

void MainWindow::saveFile(ProjectFile* file, bool asUtf8)
{
    if (!file)
        return;
    if (file->isUntitled()) {
        if (saveFileAs(file) && asUtf8)
            saveFile(file, true);
        return;
    }
    if (auto* code = qobject_cast<CodeFile*>(file); code && asUtf8) {
        QString error;
        if (!code->saveAsUtf8(&error))
            QMessageBox::warning(this, tr("Save"), error);
        return;
    }
    saveNow(file);
}

bool MainWindow::saveNow(ProjectFile* file)
{
    if (file->canSaveAsync()) {
        QPointer<MainWindow> self(this);
        file->saveAsync([self](bool ok, const QString& error) {
            if (!ok && self)
                QMessageBox::warning(self, tr("Save"), error);
        });
        return true;
    }
    QString error;
    if (!file->save(&error)) {
        QMessageBox::warning(this, tr("Save"), error);
        return false;
    }
    return true;
}

void MainWindow::waitForTasks()
{
    while (!m_context.tasks().isEmpty())
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 20);
}

void MainWindow::saveEditorFile(const QString& editorId, bool asUtf8)
{
    ProjectView* view = currentProjectView();
    if (!view)
        return;
    if (view->currentEditor() == editorId) {
        saveFile(view->currentFile(), asUtf8);
        return;
    }
    const QList<ProjectFile*> files = view->editorFiles(editorId);
    if (!files.isEmpty())
        saveFile(files.first(), asUtf8);
}

void MainWindow::newEditorFile(const QString& editorId)
{
    if (ProjectView* view = currentProjectView())
        view->newFile(editorId);
}

void MainWindow::openEditorFile(const QString& editorId)
{
    ProjectView* view = currentProjectView();
    if (!view)
        return;
    const EditorInfo info = view->engine()->editor(editorId);
    if (info.id.isEmpty() || (info.workspace != WorkspaceType::Code && editorId != QLatin1String("sprites") &&
                              editorId != QLatin1String("animations") && editorId != QLatin1String("sounds")))
        return;
    const QString key = editorId + QStringLiteral("_path");
    const QString start = m_context.config().value(key, view->project()->directory());
    const QString path = FileDialogs::getOpenFileName(this, tr("Open"), start, info.fileFilter);
    if (path.isEmpty())
        return;
    m_context.config().setValue(key, QFileInfo(path).absolutePath());
    QString error;
    if (!view->openFile(editorId, path, &error))
        QMessageBox::warning(this, tr("Open"), error);
}

void MainWindow::saveEditorFileAs(const QString& editorId)
{
    ProjectView* view = currentProjectView();
    if (!view)
        return;
    saveFileAs(view->currentEditor() == editorId ? view->currentFile() : view->editorFiles(editorId).value(0));
}

bool MainWindow::saveFileAs(ProjectFile* file)
{
    if (auto* sprites = qobject_cast<SpriteFile*>(file)) {
        ProjectView* view = currentProjectView();
        return view && view->spritesEditor() && view->spritesEditor()->saveAs(sprites, this);
    }
    auto* project = file ? qobject_cast<Project*>(file->parent()) : nullptr;
    if (!project)
        return false;
    if (qobject_cast<SoundFile*>(file)) {
        const QString target = FileDialogs::getSaveFileName(this, tr("Save sounds as..."), project->suggestedPath(file),
                                                            tr("Sounds (*.snd);;All files (*.*)"));
        if (target.isEmpty())
            return false;
        const QString folder = QDir(project->directory()).absolutePath() + QLatin1Char('/');
        QString error;
        bool written = false;
        if (!project->archivePath().isEmpty() ||
            QFileInfo(target).absoluteFilePath().startsWith(folder, Qt::CaseInsensitive)) {
            written = file->saveAs(target, &error);
        } else {
            if (QMessageBox::question(this, tr("External file!"),
                                      tr("This file isn't a child of the project directory, so will be ignored on "
                                         "'Run in engine', 'Backup' and 'Save as' functions. Do you want to save it as "
                                         "copy?"),
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
                return false;
            written = file->writeCopy(target, &error);
        }
        if (!written)
            QMessageBox::warning(this, tr("Save sounds as..."), error);
        return written;
    }
    const EditorInfo info = project->engine()->editor(file->editorId());
    const QString title = file->isNameless() ? tr("Save %1 as...").arg(info.title.toLower()) : tr("Save as");
    const QString target = FileDialogs::getSaveFileName(this, title, project->suggestedPath(file), info.fileFilter);
    if (target.isEmpty())
        return false;
    QString error;
    if (!file->saveAs(target, &error)) {
        QMessageBox::warning(this, title, error);
        return false;
    }
    m_context.config().setValue(file->editorId() + QStringLiteral("_path"), QFileInfo(target).absolutePath());
    return true;
}

void MainWindow::updateProjectTab(ProjectView* view)
{
    for (int i = 0; i < m_projectTabs->count(); ++i) {
        if (m_projectTabs->tabData(i).value<QObject*>() == view) {
            const QString marker = view->project()->isModified() ? QStringLiteral("*") : QString();
            m_projectTabs->setTabText(i, view->project()->name() + marker);
            break;
        }
    }
}

bool MainWindow::confirmClose(ProjectView* view)
{
    waitForTasks();
    Project* project = view->project();
    if (!project->isModified())
        return true;
    // One question per kind of changed document. Any "No" keeps the project open.
    QStringList kinds;
    for (const EditorInfo& editor : project->engine()->editors()) {
        for (const ProjectFile* file : project->files()) {
            if (file->editorId() == editor.id && file->isModified() && !kinds.contains(editor.title.toLower()))
                kinds << editor.title.toLower();
        }
    }
    kinds.sort();
    bool discard = true;
    for (const QString& kind : std::as_const(kinds)) {
        if (!askQuestion(this, m_context.config(), QStringLiteral("showmsg_discard"), tr("Discard changes?"),
                         tr("Do you want to discard the changes made in %1?").arg(kind)))
            discard = false;
    }
    return discard;
}

// ---------------------------------------------------------------------------------------------
// Interface (presets and themes)

void MainWindow::applyInterface()
{
    Config& config = m_context.config();
    ThemeManager& theme = m_context.theme();

    QStringList engineDirs;
    for (Engine* engine : m_context.engines().engines())
        engineDirs << engine->dataDir();
    theme.setEngineDataDirs(engineDirs);

    const QString presetName = config.value(QStringLiteral("preset"), QStringLiteral("Neo"));
    Preset::Model model;
    const QString globalPreset = theme.presetPath(presetName);
    if (!globalPreset.isEmpty())
        Preset::PresetEngine::runFile(globalPreset, model);
    for (Engine* engine : m_context.engines().engines()) {
        QString path = engine->userDataDir() + QStringLiteral("/presets/") + presetName + QStringLiteral(".lua");
        if (!QFileInfo::exists(path))
            path = engine->dataDir() + QStringLiteral("/presets/") + presetName + QStringLiteral(".lua");
        if (QFileInfo::exists(path))
            Preset::PresetEngine::runFile(path, model);
    }
    for (const QString& error : std::as_const(model.errors))
        qWarning("preset: %s", qPrintable(error));

    auto choose = [&config](const char* key, const QString& fromPreset, const char* fallback) {
        QString v = config.value(QString::fromLatin1(key));
        if (v.isEmpty())
            v = fromPreset;
        return v.isEmpty() ? QString::fromLatin1(fallback) : v;
    };
    theme.setIconSet(choose("iconset", model.iconset, "Nuvola"));
    if (theme.setStyle(choose("stylesheet", model.stylesheet, "Dark")))
        qApp->setStyleSheet(theme.styleSheet());
    theme.setColorSet(choose("colorset", model.colorset, "Dark"));
    m_context.actions().applyIcons(theme.iconSet());
    m_dock->setArrowIcons(theme.iconSet().icon(QStringLiteral("bottom_up")),
                          theme.iconSet().icon(QStringLiteral("bottom_down")));
    m_quickFind->setIcons(theme.iconSet());

    m_preset = model;
    for (ProjectView* view : projectViews())
        view->applyPreset(m_preset);
    refreshToolBars();
}

// ---------------------------------------------------------------------------------------------
// Projects

ProjectView* MainWindow::currentProjectView() const
{
    if (m_projectTabs->count() == 0)
        return nullptr;
    const QVariant tab = m_projectTabs->tabData(m_projectTabs->currentIndex());
    if (tab.toString() == kWelcomeTab)
        return nullptr;
    return qobject_cast<ProjectView*>(tab.value<QObject*>());
}

QList<ProjectView*> MainWindow::projectViews() const
{
    QList<ProjectView*> views;
    for (int i = 0; i < m_projectTabs->count(); ++i) {
        if (auto* view = qobject_cast<ProjectView*>(m_projectTabs->tabData(i).value<QObject*>()))
            views << view;
    }
    return views;
}

bool MainWindow::isWelcomeCurrent() const
{
    return currentProjectView() == nullptr;
}

void MainWindow::openProjectDialog()
{
    Config& config = m_context.config();
    QStringList filters;
    for (Engine* engine : m_context.engines().engines())
        filters << engine->projectFileFilter();
    QFileDialog::Options options;
    if (!config.boolValue(QStringLiteral("use_native_open"), true))
        options |= QFileDialog::DontUseNativeDialog;
    const QString file =
        QFileDialog::getOpenFileName(this, tr("Open project"), config.value(QStringLiteral("proj_open_path")),
                                     filters.join(QStringLiteral(";;")), nullptr, options);
    if (file.isEmpty())
        return;
    config.setValue(QStringLiteral("proj_open_path"), QFileInfo(file).absolutePath());
    openProject(file);
}

bool MainWindow::openProject(const QString& filePath)
{
    const QString path = QDir::fromNativeSeparators(QFileInfo(filePath).absoluteFilePath());
    if (QFileInfo(path).suffix().compare(QLatin1String("zip"), Qt::CaseInsensitive) == 0)
        return openArchive(path);
    for (int i = 0; i < m_projectTabs->count(); ++i) {
        auto* view = qobject_cast<ProjectView*>(m_projectTabs->tabData(i).value<QObject*>());
        if (view && view->project()->filePath().compare(path, Qt::CaseInsensitive) == 0) {
            m_projectTabs->setCurrentIndex(i);
            return true;
        }
    }

    Engine* engine = nullptr;
    for (Engine* e : m_context.engines().engines()) {
        if (e->canOpen(path)) {
            engine = e;
            break;
        }
    }
    if (!engine) {
        QMessageBox::warning(this, tr("Open project"),
                             tr("'%1' is not a project of any supported engine.").arg(QDir::toNativeSeparators(path)));
        return false;
    }
    QString error;
    Project* project = engine->openProject(path, &error);
    if (!project) {
        QMessageBox::warning(this, tr("Open project"), error);
        m_context.history().remove(path);
        return false;
    }
    addProjectView(project, QString());
    m_context.history().add(project->name(), path);
    m_context.config().setValue(QStringLiteral("project_path"), project->directory());
    backupOnLoad(project);
    return true;
}

bool MainWindow::openRecentProject(const QString& filePath)
{
    if (!QFileInfo::exists(filePath)) {
        if (QMessageBox::question(this, tr("File not found!"),
                                  tr("The file '%1' doesn't exist. Do you want to remove it from the history?")
                                      .arg(QDir::fromNativeSeparators(filePath)),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes)
            m_context.history().remove(filePath);
        return false;
    }
    return openProject(filePath);
}

void MainWindow::openArchiveDialog()
{
    Config& config = m_context.config();
    const QString file =
        FileDialogs::getOpenFileName(this, tr("Open archive"), config.value(QStringLiteral("proj_open_path")),
                                     tr("All supported (*.zip)") + QStringLiteral(";;") + tr("ZIP Archive (*.zip)"));
    if (file.isEmpty())
        return;
    config.setValue(QStringLiteral("proj_open_path"), QFileInfo(file).absolutePath());
    openArchive(file);
}

bool MainWindow::openArchive(const QString& archivePath)
{
    const QString archive = QDir::fromNativeSeparators(QFileInfo(archivePath).absoluteFilePath());
    for (ProjectView* view : projectViews()) {
        if (view->project()->archivePath().compare(archive, Qt::CaseInsensitive) == 0) {
            for (int i = 0; i < m_projectTabs->count(); ++i) {
                if (m_projectTabs->tabData(i).value<QObject*>() == view)
                    m_projectTabs->setCurrentIndex(i);
            }
            return true;
        }
    }
    // The project is the first definitions file in the archive's order.
    QString error;
    const QList<Zip::Entry> entries = Zip::entries(archive, &error);
    QString main;
    for (const Zip::Entry& entry : entries) {
        if (entry.directory)
            continue;
        for (Engine* engine : m_context.engines().engines()) {
            if (engine->projectExtensions().contains(QFileInfo(entry.name).suffix(), Qt::CaseInsensitive)) {
                main = entry.name;
                break;
            }
        }
        if (!main.isEmpty())
            break;
    }
    if (main.isEmpty()) {
        QMessageBox::warning(this, tr("Open archive"),
                             error.isEmpty() ? tr("There is no project in '%1'.").arg(QDir::toNativeSeparators(archive))
                                             : error);
        m_context.history().remove(archive);
        return false;
    }
    QTemporaryDir extracted(QDir::tempPath() + QStringLiteral("/ofs-archive-XXXXXX"));
    if (!extracted.isValid() || !Zip::extract(archive, extracted.path(), &error)) {
        QMessageBox::warning(this, tr("Open archive"), error.isEmpty() ? extracted.errorString() : error);
        return false;
    }
    const QString mainPath = QDir(extracted.path()).filePath(main);
    Engine* engine = nullptr;
    for (Engine* e : m_context.engines().engines()) {
        if (e->canOpen(mainPath)) {
            engine = e;
            break;
        }
    }
    Project* project = engine ? engine->openProject(mainPath, &error) : nullptr;
    if (!project) {
        QMessageBox::warning(this, tr("Open archive"),
                             error.isEmpty() ? tr("'%1' is not a project of any supported engine.").arg(main) : error);
        return false;
    }
    extracted.setAutoRemove(false);
    project->setArchive(archive, extracted.path());
    addProjectView(project, QString());
    const int index = m_projectTabs->currentIndex();
    m_projectTabs->setTabToolTip(index, QDir::toNativeSeparators(archive));
    m_context.history().add(project->name(), archive);
    backupOnLoad(project);
    return true;
}

void MainWindow::openSingleFileDialog()
{
    Config& config = m_context.config();
    QStringList filters;
    for (Engine* engine : m_context.engines().engines()) {
        if (!engine->singleFileFilter().isEmpty())
            filters << engine->singleFileFilter();
    }
    const QStringList files = FileDialogs::getOpenFileNames(
        this, tr("Open file"), config.value(QStringLiteral("proj_open_path")), filters.join(QStringLiteral(";;")));
    if (files.isEmpty())
        return;
    config.setValue(QStringLiteral("proj_open_path"), QFileInfo(files.first()).absolutePath());
    for (const QString& file : files) {
        QString error;
        Project* project = nullptr;
        for (Engine* engine : m_context.engines().engines()) {
            project = engine->openSingleFile(file, &error);
            if (project)
                break;
        }
        if (!project) {
            QMessageBox::warning(this, tr("Open file"), error);
            continue;
        }
        addProjectView(project, project->editorIds().value(0));
        m_projectTabs->setTabToolTip(m_projectTabs->currentIndex(), QDir::toNativeSeparators(project->filePath()));
    }
}

void MainWindow::saveProjectAs()
{
    ProjectView* view = currentProjectView();
    if (!view)
        return;
    Project* project = view->project();
    if (project->isSingleFile()) {
        if (ProjectFile* file = project->file(project->filePath()))
            saveFileAs(file);
        else if (!view->editorFiles(view->editorIds().value(0)).isEmpty())
            saveFileAs(view->editorFiles(view->editorIds().value(0)).first());
        return;
    }
    Config& config = m_context.config();
    QString folder = config.value(QStringLiteral("proj_save_path"));
    if (folder.isEmpty() || !QFileInfo(folder).isDir())
        folder = project->isMemoryOnly() || !project->archivePath().isEmpty() ? QDir::homePath() : project->directory();
    const QString suggestion = QDir(folder).filePath(QFileInfo(project->filePath()).fileName());
    QStringList patterns;
    for (const QString& extension : project->engine()->projectExtensions())
        patterns << QStringLiteral("*.") + extension;
    const QString target = FileDialogs::getSaveFileName(
        this, tr("Save project as..."), suggestion,
        tr("%1 project (%2)").arg(project->engine()->displayName(), patterns.join(QLatin1Char(' '))));
    if (target.isEmpty())
        return;
    config.setValue(QStringLiteral("proj_save_path"), QFileInfo(target).absolutePath());
    QString error;
    if (!project->writeCopyTo(target, &error)) {
        QMessageBox::warning(this, tr("Save project as..."), error);
        return;
    }
    replaceProject(view, target);
}

bool MainWindow::replaceProject(ProjectView* view, const QString& filePath)
{
    int index = -1;
    for (int i = 0; i < m_projectTabs->count(); ++i) {
        if (m_projectTabs->tabData(i).value<QObject*>() == view)
            index = i;
    }
    const QString editor = view->currentEditor();
    closeProject(index, false);
    if (!openProject(filePath))
        return false;
    const int opened = m_projectTabs->currentIndex();
    if (index >= 0 && opened != index) {
        m_projectTabs->moveTab(opened, index);
        m_projectTabs->setCurrentIndex(index);
    }
    if (ProjectView* reopened = currentProjectView(); reopened && reopened->editorIds().contains(editor))
        reopened->setCurrentEditor(editor);
    return true;
}

void MainWindow::restartAutosave()
{
    if (!m_autosave) {
        m_autosave = new QTimer(this);
        m_autosave->setObjectName(QStringLiteral("autosaveTimer"));
        connect(m_autosave, &QTimer::timeout, this, &MainWindow::autosave);
    }
    const int minutes = qBound(0, m_context.config().intValue(QStringLiteral("autosave_interval"), 0), 99);
    m_autosave->stop();
    if (minutes > 0)
        m_autosave->start(minutes * 60 * 1000);
}

void MainWindow::autosave()
{
    // Big binary files save in the background. With "Only save if asynchronous mode is supported" the others are
    // skipped so the window never stalls. Documents without a file are left for the user to name.
    const bool asyncOnly = m_context.config().boolValue(QStringLiteral("autosave_async"), true);
    for (ProjectView* view : projectViews()) {
        for (ProjectFile* file : view->project()->files()) {
            if (!file->isModified() || file->isUntitled())
                continue;
            if (file->canSaveAsync()) {
                const QString path = file->path();
                file->saveAsync([path](bool ok, const QString& error) {
                    if (!ok)
                        qWarning("auto save of %s failed: %s", qPrintable(path), qPrintable(error));
                });
                continue;
            }
            if (asyncOnly)
                continue;
            QString error;
            if (!file->save(&error))
                qWarning("auto save of %s failed: %s", qPrintable(file->path()), qPrintable(error));
        }
    }
}

void MainWindow::fileChangedOnDisk(ProjectFile* changed)
{
    QPointer<ProjectFile> file(changed);
    if (!file || m_reloadQuestions.contains(file))
        return;
    Config& config = m_context.config();
    const QString key = QStringLiteral("showmsg_reload_file");
    const QString setting = config.value(key);
    if (!setting.isEmpty() && setting != QLatin1String("1"))
        return;
    bool reload = setting == QLatin1String("1");
    // With "Always", reload without asking unless that would lose unsaved changes.
    if (!reload || file->isModified()) {
        const QString text = tr("The file '%1' has been modified. Do you want to reload all other instances of "
                                "this file now?")
                                 .arg(file->path());
        m_reloadQuestions.insert(file);
        if (reload) {
            reload = QMessageBox::question(this, tr("File modified"), text, QMessageBox::Yes | QMessageBox::No,
                                           QMessageBox::Yes) == QMessageBox::Yes;
        } else {
            reload = askQuestion(this, config, key, tr("File modified"), text);
        }
        m_reloadQuestions.remove(changed);
        if (!file)
            return;
    }
    if (!reload)
        return;
    QString error;
    if (!file->load(&error))
        QMessageBox::warning(this, tr("File modified"), error);
}

void MainWindow::createBackup()
{
    ProjectView* view = currentProjectView();
    if (!view || view->project()->isMemoryOnly())
        return;
    QString error;
    if (Backup::create(*view->project(), QDateTime::currentDateTime(), false, &error).isEmpty())
        QMessageBox::warning(this, tr("Create backup"), error);
}

void MainWindow::loadBackup()
{
    ProjectView* view = currentProjectView();
    if (!view)
        return;
    Project* project = view->project();
    BackupDialog dialog(Backup::list(*project), this);
    if (dialog.exec() != QDialog::Accepted || dialog.selectedBackup().isEmpty())
        return;
    QString error;
    if (!Backup::restore(*project, dialog.selectedBackup(), &error)) {
        QMessageBox::warning(this, tr("Restore backup"), error);
        return;
    }
    replaceProject(view, project->filePath());
}

void MainWindow::backupOnLoad(Project* project)
{
    if (!project || project->isMemoryOnly() || project->isSingleFile() ||
        !m_context.config().boolValue(QStringLiteral("backup_onload"), false))
        return;
    QString error;
    if (Backup::create(*project, QDateTime::currentDateTime(), true, &error).isEmpty())
        qWarning("backup on load failed: %s", qPrintable(error));
}

Script::Environment MainWindow::scriptEnvironment()
{
    Script::Environment environment;
    environment.engines = &m_context.engines();
    environment.parent = this;
    QString folder = m_context.config().value(QStringLiteral("project_path"));
    if (folder.isEmpty() || !QFileInfo(folder).isDir())
        folder = QDir::homePath();
    environment.projectFolder = QDir::fromNativeSeparators(folder);
    environment.openProject = [this](Project* project) {
        addProjectView(project, project->isMemoryOnly() ? QStringLiteral("definitions") : QString());
        if (!project->isMemoryOnly())
            m_context.history().add(project->name(), project->filePath());
    };
    environment.projects = [this] {
        QList<Project*> projects;
        for (ProjectView* view : projectViews())
            projects << view->project();
        return projects;
    };
    environment.currentProject = [this]() -> Project* {
        ProjectView* view = currentProjectView();
        return view ? view->project() : nullptr;
    };
    return environment;
}

void MainWindow::refreshDebugViews()
{
    ProjectView* view = currentProjectView();
    EngineWorkspace* debugger = view ? view->debugWorkspace() : nullptr;
    const Debug::SessionState session = debugger ? debugger->debugState() : Debug::SessionState::None;
    const bool on = session != Debug::SessionState::None;
    QWidget* panel = view ? view->debugPanel() : nullptr;
    QTreeView* variables = panel ? panel->findChild<QTreeView*>(QStringLiteral("debug_variables")) : nullptr;
    DebugView::showVariables(variables, on ? debugger->debugVariables() : QList<Debug::Variable>());
    DebugView::showStack(m_dock->callStackView(), session == Debug::SessionState::Interrupted
                                                      ? debugger->debugStack()
                                                      : QList<Debug::StackFrame>());
    for (int list = 0; list < 2; ++list) {
        QList<Debug::Variable> rows;
        for (const QString& path : std::as_const(m_watches[list])) {
            const std::optional<Debug::Variable> v = on ? debugger->debugVariable(path) : std::nullopt;
            rows << Debug::Variable{path, path, v ? v->value : tr("N/A"), v ? v->type : QString(), {}};
        }
        DebugView::showWatch(m_dock->watchView(list), rows);
    }
}

void MainWindow::addDataBreakpoint()
{
    ProjectView* view = currentProjectView();
    QWidget* panel = view ? view->debugPanel() : nullptr;
    const QString path =
        DebugView::currentPath(panel ? panel->findChild<QTreeView*>(QStringLiteral("debug_variables")) : nullptr);
    if (path.isEmpty())
        return;
    Debug::Breakpoint breakpoint;
    breakpoint.type = Debug::BreakpointType::Memory;
    breakpoint.address = path;
    BreakpointDialog dialog(this);
    dialog.setBreakpoint(breakpoint, false);
    if (dialog.exec() == QDialog::Accepted)
        m_breakpoints->add(dialog.breakpoint());
}

void MainWindow::addWatch(int list)
{
    ProjectView* view = currentProjectView();
    QWidget* panel = view ? view->debugPanel() : nullptr;
    const QString path =
        DebugView::currentPath(panel ? panel->findChild<QTreeView*>(QStringLiteral("debug_variables")) : nullptr);
    if (path.isEmpty() || list < 0 || list > 1)
        return;
    if (!m_watches[list].contains(path))
        m_watches[list] << path;
    refreshDebugViews();
}

void MainWindow::removeWatch(bool all)
{
    const int list = m_debugPanels->watchList();
    if (all) {
        m_watches[list].clear();
    } else if (QTreeView* view = m_dock->watchView(list); view && view->currentIndex().isValid()) {
        m_watches[list].removeAt(view->currentIndex().row());
    }
    refreshDebugViews();
}

void MainWindow::editBreakpoint(int number)
{
    BreakpointDialog dialog(this);
    const Debug::Breakpoint* existing = number ? m_breakpoints->find(number) : nullptr;
    if (existing)
        dialog.setBreakpoint(*existing, true);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (existing)
        m_breakpoints->update(dialog.breakpoint());
    else
        m_breakpoints->add(dialog.breakpoint());
}

void MainWindow::showScriptEditor()
{
    ScriptEditorDialog editor(m_context, scriptEnvironment(), Paths::dataDir(), this);
    editor.exec();
}

void MainWindow::runToolScript(const QString& path)
{
    const Script::RunResult result = Script::runScriptFile(path, scriptEnvironment());
    if (!result.ok && !result.stopped)
        QMessageBox::critical(this, tr("Script error!"), result.error);
}

void MainWindow::newProject()
{
    ProjectTemplateDialog dialog(m_context.engines(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const ProjectTemplate chosen = dialog.selectedTemplate();
    if (chosen.script.isEmpty())
        return;
    Config& config = m_context.config();
    QString folder = config.value(QStringLiteral("project_path"));
    if (folder.isEmpty() || !QFileInfo(folder).isDir())
        folder = config.value(QStringLiteral("proj_open_path"));
    if (folder.isEmpty() || !QFileInfo(folder).isDir())
        folder = QDir::homePath();
    Script::Environment environment;
    environment.engines = &m_context.engines();
    environment.parent = this;
    environment.projectFolder = QDir::fromNativeSeparators(folder);
    QString error;
    Project* project = Script::runProjectTemplate(chosen.script, environment, &error);
    if (!project) {
        if (!error.isEmpty())
            QMessageBox::warning(this, tr("Project template"), error);
        return;
    }
    addProjectView(project, QStringLiteral("definitions"));
}

void MainWindow::addProjectView(Project* project, const QString& editorId)
{
    auto* view = new ProjectView(project, m_context, m_stack);
    view->setBreakpoints(m_breakpoints);
    view->applyPreset(m_preset);
    view->setLeftPanelVisible(m_context.config().boolValue(QStringLiteral("show_left_panel"), true));
    view->setRightPanelVisible(m_context.config().boolValue(QStringLiteral("show_right_panel"), true));
    view->setOrganizerVisible(m_context.config().boolValue(QStringLiteral("show_organizer"), false));
    const QString firstEditor = editorId.isEmpty() ? m_context.config().value(QStringLiteral("lastEditor")) : editorId;
    if (view->editorIds().contains(firstEditor))
        view->setCurrentEditor(firstEditor);
    connect(view, &ProjectView::currentFileChanged, this, [this, view] {
        if (view == currentProjectView())
            refreshStatusBar();
    });
    connect(view, &ProjectView::modifiedChanged, this, [this, view] { updateProjectTab(view); });
    connect(view, &ProjectView::debugStateChanged, this, [this, view] {
        if (view != currentProjectView())
            return;
        m_debugPanels->setSessionActive(view->isDebugging());
        refreshActionStates();
        refreshDebugViews();
    });
    connect(view, &ProjectView::debugVariablesChanged, this, [this, view] {
        if (view == currentProjectView())
            refreshDebugViews();
    });
    view->setCodeMenu(m_menus->submenu(QStringLiteral("menu_code")));
    connect(view, &ProjectView::issuesChanged, this, [this] {
        refreshIssues();
        refreshStatusBar();
    });
    connect(view, &ProjectView::currentEditorChanged, this, [this, view](const QString& id) {
        m_context.config().setValue(QStringLiteral("lastEditor"), id);
        if (view == currentProjectView())
            refreshForCurrent();
    });
    if (SpritesEditor* sprites = view->spritesEditor()) {
        connect(sprites, &SpritesEditor::imageModeChanged, this, [this, view] {
            if (view == currentProjectView())
                refreshActionStates();
        });
    }
    if (BackgroundsEditor* backgrounds = view->backgroundsEditor()) {
        connect(backgrounds, &BackgroundsEditor::stateChanged, this, [this, view] {
            if (view == currentProjectView()) {
                refreshActionStates();
                refreshStatusBar();
            }
        });
    }
    if (SpritesEditor* sprites = view->spritesEditor()) {
        connect(sprites->palettes(), &PalettesPanel::stateChanged, this, [this, view] {
            if (view == currentProjectView())
                refreshActionStates();
        });
    }
    if (SoundsEditor* sounds = view->soundsEditor()) {
        connect(sounds, &SoundsEditor::stateChanged, this, [this, view] {
            if (view == currentProjectView()) {
                refreshActionStates();
                refreshStatusBar();
            }
        });
    }
    if (AnimationsEditor* animations = view->animationsEditor()) {
        connect(animations, &AnimationsEditor::stateChanged, this, [this, view] {
            if (view == currentProjectView()) {
                refreshActionStates();
                refreshStatusBar();
            }
        });
    }
    auto* watcher = new ProjectWatcher(project, view);
    watcher->setEnabled(m_context.config().boolValue(QStringLiteral("enable_file_watcher"), true));
    connect(watcher, &ProjectWatcher::fileChanged, this, &MainWindow::fileChangedOnDisk);
    connect(project, &Project::filePathChanged, this, [this, view] {
        for (int i = 0; i < m_projectTabs->count(); ++i) {
            if (m_projectTabs->tabData(i).value<QObject*>() == view)
                m_projectTabs->setTabToolTip(i, QDir::toNativeSeparators(view->project()->filePath()));
        }
        m_context.config().setValue(QStringLiteral("project_path"), view->project()->directory());
        refreshActionStates();
    });
    m_stack->addWidget(view);

    const int index = m_projectTabs->addTab(project->name());
    m_projectTabs->setTabData(index, QVariant::fromValue<QObject*>(view));
    if (!project->isMemoryOnly())
        m_projectTabs->setTabToolTip(index, QDir::toNativeSeparators(project->filePath()));
    m_projectTabs->setCurrentIndex(index);
    refreshForCurrent();
}

bool MainWindow::closeProject(int tabIndex, bool ask)
{
    if (tabIndex < 0 || tabIndex >= m_projectTabs->count())
        return false;
    const QVariant tab = m_projectTabs->tabData(tabIndex);
    auto* view = qobject_cast<ProjectView*>(tab.value<QObject*>());
    if (view && ask && !confirmClose(view))
        return false;
    if (view && m_dock->parentWidget() && view->isAncestorOf(m_dock)) {
        m_dock->hide();
        m_dock->setParent(this);
    }
    if (tab.toString() == kWelcomeTab)
        m_welcomeTab = false;
    m_projectTabs->removeTab(tabIndex);
    if (view) {
        m_stack->removeWidget(view);
        view->deleteLater();
    }
    refreshForCurrent();
    QTimer::singleShot(0, this, &MainWindow::refreshIssues);
    return true;
}

void MainWindow::closeCurrentProject()
{
    if (currentProjectView())
        closeProject(m_projectTabs->currentIndex());
}

void MainWindow::closeAllProjects()
{
    for (int i = m_projectTabs->count() - 1; i >= 0; --i) {
        if (m_projectTabs->tabData(i).toString() != kWelcomeTab && !closeProject(i))
            return;
    }
}

void MainWindow::closeOtherProjects()
{
    ProjectView* keep = currentProjectView();
    for (int i = m_projectTabs->count() - 1; i >= 0; --i) {
        auto* view = qobject_cast<ProjectView*>(m_projectTabs->tabData(i).value<QObject*>());
        if (view && view != keep && !closeProject(i))
            return;
    }
}

void MainWindow::showWelcomeTab()
{
    if (projectViews().isEmpty()) {
        refreshForCurrent();
        return;
    }
    if (!m_welcomeTab) {
        m_projectTabs->insertTab(0, tr("Welcome"));
        m_projectTabs->setTabData(0, kWelcomeTab);
        m_welcomeTab = true;
    }
    m_projectTabs->setCurrentIndex(0);
    refreshForCurrent();
}

void MainWindow::switchEditor(const QString& editorId)
{
    ProjectView* view = currentProjectView();
    if (!view)
        return;
    view->setCurrentEditor(editorId);
    // The editor did not change (unsaved image changes were kept), so its switch stays checked.
    if (view->currentEditor() != editorId) {
        if (QAction* a = m_context.actions().action(view->currentEditorInfo().switchAction))
            a->setChecked(true);
    }
}

void MainWindow::onCurrentTabChanged(int)
{
    refreshForCurrent();
}

// ---------------------------------------------------------------------------------------------
// Refreshing the interface for the current project and editor

void MainWindow::refreshForCurrent()
{
    ProjectView* view = currentProjectView();
    m_projectTabs->setVisible(m_projectTabs->count() > 0);
    if (view) {
        m_stack->setCurrentWidget(view);
        view->adoptBottomDock(m_dock);
        view->adoptQuickFind(m_quickFind);
        m_quickFind->setVisible(m_context.config().boolValue(QStringLiteral("show_quicksearch")));
        m_context.actions().setActiveEditorScope(view->currentEditorInfo().scope());
        if (QAction* a = m_context.actions().action(view->currentEditorInfo().switchAction))
            a->setChecked(true);
    } else {
        m_stack->setCurrentWidget(m_welcome);
        m_context.actions().setActiveEditorScope(QString());
    }
    for (QMenu* menu : std::as_const(m_engineMenus)) {
        const bool visible = view && !menu->property("hiddenMenu").toBool() &&
                             menu->property("engine").toString() == view->engine()->name();
        menu->menuAction()->setVisible(visible);
    }
    refreshToolBars();
    refreshActionStates();
    refreshTitle();
    refreshStatusBar();
    refreshActionList();
}

void MainWindow::refreshToolBars()
{
    if (!m_mainBar)
        return;
    ProjectView* view = currentProjectView();
    PresetBuilder builder(m_context.controls(), m_context.actions());

    builder.buildToolBar(m_preset.toolBars.value(view ? QStringLiteral("main") : QStringLiteral("start")), m_mainBar);
    if (m_mainBar->iconSize().width() <= 0)
        m_mainBar->setIconSize(QSize(32, 32));

    for (Engine* engine : m_context.engines().engines()) {
        for (const EditorInfo& info : engine->editors()) {
            if (QAction* a = m_context.actions().action(info.switchAction))
                a->setVisible(view && view->engine() == engine && view->editorIds().contains(info.id));
        }
    }

    if (!view) {
        m_extraBar->hide();
        return;
    }
    const EditorInfo info = view->currentEditorInfo();
    Preset::ToolBar extra = m_preset.toolBars.value(QStringLiteral("extra"));
    extra.name = QStringLiteral("extra");
    for (const QString& name : {workspaceToolBarName(info.workspace), info.id}) {
        const auto it = m_preset.toolBars.constFind(name);
        if (it != m_preset.toolBars.cend())
            extra.actions += it->actions;
    }
    builder.buildToolBar(extra, m_extraBar);
    m_extraBar->setIconSize(QSize(22, 22));
    m_extraBar->setVisible(!extra.actions.isEmpty());
}

void MainWindow::refreshActionStates()
{
    ActionRegistry& actions = m_context.actions();
    ProjectView* view = currentProjectView();
    const bool project = view != nullptr;
    const bool code = project && view->currentEditorInfo().workspace == WorkspaceType::Code;
    auto enable = [&actions](const QString& id, bool on) {
        if (QAction* a = actions.action(id))
            a->setEnabled(on);
    };
    auto enableAll = [&](std::initializer_list<const char*> ids, bool on) {
        for (const char* id : ids)
            enable(QString::fromLatin1(id), on);
    };

    enableAll({"file_save",          "file_saveas",       "file_close",       "file_close_all",
               "file_close_others",  "file_backup",       "file_load_backup", "file_run",
               "edit_undo",          "edit_redo",         "edit_cut",         "edit_copy",
               "edit_paste",         "edit_find_replace", "view_issues",      "view_advanced_search",
               "view_engine_output", "view_debugger",     "view_watch",       "debug_start_engine",
               "debug_start_emul",   "debug_breakpoint",  "editor_new",       "editor_open",
               "editor_save",        "editor_saveas",     "tools_export_html"},
              project);
    EngineWorkspace* debugger = view ? view->debugWorkspace() : nullptr;
    const Debug::SessionState session = debugger ? debugger->debugState() : Debug::SessionState::None;
    const bool interrupted = session == Debug::SessionState::Interrupted;
    enable(QStringLiteral("debug_start_emul"), debugger && session == Debug::SessionState::None);
    enable(QStringLiteral("debug_interrupt"), session == Debug::SessionState::Running);
    enableAll(
        {"debug_continue", "debug_start", "debug_step_over", "debug_step_into", "debug_step_out", "debug_step_frame"},
        interrupted);
    enableAll({"debug_stop", "debug_restart", "debug_data_breakpoint", "debug_data_watch1", "debug_data_watch2"},
              session != Debug::SessionState::None);
    const Project* current = view ? view->project() : nullptr;
    enable(QStringLiteral("file_backup"), current && !current->isMemoryOnly());
    enable(QStringLiteral("file_load_backup"),
           current && !current->isMemoryOnly() && !current->isSingleFile() && current->archivePath().isEmpty());
    enableAll({"code_goto", "code_prev_error", "code_next_error", "code_prev_deprecation", "code_next_deprecation",
               "code_comment", "code_fold_all", "code_unfold_all", "code_autoindent", "code_autoformat",
               "view_code_bar"},
              code);
    if (QMenu* m = m_menus->submenu(QStringLiteral("menu_code")))
        m->menuAction()->setEnabled(code);
    if (QMenu* m = m_menus->submenu(QStringLiteral("menu_encoding")))
        m->menuAction()->setEnabled(code);

    for (Engine* engine : m_context.engines().engines()) {
        for (const EditorInfo& info : engine->editors()) {
            const bool available = view && view->engine() == engine && view->editorIds().contains(info.id);
            for (QAction* a : actions.actionsInScope(info.scope()))
                a->setEnabled(available);
        }
    }
    SpritesEditor* sprites = view ? view->spritesEditor() : nullptr;
    const bool image = sprites && sprites->imageMode();
    enableAll({"spr_saveimg",        "spr_layer_add",        "spr_layer_delete",    "spr_layer_duplicate",
               "spr_layer_mergevis", "spr_layer_mergeall",   "spr_layer_save",      "spr_layer_play",
               "spr_view_original",  "spr_image_properties", "spr_select_all",      "spr_select_none",
               "spr_select_invert",  "spr_image_rotate",     "spr_image_fliph",     "spr_image_flipv",
               "spr_explorer",       "spr_tool_canvasmove",  "spr_tool_selmove",    "spr_tool_rectsel",
               "spr_tool_freesel",   "spr_tool_magicwand",   "spr_tool_picker",     "spr_tool_pen",
               "spr_tool_eraser",    "spr_tool_remap_pen",   "spr_tool_paintbucket"},
              image);
    if (image)
        enable(QStringLiteral("spr_align"), false);
    if (sprites && sprites->palettes()->current() < 0)
        enable(QStringLiteral("pal_switch_count"), false);
    if (QAction* a = actions.action(QStringLiteral("spr_edit_img")))
        a->setChecked(image);
    if (QAction* a = actions.action(QStringLiteral("spr_explorer")))
        a->setChecked(sprites && sprites->explorerShown());

    if (BackgroundsEditor* backgrounds = view ? view->backgroundsEditor() : nullptr) {
        BackgroundsPage* page = backgrounds->page();
        const auto mode = page ? page->mode() : BackgroundsPage::Mode::Stopped;
        const char* checked = mode == BackgroundsPage::Mode::Playing  ? "bgs_play"
                              : mode == BackgroundsPage::Mode::Paused ? "bgs_pause"
                                                                      : "bgs_stop";
        if (QAction* a = actions.action(QString::fromLatin1(checked)); a && !a->isChecked())
            a->setChecked(true);
        const bool playing = mode == BackgroundsPage::Mode::Playing;
        const bool hasElement = page && page->element();
        const bool hasDefinition = page && page->definition();
        enable(QStringLiteral("bgs_delete"), !playing && hasElement);
        enable(QStringLiteral("bgs_duplicate"), !playing && hasElement);
        enable(QStringLiteral("bgctrls_def_add"), !playing && page && page->currentGroup());
        enable(QStringLiteral("bgctrls_def_delete"), !playing && hasDefinition);
        enable(QStringLiteral("bgctrls_add"), !playing && hasDefinition);
        enable(QStringLiteral("bgctrls_delete"), !playing && page && page->controller());
    }
    if (SoundsEditor* sounds = view ? view->soundsEditor() : nullptr; sounds && sounds->page()) {
        const SoundPage::State state = sounds->page()->state();
        const char* checked = state == SoundPage::State::Playing
                                  ? (sounds->page()->looping() ? "snd_play_loop" : "snd_play")
                              : state == SoundPage::State::Paused ? "snd_pause"
                                                                  : "snd_stop";
        if (QAction* a = actions.action(QString::fromLatin1(checked)); a && !a->isChecked())
            a->setChecked(true);
        const WaveformView::Tool tool = sounds->page()->view()->tool();
        const char* toolId = tool == WaveformView::Tool::MoveView           ? "snd_tool_offset"
                             : tool == WaveformView::Tool::Zoom             ? "snd_tool_zoom"
                             : tool == WaveformView::Tool::MoveSelection    ? "snd_tool_move"
                             : tool == WaveformView::Tool::HorizontalResize ? "snd_tool_hresize"
                             : tool == WaveformView::Tool::VerticalResize   ? "snd_tool_vresize"
                                                                            : "snd_tool_select";
        if (QAction* a = actions.action(QString::fromLatin1(toolId)); a && !a->isChecked())
            a->setChecked(true);
    }
    AnimationsEditor* animations = view ? view->animationsEditor() : nullptr;
    AnimationPage* animationPage = animations ? animations->page() : nullptr;
    const bool textMode = animationPage && animationPage->textMode();
    const bool playing = animationPage && animationPage->isPlaying();
    const int drawing = animationPage ? animationPage->drawingBoxes() : 0;
    auto check = [&actions](const char* id, bool on) {
        if (QAction* a = actions.action(QString::fromLatin1(id)))
            a->setChecked(on);
    };
    check("anim_play", playing);
    check("anim_stop", !playing);
    check("anim_editcode", textMode);
    check("anim_box_add_attack", drawing == 1);
    check("anim_box_add_collision", drawing == 2);
    if (textMode)
        enableAll(
            {"anim_box_add_attack", "anim_box_add_collision", "anim_box_del", "anim_box_type", "anim_box_default"},
            false);
    if (playing)
        enableAll({"anim_frame_delete", "anim_pos_left", "anim_pos_up", "anim_pos_right", "anim_pos_down",
                   "anim_frameall_offset", "anim_frameall_time", "anim_frameall_flip", "anim_frameall_trans",
                   "anim_frameall_scale", "anim_frame_align", "anim_box_delall", "anim_box_delall_anim",
                   "anim_box_prev"},
                  false);
}

void MainWindow::refreshTitle()
{
    ProjectView* view = currentProjectView();
    const QString app = QStringLiteral(OFS_DISPLAY_NAME);
    setWindowTitle(view ? QStringLiteral("%1 - %2").arg(view->project()->name(), app) : app);
}

void MainWindow::refreshStatusBar()
{
    ProjectView* view = currentProjectView();
    if (!view) {
        m_status->clearSections();
        return;
    }
    const EditorInfo info = view->currentEditorInfo();
    QTabWidget* fileTabs = view->findChild<QTabWidget*>(QStringLiteral("fileTabs_") + view->currentEditor());
    auto* animation = fileTabs ? qobject_cast<AnimationPage*>(fileTabs->currentWidget()) : nullptr;
    const WorkspaceType workspace = animation && animation->textMode() ? WorkspaceType::Code : info.workspace;
    switch (workspace) {
    case WorkspaceType::Code: {
        int line = 1, column = 1, lines = 0;
        if (CodeEditor* editor = view->currentCodeEditor()) {
            line = editor->cursorLine();
            column = editor->cursorColumn();
            lines = editor->blockCount();
        }
        QString encoding = QStringLiteral("UTF-8");
        if (auto* file = qobject_cast<CodeFile*>(view->currentFile()))
            encoding = file->encoding();
        m_status->setSection(0, tr("Line: %1, Column: %2, Line count: %3").arg(line).arg(column).arg(lines));
        int errors = 0;
        if (CodePage* page = view->currentCodePage()) {
            for (const Issue& issue : page->issues()) {
                if (issue.isError())
                    ++errors;
            }
        }
        m_status->setSection(1, tr("Syntax error count: %1").arg(errors));
        m_status->setSection(2, tr("Encoding: %1").arg(encoding));
        break;
    }
    case WorkspaceType::Graphics:
    case WorkspaceType::Emulator: {
        QTabWidget* tabs = fileTabs;
        if (EngineWorkspace* engineWorkspace = view->currentWorkspace()) {
            const QStringList sections = engineWorkspace->statusSections();
            for (int i = 0; i < sections.size(); ++i)
                m_status->setSection(i, sections[i]);
            break;
        }
        if (animation) {
            const QPoint guides = animation->view()->guides();
            m_status->setSection(
                0, tr("Position: %1, %2").arg(animation->cursorPosition().x()).arg(animation->cursorPosition().y()));
            m_status->setSection(1, tr("Horizontal guide: %1, Vertical guide: %2").arg(guides.y()).arg(guides.x()));
            m_status->setSection(2, tr("Renderer: %1").arg(QStringLiteral("OpenGL")));
            break;
        }
        if (auto* page = tabs ? qobject_cast<BackgroundsPage*>(tabs->currentWidget()) : nullptr) {
            const QPoint guides = page->view()->guides();
            m_status->setSection(
                0, page->mode() == BackgroundsPage::Mode::Playing
                       ? tr("%1 FPS").arg(60)
                       : tr("Position: %1, %2").arg(page->cursorPosition().x()).arg(page->cursorPosition().y()));
            m_status->setSection(1, tr("Horizontal guide: %1, Vertical guide: %2").arg(guides.y()).arg(guides.x()));
            m_status->setSection(2, tr("Renderer: %1").arg(QStringLiteral("OpenGL")));
            break;
        }
        if (auto* page = tabs ? qobject_cast<SpritePage*>(tabs->currentWidget()) : nullptr) {
            const QPoint guides = page->view()->guides();
            static const char* const versions[] = {"1.0.1.0", "2.0.0.0", "2.0.1.0"};
            const auto version = page->file()->document().version();
            m_status->setSection(
                0, tr("Position: %1, %2").arg(page->cursorPosition().x()).arg(page->cursorPosition().y()));
            m_status->setSection(1, tr("Horizontal guide: %1, Vertical guide: %2").arg(guides.y()).arg(guides.x()));
            m_status->setSection(2, tr("Version: %1")
                                        .arg(QLatin1String(versions[version == Formats::SffVersion::V1   ? 0
                                                                    : version == Formats::SffVersion::V2 ? 1
                                                                                                         : 2])));
            break;
        }
        m_status->setSection(0, tr("Position: %1, %2").arg(0).arg(0));
        m_status->setSection(2, tr("Renderer: %1").arg(QStringLiteral("OpenGL")));
        break;
    }
    case WorkspaceType::Sound: {
        qint64 cursor = 0, count = 0, start = 0, end = 0;
        double rate = 1;
        QString state = tr("Stopped");
        if (auto* page = fileTabs ? qobject_cast<SoundPage*>(fileTabs->currentWidget()) : nullptr) {
            cursor = page->view()->cursor();
            count = qint64(page->samples().frameCount());
            start = qint64(page->view()->selection().start);
            end = qint64(page->view()->selection().end());
            rate = std::max<double>(1, page->samples().sampleRate());
            state = page->state() == SoundPage::State::Playing  ? tr("Playing")
                    : page->state() == SoundPage::State::Paused ? tr("Paused")
                                                                : tr("Stopped");
        }
        m_status->setSection(0, tr("Sample: %1/%2, Selection: %3/%4").arg(cursor).arg(count).arg(start).arg(end));
        m_status->setSection(1, tr("Time: %1/%2s, Selection: %3/%4s")
                                    .arg(cursor / rate, 0, 'f', 3)
                                    .arg(count / rate, 0, 'f', 3)
                                    .arg(start / rate, 0, 'f', 3)
                                    .arg(end / rate, 0, 'f', 3));
        m_status->setSection(2, tr("Status: %1").arg(state));
        break;
    }
    }
    m_status->setSection(3, tr("Project: %1, %2").arg(view->project()->typeName(), view->project()->versionName()));
}

void MainWindow::refreshActionList()
{
    m_actionList->clear();
    ProjectView* view = currentProjectView();
    if (!view)
        return;
    for (QAction* a : m_context.actions().actionsInScope(view->currentEditorInfo().scope())) {
        auto* item = new QListWidgetItem(a->icon(), m_context.actions().caption(a->objectName()), m_actionList);
        item->setData(Qt::UserRole, a->objectName());
        item->setToolTip(m_context.actions().description(a->objectName()));
    }
}

void MainWindow::populateEncodings(QMenu* menu, QAction* before, QList<QAction*>& created)
{
    ProjectView* view = currentProjectView();
    auto* file = view ? qobject_cast<CodeFile*>(view->currentFile()) : nullptr;
    const int current = file ? TextEncoding::indexOf(file->encoding()) : -1;
    const QList<TextEncoding>& encodings = TextEncoding::all();
    for (int i = 0; i < int(encodings.size()); ++i) {
        const TextEncoding& encoding = encodings[i];
        // Caption in the text column, the codec names in the shortcut column.
        auto* action = new QAction(encoding.groupName() + QLatin1Char('\t') + encoding.names, menu);
        action->setCheckable(true);
        action->setChecked(i == current);
        action->setEnabled(file && TextEncoding::isAvailable(encoding.codecName()));
        const QString name = encoding.codecName();
        connect(action, &QAction::triggered, this, [this, name] { changeEncoding(name); });
        menu->insertAction(before, action);
        created << action;
    }
}

void MainWindow::changeEncoding(const QString& encoding)
{
    ProjectView* view = currentProjectView();
    auto* file = view ? qobject_cast<CodeFile*>(view->currentFile()) : nullptr;
    if (!file || TextEncoding::indexOf(file->encoding()) == TextEncoding::indexOf(encoding))
        return;
    const bool reload = askQuestion(this, m_context.config(), QStringLiteral("showmsg_enc_reload"),
                                    tr("Reload the document with the new encoding?\n"
                                       "Unsaved changes will be lost; otherwise the text is converted when saved."));
    QString error;
    const bool ok = reload ? file->reloadWithEncoding(encoding, &error) : file->setEncoding(encoding, &error);
    if (!ok)
        QMessageBox::warning(this, QApplication::applicationDisplayName(), error);
    updateProjectTab(view);
    refreshStatusBar();
}

void MainWindow::populateRecentProjects(QMenu* menu, QAction* before, QList<QAction*>& created)
{
    const auto& entries = m_context.history().entries();
    for (const History::Entry& e : entries) {
        auto* a = new QAction(e.name + QLatin1Char('\t') + QDir::toNativeSeparators(e.file), menu);
        const QString file = e.file;
        connect(a, &QAction::triggered, this, [this, file] { openRecentProject(file); });
        menu->insertAction(before, a);
        created << a;
    }
    auto* sep = new QAction(menu);
    sep->setSeparator(true);
    menu->insertAction(before, sep);
    created << sep;

    auto* openAll = new QAction(tr("Open all"), menu);
    openAll->setEnabled(!entries.isEmpty());
    connect(openAll, &QAction::triggered, this, [this] {
        const auto list = m_context.history().entries();
        for (auto it = list.crbegin(); it != list.crend(); ++it)
            openRecentProject(it->file);
    });
    menu->insertAction(before, openAll);
    created << openAll;

    auto* clear = new QAction(tr("Clear"), menu);
    connect(clear, &QAction::triggered, this, [this] { m_context.history().clear(); });
    menu->insertAction(before, clear);
    created << clear;
}

// ---------------------------------------------------------------------------------------------
// Window state

void MainWindow::restoreWindowGeometry()
{
    Config& config = m_context.config();
    const int w = config.intValue(QStringLiteral("window_width"), 0);
    const int h = config.intValue(QStringLiteral("window_height"), 0);
    if (w > 0 && h > 0) {
        QRect rect(config.intValue(QStringLiteral("window_left"), 0), config.intValue(QStringLiteral("window_top"), 0),
                   w, h);
        const QScreen* screen = QGuiApplication::screenAt(rect.center());
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        const QRect avail = screen->availableGeometry();
        rect.setSize(rect.size().boundedTo(avail.size()));
        if (!avail.contains(rect.topLeft()))
            rect.moveTopLeft(avail.topLeft());
        setGeometry(rect);
    } else {
        resize(1280, 800);
    }
    const int state = config.intValue(QStringLiteral("window_state"), Qt::WindowMaximized);
    if (state & Qt::WindowMaximized)
        setWindowState(windowState() | Qt::WindowMaximized);
}

void MainWindow::saveWindowGeometry()
{
    Config& config = m_context.config();
    const QRect normal = normalGeometry();
    config.setValue(QStringLiteral("window_left"), normal.x());
    config.setValue(QStringLiteral("window_top"), normal.y());
    config.setValue(QStringLiteral("window_width"), normal.width());
    config.setValue(QStringLiteral("window_height"), normal.height());
    config.setValue(QStringLiteral("window_state"), int(windowState() & Qt::WindowMaximized) | int(Qt::WindowActive));
    config.setValue(QStringLiteral("main_toolbar"), int(toolBarArea(m_mainBar)));
    config.setValue(QStringLiteral("extra_toolbar"), int(toolBarArea(m_extraBar)));
    config.setValue(QStringLiteral("toolbar_break"), toolBarBreak(m_extraBar) || toolBarBreak(m_mainBar));
    const bool vertical =
        toolBarArea(m_mainBar) == Qt::LeftToolBarArea || toolBarArea(m_mainBar) == Qt::RightToolBarArea;
    config.setValue(QStringLiteral("toolbar_first"),
                    vertical ? m_mainBar->x() <= m_extraBar->x() || m_mainBar->y() <= m_extraBar->y()
                             : m_mainBar->y() < m_extraBar->y() ||
                                   (m_mainBar->y() == m_extraBar->y() && m_mainBar->x() <= m_extraBar->x()));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_context.config().boolValue(QStringLiteral("confirm_close"), true)) {
        QMessageBox box(QMessageBox::Question, tr("Close"),
                        tr("Do you really want to close %1?").arg(QStringLiteral(OFS_DISPLAY_NAME)),
                        QMessageBox::Yes | QMessageBox::No, this);
        box.setDefaultButton(QMessageBox::No);
        auto* dontAsk = new QCheckBox(tr("Don't show this again"), &box);
        box.setCheckBox(dontAsk);
        const bool yes = box.exec() == QMessageBox::Yes;
        if (dontAsk->isChecked())
            m_context.config().setValue(QStringLiteral("confirm_close"), false);
        if (!yes) {
            event->ignore();
            return;
        }
    }
    waitForTasks();
    for (ProjectView* view : projectViews()) {
        if (!confirmClose(view)) {
            event->ignore();
            return;
        }
    }
    saveWindowGeometry();
    QString error;
    if (!m_context.saveSettings(&error))
        qWarning("could not save the settings: %s", qPrintable(error));
    event->accept();
}

} // namespace OFS
