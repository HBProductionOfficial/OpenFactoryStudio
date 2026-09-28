#include "shell/CodePage.h"

#include "core/actions/ActionRegistry.h"
#include "core/code/CodeEditor.h"
#include "core/code/Snippet.h"
#include "core/theme/IconSet.h"

#include <QAbstractItemView>
#include <QAction>
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QTextBlock>
#include <QToolButton>
#include <QVBoxLayout>

namespace OFS {

CodePage::CodePage(ActionRegistry& actions, QWidget* parent)
    : QWidget(parent)
    , m_toolBar(new QWidget(this))
    , m_snippets(new QComboBox(m_toolBar))
    , m_navigator(new QComboBox(m_toolBar))
    , m_editor(new CodeEditor(this))
{
    setObjectName(QStringLiteral("codePage"));
    m_toolBar->setObjectName(QStringLiteral("codeToolBar"));
    m_snippets->setObjectName(QStringLiteral("codeSnippets"));
    m_snippets->setMinimumWidth(200);
    m_snippets->setMaxVisibleItems(10);
    m_snippets->installEventFilter(this);
    m_snippets->view()->installEventFilter(this);
    m_snippets->view()->viewport()->installEventFilter(this);
    m_snippets->view()->parentWidget()->installEventFilter(this);
    connect(m_snippets, &QComboBox::activated, this, [this](int index) {
        Q_EMIT snippetChosen(m_snippets->itemText(index));
        if (m_snippetByKeyboard)
            Q_EMIT snippetInsertRequested();
    });
    m_navigator->setObjectName(QStringLiteral("codeIssues"));

    auto* bar = new QHBoxLayout(m_toolBar);
    bar->setContentsMargins(2, 2, 2, 2);
    bar->setSpacing(2);
    bar->addWidget(m_snippets);
    for (const char* id : {"code_snippet_insert", "code_snippet_new", "code_snippet_edit"}) {
        auto* button = new QToolButton(m_toolBar);
        button->setObjectName(QString::fromLatin1(id));
        button->setAutoRaise(true);
        if (QAction* a = actions.action(QString::fromLatin1(id)))
            button->setDefaultAction(a);
        bar->addWidget(button);
    }
    bar->addWidget(m_navigator, 1);

    auto* column = new QVBoxLayout;
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addWidget(m_toolBar);
    column->addWidget(m_editor, 1);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(column, 1);

    connect(m_navigator, &QComboBox::activated, this, [this](int index) {
        if (index >= 0 && index < m_issues.size())
            m_editor->goToLine(m_issues[index].line + 1);
        m_editor->setFocus();
    });
}

void CodePage::setToolBarVisible(bool visible)
{
    m_toolBar->setVisible(visible);
}

bool CodePage::isToolBarVisible() const
{
    return m_toolBar->isVisible();
}

void CodePage::setIssues(const IssueList& issues, const IconSet& icons)
{
    m_issues = issues;
    std::sort(m_issues.begin(), m_issues.end(),
              [](const Issue& a, const Issue& b) { return a.line != b.line ? a.line < b.line : a.column < b.column; });
    m_navigator->clear();
    for (const Issue& issue : std::as_const(m_issues)) {
        QString icon;
        switch (issue.severity) {
        case Issue::Severity::Error:
            icon = QStringLiteral("error");
            break;
        case Issue::Severity::Warning:
        case Issue::Severity::Deprecation:
            icon = QStringLiteral("warning");
            break;
        case Issue::Severity::Information:
            icon = QStringLiteral("information");
            break;
        }
        m_navigator->addItem(icons.icon(icon), issue.message);
    }

    QList<CodeEditor::Marker> markers;
    for (const Issue& issue : std::as_const(m_issues)) {
        CodeEditor::Marker m;
        m.line = issue.line;
        m.column = issue.column;
        m.length = issue.length;
        m.error = issue.severity == Issue::Severity::Error;
        markers << m;
    }
    m_editor->setMarkers(markers);
}

bool CodePage::eventFilter(QObject* watched, QEvent* event)
{
    // The popup's own filter picks the item on Enter, so ours must see the key first. A filter installed again runs
    // first.
    if (event->type() == QEvent::Show && watched == m_snippets->view()->parentWidget()) {
        m_snippets->view()->installEventFilter(this);
        m_snippets->view()->viewport()->installEventFilter(this);
    }
    // Track whether the next combo choice comes from the keyboard. The popup picks on Enter's ShortcutOverride, before
    // the key press itself.
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride)
        m_snippetByKeyboard = true;
    else if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease)
        m_snippetByKeyboard = false;
    return QWidget::eventFilter(watched, event);
}

void CodePage::setSnippets(const QStringList& names)
{
    const QString current = m_snippets->currentText();
    m_snippets->clear();
    m_snippets->addItems(names);
    const int index = m_snippets->findText(current);
    if (index >= 0)
        m_snippets->setCurrentIndex(index);
}

QString CodePage::currentSnippet() const
{
    return m_snippets->currentText();
}

void CodePage::setCurrentSnippet(const QString& name)
{
    const int index = m_snippets->findText(name, Qt::MatchFixedString);
    if (index >= 0)
        m_snippets->setCurrentIndex(index);
}

void CodePage::setSidePanel(QWidget* panel)
{
    delete m_sidePanel;
    m_sidePanel = panel;
    if (panel) {
        panel->setParent(this);
        static_cast<QHBoxLayout*>(layout())->addWidget(panel);
    }
}

void CodePage::insertText(const QString& text)
{
    if (m_editor->isReadOnly())
        return;
    QTextCursor cursor = m_editor->textCursor();
    cursor.insertText(text);
    m_editor->setTextCursor(cursor);
    m_editor->setFocus();
}

void CodePage::insertSnippet(const Snippet& snippet, const QHash<QString, QString>& values)
{
    int marker = -1;
    const QString text = snippet.expand(values, &marker);
    QTextCursor cursor = m_editor->textCursor();
    const int start = cursor.selectionStart();
    cursor.insertText(text);
    if (marker >= 0)
        cursor.setPosition(start + marker);
    m_editor->setTextCursor(cursor);
    m_editor->setFocus();
}

bool CodePage::goToIssue(bool forward, bool deprecations)
{
    const int line = m_editor->cursorLine() - 1;
    const Issue* target = nullptr;
    for (const Issue& issue : std::as_const(m_issues)) {
        const bool wanted =
            deprecations ? issue.severity == Issue::Severity::Deprecation : issue.severity == Issue::Severity::Error;
        if (!wanted)
            continue;
        if (forward && issue.line > line && (!target || issue.line < target->line))
            target = &issue;
        if (!forward && issue.line < line && (!target || issue.line > target->line))
            target = &issue;
    }
    if (!target)
        return false;
    m_editor->goToLine(target->line + 1);
    return true;
}

} // namespace OFS
