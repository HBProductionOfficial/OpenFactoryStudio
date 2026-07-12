#include "shell/ProjectView.h"

#include "core/actions/ActionRegistry.h"
#include "core/app/AppContext.h"
#include "core/code/CodeEditor.h"
#include "core/engine/Engine.h"
#include "core/preset/ControlRegistry.h"
#include "core/preset/PresetBuilder.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"
#include "core/settings/Config.h"
#include "core/theme/ThemeManager.h"
#include "core/widgets/EPanelWidget.h"
#include "shell/BottomDock.h"
#include "shell/CodePage.h"

#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>

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
    m_explorer->setRootIsDecorated(false);
    m_explorer->setEditTriggers(QAbstractItemView::NoEditTriggers);
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
    disconnect(m_project, nullptr, this, nullptr);
    delete m_center;
    m_center = nullptr;
    m_fileTabs.clear();
}

Engine* ProjectView::engine() const
{
    return m_project->engine();
}

QStringList ProjectView::editorIds() const
{
    return m_project->editorIds();
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
    for (const QString& file : m_project->editorFiles(info.id)) {
        if (file.isEmpty())
            continue;
        const int index = tabs->addTab(createFilePage(info, file), QFileInfo(file).fileName());
        tabs->setTabToolTip(index, QDir::toNativeSeparators(file));
    }
    connect(tabs, &QTabWidget::currentChanged, this, [this, tabs] {
        if (tabs == m_center->currentWidget())
            Q_EMIT currentFileChanged();
    });
    if (tabs->count() == 0) {
        auto* empty = new QWidget(tabs);
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
    if (info.workspace == WorkspaceType::Code) {
        QString error;
        CodeFile* file = QFileInfo::exists(filePath) ? m_project->codeFile(filePath, info.id, &error) : nullptr;
        auto* page = new CodePage(m_context.actions());
        page->setProperty("filePath", filePath);
        page->setProperty("editorId", info.id);
        CodeEditor* editor = page->editor();
        if (file) {
            editor->setCodeDocument(file->document());
            connect(file, &ProjectFile::modifiedChanged, this, [this] { Q_EMIT modifiedChanged(); });
            auto* timer = new QTimer(page);
            timer->setSingleShot(true);
            timer->setInterval(300);
            connect(timer, &QTimer::timeout, this, [this, page] { validate(page); });
            connect(file->document(), &QTextDocument::contentsChanged, timer, qOverload<>(&QTimer::start));
            QTimer::singleShot(0, this, [this, page] { validate(page); });
        } else {
            editor->setReadOnly(true);
        }
        page->setToolBarVisible(m_context.config().boolValue(QStringLiteral("show_code_bar"), true));
        configureCodeEditor(editor, info.id);
        return page;
    }
    auto* workspace = new QWidget;
    workspace->setObjectName(QStringLiteral("workspace_") + info.id);
    return workspace;
}

void ProjectView::configureCodeEditor(CodeEditor* editor, const QString& editorId)
{
    const Config& config = m_context.config();
    QFont font(config.value(QStringLiteral("font_name"), QStringLiteral("Courier New")),
               config.intValue(QStringLiteral("font_size"), 10));
    font.setStyleHint(QFont::Monospace);
    editor->setFont(font);
    editor->setTabStopDistance(editor->fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);
    CodeEditor::Options options;
    options.lineNumbers = config.boolValue(QStringLiteral("code_line_numbers"), true);
    options.foldingMarkers = config.boolValue(QStringLiteral("code_folding_markers"), true);
    options.highlightLine = config.boolValue(QStringLiteral("code_highlight_line"), true);
    options.parenMatching = config.boolValue(QStringLiteral("code_paren_matching"), true);
    editor->setOptions(options);
    editor->setSyntax(engine()->syntax(), engine()->syntaxFileType(*m_project, editorId));
    editor->setColorSet(m_context.theme().colorSet());
    connect(editor, &CodeEditor::cursorPositionChanged, this, [this, editor] {
        if (editor == currentCodeEditor())
            Q_EMIT currentFileChanged();
    });
}

void ProjectView::reconfigureEditors()
{
    for (auto it = m_fileTabs.cbegin(); it != m_fileTabs.cend(); ++it) {
        for (int i = 0; i < it.value()->count(); ++i) {
            if (auto* page = qobject_cast<CodePage*>(it.value()->widget(i)))
                configureCodeEditor(page->editor(), it.key());
        }
    }
}

CodeEditor* ProjectView::currentCodeEditor() const
{
    CodePage* page = currentCodePage();
    return page ? page->editor() : nullptr;
}

CodePage* ProjectView::currentCodePage() const
{
    QTabWidget* tabs = m_fileTabs.value(m_currentEditor);
    return tabs ? qobject_cast<CodePage*>(tabs->currentWidget()) : nullptr;
}

QList<CodePage*> ProjectView::codePages() const
{
    QList<CodePage*> pages;
    for (QTabWidget* tabs : m_fileTabs) {
        for (int i = 0; i < tabs->count(); ++i) {
            if (auto* page = qobject_cast<CodePage*>(tabs->widget(i)))
                pages << page;
        }
    }
    return pages;
}

void ProjectView::setCodeToolBarsVisible(bool visible)
{
    for (CodePage* page : codePages())
        page->setToolBarVisible(visible);
}

void ProjectView::validate(CodePage* page)
{
    const QString path = page->property("filePath").toString();
    auto* file = qobject_cast<CodeFile*>(m_project->file(path));
    if (!file)
        return;
    IssueList issues =
        engine()->validate(*m_project, page->property("editorId").toString(), file->document()->toPlainText());
    for (Issue& issue : issues)
        issue.file = path;
    page->setIssues(issues, m_context.theme().iconSet());
    m_issues.insert(path, issues);
    Q_EMIT issuesChanged();
}

void ProjectView::validateAll()
{
    for (CodePage* page : codePages())
        validate(page);
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
            if (auto* page = qobject_cast<CodePage*>(tabs->widget(i))) {
                page->editor()->goToLine(line);
                page->editor()->setFocus();
            }
            return true;
        }
    }
    return false;
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
            const ProjectFile* f = m_project->file(path);
            tabs->setTabText(i, QFileInfo(path).fileName() + (f && f->isModified() ? QStringLiteral("*") : QString()));
        }
    }
}

