#pragma once

#include "core/engine/Engine.h"
#include "core/preset/PresetModel.h"
#include "core/script/ScriptHost.h"
#include "shell/dialogs/MultiLineFindDialog.h"

#include <QDateTime>
#include <QHash>
#include <QMainWindow>
#include <QPointer>
#include <QProcess>
#include <QSet>

#include <memory>

class QActionGroup;
class QDockWidget;
class QListWidget;
class QMenu;
class QPlainTextEdit;

class QStackedWidget;
class QTabBar;
class QToolBar;

namespace OFS {

class AppContext;
class BottomDock;
class DebuggerPanels;
namespace Debug {
class BreakpointList;
}
class CodeEditor;
class MenuBuilder;
class ProjectFile;
class ProjectView;
class QuickFindBar;
class StatusBar;
class UpdateChecker;
class WelcomePage;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(AppContext& context, QWidget* parent = nullptr);
    ~MainWindow() override;

    // Returns false, after telling the user, when no engine can open the file.
    bool openProject(const QString& filePath);
    bool openRecentProject(const QString& filePath);

    ProjectView* currentProjectView() const;
    QList<ProjectView*> projectViews() const;
    bool isWelcomeCurrent() const;

    void applyInterface();

    const Preset::Model& presetModel() const { return m_preset; }
    BottomDock* bottomDock() const { return m_dock; }
    StatusBar* ofsStatusBar() const { return m_status; }
    QToolBar* mainToolBar() const { return m_mainBar; }
    QToolBar* extraToolBar() const { return m_extraBar; }

public Q_SLOTS:
    void openProjectDialog();
    void newProject();
    void openArchiveDialog();
    bool openArchive(const QString& archivePath);
    void openSingleFileDialog();
    void saveProjectAs();
    void createBackup();
    void loadBackup();
    void backupOnLoad(Project* project);
    // The tab of `view` is closed without asking.
    bool replaceProject(ProjectView* view, const QString& filePath);
    void restartAutosave();
    void autosave();
    void fileChangedOnDisk(ProjectFile* file);
    bool closeProject(int tabIndex, bool ask = true);
    void closeCurrentProject();
    void closeAllProjects();
    void closeOtherProjects();
    void showWelcomeTab();
    void switchEditor(const QString& editorId);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildMenus();
    void buildToolBars();
    void buildCentralArea();
    void connectActions();
    void restoreWindowGeometry();
    void saveWindowGeometry();

    void addProjectView(Project* project, const QString& editorId);
    void onCurrentTabChanged(int index);
    void refreshForCurrent();
    void refreshToolBars();
    void refreshActionStates();
    void refreshTitle();
    void refreshStatusBar();
    void refreshActionList();
    void populateRecentProjects(QMenu* menu, QAction* before, QList<QAction*>& created);
    void populateEncodings(QMenu* menu, QAction* before, QList<QAction*>& created);
    void changeEncoding(const QString& encoding);
    void connectEditActions();
    void refreshIssues();
    void updateProjectTab(ProjectView* view);
    bool confirmClose(ProjectView* view);
    void saveFile(ProjectFile* file, bool asUtf8);
    void saveEditorFile(const QString& editorId, bool asUtf8);
    void newEditorFile(const QString& editorId);
    void openEditorFile(const QString& editorId);
    void saveEditorFileAs(const QString& editorId);
    bool saveFileAs(ProjectFile* file);
    QPlainTextEdit* searchTarget() const;
    void setQuickFindOpen(bool open);
    void quickFind(int operation);
    void showMultiLineFind();
    void multiLineFind(int operation, bool backward);
    void goToLineDialog(CodeEditor* editor);

public:
    void runInEngine();
    bool isEngineRunning() const { return m_engineProcess != nullptr; }

    Script::Environment scriptEnvironment();
    // Number 0 makes a new breakpoint.
    void editBreakpoint(int number);
    void showScriptEditor();
    void runToolScript(const QString& path);

private:
    void finishEngineRun(QProcess* process);
    void runEngineTool(Engine* engine, const QString& actionId);

    AppContext& m_context;
    std::unique_ptr<MenuBuilder> m_menus;
    Preset::Model m_preset;
    QList<QMenu*> m_engineMenus;

    QToolBar* m_mainBar = nullptr;
    QToolBar* m_extraBar = nullptr;
    QTabBar* m_projectTabs = nullptr;
    QStackedWidget* m_stack = nullptr;
    WelcomePage* m_welcome = nullptr;
    BottomDock* m_dock = nullptr;
    Debug::BreakpointList* m_breakpoints = nullptr;
    DebuggerPanels* m_debugPanels = nullptr;
    StatusBar* m_status = nullptr;
    QActionGroup* m_editorGroup = nullptr;
    QDockWidget* m_actionListDock = nullptr;
    QListWidget* m_actionList = nullptr;
    UpdateChecker* m_updates = nullptr;
    QuickFindBar* m_quickFind = nullptr;
    QPointer<QProcess> m_engineProcess;
    QTimer* m_autosave = nullptr;
    QSet<ProjectFile*> m_reloadQuestions;
    RunCopy m_runCopy;
    QString m_runLog;
    QDateTime m_runStarted;
    QString m_runOutput;
    QPointer<MultiLineFindDialog> m_multiFind;
    bool m_welcomeTab = false;
};

} // namespace OFS
