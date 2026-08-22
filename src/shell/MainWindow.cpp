#include "shell/MainWindow.h"

#include "core/Version.h"
#include "core/actions/ActionRegistry.h"
#include "core/actions/MenuSpec.h"
#include "core/app/AppContext.h"
#include "core/app/Messages.h"
#include "core/app/Paths.h"
#include "core/code/CodeEditor.h"
#include "core/code/Snippet.h"
#include "core/code/TextSearch.h"
#include "core/engine/Engine.h"
#include "core/engine/EngineManager.h"
#include "core/engine/RunConfiguration.h"
#include "core/preset/PresetBuilder.h"
#include "core/preset/PresetEngine.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/settings/Config.h"
#include "core/settings/History.h"
#include "core/sprites/SpriteFile.h"
#include "core/text/Encodings.h"
#include "core/theme/ThemeManager.h"
#include "core/widgets/GraphicsView.h"
#include "shell/AdvancedSearchPanel.h"
#include "shell/BottomDock.h"
#include "shell/CodePage.h"
#include "shell/ControllerTemplatesWidget.h"
#include "shell/FileDialogs.h"
#include "shell/MenuBuilder.h"
#include "shell/ProjectView.h"
#include "shell/QuickFindBar.h"
#include "shell/SpritePage.h"
#include "shell/SpritesEditor.h"
#include "shell/StatusBar.h"
#include "shell/UpdateChecker.h"
#include "shell/WelcomePage.h"
#include "shell/dialogs/AboutDialog.h"
#include "shell/dialogs/CalculatorDialog.h"
#include "shell/dialogs/ClipboardContainer.h"
#include "shell/dialogs/OptionsDialog.h"
#include "shell/dialogs/RunChooserDialog.h"
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
#include <QTimer>
#include <QToolBar>
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
    connect(m_status->sectionLabel(2), &EClickableLabel::clicked, this, [this] {
        ProjectView* view = currentProjectView();
        QMenu* encodings = m_menus->submenu(QStringLiteral("menu_encoding"));
        if (view && encodings && view->currentEditorInfo().workspace == WorkspaceType::Code)
            encodings->popup(QCursor::pos());
    });
    connectActions();

    applyInterface();
    restoreWindowGeometry();
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
    m_menus->setDynamic(QStringLiteral("encodings"), [this](QMenu* menu, QAction* before, QList<QAction*>& created) {
        populateEncodings(menu, before, created);
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
    addToolBar(Qt::TopToolBarArea, m_mainBar);
    addToolBarBreak(Qt::TopToolBarArea);
    m_extraBar = new QToolBar(tr("Extra"), this);
    m_extraBar->setObjectName(QStringLiteral("extraToolBar"));
    m_extraBar->setIconSize(QSize(22, 22));
    addToolBar(Qt::TopToolBarArea, m_extraBar);

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
    connect(m_projectTabs, &QTabBar::tabCloseRequested, this, &MainWindow::closeProject);
    connect(m_projectTabs, &QTabBar::tabMoved, this, [this] { refreshForCurrent(); });
    connect(m_welcome, &WelcomePage::openProjectRequested, this, &MainWindow::openProject);
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

    on("file_open", [this] { openProjectDialog(); });
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
            applyInterface();
            for (ProjectView* view : projectViews())
                view->reconfigureEditors();
        });
    });
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
        auto* page = tabs ? qobject_cast<SpritePage*>(tabs->currentWidget()) : nullptr;
        return page && page->file() ? page->file()->undoStack() : nullptr;
    };
    auto undoRedo = [textTarget, spriteUndo](bool redo) {
        return [textTarget, spriteUndo, redo] {
            QWidget* w = textTarget();
            if (auto* p = qobject_cast<QPlainTextEdit*>(w))
                redo ? p->redo() : p->undo();
            else if (auto* l = qobject_cast<QLineEdit*>(w))
                redo ? l->redo() : l->undo();
            else if (QUndoStack* stack = spriteUndo())
                redo ? stack->redo() : stack->undo();
        };
    };
    on("edit_undo", undoRedo(false));
    on("edit_redo", undoRedo(true));
    on("edit_cut", forText(&QPlainTextEdit::cut, &QLineEdit::cut));
    on("edit_copy", forText(&QPlainTextEdit::copy, &QLineEdit::copy));
    on("edit_paste", forText(&QPlainTextEdit::paste, &QLineEdit::paste));

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
    for (const char* id : {"spr_prev",           "spr_next",       "spr_prev_grp",
                           "spr_next_grp",       "spr_pos_left",   "spr_pos_up",
                           "spr_pos_right",      "spr_pos_down",   "spr_delete",
                           "spr_duplicate",      "spr_crop",       "spr_resize",
                           "spr_delete_grp",     "spr_rename_grp", "spr_align",
                           "spr_openimg",        "spr_saveimgas",  "spr_saveimg_aligned",
                           "spr_split_channels", "spr_add",        "spr_export",
                           "spr_import",         "org_up",         "org_down",
                           "org_up_10",          "org_down_10",    "org_top",
                           "org_bottom",         "org_auto"}) {
        const QString action = QString::fromLatin1(id);
        on(id, [this, action] {
            if (ProjectView* view = currentProjectView(); view && view->spritesEditor())
                view->spritesEditor()->perform(action);
        });
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
    auto currentWorkspace = [this]() -> GraphicsView* {
        ProjectView* view = currentProjectView();
        QTabWidget* tabs =
            view ? view->findChild<QTabWidget*>(QStringLiteral("fileTabs_") + view->currentEditor()) : nullptr;
        auto* page = tabs ? qobject_cast<SpritePage*>(tabs->currentWidget()) : nullptr;
        return page ? page->view() : nullptr;
    };
    auto zoom = [this, currentWorkspace](void (GraphicsView::*graphic)(), std::function<void(CodeEditor*)> code) {
        return [this, currentWorkspace, graphic, code] {
            if (GraphicsView* workspace = currentWorkspace()) {
                (workspace->*graphic)();
            } else if (code) {
                if (ProjectView* view = currentProjectView(); view && view->currentCodeEditor())
                    code(view->currentCodeEditor());
            }
        };
    };
    on("view_zoomin", zoom(&GraphicsView::zoomIn, [](CodeEditor* e) { e->zoomIn(); }));
    on("view_zoomout", zoom(&GraphicsView::zoomOut, [](CodeEditor* e) { e->zoomOut(); }));
    on("view_zoomreset", zoom(&GraphicsView::zoomReset, [](CodeEditor* e) { e->resetZoom(); }));
    on("view_zoomfit", zoom(&GraphicsView::zoomFit, {}));
    on("view_center_origin", zoom(&GraphicsView::centerOrigin, {}));

    // Saving
    on("file_save", [this] {
        ProjectView* view = currentProjectView();
        if (!view)
            return;
        for (ProjectFile* file : view->project()->files()) {
            if (file->isUntitled() && file->isModified() && !saveFileAs(file))
                return;
        }
        QString error;
        if (!view->project()->save(&error))
            QMessageBox::warning(this, tr("Save"), error);
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
    QString parameters = run.parameters;
    parameters.replace(QStringLiteral("%0"), copy.name);

    auto* process = new QProcess(this);
    process->setProgram(run.executable);
    process->setArguments(QProcess::splitCommand(parameters));
    process->setWorkingDirectory(engineDir);
    m_engineProcess = process;
    m_runCopy = copy;
    m_runLog = engine->runLogFile(engineDir);
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
    engine->runTool(actionId, context);
}

void MainWindow::finishEngineRun(QProcess* process)
{
    if (process != m_engineProcess)
        return;
    QFile log(m_runLog);
    if (!m_runLog.isEmpty() && log.open(QIODevice::ReadOnly)) {
        QString text = QString::fromLocal8Bit(log.readAll());
        text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
        m_dock->engineOutput()->setPlainText(text);
    }
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
    QString error;
    bool ok = false;
    if (auto* code = qobject_cast<CodeFile*>(file); code && asUtf8)
        ok = code->saveAsUtf8(&error);
    else
        ok = file->save(&error);
    if (!ok)
        QMessageBox::warning(this, tr("Save"), error);
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
    if (info.id.isEmpty() || info.workspace != WorkspaceType::Code)
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
    const EditorInfo info = project->engine()->editor(file->editorId());
    const QString target = FileDialogs::getSaveFileName(this, tr("Save as"), file->path(), info.fileFilter);
    if (target.isEmpty())
        return false;
    QString error;
    if (!file->saveAs(target, &error)) {
        QMessageBox::warning(this, tr("Save as"), error);
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
    Project* project = view->project();
    if (!project->isModified())
        return true;
    for (const EditorInfo& editor : project->engine()->editors()) {
        bool modified = false;
        for (const ProjectFile* file : project->files())
            modified = modified || (file->editorId() == editor.id && file->isModified());
        if (modified &&
            !askQuestion(this, m_context.config(), QStringLiteral("showmsg_discard"), tr("Discard changes?"),
                         tr("Do you want to discard the changes made in %1?").arg(editor.title.toLower())))
            return false;
    }
    return true;
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

    auto* view = new ProjectView(project, m_context, m_stack);
    view->applyPreset(m_preset);
    view->setLeftPanelVisible(m_context.config().boolValue(QStringLiteral("show_left_panel"), true));
    view->setRightPanelVisible(m_context.config().boolValue(QStringLiteral("show_right_panel"), true));
    view->setOrganizerVisible(m_context.config().boolValue(QStringLiteral("show_organizer"), false));
    const QString lastEditor = m_context.config().value(QStringLiteral("lastEditor"));
    if (view->editorIds().contains(lastEditor))
        view->setCurrentEditor(lastEditor);
    connect(view, &ProjectView::currentFileChanged, this, [this, view] {
        if (view == currentProjectView())
            refreshStatusBar();
    });
    connect(view, &ProjectView::modifiedChanged, this, [this, view] { updateProjectTab(view); });
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
    m_stack->addWidget(view);

    const int index = m_projectTabs->addTab(project->name());
    m_projectTabs->setTabData(index, QVariant::fromValue<QObject*>(view));
    m_projectTabs->setTabToolTip(index, QDir::toNativeSeparators(path));
    m_context.history().add(project->name(), path);
    m_projectTabs->setCurrentIndex(index);
    refreshForCurrent();
    return true;
}

void MainWindow::closeProject(int tabIndex)
{
    if (tabIndex < 0 || tabIndex >= m_projectTabs->count())
        return;
    const QVariant tab = m_projectTabs->tabData(tabIndex);
    auto* view = qobject_cast<ProjectView*>(tab.value<QObject*>());
    if (view && !confirmClose(view))
        return;
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
}

void MainWindow::closeCurrentProject()
{
    if (currentProjectView())
        closeProject(m_projectTabs->currentIndex());
}

void MainWindow::closeAllProjects()
{
    for (int i = m_projectTabs->count() - 1; i >= 0; --i) {
        if (m_projectTabs->tabData(i).toString() != kWelcomeTab)
            closeProject(i);
    }
}

void MainWindow::closeOtherProjects()
{
    ProjectView* keep = currentProjectView();
    for (int i = m_projectTabs->count() - 1; i >= 0; --i) {
        auto* view = qobject_cast<ProjectView*>(m_projectTabs->tabData(i).value<QObject*>());
        if (view && view != keep)
            closeProject(i);
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
    if (ProjectView* view = currentProjectView())
        view->setCurrentEditor(editorId);
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
    enableAll({"debug_interrupt", "debug_continue", "debug_stop", "debug_restart", "debug_step_over", "debug_step_into",
               "debug_step_out", "debug_step_frame"},
              false);
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
    enableAll({"spr_saveimg", "spr_layer_add", "spr_layer_delete", "spr_layer_duplicate", "spr_layer_mergevis",
               "spr_layer_mergeall", "spr_layer_save", "spr_layer_play", "spr_view_original", "spr_image_properties",
               "spr_select_all", "spr_select_none", "spr_select_invert", "spr_image_rotate", "spr_image_fliph",
               "spr_image_flipv", "spr_explorer"},
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
    switch (info.workspace) {
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
        QTabWidget* tabs = view->findChild<QTabWidget*>(QStringLiteral("fileTabs_") + view->currentEditor());
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
    case WorkspaceType::Sound:
        m_status->setSection(0, tr("Sample: %1/%2, Selection: %3/%4").arg(0).arg(0).arg(0).arg(0));
        m_status->setSection(1, tr("Time: %1/%2s, Selection: %3/%4s")
                                    .arg(0.0, 0, 'f', 3)
                                    .arg(0.0, 0, 'f', 3)
                                    .arg(0.0, 0, 'f', 3)
                                    .arg(0.0, 0, 'f', 3));
        m_status->setSection(2, tr("Status: %1").arg(tr("Stopped")));
        break;
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
        connect(a, &QAction::triggered, this, [this, file] { openProject(file); });
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
            openProject(it->file);
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
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_context.config().boolValue(QStringLiteral("confirm_close"), true)) {
        QMessageBox box(QMessageBox::Question, tr("Close"),
                        tr("Do you really want to close %1?").arg(QStringLiteral(OFS_DISPLAY_NAME)),
                        QMessageBox::Yes | QMessageBox::No, this);
        box.setDefaultButton(QMessageBox::Yes);
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
