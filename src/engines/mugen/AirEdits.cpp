#include "engines/mugen/AirEdits.h"

#include "core/anims/AirDocument.h"
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
        // The last line has no line end after it, so take the one before it.
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

void setFrame(QTextDocument* document, const AirFrame& frame, bool joinPrevious)
{
    QTextCursor cursor(document);
    if (joinPrevious)
        cursor.joinPreviousEditBlock();
    else
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

int scaleContactBoxes(QTextDocument* document, const QList<QPair<int, int>>& sprites, double widthScale,
                      double heightScale)
{
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    int changed = 0;
    bool inDefault = false;
    QList<int> pending;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        const CodeLine line = CodeLine::parse(block.text());
        if (line.kind == CodeLine::Kind::Section) {
            pending.clear();
            inDefault = false;
            continue;
        }
        if (line.kind == CodeLine::Kind::KeyValue && line.key.startsWith(QLatin1String("clsn"), Qt::CaseInsensitive)) {
            if (line.index.isEmpty())
                inDefault = line.key.endsWith(QLatin1String("default"), Qt::CaseInsensitive);
            else if (!inDefault)
                pending << block.blockNumber();
            continue;
        }
        if (line.kind != CodeLine::Kind::Raw)
            continue;
        const QStringList fields = line.value.split(QLatin1Char(','));
        bool okGroup = false;
        bool okIndex = false;
        const int group = fields.value(0).trimmed().toInt(&okGroup);
        const int index = fields.value(1).trimmed().toInt(&okIndex);
        if (fields.size() >= 5 && okGroup && okIndex && sprites.contains(qMakePair(group, index))) {
            for (int number : std::as_const(pending)) {
                const QTextBlock boxBlock = document->findBlockByNumber(number);
                const CodeLine box = CodeLine::parse(boxBlock.text());
                const QStringList values = box.value.split(QLatin1Char(','));
                if (values.size() < 4)
                    continue;
                const int x1 = values[0].trimmed().toInt();
                const int y1 = values[1].trimmed().toInt();
                const int x2 = values[2].trimmed().toInt();
                const int y2 = values[3].trimmed().toInt();
                const int left = qRound(std::min(x1, x2) * widthScale);
                const int top = qRound(std::min(y1, y2) * heightScale);
                const int width = qRound((std::abs(x2 - x1) + 1) * widthScale);
                const int height = qRound((std::abs(y2 - y1) + 1) * heightScale);
                const QString text = QStringLiteral("  %1[%2] = %3, %4, %5, %6")
                                         .arg(box.key, box.index)
                                         .arg(left)
                                         .arg(top)
                                         .arg(left + width - 1)
                                         .arg(top + height - 1);
                replaceLine(cursor, number, text);
                ++changed;
            }
        }
        pending.clear();
    }
    cursor.endEditBlock();
    return changed;
}

void appendAnimation(QTextDocument* document, int number, const QString& name, int time)
{
    const QString text = document->toPlainText();
    QString added;
    if (!text.isEmpty() && !text.endsWith(QLatin1Char('\n')))
        added += QLatin1Char('\n');
    if (!name.trimmed().isEmpty())
        added += QStringLiteral("; ") + name.trimmed() + QLatin1Char('\n');
    added += AirAnimation::header(number) + QLatin1Char('\n');
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
