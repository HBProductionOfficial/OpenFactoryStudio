#pragma once

#include "core/script/ScriptHost.h"

#include <QDialog>

class QAction;
class QComboBox;
class QListView;
class QMenu;
class QPlainTextEdit;
class QStringListModel;
class QToolBar;

namespace OFS {

class AppContext;
class CodeEditor;
class LuaHighlighter;

// Tools > Script editor for Lua scripts.
class ScriptEditorDialog : public QDialog
{
    Q_OBJECT

public:
    ScriptEditorDialog(AppContext& context, const Script::Environment& environment, const QString& dataDir,
                       QWidget* parent = nullptr);
    ~ScriptEditorDialog() override;

    CodeEditor* editor() const { return m_editor; }
    QPlainTextEdit* output() const { return m_output; }
    QListView* messageView() const { return m_messageView; }
    QStringListModel* messages() const { return m_messages; }
    QComboBox* engineFilter() const { return m_engines; }
    QComboBox* typeFilter() const { return m_types; }
    QListView* scriptList() const { return m_list; }
    QStringListModel* scriptNames() const { return m_listModel; }
    QToolBar* toolBar() const { return m_toolBar; }
    QAction* runAction() const { return m_run; }
    QMenu* codeMenu() const { return m_codeMenu; }

    QString fileName() const { return m_file; }
    bool isModified() const;
    bool isRunning() const { return m_running; }
    QString selectedFolder() const;

    void newScript();
    bool openFile(const QString& path);
    bool save();
    bool saveAs(const QString& path);
    void run();
    bool saveForTranslation();

    // Esc and closing stop a running script, or ask before losing changes.
    void reject() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void buildCodeMenu();
    void goToMessageLine(bool forward);
    void updateTitle();
    void refreshList();
    void setRunning(bool running);
    void reportError(const QString& error);
    void goToMessage(const QModelIndex& index);
    void openListed(const QModelIndex& index);
    // Unsaved changes: Yes saves, No discards, Cancel keeps them and returns false.
    bool confirmDiscard();
    void storeGeometry();

    AppContext& m_context;
    Script::Environment m_environment;
    QString m_dataDir;
    QString m_file;
    bool m_running = false;
    bool m_stopRequested = false;

    QToolBar* m_toolBar = nullptr;
    QAction* m_new = nullptr;
    QAction* m_open = nullptr;
    QAction* m_save = nullptr;
    QAction* m_saveAs = nullptr;
    QAction* m_run = nullptr;
    QAction* m_translate = nullptr;
    QMenu* m_codeMenu = nullptr;
    CodeEditor* m_editor = nullptr;
    LuaHighlighter* m_highlighter = nullptr;
    QListView* m_messageView = nullptr;
    QStringListModel* m_messages = nullptr;
    QPlainTextEdit* m_output = nullptr;
    QComboBox* m_engines = nullptr;
    QComboBox* m_types = nullptr;
    QListView* m_list = nullptr;
    QStringListModel* m_listModel = nullptr;
};

} // namespace OFS
