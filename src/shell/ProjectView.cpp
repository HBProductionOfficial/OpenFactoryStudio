#include "shell/ProjectView.h"

#include "core/actions/ActionRegistry.h"
#include "core/anims/AirEdits.h"
#include "core/app/AppContext.h"
#include "core/code/CodeCompletion.h"
#include "core/code/CodeEditor.h"
#include "core/code/CodeOutline.h"
#include "core/code/Snippet.h"
#include "core/debug/Breakpoints.h"
#include "core/editors/Organize.h"
#include "core/engine/Engine.h"
#include "core/engine/EngineWorkspace.h"
#include "core/preset/ControlRegistry.h"
#include "core/preset/PresetBuilder.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/settings/Config.h"
#include "core/sounds/SoundFile.h"
#include "core/sprites/PaletteEdits.h"
#include "core/sprites/SpriteEdits.h"
#include "core/sprites/SpriteFile.h"
#include "core/theme/IconSet.h"
#include "core/theme/ThemeManager.h"
#include "core/widgets/EPanelWidget.h"
#include "core/widgets/ImageCanvas.h"
#include "core/widgets/WaveformView.h"
#include "shell/AnimationPage.h"
#include "shell/AnimationsEditor.h"
#include "shell/BackgroundsEditor.h"
#include "shell/BackgroundsPage.h"
#include "shell/BottomDock.h"
#include "shell/CodePage.h"
#include "shell/ControllerTemplatesWidget.h"
#include "shell/DocumentationPopup.h"
#include "shell/ImageEditor.h"
#include "shell/OrganizerBinding.h"
#include "shell/SoundPage.h"
#include "shell/SoundsEditor.h"
#include "shell/SpritePage.h"
#include "shell/SpritesEditor.h"

#include <QAction>
#include <QCursor>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <functional>

namespace OFS {

namespace {

constexpr int kPanelWidth = 262;

QScrollArea* makeScrollArea(QWidget* parent)
{
    auto* scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("panelContent"));
    scroll->setWidget(content);
    return scroll;
}

QString iconForFile(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("sff"))
        return QStringLiteral("icon_sprite");
    if (ext == QLatin1String("air"))
        return QStringLiteral("icon_animation");
    if (ext == QLatin1String("snd"))
        return QStringLiteral("icon_sound");
    if (ext == QLatin1String("cmd"))
        return QStringLiteral("icon_command");
    if (ext == QLatin1String("def") || ext == QLatin1String("cns") || ext == QLatin1String("st"))
        return QStringLiteral("icon_code");
    return QStringLiteral("icon_file");
}

} // namespace

ProjectView::ProjectView(Project* project, AppContext& context, QWidget* parent)
    : QWidget(parent)
    , m_project(project)
    , m_context(context)
    , m_left(new EPanelWidget(this))
    , m_leftStack(new QStackedWidget(m_left))
    , m_leftTabs(new QTabBar(m_left))
    , m_explorer(new QTreeView(m_left))
    , m_center(new QStackedWidget(this))
    , m_workspaceLayout(new QVBoxLayout)
    , m_right(new EPanelWidget(this))
    , m_rightStack(new QStackedWidget(m_right))
{
    setObjectName(QStringLiteral("projectView"));
    m_project->setParent(this);

    // Left panel: the editor panels and the project explorer, switched by the bottom tabs.
    m_left->setObjectName(QStringLiteral("leftPanel"));
    m_left->setFixedWidth(kPanelWidth);
    m_explorer->setObjectName(QStringLiteral("projectExplorer"));
    m_explorer->setHeaderHidden(true);
    m_explorer->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_explorer, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
        const QString path = index.data(Qt::UserRole).toString();
        if (showFileLine(path, 0))
            return;
        for (const QString& id : editorIds()) {
            if (m_project->editorFiles(id).contains(path, Qt::CaseInsensitive)) {
                setCurrentEditor(id);
                return;
            }
        }
    });
    auto* explorerHost = new QWidget(m_leftStack);
    auto* explorerLayout = new QVBoxLayout(explorerHost);
    explorerLayout->setContentsMargins(11, 11, 11, 11);
    explorerLayout->addWidget(m_explorer);
    m_leftStack->addWidget(explorerHost);
    m_leftTabs->setObjectName(QStringLiteral("leftPanelTabs"));
    m_leftTabs->setShape(QTabBar::RoundedSouth);
    m_leftTabs->setExpanding(true);
    m_leftTabs->setDrawBase(false);
    m_leftTabs->addTab(tr("Editor"));
    m_leftTabs->addTab(tr("Project explorer"));
    m_left->bodyLayout()->addWidget(m_leftStack, 1);
    m_left->bodyLayout()->addWidget(m_leftTabs);
    connect(m_leftTabs, &QTabBar::currentChanged, this, [this](int index) {
        if (index == 1)
            m_leftStack->setCurrentIndex(0);
        else if (auto* panel = m_editorPanels.value(m_currentEditor))
            m_leftStack->setCurrentWidget(panel);
    });

    // Right panel: the extra panel of the current editor.
    m_right->setObjectName(QStringLiteral("rightPanel"));
    m_right->setFixedWidth(kPanelWidth);
    m_right->bodyLayout()->addWidget(m_rightStack, 1);

    m_workspaceLayout->setContentsMargins(0, 0, 0, 0);
    m_workspaceLayout->setSpacing(0);
    m_workspaceLayout->addWidget(m_center, 1);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_left);
    layout->addLayout(m_workspaceLayout, 1);
    layout->addWidget(m_right);

    for (const QString& id : m_project->editorIds())
        createEditor(engine()->editor(id));
    fillProjectExplorer();
    connect(this, &ProjectView::modifiedChanged, this, &ProjectView::updateTabTitles);

    const QStringList ids = editorIds();
    if (!ids.isEmpty())
        setCurrentEditor(ids.first());
}

ProjectView::~ProjectView()
{
    // Worker thread analyses read the project, so let them finish first.
    for (const QPointer<QFutureWatcher<Analysis>>& watcher : std::as_const(m_analyses)) {
        if (watcher)
            watcher->waitForFinished();
    }
    // Editors show documents the project owns, so delete them first. Disconnect so the files do not report back
    // meanwhile.
    disconnect(m_project, nullptr, this, nullptr);
    for (ProjectFile* file : m_project->files())
        disconnect(file, nullptr, this, nullptr);
    m_fileTabs.clear();
    // Same for the tabs, since the window may be going away too.
    for (QTabWidget* tabs : m_center->findChildren<QTabWidget*>())
        disconnect(tabs, nullptr, this, nullptr);
    for (QObject* editor : std::initializer_list<QObject*>{m_sprites, m_animations, m_sounds, m_backgrounds}) {
        if (editor)
            editor->blockSignals(true);
    }
    delete m_center;
    m_center = nullptr;
}

Engine* ProjectView::engine() const
{
    return m_project->engine();
}

QStringList ProjectView::editorIds() const
{
    QStringList ids = m_project->editorIds();
    ids.removeIf([this](const QString& id) { return m_closedEditors.contains(id); });
    return ids;
}

EditorInfo ProjectView::currentEditorInfo() const
{
    return engine()->editor(m_currentEditor);
}

