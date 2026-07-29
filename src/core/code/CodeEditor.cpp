#include "core/code/CodeEditor.h"

#include "core/code/CodeBlockData.h"
#include "core/code/CodeCompletion.h"
#include "core/code/CodeHighlighter.h"
#include "core/code/CodeLine.h"
#include "core/syntax/SyntaxDatabase.h"

#include <QAbstractItemView>
#include <QAbstractTextDocumentLayout>
#include <QCompleter>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStringListModel>
#include <QTextBlock>
#include <QWheelEvent>

namespace OFS {

class CodeGutter : public QWidget
{
public:
    explicit CodeGutter(CodeEditor* editor)
        : QWidget(editor)
        , m_editor(editor)
    {
        setObjectName(QStringLiteral("codeGutter"));
    }
    QSize sizeHint() const override { return QSize(m_editor->gutterWidth(), 0); }

protected:
    void paintEvent(QPaintEvent* event) override { m_editor->paintGutter(event); }
    void mousePressEvent(QMouseEvent* event) override { m_editor->gutterClicked(event->pos()); }

private:
    CodeEditor* m_editor;
};

namespace {

constexpr int kFoldMarkerWidth = 14;

bool isOpen(QChar c)
{
    return c == QLatin1Char('(') || c == QLatin1Char('[');
}

bool isClose(QChar c)
{
    return c == QLatin1Char(')') || c == QLatin1Char(']');
}

QChar partner(QChar c)
{
    switch (c.unicode()) {
    case '(':
        return QLatin1Char(')');
    case ')':
        return QLatin1Char('(');
    case '[':
        return QLatin1Char(']');
    case ']':
        return QLatin1Char('[');
    default:
        return {};
    }
}

} // namespace

CodeEditor::CodeEditor(QWidget* parent)
    : QPlainTextEdit(parent)
    , m_gutter(new CodeGutter(this))
{
    setObjectName(QStringLiteral("codeEditor"));
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setWordWrapMode(QTextOption::NoWrap);
    QFont f(QStringLiteral("Courier New"), 10);
    f.setStyleHint(QFont::Monospace);
    setFont(f);
    m_baseFontSize = 10;
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);

    // Sections are recognised (for folding) even without a syntax database.
    setSyntax(nullptr, QString());

    connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateGutterWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateGutter);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::updateExtraSelections);
    updateGutterWidth();
    updateExtraSelections();
}

CodeEditor::~CodeEditor() = default;

void CodeEditor::setOptions(const Options& options)
{
    m_options = options;
    updateGutterWidth();
    updateExtraSelections();
    m_gutter->update();
}

void CodeEditor::setCodeDocument(QTextDocument* doc)
{
    setDocument(doc);
    if (m_highlighter)
        m_highlighter->setDocument(doc);
    updateGutterWidth();
    updateExtraSelections();
}

void CodeEditor::setSyntax(const SyntaxDatabase* syntax, const QString& fileType)
{
    delete m_highlighter.data();
    m_syntax = syntax;
    m_highlighter = new CodeHighlighter(document(), syntax, fileType);
    m_highlighter->setParent(this);
    m_highlighter->setColorSet(m_colors);
}

void CodeEditor::setColorSet(const ColorSet& colors)
{
    m_colors = colors;
    QPalette p = palette();
    if (colors.contains(QStringLiteral("background")))
        p.setColor(QPalette::Base, colors.color(QStringLiteral("background")));
    if (colors.contains(QStringLiteral("foreground")))
        p.setColor(QPalette::Text, colors.color(QStringLiteral("foreground")));
    setPalette(p);
    if (m_highlighter)
        m_highlighter->setColorSet(colors);
    updateExtraSelections();
    m_gutter->update();
}

void CodeEditor::setMarkers(const QList<Marker>& markers)
{
    m_markers = markers;
    updateExtraSelections();
}

int CodeEditor::cursorLine() const
{
    return textCursor().blockNumber() + 1;
}

int CodeEditor::cursorColumn() const
{
    return textCursor().positionInBlock() + 1;
}

void CodeEditor::goToLine(int line)
{
    const QTextBlock block = document()->findBlockByNumber(qBound(1, line, blockCount()) - 1);
    revealPosition(block.position());
    QTextCursor c(block);
    setTextCursor(c);
    centerCursor();
}

void CodeEditor::revealPosition(int position)
{
    const QTextBlock block = document()->findBlock(position);
    if (block.isVisible())
        return;
    for (QTextBlock b = block; b.isValid(); b = b.previous()) {
        if (auto* blockData = CodeBlockData::get(b); blockData && blockData->isHeader) {
            setFolded(b, false);
            break;
        }
    }
}

