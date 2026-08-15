#include "engines/mugen/AirEdits.h"

#include "core/code/CodeLine.h"

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

namespace OFS::Mugen::AirEdits {

namespace {

void replaceLine(QTextCursor& cursor, int line, const QString& text)
{
    const QTextBlock block = cursor.document()->findBlockByNumber(line);
    if (!block.isValid())
        return;
    cursor.setPosition(block.position());
    cursor.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
    cursor.insertText(text);
}

void removeLines(QTextCursor& cursor, int first, int last)
{
    QTextDocument* document = cursor.document();
    const QTextBlock from = document->findBlockByNumber(first);
    const QTextBlock to = document->findBlockByNumber(last);
    if (!from.isValid() || !to.isValid())
        return;
    if (to.next().isValid()) {
        cursor.setPosition(from.position());
        cursor.setPosition(to.next().position(), QTextCursor::KeepAnchor);
    } else if (from.previous().isValid()) {
        cursor.setPosition(from.previous().position() + from.previous().length() - 1);
        cursor.setPosition(to.position() + to.length() - 1, QTextCursor::KeepAnchor);
    } else {
        cursor.setPosition(from.position());
        cursor.setPosition(to.position() + to.length() - 1, QTextCursor::KeepAnchor);
    }
    cursor.removeSelectedText();
}

bool isBoxLine(const QString& text)
{
    // "Clsn2: 2" and "Clsn2[0] = ..." belong to the next element. "Clsn2Default" applies to all.
    const CodeLine line = CodeLine::parse(text);
    if (line.kind != CodeLine::Kind::KeyValue)
        return false;
    const QString key = line.key.toLower();
    return key.startsWith(QLatin1String("clsn")) && !key.endsWith(QLatin1String("default"));
}

} // namespace

void setFrame(QTextDocument* document, const AirFrame& frame)
{
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    replaceLine(cursor, frame.line, frame.text());
    cursor.endEditBlock();
}

bool splitFrame(QTextDocument* document, const AirAction& action, qint64 tick)
{
    const int element = elementAt(action, tick);
    if (element < 0 || tick == Never)
        return false;
    const qint64 start = elementStart(action, element);
    if (start == Never || tick <= start)
        return false;
    AirFrame split = action.frames.at(element);
    const bool last = element == action.frames.size() - 1;
    const qint64 rest = split.time - (tick - start);
    split.time = int(tick - start);
    AirFrame blank;
    blank.group = -1;
    blank.time = last || rest <= 0 ? -1 : int(rest);
    blank.indent = split.indent;

    QTextCursor cursor(document);
    cursor.beginEditBlock();
    replaceLine(cursor, split.line, split.text());
    const QTextBlock block = document->findBlockByNumber(split.line);
    cursor.setPosition(block.position() + block.length() - 1);
    cursor.insertText(QLatin1Char('\n') + blank.text());
    cursor.endEditBlock();
    return true;
}

bool removeFrame(QTextDocument* document, const AirAction& action, qint64 tick)
{
    const int element = elementAt(action, tick);
    if (element <= 0 || action.frames.size() < 2)
        return false;
    const AirFrame removed = action.frames.at(element);
    AirFrame previous = action.frames.at(element - 1);
    previous.time = removed.time < 0 || previous.time < 0 ? -1 : previous.time + removed.time;

    int first = removed.line;
    for (int line = removed.line - 1; line > previous.line; --line) {
        if (!isBoxLine(document->findBlockByNumber(line).text()))
            break;
        first = line;
    }
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    removeLines(cursor, first, removed.line);
    replaceLine(cursor, previous.line, previous.text());
    cursor.endEditBlock();
    return true;
}

void appendAnimation(QTextDocument* document, int number, const QString& name, int time)
{
    const QString text = document->toPlainText();
    QString added;
    if (!text.isEmpty() && !text.endsWith(QLatin1Char('\n')))
        added += QLatin1Char('\n');
    if (!name.trimmed().isEmpty())
        added += QStringLiteral("; ") + name.trimmed() + QLatin1Char('\n');
    added += QStringLiteral("[Begin Action %1]\n").arg(number);
    AirFrame blank;
    blank.group = -1;
    blank.time = time == 0 ? -1 : time;
    added += blank.text() + QLatin1Char('\n');

    QTextCursor cursor(document);
    cursor.movePosition(QTextCursor::End);
    cursor.beginEditBlock();
    cursor.insertText(added);
    cursor.endEditBlock();
}

} // namespace OFS::Mugen::AirEdits