void ProjectView::fillProjectExplorer()
{
    auto* model = new QStandardItemModel(m_explorer);
    QStringList files;
    for (const QString& id : editorIds()) {
        for (const QString& f : m_project->editorFiles(id)) {
            if (!f.isEmpty() && QFileInfo::exists(f) && !files.contains(f, Qt::CaseInsensitive))
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
    const bool changed = m_currentEditor != id;
    m_currentEditor = id;
    const EditorInfo info = currentEditorInfo();
    m_left->setTitle(info.title);
    if (m_leftTabs->currentIndex() == 0)
        m_leftStack->setCurrentWidget(m_editorPanels.value(id));
    m_center->setCurrentWidget(m_fileTabs.value(id));
    updateRightPanel();
    if (changed) {
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
    updateRightPanel();
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
                                                 QStringLiteral("sounds"), QStringLiteral("commands")};
    QString name = currentEditorInfo().extraPanel;
    if (m_organizerVisible && organizerEditors.contains(m_currentEditor))
        name = QStringLiteral("organizer");
    QScrollArea* panel = m_extraPanels.value(name);
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

void ProjectView::updateDockStretch()
{
    if (!m_dock)
        return;
    m_workspaceLayout->setStretchFactor(m_center, 1);
    m_workspaceLayout->setStretchFactor(m_dock, m_dock->isExpanded() ? 1 : 0);
}

} // namespace OFS
