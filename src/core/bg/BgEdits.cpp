#include "core/bg/BgEdits.h"

#include <cmath>

namespace OFS::BgEdits {

namespace {

QList<BgSection> sections(const QStringList& lines)
{
    return BgDocument::parse(lines).sections();
}

BgSection sectionAt(const QStringList& lines, int headerLine)
{
    for (const BgSection& section : sections(lines)) {
        if (section.headerLine == headerLine)
            return section;
    }
    return {};
}

QString stripped(const QString& line)
{
    QString text = line;
    if (text.endsWith(QLatin1Char('\r')))
        text.chop(1);
    return text;
}

bool isBlank(const QString& line)
{
    return stripped(line).trimmed().isEmpty();
}

bool isComment(const QString& line)
{
    return stripped(line).trimmed().startsWith(QLatin1Char(';'));
}

QString lineBreak(const QStringList& lines)
{
    return !lines.isEmpty() && lines.first().endsWith(QLatin1Char('\r')) ? QStringLiteral("\r") : QString();
}

int commentStart(const QStringList& lines, int headerLine, int floor)
{
    int first = headerLine;
    while (first - 1 > floor && isComment(lines.at(first - 1)))
        --first;
    return first;
}

} // namespace

QString number(double value)
{
    QString text = QString::number(value, 'f', 4);
    if (text.contains(QLatin1Char('.'))) {
        while (text.endsWith(QLatin1Char('0')))
            text.chop(1);
        if (text.endsWith(QLatin1Char('.')))
            text.chop(1);
    }
    if (text == QLatin1String("-0"))
        text = QStringLiteral("0");
    return text;
}

QString numbers(const QList<double>& values)
{
    QStringList parts;
    for (double v : values)
        parts << number(v);
    return parts.join(QStringLiteral(", "));
}

void setValue(QStringList& lines, int headerLine, const QString& key, const QString& value)
{
    const BgSection section = sectionAt(lines, headerLine);
    if (section.headerLine < 0)
        return;
    const QString lower = key.toLower();
    for (auto it = section.params.crbegin(); it != section.params.crend(); ++it) {
        if (it->key != lower)
            continue;
        const QString line = lines.at(it->line);
        const bool cr = line.endsWith(QLatin1Char('\r'));
        const QString text = stripped(line);
        const qsizetype equals = text.indexOf(QLatin1Char('='));
        const QString written = text.left(equals).trimmed();
        const qsizetype comment = text.indexOf(QLatin1Char(';'), equals);
        QString tail;
        if (comment >= 0) {
            qsizetype valueEnd = comment;
            while (valueEnd > equals + 1 && text.at(valueEnd - 1).isSpace())
                --valueEnd;
            tail = text.mid(valueEnd);
            if (!tail.front().isSpace())
                tail.prepend(QLatin1Char(' '));
        }
        lines[it->line] = written + QStringLiteral(" = ") + value + tail + (cr ? QStringLiteral("\r") : QString());
        return;
    }
    lines.insert(section.lastLine + 1, key + QStringLiteral(" = ") + value + lineBreak(lines));
}

void setNumbers(QStringList& lines, int headerLine, const QString& key, const QList<double>& values,
                const QList<double>& defaults)
{
    bool atDefault = true;
    for (int i = 0; i < values.size(); ++i) {
        if (std::abs(values[i] - defaults.value(i)) > 1e-9)
            atDefault = false;
    }
    if (atDefault)
        removeKey(lines, headerLine, key);
    else
        setValue(lines, headerLine, key, numbers(values));
}

void removeKey(QStringList& lines, int headerLine, const QString& key)
{
    const BgSection section = sectionAt(lines, headerLine);
    const QString lower = key.toLower();
    for (auto it = section.params.crbegin(); it != section.params.crend(); ++it) {
        if (it->key == lower)
            lines.removeAt(it->line);
    }
}

void setHeader(QStringList& lines, int headerLine, const QString& header)
{
    if (headerLine < 0 || headerLine >= lines.size())
        return;
    const QString line = lines.at(headerLine);
    const bool cr = line.endsWith(QLatin1Char('\r'));
    const QString text = stripped(line);
    const qsizetype close = text.indexOf(QLatin1Char(']'));
    const qsizetype open = text.indexOf(QLatin1Char('['));
    const QString indent = open > 0 ? text.left(open) : QString();
    const QString rest = close >= 0 ? text.mid(close + 1) : QString();
    lines[headerLine] =
        indent + QLatin1Char('[') + header + QLatin1Char(']') + rest + (cr ? QStringLiteral("\r") : QString());
}

std::pair<int, int> block(const QStringList& lines, int headerLine)
{
    const QList<BgSection> all = sections(lines);
    int previousEnd = -1;
    int next = -1;
    for (int i = 0; i < all.size(); ++i) {
        if (all[i].headerLine == headerLine) {
            if (i > 0)
                previousEnd = all[i - 1].lastLine;
            if (i + 1 < all.size())
                next = all[i + 1].headerLine;
            break;
        }
    }
    const int first = commentStart(lines, headerLine, previousEnd);
    int end = int(lines.size());
    if (next >= 0)
        end = commentStart(lines, next, headerLine);
    return {first, end};
}

int insertSection(QStringList& lines, int at, const QStringList& text)
{
    const QString cr = lineBreak(lines);
    QStringList block;
    for (const QString& line : text)
        block << line + cr;
    // At the end of the file, insert before the final empty line with a blank line first.
    int end = int(lines.size());
    if (end > 0 && lines.last().isEmpty())
        --end;
    at = std::clamp(at, 0, int(lines.size()));
    if (at >= end) {
        at = end;
        const bool blankBefore = at > 0 && !isBlank(lines.at(at - 1));
        if (blankBefore)
            block.prepend(cr);
        for (int i = 0; i < block.size(); ++i)
            lines.insert(at + i, block.at(i));
        return at + (blankBefore ? 1 : 0);
    }
    const bool blankBefore = at > 0 && !isBlank(lines.at(at - 1));
    if (blankBefore)
        block.prepend(cr);
    block << cr;
    for (int i = 0; i < block.size(); ++i)
        lines.insert(at + i, block.at(i));
    return at + (blankBefore ? 1 : 0);
}

int duplicate(QStringList& lines, int headerLine)
{
    const auto [first, end] = block(lines, headerLine);
    const QStringList copy = lines.mid(first, end - first);
    for (int i = 0; i < copy.size(); ++i)
        lines.insert(end + i, copy.at(i));
    return end + (headerLine - first);
}

void removeSection(QStringList& lines, int headerLine)
{
    const auto [first, end] = block(lines, headerLine);
    removeLines(lines, first, end);
}

void removeLines(QStringList& lines, int first, int end)
{
    first = std::clamp(first, 0, int(lines.size()));
    end = std::clamp(end, first, int(lines.size()));
    lines.erase(lines.begin() + first, lines.begin() + end);
}

int swapSections(QStringList& lines, int headerLine, int otherHeaderLine)
{
    auto a = block(lines, headerLine);
    auto b = block(lines, otherHeaderLine);
    const bool firstIsA = a.first < b.first;
    const auto upper = firstIsA ? a : b;
    const auto lower = firstIsA ? b : a;
    const QStringList upperText = lines.mid(upper.first, upper.second - upper.first);
    const QStringList lowerText = lines.mid(lower.first, lower.second - lower.first);
    const QStringList between = lines.mid(upper.second, lower.first - upper.second);
    QStringList replaced = lowerText + between + upperText;
    lines.erase(lines.begin() + upper.first, lines.begin() + lower.second);
    for (int i = 0; i < replaced.size(); ++i)
        lines.insert(upper.first + i, replaced.at(i));
    const int offsetInBlock = headerLine - a.first;
    if (firstIsA)
        return upper.first + int(lowerText.size() + between.size()) + offsetInBlock;
    return upper.first + offsetInBlock;
}

} // namespace OFS::BgEdits