void ProjectView::createEditor(const EditorInfo& info)
{
    if (info.id.isEmpty())
        return;

    QScrollArea* panel = makeScrollArea(m_leftStack);
    panel->setObjectName(QStringLiteral("panel_") + info.id);
    m_leftStack->addWidget(panel);
    m_editorPanels.insert(info.id, panel);

    auto* tabs = new QTabWidget(m_center);
    tabs->setObjectName(QStringLiteral("fileTabs_") + info.id);
    tabs->setDocumentMode(true);
    tabs->setTabsClosable(true);
    tabs->setMovable(true);
    // A single file shows no tab bar unless the last file may be closed (can_close_editor).
    tabs->setTabBarAutoHide(!m_context.config().boolValue(QStringLiteral("can_close_editor"), true));
    connect(tabs, &QTabWidget::tabCloseRequested, this,
            [this, tabs, id = info.id](int index) { closeFileTab(id, tabs, index); });
    for (const QString& file : m_project->editorFiles(info.id)) {
        if (file.isEmpty())
            continue;
        const int index = tabs->addTab(createFilePage(info, file), tabCaption(file));
        tabs->setTabToolTip(index, tabToolTip(file));
    }
    if (info.id == QLatin1String("sprites")) {
        m_sprites = new SpritesEditor(m_context, this);
        m_sprites->setPage(qobject_cast<SpritePage*>(tabs->currentWidget()));
        connect(m_sprites, &SpritesEditor::explorerChanged, this, &ProjectView::updateRightPanel);
    }
    if (info.id == QLatin1String("animations")) {
        m_animations = new AnimationsEditor(m_context, this);
        m_animations->setPage(qobject_cast<AnimationPage*>(tabs->currentWidget()));
    }
    if (info.id == QLatin1String("sounds")) {
        m_sounds = new SoundsEditor(m_context, this);
        m_sounds->setPage(qobject_cast<SoundPage*>(tabs->currentWidget()));
    }
    if (info.id == QLatin1String("backgrounds")) {
        m_backgrounds = new BackgroundsEditor(m_context, this);
        m_backgrounds->setPage(qobject_cast<BackgroundsPage*>(tabs->currentWidget()));
    }
    if (info.id == QLatin1String("commands"))
        createCommandsOrganizer();
    connect(tabs, &QTabWidget::currentChanged, this, [this, tabs] {
        if (auto* page = qobject_cast<CodePage*>(tabs->currentWidget()))
            updateOutline(page);
        if (m_sprites && tabs->objectName() == QLatin1String("fileTabs_sprites"))
            m_sprites->setPage(qobject_cast<SpritePage*>(tabs->currentWidget()));
        if (m_animations && tabs->objectName() == QLatin1String("fileTabs_animations"))
            m_animations->setPage(qobject_cast<AnimationPage*>(tabs->currentWidget()));
        if (m_sounds && tabs->objectName() == QLatin1String("fileTabs_sounds"))
            m_sounds->setPage(qobject_cast<SoundPage*>(tabs->currentWidget()));
        if (m_backgrounds && tabs->objectName() == QLatin1String("fileTabs_backgrounds"))
            m_backgrounds->setPage(qobject_cast<BackgroundsPage*>(tabs->currentWidget()));
        if (m_commandsOrganizer && tabs->objectName() == QLatin1String("fileTabs_commands"))
            m_commandsOrganizer->refresh();
        if (tabs == m_center->currentWidget())
            Q_EMIT currentFileChanged();
    });
    if (tabs->count() == 0) {
        QWidget* empty = nullptr;
        if (info.workspace == WorkspaceType::Emulator) {
            if (EngineWorkspace* workspace = engine()->createWorkspace(info.id, *m_project, m_context, tabs)) {
                m_workspaces.insert(info.id, workspace);
                connect(workspace, &EngineWorkspace::statusChanged, this, [this, workspace] {
                    if (workspace == currentWorkspace())
                        Q_EMIT currentFileChanged();
                });
                workspace->setBreakpoints(m_breakpoints);
                connect(workspace, &EngineWorkspace::debugStateChanged, this, [this, workspace] {
                    setDebugging(workspace->debugState() != Debug::SessionState::None);
                    Q_EMIT debugStateChanged();
                });
                connect(workspace, &EngineWorkspace::debugLineChanged, this, &ProjectView::showDebugLine);
                connect(workspace, &EngineWorkspace::debugVariablesChanged, this, &ProjectView::debugVariablesChanged);
                empty = workspace;
            }
        }
        if (!empty)
            empty = new QWidget(tabs);
        empty->setObjectName(QStringLiteral("workspace_") + info.id);
        tabs->addTab(empty, QString());
        tabs->tabBar()->hide();
    }
    m_center->addWidget(tabs);
    m_fileTabs.insert(info.id, tabs);

    if (!info.extraPanel.isEmpty() && !m_extraPanels.contains(info.extraPanel)) {
        QScrollArea* extra = makeScrollArea(m_rightStack);
        extra->setObjectName(QStringLiteral("extra_") + info.extraPanel);
        m_rightStack->addWidget(extra);
        m_extraPanels.insert(info.extraPanel, extra);
    }
}