void CodeEditor::toggleComment()
{
    QTextCursor cursor = textCursor();
    const int startBlock = document()->findBlock(cursor.selectionStart()).blockNumber();
    int endBlock = document()->findBlock(cursor.selectionEnd()).blockNumber();
    // A selection ending at the start of a line does not include that line.
    if (cursor.hasSelection() && endBlock > startBlock &&
        document()->findBlock(cursor.selectionEnd()).position() == cursor.selectionEnd())
        --endBlock;

    const QString first = document()->findBlockByNumber(startBlock).text();
    const bool uncomment = first.trimmed().startsWith(QLatin1Char(';'));

    QTextCursor edit(document());
    edit.beginEditBlock();
    for (int n = startBlock; n <= endBlock; ++n) {
        QTextBlock block = document()->findBlockByNumber(n);
        const QString text = block.text();
        QTextCursor c(block);
        if (uncomment) {
            const int pos = int(text.indexOf(QLatin1Char(';')));
            if (pos >= 0 && text.left(pos).trimmed().isEmpty()) {
                c.setPosition(block.position() + pos);
                c.deleteChar();
            }
        } else {
            c.insertText(QStringLiteral(";"));
        }
    }
    edit.endEditBlock();
}

bool CodeEditor::isFolded(int blockNumber) const
{
    const auto* blockData = CodeBlockData::get(document()->findBlockByNumber(blockNumber));
    return blockData && blockData->isHeader && blockData->folded;
}

void CodeEditor::setFolded(QTextBlock header, bool folded)
{
    auto* blockData = CodeBlockData::get(header);
    if (!blockData || !blockData->isHeader)
        return;
    blockData->folded = folded;
    for (QTextBlock b = header.next(); b.isValid(); b = b.next()) {
        const auto* d = CodeBlockData::get(b);
        if (d && d->isHeader)
            break;
        b.setVisible(!folded);
        b.setLineCount(folded ? 0 : qMax(1, b.layout()->lineCount()));
    }
    document()->markContentsDirty(header.position(), document()->characterCount() - header.position());
    viewport()->update();
    m_gutter->update();
    ensureCursorVisible();
}

void CodeEditor::toggleFold(int blockNumber)
{
    const QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (const auto* blockData = CodeBlockData::get(block); blockData && blockData->isHeader)
        setFolded(block, !blockData->folded);
}

void CodeEditor::foldAll()
{
    for (QTextBlock b = document()->begin(); b.isValid(); b = b.next()) {
        if (const auto* blockData = CodeBlockData::get(b); blockData && blockData->isHeader)
            setFolded(b, true);
    }
}

void CodeEditor::unfoldAll()
{
    for (QTextBlock b = document()->begin(); b.isValid(); b = b.next()) {
        if (const auto* blockData = CodeBlockData::get(b); blockData && blockData->isHeader)
            setFolded(b, false);
    }
}

void CodeEditor::zoomIn(int steps)
{
    m_zoom += steps;
    QPlainTextEdit::zoomIn(steps);
    updateGutterWidth();
}

void CodeEditor::zoomOut(int steps)
{
    m_zoom -= steps;
    QPlainTextEdit::zoomOut(steps);
    updateGutterWidth();
}

void CodeEditor::resetZoom()
{
    if (m_zoom > 0)
        QPlainTextEdit::zoomOut(m_zoom);
    else if (m_zoom < 0)
        QPlainTextEdit::zoomIn(-m_zoom);
    m_zoom = 0;
    updateGutterWidth();
}

int CodeEditor::gutterWidth() const
{
    int width = 4;
    if (m_options.lineNumbers) {
        const int digits = qMax(3, int(QString::number(qMax(1, blockCount())).size()));
        width += fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits + 6;
    }
    if (m_options.foldingMarkers)
        width += kFoldMarkerWidth;
    return width;
}

void CodeEditor::updateGutterWidth()
{
    setViewportMargins(gutterWidth(), 0, 0, 0);
    const QRect cr = contentsRect();
    m_gutter->setGeometry(QRect(cr.left(), cr.top(), gutterWidth(), cr.height()));
}

void CodeEditor::updateGutter(const QRect& rect, int dy)
{
    if (dy)
        m_gutter->scroll(0, dy);
    else
        m_gutter->update(0, rect.y(), m_gutter->width(), rect.height());
    if (rect.contains(viewport()->rect()))
        updateGutterWidth();
}

void CodeEditor::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);
    const QRect cr = contentsRect();
    m_gutter->setGeometry(QRect(cr.left(), cr.top(), gutterWidth(), cr.height()));
}

