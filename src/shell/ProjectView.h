#pragma once

#include "core/code/CodeOutline.h"
#include "core/code/VariableUsage.h"
#include "core/editors/EditorInfo.h"
#include "core/engine/EngineDebugSession.h"
#include "core/preset/PresetModel.h"
#include "core/syntax/Issue.h"

#include <QFutureWatcher>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QWidget>

#include <functional>

class EPanelWidget;
class QMenu;
class QScrollArea;
class QStackedWidget;
class QTabBar;
class QTabWidget;
class QTreeView;
class QVBoxLayout;

namespace OFS {

class EngineWorkspace;

namespace Debug {
class BreakpointList;
class Target;
} // namespace Debug

class AppContext;
class BottomDock;
class CodeEditor;
class CodePage;
class Engine;
class Project;
class ProjectFile;
class AnimationsEditor;
class CodeFile;
class OrganizerBinding;
class BackgroundsEditor;
class SoundsEditor;
class SpritesEditor;

// Everything below the project tab bar for one open project.
class ProjectView : public QWidget
{
    Q_OBJECT

public:
    // `progress` gets the steps done of the total (each file shown and each editor) for the loading window.
    ProjectView(Project* project, AppContext& context, QWidget* parent = nullptr,
                const std::function<void(int done, int total)>& progress = {});
    ~ProjectView() override;

    Project* project() const { return m_project; }
    Engine* engine() const;

    QStringList editorIds() const;
    QString currentEditor() const { return m_currentEditor; }
    QTabWidget* fileTabs(const QString& editorId) const { return m_fileTabs.value(editorId); }
    EditorInfo currentEditorInfo() const;
    void setCurrentEditor(const QString& id);

    void applyPreset(const Preset::Model& model);

    void setLeftPanelVisible(bool visible);
    void setRightPanelVisible(bool visible);
    void setOrganizerVisible(bool visible);

    void adoptBottomDock(BottomDock* dock);
    void setBreakpoints(Debug::BreakpointList* breakpoints);
    void adoptQuickFind(QWidget* bar);

    static QString extraPanelTitle(const QString& name);

    CodeEditor* currentCodeEditor() const;
    CodePage* currentCodePage() const;
    EngineWorkspace* currentWorkspace() const;
    EngineWorkspace* debugWorkspace() const;
    QString debugEditor() const;
    EngineDebugSession* engineDebugSession() const { return m_engineSession; }
    // Where the debug actions go: the engine's session while it runs, else the Emulator.
    Debug::Target* debugTarget() const;
    QWidget* debugPanel() const;
    bool isDebugging() const { return m_debugging; }
    QList<CodePage*> codePages() const;
    // `line` is 1-based, 0 keeps the cursor where it is.
    bool showFileLine(const QString& filePath, int line);
    // Returns false when the Sprites editor's list is shown.
    bool performOrganizer(const QString& actionId);
    OrganizerBinding* commandsOrganizer() const { return m_commandsOrganizer; }
    void setCodeToolBarsVisible(bool visible);
    void setCodeMenu(QMenu* menu);
    void refreshSnippets();
    const QHash<QString, IssueList>& issues() const { return m_issues; }
    void validateAll();
    QTreeView* outlineTree(const QString& editorId) const;
    void updateOutline(CodePage* page);
    void followCursorInOutline(CodePage* page);
    void refreshOutlines();
    ProjectFile* currentFile() const;
    ProjectFile* newFile(const QString& editorId);
    bool openFile(const QString& editorId, const QString& path, QString* error = nullptr);
    QList<ProjectFile*> editorFiles(const QString& editorId) const;
    QList<CodeEditor*> codeEditors() const;
    // Panel controllers, nullptr when the project does not have that editor.
    SpritesEditor* spritesEditor() const { return m_sprites; }
    AnimationsEditor* animationsEditor() const { return m_animations; }
    BackgroundsEditor* backgroundsEditor() const { return m_backgrounds; }
    SoundsEditor* soundsEditor() const { return m_sounds; }
    // The code page of a file tab: the page itself, or the text mode of an animation page.
    static CodePage* codePageOf(QWidget* tab);
    QHash<QString, QPointer<EngineWorkspace>> m_workspaces;
    void refreshBreakpointMarks(CodePage* page);
    void showFrameSprite();
    void reconfigureEditors();
    // A modified file asks Save / Discard / Cancel, and the last file closes the editor. Returns whether the tab
    // closed.
    bool closeFileTab(const QString& editorId, QTabWidget* tabs, int index);

Q_SIGNALS:
    void currentEditorChanged(const QString& editorId);
    void currentFileChanged();
    void modifiedChanged();
    void editorsChanged();
    void saveAsRequested(ProjectFile* file);
    void issuesChanged();
    void debugStateChanged();
    void debugVariablesChanged();

private:
    QSet<QString> m_closedEditors;
    void createEditor(const EditorInfo& info);
    std::function<void()> m_loadStep;
    QWidget* createFilePage(const EditorInfo& info, const QString& filePath);
    void addFileTab(const EditorInfo& info, const QString& filePath);
    void fileRenamed(QWidget* page, const QString& oldPath, const QString& newPath);
    void configureCodeEditor(CodeEditor* editor, const QString& editorId);
    void applyWorkspaceColors(QWidget* page);
    void refreshDebugging();
    void setDebugging(bool debugging);
    void showDebugLine(const QString& file, int line);
    struct Analysis
    {
        IssueList issues;
        QList<OutlineNode> outline;
        VariableUsage variables;
    };
    bool showsVariables(CodePage* page) const;
    void analyze(CodePage* page);
    Analysis computeAnalysis(const QString& editorId, const QString& text) const;
    void applyAnalysis(CodePage* page, Analysis analysis);
    void updateTabTitles();
    // The file name, nothing for never saved documents of a new project, and "*" when modified.
    QString tabCaption(const QString& path) const;
    QString tabToolTip(const QString& path) const;
    void fillProjectExplorer();
    void updateRightPanel();
    void refreshPanelIcons();
    CodeFile* commandsFile() const;
    void createCommandsOrganizer();
    void updateDockStretch();

    Project* m_project;
    AppContext& m_context;
    QPointer<Debug::BreakpointList> m_breakpoints;
    QPointer<EngineDebugSession> m_engineSession;
    QString m_currentEditor;
    bool m_rightPanelVisible = true;
    bool m_explorerForSprites = false;
    bool m_organizerVisible = false;
    bool m_debugging = false;

    EPanelWidget* m_left;
    QStackedWidget* m_leftStack;
    QTabBar* m_leftTabs;
    QTreeView* m_explorer;
    QStackedWidget* m_center;
    QVBoxLayout* m_workspaceLayout;
    EPanelWidget* m_right;
    QStackedWidget* m_rightStack;
    QPointer<BottomDock> m_dock;

    QHash<QString, QScrollArea*> m_editorPanels;
    QHash<QString, QTabWidget*> m_fileTabs;
    QHash<QString, QScrollArea*> m_extraPanels;
    QHash<QString, IssueList> m_issues;
    QHash<QString, QList<OutlineNode>> m_outlines;
    QHash<QString, VariableUsage> m_variables;
    QList<QPointer<QFutureWatcher<Analysis>>> m_analyses;
    QPointer<QMenu> m_codeMenu;
    SpritesEditor* m_sprites = nullptr;
    AnimationsEditor* m_animations = nullptr;
    BackgroundsEditor* m_backgrounds = nullptr;
    SoundsEditor* m_sounds = nullptr;
    OrganizerBinding* m_commandsOrganizer = nullptr;
};

} // namespace OFS