QWidget* ProjectView::createFilePage(const EditorInfo& info, const QString& filePath)
{
    if (info.id == QLatin1String("backgrounds")) {
        QString error;
        auto* file = qobject_cast<CodeFile*>(m_project->file(filePath));
        if (!file && QFileInfo::exists(filePath))
            file = m_project->codeFile(filePath, QStringLiteral("definitions"), &error);
        if (file) {
            auto* page = new BackgroundsPage(file, m_project);
            page->setProperty("filePath", filePath);
            page->setProperty("editorId", info.id);
            page->applySettings(m_context.config());
            connect(page, &BackgroundsPage::statusChanged, this, [this, page] {
                if (QTabWidget* tabs = m_fileTabs.value(m_currentEditor); tabs && tabs->currentWidget() == page)
                    Q_EMIT currentFileChanged();
            });
            connect(file, &ProjectFile::pathChanged, this,
                    [this, page, file](const QString& oldPath) { fileRenamed(page, oldPath, file->path()); });
            return page;
        }
    }
    const bool animations = info.id == QLatin1String("animations");
    if (info.workspace == WorkspaceType::Code || animations) {
        QString error;
        auto* file = qobject_cast<CodeFile*>(m_project->file(filePath));
        if (!file && QFileInfo::exists(filePath))
            file = m_project->codeFile(filePath, info.id, &error);
        auto* page = new CodePage(m_context.actions());
        page->setProperty("filePath", filePath);
        page->setProperty("editorId", info.id);
        CodeEditor* editor = page->editor();
        if (file) {
            editor->setCodeDocument(file->document());
            connect(file, &ProjectFile::modifiedChanged, this, [this] { Q_EMIT modifiedChanged(); });
            connect(file, &ProjectFile::pathChanged, this,
                    [this, page, file](const QString& oldPath) { fileRenamed(page, oldPath, file->path()); });
            auto* timer = new QTimer(page);
            timer->setSingleShot(true);
            timer->setInterval(300);
            connect(timer, &QTimer::timeout, this, [this, page] { analyze(page); });
            connect(file->document(), &QTextDocument::contentsChanged, timer, qOverload<>(&QTimer::start));
            QTimer::singleShot(0, this, [this, page] { analyze(page); });
        } else {
            editor->setReadOnly(true);
        }
        page->setToolBarVisible(m_context.config().boolValue(QStringLiteral("show_code_bar"), true));
        page->setSnippets(engine()->snippets().names());
        const QStringList templateDirs = engine()->codeTemplateDirs(info.id);
        if (!templateDirs.isEmpty()) {
            auto* templates = new ControllerTemplatesWidget(m_context.actions(), templateDirs);
            templates->setVisible(m_context.config().boolValue(QStringLiteral("show_sctrl_list"), true));
            connect(templates, &ControllerTemplatesWidget::insertRequested, page, &CodePage::insertText);
            page->setSidePanel(templates);
        }
        configureCodeEditor(editor, info.id);
        connect(editor, &CodeEditor::breakpointToggled, this, [this, page](int line) {
            if (m_breakpoints)
                m_breakpoints->toggle(page->property("filePath").toString(), line + 1);
        });
        refreshBreakpointMarks(page);
        connect(editor, &CodeEditor::cursorPositionChanged, this, [this, editor, page] {
            if (editor != currentCodeEditor())
                return;
            followCursorInOutline(page);
            Q_EMIT currentFileChanged();
        });
        connect(editor, &CodeEditor::documentationRequested, this,
                [this, editor](const QString& kind, const QString& name, const QPoint&) {
                    const QString pagePath = engine()->documentationPage(*m_project, kind, name);
                    if (!pagePath.isEmpty())
                        DocumentationPopup::showPage(pagePath, QCursor::pos(), editor);
                });
        if (animations && file) {
            auto* animationPage = new AnimationPage(file, page, m_project);
            animationPage->setProperty("filePath", filePath);
            animationPage->setProperty("editorId", info.id);
            animationPage->applySettings(m_context.config(), &m_context.engineConfig(engine()->name()));
            applyWorkspaceColors(animationPage);
            connect(file, &ProjectFile::pathChanged, this, [this, animationPage, file](const QString& oldPath) {
                fileRenamed(animationPage, oldPath, file->path());
            });
            connect(animationPage, &AnimationPage::statusChanged, this, [this, animationPage] {
                if (QTabWidget* tabs = m_fileTabs.value(m_currentEditor);
                    tabs && tabs->currentWidget() == animationPage)
                    Q_EMIT currentFileChanged();
            });
            return animationPage;
        }
        return page;
    }
    if (info.id == QLatin1String("sounds")) {
        QString error;
        auto* file = qobject_cast<SoundFile*>(m_project->file(filePath));
        if (!file && QFileInfo::exists(filePath))
            file = m_project->soundFile(filePath, info.id, &error);
        if (file) {
            auto* page = new SoundPage(file);
            const auto waveColor = [this, page] {
                page->view()->setWaveColor(m_context.theme().colorSet().color(QStringLiteral("sound_wave")));
            };
            waveColor();
            connect(&m_context.theme(), &ThemeManager::colorSetChanged, page, waveColor);
            page->setProperty("filePath", filePath);
            page->setProperty("editorId", info.id);
            auto updateIssues = [this, page, file] {
                IssueList issues;
                const SoundDocument& doc = file->document();
                for (int i = 0; i < doc.count(); ++i) {
                    for (int j = 0; j < i; ++j) {
                        if (doc.sounds().at(j).group == doc.sounds().at(i).group &&
                            doc.sounds().at(j).number == doc.sounds().at(i).number) {
                            Issue issue;
                            issue.severity = Issue::Severity::Warning;
                            issue.line = i;
                            issue.message = tr("The group %1, index %2 is already in use by sound #%3!")
                                                .arg(doc.sounds().at(i).group)
                                                .arg(doc.sounds().at(i).number)
                                                .arg(j + 1);
                            issue.file = page->property("filePath").toString();
                            issues << issue;
                            break;
                        }
                    }
                }
                m_issues.insert(page->property("filePath").toString(), issues);
                Q_EMIT issuesChanged();
            };
            connect(file, &SoundFile::documentChanged, this, updateIssues);
            QTimer::singleShot(0, this, updateIssues);
            connect(page, &SoundPage::statusChanged, this, [this, page] {
                if (QTabWidget* tabs = m_fileTabs.value(m_currentEditor); tabs && tabs->currentWidget() == page)
                    Q_EMIT currentFileChanged();
            });
            connect(file, &ProjectFile::modifiedChanged, this, [this] { Q_EMIT modifiedChanged(); });
            connect(file, &ProjectFile::pathChanged, this,
                    [this, page, file](const QString& oldPath) { fileRenamed(page, oldPath, file->path()); });
            return page;
        }
    }
    if (info.id == QLatin1String("sprites")) {
        QString error;
        auto* file = qobject_cast<SpriteFile*>(m_project->file(filePath));
        if (!file && QFileInfo::exists(filePath))
            file = m_project->spriteFile(filePath, info.id, &error);
        if (file) {
            auto* page = new SpritePage(file);
            page->applySettings(m_context.config());
            applyWorkspaceColors(page);
            page->setProperty("filePath", filePath);
            auto updateIssues = [this, page, file] {
                IssueList issues = SpriteEdits::identifierIssues(file->document());
                issues << PaletteEdits::issues(file->document());
                const QString path = page->property("filePath").toString();
                for (Issue& issue : issues)
                    issue.file = path;
                m_issues.insert(path, issues);
                Q_EMIT issuesChanged();
            };
            connect(file, &SpriteFile::documentChanged, this, updateIssues);
            QTimer::singleShot(0, this, updateIssues);
            connect(page, &SpritePage::statusChanged, this, [this, page] {
                if (QTabWidget* tabs = m_fileTabs.value(m_currentEditor); tabs && tabs->currentWidget() == page)
                    Q_EMIT currentFileChanged();
            });
            page->setProperty("editorId", info.id);
            connect(file, &ProjectFile::modifiedChanged, this, [this] { Q_EMIT modifiedChanged(); });
            connect(file, &ProjectFile::pathChanged, this,
                    [this, page, file](const QString& oldPath) { fileRenamed(page, oldPath, file->path()); });
            return page;
        }
    }
    auto* workspace = new QWidget;
    workspace->setObjectName(QStringLiteral("workspace_") + info.id);
    return workspace;
}

void ProjectView::configureCodeEditor(CodeEditor* editor, const QString& editorId)
{
    const Config& config = m_context.config();
    editor->setFont(CodeEditor::configuredFont(&config));
    editor->setTabStopDistance(editor->fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);
    CodeEditor::Options options;
    options.lineNumbers = config.boolValue(QStringLiteral("code_line_numbers"), true);
    options.foldingMarkers = config.boolValue(QStringLiteral("code_folding_markers"), true);
    options.highlightLine = config.boolValue(QStringLiteral("code_highlight_line"), true);
    options.parenMatching = config.boolValue(QStringLiteral("code_paren_matching"), true);
    options.highlightBlocks = config.boolValue(QStringLiteral("code_highlight_blocks"), false);
    options.breakpoints = true;
    editor->setOptions(options);
    const IconSet& icons = m_context.theme().iconSet();
    editor->setBreakpointIcons(
        {{CodeEditor::BreakpointMark::Pending, icons.icon(QStringLiteral("breakpoint_pending"))},
         {CodeEditor::BreakpointMark::Active, icons.icon(QStringLiteral("breakpoint_active"))},
         {CodeEditor::BreakpointMark::Disabled, icons.icon(QStringLiteral("breakpoint_disabled"))},
         {CodeEditor::BreakpointMark::Ignored, icons.icon(QStringLiteral("breakpoint_ignored"))}},
        icons.icon(QStringLiteral("debug_curr_line")));
    const QString fileType = engine()->syntaxFileTypeForText(*m_project, editorId, editor->toPlainText());
    editor->setSyntax(engine()->syntax(), fileType);
    editor->setColorSet(m_context.theme().colorSet());
    if (!editor->property("followsColorSet").toBool()) {
        editor->setProperty("followsColorSet", true);
        connect(&m_context.theme(), &ThemeManager::colorSetChanged, editor,
                [this, editor] { editor->setColorSet(m_context.theme().colorSet()); });
    }
    editor->setCompletionProvider(
        std::make_unique<SyntaxCompletionProvider>(engine()->syntax(), fileType, engine()->completion()));
}

void ProjectView::refreshSnippets()
{
    const QStringList names = engine()->snippets().names();
    for (CodePage* page : codePages())
        page->setSnippets(names);
}

bool ProjectView::closeFileTab(const QString& editorId, QTabWidget* tabs, int index)
{
    QWidget* page = tabs->widget(index);
    if (!page)
        return false;
    const QString path = page->property("filePath").toString();
    // The Backgrounds editor's groups are parts of the definitions file, which stays open.
    ProjectFile* file = editorId == QLatin1String("backgrounds") ? nullptr : m_project->file(path);
    if (file && file->isModified()) {
        const QString name = file->isUntitled() ? tabs->tabText(index).remove(QLatin1Char('*')) : file->fileName();
        QMessageBox box(QMessageBox::Question, tr("Close file"),
                        tr("The file '%1' has been modified. Do you want to save the changes?").arg(name),
                        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, window());
        box.setDefaultButton(QMessageBox::Save);
        switch (box.exec()) {
        case QMessageBox::Save: {
            if (file->isUntitled()) {
                Q_EMIT saveAsRequested(file);
                if (file->isModified())
                    return false;
                break;
            }
            QString error;
            if (!file->save(&error)) {
                QMessageBox::warning(window(), tr("Close file"), error);
                return false;
            }
            break;
        }
        case QMessageBox::Discard:
            if (!file->isUntitled() && QFileInfo::exists(file->path())) {
                QString error;
                file->load(&error);
            }
            break;
        default:
            return false;
        }
    }
    tabs->removeTab(index);
    page->deleteLater();
    if (tabs->count() == 0) {
        m_closedEditors.insert(editorId);
        const QStringList open = editorIds();
        if (!open.isEmpty())
            setCurrentEditor(open.first());
        Q_EMIT editorsChanged();
    }
    Q_EMIT currentFileChanged();
    return true;
}