void CodeEditor::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0)
            zoomIn();
        else if (event->angleDelta().y() < 0)
            zoomOut();
        event->accept();
        return;
    }
    QPlainTextEdit::wheelEvent(event);
}

void CodeEditor::setCompletionProvider(std::unique_ptr<CompletionProvider> provider)
{
    m_completion = std::move(provider);
    if (!m_completion && m_completer)
        m_completer->popup()->hide();
}

bool CodeEditor::isCompleting() const
{
    return m_completer && m_completer->popup()->isVisible();
}

void CodeEditor::updateCompletion(bool typed)
{
    if (!m_completion || isReadOnly())
        return;
    const QTextCursor cursor = textCursor();

    if (!isCompleting()) {
        const CompletionRequest request =
            typed ? m_completion->complete(cursor.block(), cursor.positionInBlock()) : CompletionRequest();
        if (!request.isValid())
            return;
        if (!m_completer) {
            m_completionList = new QStringListModel(this);
            m_completer = new QCompleter(m_completionList, this);
            m_completer->setWidget(this);
            m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
            m_completer->setCaseSensitivity(Qt::CaseInsensitive);
            m_completer->setModelSorting(QCompleter::UnsortedModel);
            m_completer->setMaxVisibleItems(7);
            connect(m_completer, qOverload<const QString&>(&QCompleter::activated), this,
                    &CodeEditor::insertCompletion);
        }
        m_completionList->setStringList(request.words);
        m_completionStart = request.start;
    }

    const int length = cursor.position() - m_completionStart;
    if (length < 0 || cursor.block() != document()->findBlock(m_completionStart)) {
        m_completer->popup()->hide();
        return;
    }
    const QString typedText = cursor.block().text().mid(m_completionStart - cursor.block().position(), length);
    const QString filter = QString(typedText).remove(QRegularExpression(QStringLiteral("^\\s+")));
    for (QChar c : filter) {
        if (!c.isLetterOrNumber() && c != QLatin1Char('_') && c != QLatin1Char('.')) {
            m_completer->popup()->hide(); // a space, "(" or an operator ends the word...
            if (typed)
                updateCompletion(true); // ...and may start another list ("const(")
            return;
        }
    }
    m_completer->setCompletionPrefix(filter);
    QRect rect = cursorRect();
    rect.setWidth(m_completer->popup()->sizeHintForColumn(0) +
                  m_completer->popup()->verticalScrollBar()->sizeHint().width() + 8);
    m_completer->complete(rect);
    int current = filter.isEmpty() ? 0 : -1;
    const QStringList words = m_completionList->stringList();
    for (int row = 0; row < words.size() && !filter.isEmpty(); ++row) {
        if (words.at(row).startsWith(filter, Qt::CaseInsensitive)) {
            current = row;
            break;
        }
    }
    QAbstractItemView* popup = m_completer->popup();
    if (current >= 0) {
        popup->setCurrentIndex(popup->model()->index(current, 0));
        popup->scrollTo(popup->currentIndex(), QAbstractItemView::PositionAtTop);
    } else {
        popup->setCurrentIndex(QModelIndex());
        popup->scrollToTop();
    }
}

void CodeEditor::insertCompletion(const QString& word)
{
    QTextCursor cursor = textCursor();
    const int start = m_completionStart;
    m_completionStart = -1;
    if (start < 0 || start > cursor.position() || word.isEmpty())
        return;
    QTextCursor range(document());
    range.setPosition(start);
    range.setPosition(cursor.position(), QTextCursor::KeepAnchor);
    const QString typed = range.selectedText();
    qsizetype keep = 0;
    while (keep < typed.size() && typed.at(keep).isSpace())
        ++keep;
    range.setPosition(start + int(keep));
    range.setPosition(cursor.position(), QTextCursor::KeepAnchor);
    range.insertText(word);
    setTextCursor(range);
}

