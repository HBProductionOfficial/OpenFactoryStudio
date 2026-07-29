#pragma once

#include "core/syntax/Issue.h"

#include <QHash>
#include <QWidget>

class QComboBox;
class QToolButton;

namespace OFS {

class ActionRegistry;
class CodeEditor;
class IconSet;
class Snippet;

// Code file tab: a toolbar with the snippets and the issue navigator above the code editor.
class CodePage : public QWidget
{
    Q_OBJECT

public:
    CodePage(ActionRegistry& actions, QWidget* parent = nullptr);

    CodeEditor* editor() const { return m_editor; }

    void setToolBarVisible(bool visible);
    bool isToolBarVisible() const;

    void setIssues(const IssueList& issues, const IconSet& icons);
    const IssueList& issues() const { return m_issues; }

    void setSnippets(const QStringList& names);
    QString currentSnippet() const;
    void setCurrentSnippet(const QString& name);
    void insertSnippet(const Snippet& snippet, const QHash<QString, QString>& values);

    // The page takes ownership.
    void setSidePanel(QWidget* panel);
    QWidget* sidePanel() const { return m_sidePanel; }
    void insertText(const QString& text);

    bool goToIssue(bool forward, bool deprecations);

private:
    QWidget* m_toolBar;
    QComboBox* m_snippets;
    QComboBox* m_navigator;
    CodeEditor* m_editor;
    QWidget* m_sidePanel = nullptr;
    IssueList m_issues;
};

} // namespace OFS