void ProjectView::applyWorkspaceColors(QWidget* page)
{
    const ColorSet& colors = m_context.theme().colorSet();
    const QColor top = colors.color(QStringLiteral("back_start"));
    const QColor bottom = colors.color(QStringLiteral("back_end"));
    if (auto* sprites = qobject_cast<SpritePage*>(page)) {
        sprites->view()->setBackgroundGradient(top, bottom);
        if (sprites->imageEditor())
            sprites->imageEditor()->canvas()->setWorkspaceColors(top, bottom);
    } else if (auto* animation = qobject_cast<AnimationPage*>(page)) {
        animation->view()->setBackgroundGradient(top, bottom);
    } else {
        return;
    }
    if (!page->property("followsColorSet").toBool()) {
        page->setProperty("followsColorSet", true);
        connect(&m_context.theme(), &ThemeManager::colorSetChanged, page, [this, page] { applyWorkspaceColors(page); });
    }
}

void ProjectView::reconfigureEditors()
{
    const bool closable = m_context.config().boolValue(QStringLiteral("can_close_editor"), true);
    for (QTabWidget* tabs : std::as_const(m_fileTabs))
        tabs->setTabBarAutoHide(!closable);
    for (auto it = m_fileTabs.cbegin(); it != m_fileTabs.cend(); ++it) {
        for (int i = 0; i < it.value()->count(); ++i) {
            if (auto* sprites = qobject_cast<SpritePage*>(it.value()->widget(i)))
                sprites->applySettings(m_context.config());
            if (auto* animation = qobject_cast<AnimationPage*>(it.value()->widget(i)))
                animation->applySettings(m_context.config(), &m_context.engineConfig(engine()->name()));
            if (auto* page = codePageOf(it.value()->widget(i)))
                configureCodeEditor(page->editor(), it.key());
        }
    }
    for (EngineWorkspace* workspace : std::as_const(m_workspaces)) {
        if (workspace)
            workspace->applySettings();
    }
    if (m_sprites)
        m_sprites->applySettings();
}

CodeEditor* ProjectView::currentCodeEditor() const
{
    CodePage* page = currentCodePage();
    return page ? page->editor() : nullptr;
}

void ProjectView::showFrameSprite()
{
    QAction* option = m_context.actions().action(QStringLiteral("spr_frame_spr"));
    if (!option || !option->isChecked() || !m_animations || !m_sprites)
        return;
    AnimationPage* animation = m_animations->page();
    SpritePage* sprites = m_sprites->page();
    if (!animation || !sprites || !animation->frame())
        return;
    const int index = sprites->file()->document().findSprite(animation->frame()->group, animation->frame()->number);
    if (index >= 0)
        sprites->setCurrent(index);
}

CodePage* ProjectView::codePageOf(QWidget* tab)
{
    if (auto* animation = qobject_cast<AnimationPage*>(tab))
        return animation->codePage();
    return qobject_cast<CodePage*>(tab);
}

EngineWorkspace* ProjectView::currentWorkspace() const
{
    const EngineWorkspace* found = m_workspaces.value(m_currentEditor);
    return const_cast<EngineWorkspace*>(found);
}

CodePage* ProjectView::currentCodePage() const
{
    QTabWidget* tabs = m_fileTabs.value(m_currentEditor);
    QWidget* tab = tabs ? tabs->currentWidget() : nullptr;
    if (auto* animation = qobject_cast<AnimationPage*>(tab))
        return animation->textMode() ? animation->codePage() : nullptr;
    return qobject_cast<CodePage*>(tab);
}

QList<CodePage*> ProjectView::codePages() const
{
    QList<CodePage*> pages;
    for (QTabWidget* tabs : m_fileTabs) {
        for (int i = 0; i < tabs->count(); ++i) {
            if (auto* page = codePageOf(tabs->widget(i)))
                pages << page;
        }
    }
    return pages;
}

void ProjectView::setCodeMenu(QMenu* menu)
{
    m_codeMenu = menu;
    for (CodeEditor* editor : codeEditors())
        editor->setCodeMenu(menu);
}

void ProjectView::setCodeToolBarsVisible(bool visible)
{
    for (CodePage* page : codePages())
        page->setToolBarVisible(visible);
}

ProjectView::Analysis ProjectView::computeAnalysis(const QString& editorId, const QString& text) const
{
    Analysis analysis;
    analysis.issues = engine()->validate(*m_project, editorId, text);
    analysis.outline = engine()->outline(*m_project, editorId, text);
    analysis.variables = engine()->variableUsage(*m_project, editorId, text);
    return analysis;
}

void ProjectView::applyAnalysis(CodePage* page, Analysis analysis)
{
    const QString path = page->property("filePath").toString();
    for (Issue& issue : analysis.issues)
        issue.file = path;
    page->setIssues(analysis.issues, m_context.theme().iconSet());
    m_issues.insert(path, analysis.issues);
    m_outlines.insert(path, analysis.outline);
    m_variables.insert(path, analysis.variables);
    updateOutline(page);
    Q_EMIT issuesChanged();
}

void ProjectView::analyze(CodePage* page)
{
    const QString path = page->property("filePath").toString();
    auto* file = qobject_cast<CodeFile*>(m_project->file(path));
    if (!file)
        return;
    // One analysis per page at a time. Edits made meanwhile start another one when it ends.
    if (page->property("analysisRunning").toBool()) {
        page->setProperty("analysisPending", true);
        return;
    }
    page->setProperty("analysisRunning", true);
    page->setProperty("analysisPending", false);
    const QString editorId = page->property("editorId").toString();
    const QString text = file->document()->toPlainText();
    const int revision = file->document()->revision();
    QPointer<QTextDocument> document = file->document();

    auto* watcher = new QFutureWatcher<Analysis>(page);
    m_analyses << watcher;
    connect(watcher, &QFutureWatcher<Analysis>::finished, this, [this, page, watcher, revision, document] {
        m_analyses.removeAll(watcher);
        watcher->deleteLater();
        page->setProperty("analysisRunning", false);
        // Results for an older text are dropped: a newer analysis follows.
        if (document && document->revision() == revision)
            applyAnalysis(page, watcher->result());
        else
            page->setProperty("analysisPending", true);
        if (page->property("analysisPending").toBool())
            analyze(page);
    });
    // The analysis runs in a worker thread so large files do not freeze the editor.
    watcher->setFuture(QtConcurrent::run([this, editorId, text] { return computeAnalysis(editorId, text); }));
}

void ProjectView::followCursorInOutline(CodePage* page)
{
    if (!m_context.config().boolValue(QStringLiteral("auto_expand_tree")))
        return;
    QTreeView* tree = outlineTree(page->property("editorId").toString());
    auto* model = tree ? qobject_cast<QStandardItemModel*>(tree->model()) : nullptr;
    if (!model || tree->property("outlineMode").toString() == QLatin1String("variables"))
        return;
    // Entries are in document order, so the last one starting at or before the caret line is the most specific.
    const int line = page->editor()->cursorLine() - 1;
    QStandardItem* best = nullptr;
    std::function<void(QStandardItem*)> visit = [&](QStandardItem* parent) {
        for (int row = 0; row < parent->rowCount(); ++row) {
            QStandardItem* item = parent->child(row);
            if (item->data(Qt::UserRole).toInt() > line)
                return;
            best = item;
            visit(item);
        }
    };
    visit(model->invisibleRootItem());
    if (!best)
        return;
    tree->collapseAll();
    for (QStandardItem* parent = best->parent(); parent; parent = parent->parent())
        tree->expand(parent->index());
    tree->setCurrentIndex(best->index());
    tree->scrollTo(best->index());
}

QTreeView* ProjectView::outlineTree(const QString& editorId) const
{
    QScrollArea* panel = m_editorPanels.value(editorId);
    if (!panel || !panel->widget())
        return nullptr;
    for (QTreeView* tree : panel->widget()->findChildren<QTreeView*>()) {
        if (tree->objectName().endsWith(QLatin1String("_tree")))
            return tree;
    }
    return nullptr;
}