std::pair<QString, QString> CodeEditor::documentationTarget(int position) const
{
    const QTextBlock block = document()->findBlock(position);
    const QString text = block.text();
    const int at = position - block.position();
    if (!m_syntax || at < 0 || at > text.size())
        return {};
    auto isWordChar = [](QChar c) {
        return c.isLetterOrNumber() || c == QLatin1Char('_');
    };
    int start = at;
    int end = at;
    while (start > 0 && isWordChar(text.at(start - 1)))
        --start;
    while (end < text.size() && isWordChar(text.at(end)))
        ++end;
    if (start == end)
        return {};
    const CodeLine line = CodeLine::parse(text);
    if (line.kind != CodeLine::Kind::KeyValue || start < line.valueStart ||
        (line.commentStart >= 0 && start >= line.commentStart))
        return {};
    const QString word = text.mid(start, end - start);

    // "type = ChangeState" inside a [State ...] section names a controller.
    if (line.key.compare(QLatin1String("type"), Qt::CaseInsensitive) == 0) {
        for (QTextBlock b = block.previous(); b.isValid(); b = b.previous()) {
            const CodeLine header = CodeLine::parse(b.text());
            if (header.kind != CodeLine::Kind::Section)
                continue;
            const QString name = header.header.section(QLatin1Char(' '), 0, 0).section(QLatin1Char(','), 0, 0);
            if (name.compare(QLatin1String("State"), Qt::CaseInsensitive) == 0)
                return {QStringLiteral("sctrl"), word};
            break;
        }
    }
    if (const SyntaxTrigger* trigger = m_syntax->trigger(word)) {
        QString name = trigger->name;
        name.remove(QLatin1Char('*')); // "ProjHit*" covers "ProjHit1000"
        return {QStringLiteral("trigger"), name};
    }
    return {};
}

void CodeEditor::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && (event->modifiers() & Qt::ControlModifier)) {
        const auto [kind, name] = documentationTarget(cursorForPosition(event->position().toPoint()).position());
        if (!kind.isEmpty()) {
            Q_EMIT documentationRequested(kind, name, event->globalPosition().toPoint());
            event->accept();
            return;
        }
    }
    QPlainTextEdit::mouseReleaseEvent(event);
}

void CodeEditor::keyPressEvent(QKeyEvent* event)
{
    if (isCompleting()) {
        switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Escape:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            event->ignore();
            return;
        default:
            break;
        }
    }
    const bool typed = !event->text().isEmpty() && event->text().at(0).isPrint();
    QPlainTextEdit::keyPressEvent(event);
    if (typed || (isCompleting() && event->key() == Qt::Key_Backspace))
        updateCompletion(typed);
    else if (isCompleting() && event->key() != Qt::Key_Shift)
        m_completer->popup()->hide();
}

void CodeEditor::paintGutter(QPaintEvent* event)
{
    QPainter painter(m_gutter);
    const QColor background = m_colors.color(QStringLiteral("background"), palette().color(QPalette::Base));
    const QColor foreground = m_colors.color(QStringLiteral("foreground"), palette().color(QPalette::Text));
    painter.fillRect(event->rect(), background);
    painter.setFont(font());

    QTextBlock block = firstVisibleBlock();
    int number = block.blockNumber();
    qreal top = blockBoundingGeometry(block).translated(contentOffset()).top();
    qreal bottom = top + blockBoundingRect(block).height();
    const int numberWidth = gutterWidth() - (m_options.foldingMarkers ? kFoldMarkerWidth : 0) - 6;
    const int lineHeight = fontMetrics().height();

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            if (m_options.lineNumbers) {
                painter.setPen(number == textCursor().blockNumber() ? foreground : foreground.darker(140));
                painter.drawText(0, int(top), numberWidth, lineHeight, Qt::AlignRight | Qt::AlignVCenter,
                                 QString::number(number + 1));
            }
            if (m_options.foldingMarkers) {
                if (const auto* blockData = CodeBlockData::get(block); blockData && blockData->isHeader) {
                    const int size = qMin(9, lineHeight - 4);
                    const QRect box(gutterWidth() - kFoldMarkerWidth + (kFoldMarkerWidth - size) / 2,
                                    int(top) + (lineHeight - size) / 2, size, size);
                    painter.setPen(foreground);
                    painter.fillRect(box, background);
                    painter.drawRect(box);
                    const int mid = box.center().y();
                    painter.drawLine(box.left() + 2, mid, box.right() - 2, mid);
                    if (blockData->folded)
                        painter.drawLine(box.center().x(), box.top() + 2, box.center().x(), box.bottom() - 2);
                }
            }
        }
        block = block.next();
        top = bottom;
        bottom = top + blockBoundingRect(block).height();
        ++number;
    }
}

void CodeEditor::gutterClicked(const QPoint& pos)
{
    if (!m_options.foldingMarkers || pos.x() < gutterWidth() - kFoldMarkerWidth)
        return;
    QTextBlock block = firstVisibleBlock();
    qreal top = blockBoundingGeometry(block).translated(contentOffset()).top();
    while (block.isValid()) {
        const qreal height = blockBoundingRect(block).height();
        if (block.isVisible() && pos.y() >= top && pos.y() < top + height) {
            toggleFold(block.blockNumber());
            return;
        }
        top += height;
        block = block.next();
    }
}

