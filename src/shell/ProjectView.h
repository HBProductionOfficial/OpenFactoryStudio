#pragma once

#include "core/code/CodeOutline.h"
#include "core/editors/EditorInfo.h"
#include "core/preset/PresetModel.h"
#include "core/syntax/Issue.h"

#include <QFutureWatcher>
#include <QHash>
#include <QPointer>
#include <QWidget>

class EPanelWidget;
class QMenu;
class QScrollArea;
class QStackedWidget;
class QTabBar;
class QTabWidget;
class QTreeView;
class QVBoxLayout;

namespace OFS {

class AppContext;
class BottomDock;
class CodeEditor;
class CodePage;
class Engine;
class Project;
class ProjectFile;

// Everything below the project tab bar for one open project.
class ProjectView : public QWidget
{
    Q_OBJECT

public:
    ProjectView(Project* project, AppContext& context, QWidget* parent = nullptr);
    ~ProjectView() override;

    Project* project() const { return m_project; }
    Engine* engine() const;

    QStringList editorIds() const;
    QString currentEditor() const { return m_currentEditor; }
    EditorInfo currentEditorInfo() const;
    void setCurrentEditor(const QString& id);

    void applyPreset(const Preset::Model& model);

    void setLeftPanelVisible(bool visible);
    void setRightPanelVisible(bool visible);
    void setOrganizerVisible(bool visible);

    void adoptBottomDock(BottomDock* dock);
    void adoptQuickFind(QWidget* bar);

    static QString extraPanelTitle(const QString& name);

    CodeEditor* currentCodeEditor() const;
    CodePage* currentCodePage() const;
    QList<CodePage*> codePages() const;
    // `line` is 1-based, 0 keeps the cursor where it is.
    bool showFileLine(const QString& filePath, int line);
    void setCodeToolBarsVisible(bool visible);
    void setCodeMenu(QMenu* menu);
    void refreshSnippets();
    const QHash<QString, IssueList>& issues() const { return m_issues; }
    void validateAll();
    QTreeView* outlineTree(const QString& editorId) const;
    void updateOutline(CodePage* page);
    void followCursorInOutline(CodePage* page);
    ProjectFile* currentFile() const;
    QList<ProjectFile*> editorFiles(const QString& editorId) const;
    QList<CodeEditor*> codeEditors() const;
    void reconfigureEditors();

Q_SIGNALS:
    void currentEditorChanged(const QString& editorId);
    void currentFileChanged();
    void modifiedChanged();
    void issuesChanged();

private:
    void createEditor(const EditorInfo& info);
    QWidget* createFilePage(const EditorInfo& info, const QString& filePath);
    void configureCodeEditor(CodeEditor* editor, const QString& editorId);
    struct Analysis
    {
        IssueList issues;
        QList<OutlineNode> outline;
    };
    void analyze(CodePage* page);
    Analysis computeAnalysis(const QString& editorId, const QString& text) const;
    void applyAnalysis(CodePage* page, Analysis analysis);
    void updateTabTitles();
    void fillProjectExplorer();
    void updateRightPanel();
    void updateDockStretch();

    Project* m_project;
    AppContext& m_context;
    QString m_currentEditor;
    bool m_rightPanelVisible = true;
    bool m_organizerVisible = false;

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
    QList<QPointer<QFutureWatcher<Analysis>>> m_analyses;
};

} // namespace OFS