namespace {

QString itemPath(const QStandardItem* item)
{
    QString path;
    for (const QStandardItem* i = item; i; i = i->parent())
        path.prepend(i->text() + QLatin1Char('\x1f'));
    return path;
}

void collectExpanded(const QTreeView* tree, const QStandardItem* parent, QSet<QString>& expanded)
{
    for (int row = 0; row < parent->rowCount(); ++row) {
        const QStandardItem* item = parent->child(row);
        if (tree->isExpanded(item->index())) {
            expanded.insert(itemPath(item));
            collectExpanded(tree, item, expanded);
        }
    }
}

void addOutline(QStandardItem* parent, const QList<OutlineNode>& nodes, const IconSet& icons)
{
    for (const OutlineNode& node : nodes) {
        auto* item = new QStandardItem(node.text);
        item->setEditable(false);
        item->setData(node.line, Qt::UserRole);
        if (!node.icon.isEmpty())
            item->setIcon(icons.icon(node.icon));
        addOutline(item, node.children, icons);
        parent->appendRow(item);
    }
}

void restoreExpanded(QTreeView* tree, const QStandardItem* parent, const QSet<QString>& expanded)
{
    for (int row = 0; row < parent->rowCount(); ++row) {
        const QStandardItem* item = parent->child(row);
        if (expanded.contains(itemPath(item))) {
            tree->setExpanded(item->index(), true);
            restoreExpanded(tree, item, expanded);
        }
    }
}

} // namespace

bool ProjectView::showsVariables(CodePage* page) const
{
    const QAction* toggle = m_context.actions().action(QStringLiteral("st_var_list"));
    return toggle && toggle->isChecked() && !m_variables.value(page->property("filePath").toString()).isEmpty();
}

void ProjectView::refreshOutlines()
{
    for (QTabWidget* tabs : std::as_const(m_fileTabs)) {
        if (auto* page = qobject_cast<CodePage*>(tabs->currentWidget()))
            updateOutline(page);
    }
}

void ProjectView::updateOutline(CodePage* page)
{
    const QString editorId = page->property("editorId").toString();
    QTabWidget* tabs = m_fileTabs.value(editorId);
    QTreeView* tree = outlineTree(editorId);
    if (!tabs || tabs->currentWidget() != page || !tree)
        return;
    auto* model = qobject_cast<QStandardItemModel*>(tree->model());
    if (!model || model->parent() != tree) {
        model = new QStandardItemModel(tree);
        tree->setModel(model);
        tree->setHeaderHidden(true);
        tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
        const auto goToEntry = [this, editorId, tree](const QModelIndex& index) {
            const int line = index.data(Qt::UserRole).toInt();
            QTabWidget* fileTabs = m_fileTabs.value(editorId);
            auto* current = fileTabs ? qobject_cast<CodePage*>(fileTabs->currentWidget()) : nullptr;
            if (!current || line < 0)
                return;
            if (tree->property("outlineMode").toString() == QLatin1String("variables"))
                current->editor()->goToLineAtTop(line + 1);
            else
                current->editor()->goToLine(line + 1);
            current->editor()->setFocus();
        };
        connect(tree, &QTreeView::clicked, this, goToEntry);
        connect(tree, &QTreeView::activated, this, goToEntry);
    }
    const QString shown = tree->property("outlineMode").toString();
    const QString mode = showsVariables(page) ? QStringLiteral("variables") : QStringLiteral("code");
    QSet<QString> expanded;
    collectExpanded(tree, model->invisibleRootItem(), expanded);
    if (!shown.isEmpty()) {
        tree->setProperty(("expanded_" + shown).toLatin1().constData(),
                          QStringList(expanded.cbegin(), expanded.cend()));
        tree->setProperty(("scroll_" + shown).toLatin1().constData(), tree->verticalScrollBar()->value());
    }
    if (shown != mode) {
        const QStringList saved = tree->property(("expanded_" + mode).toLatin1().constData()).toStringList();
        expanded = QSet<QString>(saved.cbegin(), saved.cend());
    }
    const int scroll = tree->property(("scroll_" + mode).toLatin1().constData()).toInt();
    tree->setProperty("outlineMode", mode);
    model->clear();
    const QString path = page->property("filePath").toString();
    addOutline(model->invisibleRootItem(),
               mode == QLatin1String("variables") ? variableOutline(m_variables.value(path)) : m_outlines.value(path),
               m_context.theme().iconSet());
    restoreExpanded(tree, model->invisibleRootItem(), expanded);
    tree->doItemsLayout();
    tree->verticalScrollBar()->setValue(scroll);
}

void ProjectView::validateAll()
{
    for (CodePage* page : codePages()) {
        auto* file = qobject_cast<CodeFile*>(m_project->file(page->property("filePath").toString()));
        if (file)
            applyAnalysis(page,
                          computeAnalysis(page->property("editorId").toString(), file->document()->toPlainText()));
    }
}

bool ProjectView::performOrganizer(const QString& actionId)
{
    if (m_currentEditor == QLatin1String("backgrounds") && m_backgrounds)
        return m_backgrounds->perform(actionId);
    if (m_currentEditor == QLatin1String("animations") && m_animations)
        return m_animations->organizer()->perform(actionId);
    if (m_currentEditor == QLatin1String("sounds") && m_sounds)
        return m_sounds->organizer()->perform(actionId);
    if (m_currentEditor == QLatin1String("commands") && m_commandsOrganizer)
        return m_commandsOrganizer->perform(actionId);
    return false;
}

CodeFile* ProjectView::commandsFile() const
{
    QTabWidget* tabs = m_fileTabs.value(QStringLiteral("commands"));
    QWidget* page = tabs ? tabs->currentWidget() : nullptr;
    return page ? qobject_cast<CodeFile*>(m_project->file(page->property("filePath").toString())) : nullptr;
}