void CodeEditor::updateExtraSelections()
{
    QList<QTextEdit::ExtraSelection> selections;

    if (m_options.highlightBlocks) {
        QTextBlock first = textCursor().block();
        while (first.isValid()) {
            const auto* blockData = CodeBlockData::get(first);
            if (blockData && blockData->isHeader)
                break;
            first = first.previous();
        }
        if (first.isValid()) {
            QTextBlock last = first;
            for (QTextBlock b = first.next(); b.isValid(); b = b.next()) {
                const auto* blockData = CodeBlockData::get(b);
                if (blockData && blockData->isHeader)
                    break;
                last = b;
            }
            const QColor background = m_colors.color(QStringLiteral("background"), palette().color(QPalette::Base));
            QColor tint = m_colors.color(QStringLiteral("block_range"), palette().color(QPalette::Highlight));
            tint = QColor((background.red() + tint.red()) / 2, (background.green() + tint.green()) / 2,
                          (background.blue() + tint.blue()) / 2);
            QTextEdit::ExtraSelection block;
            block.format.setBackground(tint);
            block.format.setProperty(QTextFormat::FullWidthSelection, true);
            block.cursor = QTextCursor(first);
            block.cursor.setPosition(last.position() + last.length() - 1, QTextCursor::KeepAnchor);
            selections << block;
        }
    }

    if (m_options.highlightLine && !isReadOnly()) {
        QTextEdit::ExtraSelection line;
        const QColor color = m_colors.color(QStringLiteral("line"), palette().color(QPalette::AlternateBase));
        line.format.setBackground(color);
        line.format.setProperty(QTextFormat::FullWidthSelection, true);
        line.cursor = textCursor();
        line.cursor.clearSelection();
        selections << line;
    }

    if (m_options.parenMatching) {
        const QTextCursor cursor = textCursor();
        const QString text = cursor.block().text();
        const int pos = cursor.positionInBlock();
        int at = -1;
        if (pos < text.size() && (isOpen(text.at(pos)) || isClose(text.at(pos))))
            at = pos;
        else if (pos > 0 && (isOpen(text.at(pos - 1)) || isClose(text.at(pos - 1))))
            at = pos - 1;
        if (at >= 0) {
            const QChar c = text.at(at);
            const QChar match = partner(c);
            const int step = isOpen(c) ? 1 : -1;
            int depth = 0;
            int other = -1;
            for (int i = at; i >= 0 && i < text.size(); i += step) {
                if (text.at(i) == c)
                    ++depth;
                else if (text.at(i) == match && --depth == 0) {
                    other = i;
                    break;
                }
            }
            const QColor parenColor = m_colors.color(QStringLiteral("paren"), QColor(Qt::yellow));
            for (int p : {at, other}) {
                if (p < 0)
                    continue;
                QTextEdit::ExtraSelection s;
                s.format.setForeground(parenColor);
                s.format.setFontWeight(QFont::Bold);
                s.cursor = QTextCursor(cursor.block());
                s.cursor.setPosition(cursor.block().position() + p);
                s.cursor.setPosition(cursor.block().position() + p + 1, QTextCursor::KeepAnchor);
                selections << s;
            }
            if (other >= 0 && m_colors.contains(QStringLiteral("paren_range"))) {
                QTextEdit::ExtraSelection range;
                range.format.setBackground(m_colors.color(QStringLiteral("paren_range")));
                range.cursor = QTextCursor(cursor.block());
                range.cursor.setPosition(cursor.block().position() + qMin(at, other) + 1);
                range.cursor.setPosition(cursor.block().position() + qMax(at, other), QTextCursor::KeepAnchor);
                selections << range;
            }
        }
    }

    for (const Marker& m : std::as_const(m_markers)) {
        const QTextBlock block = document()->findBlockByNumber(m.line);
        if (!block.isValid())
            continue;
        QTextEdit::ExtraSelection s;
        s.format.setUnderlineStyle(QTextCharFormat::WaveUnderline);
        s.format.setUnderlineColor(m.error ? QColor(230, 40, 40) : QColor(240, 160, 0));
        s.cursor = QTextCursor(block);
        const int start = qBound(0, m.column, block.length() - 1);
        const int length = m.length > 0 ? m.length : qMax(1, block.length() - 1 - start);
        s.cursor.setPosition(block.position() + start);
        s.cursor.setPosition(block.position() + qMin(start + length, block.length() - 1), QTextCursor::KeepAnchor);
        selections << s;
    }

    setExtraSelections(selections);
}

} // namespace OFS
