#pragma once

#include "core/CoreExport.h"
#include "core/code/CodeHighlighter.h"
#include "core/theme/ColorSet.h"

#include <QHash>
#include <QIcon>
#include <QList>
#include <QPlainTextEdit>
#include <QPointer>

#include <memory>
#include <utility>

class QCompleter;
class QMenu;
class QStringListModel;

namespace OFS {

class Config;

class CompletionProvider;
class SyntaxDatabase;

class OFS_CORE_EXPORT CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit CodeEditor(QWidget* parent = nullptr);
    ~CodeEditor() override;

    struct Options
    {
        bool lineNumbers = true;
        bool foldingMarkers = true;
        bool highlightLine = true;
        bool parenMatching = true;
        bool highlightBlocks = false;
        bool breakpoints = false;
    };
    void setOptions(const Options& options);
    const Options& options() const { return m_options; }

    void setCodeDocument(QTextDocument* document);

    void setSyntax(const SyntaxDatabase* syntax, const QString& fileType);
    CodeHighlighter* highlighter() const { return m_highlighter.data(); }
    void setColorSet(const ColorSet& colors);

    struct Marker
    {
        int line = 0;
        int column = 0;
        int length = 0;
        bool error = true;
        QString message; // the tooltip over the underlined text
    };
    void setMarkers(const QList<Marker>& markers);
    // The message of the marker under a viewport point, and the area where it applies.
    QString markerMessageAt(const QPoint& pos, QRect* area = nullptr) const;

    enum class BreakpointMark
    {
        Pending, // not bound to a running session
        Active,
        Disabled,
        Ignored, // its ignore count is not used up
    };
    void setBreakpointMarks(const QHash<int, BreakpointMark>& marks);
    const QHash<int, BreakpointMark>& breakpointMarks() const { return m_breakpointMarks; }
    void setCurrentDebugLine(int line);
    int currentDebugLine() const { return m_debugLine; }
    void setBreakpointIcons(const QHash<BreakpointMark, QIcon>& icons, const QIcon& currentLine);
    int breakpointStripWidth() const;

    // Both 1-based.
    int cursorLine() const;
    int cursorColumn() const;
    void goToLine(int line); // 1-based
    void goToLineAtTop(int line);

    void setCompletionProvider(std::unique_ptr<CompletionProvider> provider);
    QCompleter* completer() const { return m_completer; }
    bool isCompleting() const;

    // Ctrl+click target as ("sctrl" or "trigger", name). Empty kind when it is neither.
    std::pair<QString, QString> documentationTarget(int position) const;

Q_SIGNALS:
    void documentationRequested(const QString& kind, const QString& name, const QPoint& globalPos);
    void breakpointToggled(int line);

public:
    void revealPosition(int position);

    static QFont configuredFont(const Config* config);
    static QString defaultFontFamily() { return QStringLiteral("Courier New"); }
    static constexpr int DefaultFontSize = 10;

    void setCodeMenu(QMenu* menu);
    // The caller owns the menu and must call releaseContextMenu() before deleting it, because the
    // Code submenu is shared.
    QMenu* createContextMenu(const QPoint& position);
    void releaseContextMenu(QMenu* menu);
    void toggleComment();
    void setCommentPrefix(const QString& prefix) { m_commentPrefix = prefix; }
    void autoIndentSelection();
    void autoFormatSelection();
    void foldAll();
    void unfoldAll();
    void toggleFold(int blockNumber);
    bool isFolded(int blockNumber) const;

    void zoomIn(int steps = 1);
    void zoomOut(int steps = 1);
    void resetZoom();

    int gutterWidth() const;

    // The gutter shows a red bar beside changed lines. Undoing the change removes it.
    bool isLineChanged(int line) const;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    bool viewportEvent(QEvent* event) override;

private:
    friend class CodeGutter;
    void paintGutter(QPaintEvent* event);
    void gutterClicked(const QPoint& pos);
    void updateGutterWidth();
    void updateGutter(const QRect& rect, int dy);
    void updateExtraSelections();
    void watchDocument();
    void markTextUnchanged();
    void setFolded(QTextBlock header, bool folded);
    void formatSelection(bool format);
    void updateCompletion(bool typed);
    void insertCompletion(const QString& word);

    QWidget* m_gutter;
    QMetaObject::Connection m_documentChanges;
    QString m_commentPrefix = QStringLiteral(";");
    QPointer<CodeHighlighter> m_highlighter;
    const SyntaxDatabase* m_syntax = nullptr;
    QString m_fileType;
    QPointer<QMenu> m_codeMenu;
    Options m_options;
    ColorSet m_colors;
    QHash<int, BreakpointMark> m_breakpointMarks;
    QHash<BreakpointMark, QIcon> m_breakpointIcons;
    QIcon m_currentLineIcon;
    int m_debugLine = -1;
    QList<Marker> m_markers;
    int m_baseFontSize = 10;
    int m_zoom = 0;
    std::unique_ptr<CompletionProvider> m_completion;
    QCompleter* m_completer = nullptr;
    QStringListModel* m_completionList = nullptr;
    int m_completionStart = -1;
};

} // namespace OFS