void ProjectView::createCommandsOrganizer()
{
    // The organizer lists the [Command] blocks by name and moves whole blocks. Organize automatically leaves them alone
    // because their order decides which command wins.
    static const QRegularExpression header(QStringLiteral("^\\s*\\[\\s*command\\s*\\]"),
                                           QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression name(QStringLiteral("^\\s*name\\s*=\\s*([^;]*)"),
                                         QRegularExpression::CaseInsensitiveOption);
    OrganizerBinding::Source source;
    source.captions = [this] {
        QStringList rows;
        const CodeFile* file = commandsFile();
        if (!file)
            return rows;
        const QStringList lines = AirEdits::lines(file->document());
        for (const Organize::Block& block : Organize::sectionBlocks(lines, header)) {
            QString caption;
            for (int i = block.start; i < block.contentEnd && caption.isEmpty(); ++i) {
                const QRegularExpressionMatch m = name.match(lines.at(i));
                if (m.hasMatch())
                    caption = m.captured(1).trimmed().remove(QLatin1Char('"'));
            }
            rows << caption;
        }
        return rows;
    };
    source.reorder = [this](const QList<int>& order) {
        CodeFile* file = commandsFile();
        if (!file)
            return;
        QStringList lines = AirEdits::lines(file->document());
        const QList<Organize::Block> blocks = Organize::sectionBlocks(lines, header);
        if (blocks.size() != order.size())
            return;
        Organize::reorderBlocks(lines, blocks, order);
        AirEdits::apply(file->document(), lines);
    };
    source.activate = [this](int row) {
        const CodeFile* file = commandsFile();
        if (!file)
            return;
        const QStringList lines = AirEdits::lines(file->document());
        const QList<Organize::Block> blocks = Organize::sectionBlocks(lines, header);
        if (row < 0 || row >= blocks.size())
            return;
        int line = blocks[row].start;
        while (line < blocks[row].contentEnd && !header.match(lines.at(line)).hasMatch())
            ++line;
        showFileLine(file->path(), line + 1);
    };
    m_commandsOrganizer = new OrganizerBinding(source, this);
    auto* timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(300);
    connect(timer, &QTimer::timeout, m_commandsOrganizer, &OrganizerBinding::refresh);
    for (ProjectFile* f : m_project->files()) {
        if (auto* code = qobject_cast<CodeFile*>(f); code && code->editorId() == QLatin1String("commands"))
            connect(code->document(), &QTextDocument::contentsChanged, timer, qOverload<>(&QTimer::start));
    }
    connect(m_project, &Project::fileAdded, this, [timer](ProjectFile* f) {
        if (auto* code = qobject_cast<CodeFile*>(f); code && code->editorId() == QLatin1String("commands"))
            connect(code->document(), &QTextDocument::contentsChanged, timer, qOverload<>(&QTimer::start));
    });
}

bool ProjectView::showFileLine(const QString& filePath, int line)
{
    for (auto it = m_fileTabs.cbegin(); it != m_fileTabs.cend(); ++it) {
        QTabWidget* tabs = it.value();
        for (int i = 0; i < tabs->count(); ++i) {
            if (tabs->widget(i)->property("filePath").toString().compare(filePath, Qt::CaseInsensitive) != 0)
                continue;
            setCurrentEditor(it.key());
            tabs->setCurrentIndex(i);
            if (auto* sprites = qobject_cast<SpritePage*>(tabs->widget(i))) {
                if (line > 0)
                    sprites->setCurrent(line - 1);
            }
            if (auto* animation = qobject_cast<AnimationPage*>(tabs->widget(i))) {
                if (line > 0)
                    animation->showLine(line);
                return true;
            }
            if (auto* page = qobject_cast<CodePage*>(tabs->widget(i))) {
                if (line > 0)
                    page->editor()->goToLine(line);
                page->editor()->setFocus();
            }
            return true;
        }
    }
    return false;
}

ProjectFile* ProjectView::newFile(const QString& editorId)
{
    const EditorInfo info = engine()->editor(editorId);
    const bool sprites = editorId == QLatin1String("sprites");
    const bool sounds = editorId == QLatin1String("sounds");
    if (info.id.isEmpty() ||
        (info.workspace != WorkspaceType::Code && !sprites && !sounds && editorId != QLatin1String("animations")))
        return nullptr;
    const QDir dir(m_project->directory());
    QString path;
    for (int n = 1;; ++n) {
        path = dir.filePath(QStringLiteral("untitled%1.%2").arg(n).arg(info.defaultSuffix));
        if (!m_project->file(path) && !QFileInfo::exists(path))
            break;
    }
    ProjectFile* file = nullptr;
    if (sounds) {
        file = m_project->newSoundFile(path, editorId);
    } else if (sprites) {
        // The SFF version the project's engine version reads.
        const QString version = m_project->versionId();
        file = m_project->newSpriteFile(path, editorId,
                                        version == QLatin1String("1.1")   ? Formats::SffVersion::V201
                                        : version == QLatin1String("1.0") ? Formats::SffVersion::V2
                                                                          : Formats::SffVersion::V1);
    } else {
        file = m_project->newCodeFile(path, editorId);
    }
    addFileTab(info, file->path());
    return file;
}

bool ProjectView::openFile(const QString& editorId, const QString& path, QString* error)
{
    const EditorInfo info = engine()->editor(editorId);
    QTabWidget* tabs = m_fileTabs.value(editorId);
    const bool sprites = editorId == QLatin1String("sprites");
    const bool sounds = editorId == QLatin1String("sounds");
    if (info.id.isEmpty() ||
        (info.workspace != WorkspaceType::Code && !sprites && !sounds && editorId != QLatin1String("animations")) ||
        !tabs)
        return false;
    const QString target = QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath());
    for (int i = 0; i < tabs->count(); ++i) {
        if (QFileInfo(tabs->widget(i)->property("filePath").toString()) == QFileInfo(target)) {
            setCurrentEditor(editorId);
            tabs->setCurrentIndex(i);
            return true;
        }
    }
    ProjectFile* file = sprites  ? static_cast<ProjectFile*>(m_project->addSpriteFile(target, editorId, error))
                        : sounds ? static_cast<ProjectFile*>(m_project->addSoundFile(target, editorId, error))
                                 : m_project->addCodeFile(target, editorId, error);
    if (!file)
        return false;
    addFileTab(info, file->path());
    fillProjectExplorer();
    return true;
}

void ProjectView::addFileTab(const EditorInfo& info, const QString& filePath)
{
    QTabWidget* tabs = m_fileTabs.value(info.id);
    if (!tabs)
        return;
    if (tabs->count() == 1 && tabs->widget(0)->objectName() == QStringLiteral("workspace_") + info.id) {
        QWidget* empty = tabs->widget(0);
        tabs->removeTab(0);
        empty->deleteLater();
        tabs->tabBar()->show();
    }
    const int index = tabs->addTab(createFilePage(info, filePath), QFileInfo(filePath).fileName());
    tabs->setTabToolTip(index, QDir::toNativeSeparators(filePath));
    setCurrentEditor(info.id);
    tabs->setCurrentIndex(index);
    updateTabTitles();
    if (auto* page = qobject_cast<CodePage*>(tabs->widget(index))) {
        page->editor()->setCodeMenu(m_codeMenu);
        page->editor()->setFocus();
    }
}

void ProjectView::setBreakpoints(Debug::BreakpointList* breakpoints)
{
    m_breakpoints = breakpoints;
    for (EngineWorkspace* workspace : std::as_const(m_workspaces)) {
        if (workspace)
            workspace->setBreakpoints(breakpoints);
    }
    if (!breakpoints)
        return;
    connect(breakpoints, &Debug::BreakpointList::changed, this, [this] {
        for (CodePage* page : codePages())
            refreshBreakpointMarks(page);
    });
    for (CodePage* page : codePages())
        refreshBreakpointMarks(page);
}

void ProjectView::refreshBreakpointMarks(CodePage* page)
{
    // Bound to a running session (active, or waiting for its ignore count), else pending.
    QHash<int, CodeEditor::BreakpointMark> marks;
    if (m_breakpoints) {
        for (const Debug::Breakpoint& b : m_breakpoints->inFile(page->property("filePath").toString())) {
            CodeEditor::BreakpointMark mark = CodeEditor::BreakpointMark::Pending;
            if (!b.enabled)
                mark = CodeEditor::BreakpointMark::Disabled;
            else if (m_debugging)
                mark = b.ignoreCount > 0 ? CodeEditor::BreakpointMark::Ignored : CodeEditor::BreakpointMark::Active;
            marks.insert(b.line - 1, mark);
        }
    }
    page->editor()->setBreakpointMarks(marks);
}

EngineWorkspace* ProjectView::debugWorkspace() const
{
    const QString id = debugEditor();
    return id.isEmpty() ? nullptr : m_workspaces.value(id);
}

QString ProjectView::debugEditor() const
{
    for (const EditorInfo& info : engine()->editors()) {
        if (info.workspace == WorkspaceType::Emulator && m_workspaces.value(info.id))
            return info.id;
    }
    return QString();
}

QWidget* ProjectView::debugPanel() const
{
    QScrollArea* panel = m_extraPanels.value(QStringLiteral("debug"));
    return panel ? panel->widget() : nullptr;
}

void ProjectView::setDebugging(bool debugging)
{
    if (m_debugging == debugging)
        return;
    m_debugging = debugging;
    updateRightPanel();
    for (CodePage* page : codePages())
        refreshBreakpointMarks(page);
}

void ProjectView::showDebugLine(const QString& file, int line)
{
    for (CodeEditor* editor : codeEditors())
        editor->setCurrentDebugLine(-1);
    if (file.isEmpty() || line <= 0)
        return;
    // A file of the match without a tab (the engine's common states) opens in the States editor.
    if (!showFileLine(file, line)) {
        const QString states = QStringLiteral("states");
        if (!editorIds().contains(states) || !openFile(states, file) || !showFileLine(file, line))
            return;
    }
    CodePage* page = currentCodePage();
    if (page && page->property("filePath").toString().compare(file, Qt::CaseInsensitive) == 0)
        page->editor()->setCurrentDebugLine(line - 1);
}

