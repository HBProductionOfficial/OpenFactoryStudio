#pragma once

#include "core/CoreExport.h"
#include "core/code/CodeHighlighter.h"
#include "core/theme/ColorSet.h"

#include <QList>
#include <QPlainTextEdit>
#include <QPointer>

namespace OFS {

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
    };
    void setMarkers(const QList<Marker>& markers);

    // Both 1-based.
    int cursorLine() const;
    int cursorColumn() const;
    void goToLine(int line); // 1-based

    void toggleComment();
    void foldAll();
    void unfoldAll();
    void toggleFold(int blockNumber);
    bool isFolded(int blockNumber) const;

    void zoomIn(int steps = 1);
    void zoomOut(int steps = 1);
    void resetZoom();

    int gutterWidth() const;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    friend class CodeGutter;
    void paintGutter(QPaintEvent* event);
    void gutterClicked(const QPoint& pos);
    void updateGutterWidth();
    void updateGutter(const QRect& rect, int dy);
    void updateExtraSelections();
    void setFolded(QTextBlock header, bool folded);

    QWidget* m_gutter;
    QPointer<CodeHighlighter> m_highlighter;
    Options m_options;
    ColorSet m_colors;
    QList<Marker> m_markers;
    int m_baseFontSize = 10;
    int m_zoom = 0;
};

} // namespace OFS