void ProjectView::fileRenamed(QWidget* page, const QString& oldPath, const QString& newPath)
{
    if (m_breakpoints)
        m_breakpoints->renameFile(oldPath, newPath);
    page->setProperty("filePath", newPath);
    if (QTabWidget* tabs = m_fileTabs.value(page->property("editorId").toString())) {
        const int index = tabs->indexOf(page);
        if (index >= 0)
            tabs->setTabToolTip(index, tabToolTip(newPath));
    }
    if (m_issues.contains(oldPath)) {
        IssueList issues = m_issues.take(oldPath);
        for (Issue& issue : issues)
            issue.file = newPath;
        if (auto* code = qobject_cast<CodePage*>(page))
            code->setIssues(issues, m_context.theme().iconSet());
        m_issues.insert(newPath, issues);
    }
    if (m_outlines.contains(oldPath))
        m_outlines.insert(newPath, m_outlines.take(oldPath));
    if (m_variables.contains(oldPath))
        m_variables.insert(newPath, m_variables.take(oldPath));
    updateTabTitles();
    fillProjectExplorer();
    Q_EMIT issuesChanged();
    Q_EMIT currentFileChanged();
}

ProjectFile* ProjectView::currentFile() const
{
    QTabWidget* tabs = m_fileTabs.value(m_currentEditor);
    if (!tabs || !tabs->currentWidget())
        return nullptr;
    return m_project->file(tabs->currentWidget()->property("filePath").toString());
}

QList<ProjectFile*> ProjectView::editorFiles(const QString& editorId) const
{
    QList<ProjectFile*> files;
    if (QTabWidget* tabs = m_fileTabs.value(editorId)) {
        for (int i = 0; i < tabs->count(); ++i) {
            if (ProjectFile* f = m_project->file(tabs->widget(i)->property("filePath").toString()))
                files << f;
        }
    }
    return files;
}

QList<CodeEditor*> ProjectView::codeEditors() const
{
    QList<CodeEditor*> editors;
    for (CodePage* page : codePages())
        editors << page->editor();
    return editors;
}

void ProjectView::updateTabTitles()
{
    for (QTabWidget* tabs : std::as_const(m_fileTabs)) {
        for (int i = 0; i < tabs->count(); ++i) {
            const QString path = tabs->widget(i)->property("filePath").toString();
            if (path.isEmpty())
                continue;
            tabs->setTabText(i, tabCaption(path));
        }
    }
}

QString ProjectView::tabCaption(const QString& path) const
{
    const ProjectFile* f = m_project->file(path);
    const QString name = f && f->isNameless() ? QString() : QFileInfo(path).fileName();
    return name + (f && f->isModified() ? QStringLiteral("*") : QString());
}

QString ProjectView::tabToolTip(const QString& path) const
{
    const ProjectFile* f = m_project->file(path);
    if (f && f->isNameless())
        return {};
    if (!m_project->archivePath().isEmpty() && (!f || f->isUntitled()))
        return QDir::toNativeSeparators(m_project->archivePath());
    return QDir::toNativeSeparators(path);
}

void ProjectView::fillProjectExplorer()
{
    auto* model = new QStandardItemModel(m_explorer);
    QStringList files;
    for (const QString& id : editorIds()) {
        for (const QString& f : m_project->editorFiles(id)) {
            // Blank documents never saved are not on disk yet, whatever is at their suggested path.
            const ProjectFile* file = m_project->file(f);
            if (!f.isEmpty() && QFileInfo::exists(f) && !(file && file->isNameless() && !file->isCopy()) &&
                !m_project->isSingleFile() && !files.contains(f, Qt::CaseInsensitive))
                files << f;
        }
    }
    std::sort(files.begin(), files.end(), [](const QString& a, const QString& b) {
        return QFileInfo(a).fileName().compare(QFileInfo(b).fileName(), Qt::CaseInsensitive) < 0;
    });
    const IconSet& icons = m_context.theme().iconSet();
    for (const QString& f : files) {
        auto* item = new QStandardItem(icons.icon(iconForFile(f)), QFileInfo(f).fileName());
        item->setToolTip(QDir::toNativeSeparators(f));
        item->setData(f, Qt::UserRole);
        model->appendRow(item);
    }
    if (QAbstractItemModel* old = m_explorer->model())
        old->deleteLater();
    m_explorer->setModel(model);
}

void ProjectView::setCurrentEditor(const QString& id)
{
    if (!m_editorPanels.contains(id))
        return;
    if (m_currentEditor == QLatin1String("sprites") && id != m_currentEditor && m_sprites && !m_sprites->confirmLeave())
        return;
    if (m_currentEditor == QLatin1String("sounds") && id != m_currentEditor && m_sounds && !m_sounds->confirmLeave())
        return;
    if (m_currentEditor == QLatin1String("animations") && id == QLatin1String("sprites"))
        showFrameSprite();
    const bool changed = m_currentEditor != id;
    m_currentEditor = id;
    const EditorInfo info = currentEditorInfo();
    m_left->setTitle(info.title);
    if (m_leftTabs->currentIndex() == 0)
        m_leftStack->setCurrentWidget(m_editorPanels.value(id));
    m_center->setCurrentWidget(m_fileTabs.value(id));
    updateRightPanel();
    if (changed) {
        // A code editor, the Backgrounds canvas and the Emulator take the keyboard focus when their editor is chosen.
        if (QTabWidget* tabs = m_fileTabs.value(id)) {
            if (CodePage* code = codePageOf(tabs->currentWidget()))
                code->editor()->setFocus();
            else if (auto* backgrounds = qobject_cast<BackgroundsPage*>(tabs->currentWidget()))
                backgrounds->view()->setFocus();
            else if (EngineWorkspace* workspace = m_workspaces.value(id))
                workspace->setFocus();
        }
        Q_EMIT currentEditorChanged(id);
        Q_EMIT currentFileChanged();
    }
}

void ProjectView::applyPreset(const Preset::Model& model)
{
    PresetBuilder builder(m_context.controls(), m_context.actions());
    for (auto it = m_editorPanels.cbegin(); it != m_editorPanels.cend(); ++it) {
        QWidget* content = it.value()->widget();
        const auto panel = model.panels.constFind(it.key());
        if (panel != model.panels.cend() && panel->layout)
            builder.buildPanel(*panel->layout, content, 22);
    }
    for (auto it = m_extraPanels.cbegin(); it != m_extraPanels.cend(); ++it) {
        QWidget* content = it.value()->widget();
        const auto panel = model.extraPanels.constFind(it.key());
        if (panel != model.extraPanels.cend() && panel->layout)
            builder.buildPanel(*panel->layout, content, 22);
    }
    if (!m_extraPanels.contains(QStringLiteral("organizer"))) {
        QScrollArea* organizer = makeScrollArea(m_rightStack);
        organizer->setObjectName(QStringLiteral("extra_organizer"));
        m_rightStack->addWidget(organizer);
        m_extraPanels.insert(QStringLiteral("organizer"), organizer);
    }
    const auto organizer = model.extraPanels.constFind(QStringLiteral("organizer"));
    if (organizer != model.extraPanels.cend() && organizer->layout)
        builder.buildPanel(*organizer->layout, m_extraPanels.value(QStringLiteral("organizer"))->widget(), 22);
    if (!debugEditor().isEmpty() && !m_extraPanels.contains(QStringLiteral("debug"))) {
        QScrollArea* debug = makeScrollArea(m_rightStack);
        debug->setObjectName(QStringLiteral("extra_debug"));
        m_rightStack->addWidget(debug);
        m_extraPanels.insert(QStringLiteral("debug"), debug);
        const auto panel = model.extraPanels.constFind(QStringLiteral("debug"));
        if (panel != model.extraPanels.cend() && panel->layout)
            builder.buildPanel(*panel->layout, debug->widget(), 22);
    }
    if (QWidget* debug = debugPanel()) {
        if (auto* variables = debug->findChild<QTreeView*>(QStringLiteral("debug_variables"))) {
            variables->setHeaderHidden(false);
            variables->setContextMenuPolicy(Qt::ActionsContextMenu);
            for (const char* id : {"debug_data_breakpoint", "debug_data_watch1", "debug_data_watch2"}) {
                if (QAction* action = m_context.actions().action(QString::fromLatin1(id)))
                    variables->addAction(action);
            }
        }
    }
    updateRightPanel();
    if (m_sprites) {
        if (!m_extraPanels.contains(QStringLiteral("thumbnails"))) {
            QScrollArea* thumbnails = makeScrollArea(m_rightStack);
            thumbnails->setObjectName(QStringLiteral("extra_thumbnails"));
            m_rightStack->addWidget(thumbnails);
            m_extraPanels.insert(QStringLiteral("thumbnails"), thumbnails);
            const auto panel = model.extraPanels.constFind(QStringLiteral("thumbnails"));
            if (panel != model.extraPanels.cend() && panel->layout)
                builder.buildPanel(*panel->layout, thumbnails->widget(), 22);
        }
        m_sprites->bindPanel(m_editorPanels.value(QStringLiteral("sprites"))->widget());
        m_sprites->bindOrganizer(m_extraPanels.value(QStringLiteral("organizer"))->widget());
        if (QScrollArea* palettes = m_extraPanels.value(QStringLiteral("palettes")))
            m_sprites->bindPalettes(palettes->widget());
        m_sprites->bindExplorer(m_extraPanels.value(QStringLiteral("thumbnails"))->widget());
    }
    if (m_sounds)
        m_sounds->bindPanel(m_editorPanels.value(QStringLiteral("sounds"))->widget());
    for (auto it = m_workspaces.cbegin(); it != m_workspaces.cend(); ++it) {
        if (it.value() && m_editorPanels.contains(it.key()))
            it.value()->bindPanel(m_editorPanels.value(it.key())->widget());
    }
    if (m_backgrounds) {
        m_backgrounds->bindPanel(m_editorPanels.value(QStringLiteral("backgrounds"))->widget());
        if (QScrollArea* controllers = m_extraPanels.value(QStringLiteral("controllers")))
            m_backgrounds->bindControllers(controllers->widget());
        m_backgrounds->bindOrganizer(m_extraPanels.value(QStringLiteral("organizer"))->widget());
    }
    QWidget* organizerPanel = m_extraPanels.value(QStringLiteral("organizer"))->widget();
    if (m_sounds)
        m_sounds->organizer()->bind(organizerPanel);
    if (m_commandsOrganizer)
        m_commandsOrganizer->bind(organizerPanel);
    if (m_animations) {
        m_animations->organizer()->bind(organizerPanel);
        m_animations->bindPanel(m_editorPanels.value(QStringLiteral("animations"))->widget());
        if (QScrollArea* thumbnails = m_extraPanels.value(QStringLiteral("thumbnails")))
            m_animations->bindExplorer(thumbnails->widget());
    }
    for (QTabWidget* tabs : std::as_const(m_fileTabs)) {
        if (auto* page = qobject_cast<CodePage*>(tabs->currentWidget()))
            updateOutline(page);
    }
}

void ProjectView::setLeftPanelVisible(bool visible)
{
    m_left->setVisible(visible);
}

void ProjectView::setRightPanelVisible(bool visible)
{
    m_rightPanelVisible = visible;
    updateRightPanel();
}

void ProjectView::setOrganizerVisible(bool visible)
{
    m_organizerVisible = visible;
    updateRightPanel();
}

QString ProjectView::extraPanelTitle(const QString& name)
{
    if (name == QLatin1String("palettes"))
        return tr("Palettes");
    if (name == QLatin1String("thumbnails"))
        return tr("Sprite explorer");
    if (name == QLatin1String("controllers"))
        return tr("Controllers");
    if (name == QLatin1String("organizer"))
        return tr("Organizer");
    if (name == QLatin1String("debug"))
        return tr("Debug");
    return name;
}

void ProjectView::updateRightPanel()
{
    static const QStringList organizerEditors = {QStringLiteral("sprites"), QStringLiteral("animations"),
                                                 QStringLiteral("sounds"), QStringLiteral("commands"),
                                                 QStringLiteral("backgrounds")};
    QString name = currentEditorInfo().extraPanel;
    if (m_organizerVisible && organizerEditors.contains(m_currentEditor))
        name = QStringLiteral("organizer");
    const WorkspaceType workspace = currentEditorInfo().workspace;
    if (m_debugging && m_extraPanels.contains(QStringLiteral("debug")) &&
        (workspace == WorkspaceType::Code || workspace == WorkspaceType::Emulator))
        name = QStringLiteral("debug");
    // The organizer lists the items of the editor shown, or the sprites in other editors.
    const bool organizer = name == QLatin1String("organizer");
    const bool backgroundsOrganizer = organizer && m_currentEditor == QLatin1String("backgrounds") && m_backgrounds;
    const bool animationsOrganizer = organizer && m_currentEditor == QLatin1String("animations") && m_animations;
    const bool soundsOrganizer = organizer && m_currentEditor == QLatin1String("sounds") && m_sounds;
    const bool commandsOrganizer = organizer && m_currentEditor == QLatin1String("commands") && m_commandsOrganizer;
    const bool spritesOrganizer =
        !backgroundsOrganizer && !animationsOrganizer && !soundsOrganizer && !commandsOrganizer;
    if (m_sprites && !spritesOrganizer)
        m_sprites->setOrganizerActive(false);
    if (m_backgrounds && !backgroundsOrganizer)
        m_backgrounds->setOrganizerActive(false);
    if (m_animations && !animationsOrganizer)
        m_animations->organizer()->setActive(false);
    if (m_sounds && !soundsOrganizer)
        m_sounds->organizer()->setActive(false);
    if (m_commandsOrganizer && !commandsOrganizer)
        m_commandsOrganizer->setActive(false);
    if (m_sprites && spritesOrganizer)
        m_sprites->setOrganizerActive(true);
    if (backgroundsOrganizer)
        m_backgrounds->setOrganizerActive(true);
    if (animationsOrganizer)
        m_animations->organizer()->setActive(true);
    if (soundsOrganizer)
        m_sounds->organizer()->setActive(true);
    if (commandsOrganizer)
        m_commandsOrganizer->setActive(true);
    const bool spriteExplorer = m_currentEditor == QLatin1String("sprites") && m_sprites && m_sprites->explorerShown();
    if (spriteExplorer)
        name = QStringLiteral("thumbnails");
    if (m_sprites)
        m_sprites->setExplorerActive(spriteExplorer && m_rightPanelVisible);
    if (m_animations)
        m_animations->setExplorerActive(name == QLatin1String("thumbnails") &&
                                        m_currentEditor == QLatin1String("animations") && m_rightPanelVisible);
    QScrollArea* panel = m_extraPanels.value(name);
    if (panel && name == QLatin1String("thumbnails")) {
        static const char* const spriteCaptions[] = {
            QT_TR_NOOP("Add selected as layers"), QT_TR_NOOP("Add all as layers"),
            QT_TR_NOOP("Add selected as layers (aligned)"), QT_TR_NOOP("Add all as layers (aligned)")};
        static const char* const animationCaptions[] = {
            QT_TR_NOOP("Add selected to the current animation"), QT_TR_NOOP("Add all to the current animation"),
            QT_TR_NOOP("New animation with selected"), QT_TR_NOOP("New animation with all")};
        for (int i = 0; i < 4; ++i) {
            auto* button = panel->widget()->findChild<QToolButton*>(QStringLiteral("thumb_action%1").arg(i + 1));
            if (!button)
                continue;
            const QString caption = tr(spriteExplorer ? spriteCaptions[i] : animationCaptions[i]);
            button->setText(caption);
            button->setToolTip(caption);
            button->setStatusTip(caption);
        }
    }
    const bool show = m_rightPanelVisible && panel;
    if (panel) {
        m_rightStack->setCurrentWidget(panel);
        m_right->setTitle(extraPanelTitle(name));
    }
    m_right->setVisible(show);
}

void ProjectView::adoptBottomDock(BottomDock* dock)
{
    if (m_dock == dock && dock->parentWidget() == this)
        return;
    if (m_dock)
        disconnect(m_dock, nullptr, this, nullptr);
    m_dock = dock;
    m_workspaceLayout->addWidget(dock);
    dock->show();
    connect(dock, &BottomDock::expandedChanged, this, &ProjectView::updateDockStretch);
    updateDockStretch();
}

void ProjectView::adoptQuickFind(QWidget* bar)
{
    if (bar->parentWidget() == this)
        return;
    m_workspaceLayout->insertWidget(m_workspaceLayout->indexOf(m_center) + 1, bar);
}

void ProjectView::updateDockStretch()
{
    if (!m_dock)
        return;
    m_workspaceLayout->setStretchFactor(m_center, 1);
    m_workspaceLayout->setStretchFactor(m_dock, m_dock->isExpanded() ? 1 : 0);
}

} // namespace OFS
