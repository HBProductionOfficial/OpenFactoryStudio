#include "core/bg/BgEdits.h"

namespace OFS::BgEdits {

namespace {

BgSection sectionAt(const QStringList& lines, int headerLine)
{
    const BgDocument doc = BgDocument::parse(lines);
    for (const BgSection& section : doc.sections()) {
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

} // namespace

QString number(double value)
{
    QString text = QString::number(value, 'f', 6);
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
        qsizetype valueStart = equals + 1;
        while (valueStart < text.size() && text.at(valueStart).isSpace())
            ++valueStart;
        const qsizetype comment = text.indexOf(QLatin1Char(';'), equals);
        QString tail;
        if (comment >= 0) {
            qsizetype valueEnd = comment;
            while (valueEnd > valueStart && text.at(valueEnd - 1).isSpace())
                --valueEnd;
            tail = text.mid(valueEnd);
        }
        QString prefix = text.left(valueStart);
        if ((valueStart >= text.size() || text.at(valueStart) == QLatin1Char(';')) && !value.isEmpty() &&
            !prefix.endsWith(QLatin1Char(' ')))
            prefix += QLatin1Char(' ');
        lines[it->line] = prefix + value + tail + (cr ? QStringLiteral("\r") : QString());
        return;
    }
    const bool cr = !lines.isEmpty() && lines.first().endsWith(QLatin1Char('\r'));
    lines.insert(section.lastLine + 1, key + QStringLiteral(" = ") + value + (cr ? QStringLiteral("\r") : QString()));
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

QStringList sectionText(const QStringList& lines, int headerLine)
{
    const BgSection section = sectionAt(lines, headerLine);
    if (section.headerLine < 0)
        return {};
    return lines.mid(section.headerLine, section.lastLine - section.headerLine + 1);
}

int insertSection(QStringList& lines, int afterHeaderLine, const QStringList& text)
{
    int at = int(lines.size());
    if (afterHeaderLine >= 0) {
        const BgSection section = sectionAt(lines, afterHeaderLine);
        if (section.headerLine >= 0)
            at = section.lastLine + 1;
    }
    QStringList block;
    const bool blankBefore = at > 0 && !isBlank(lines.at(at - 1));
    if (blankBefore)
        block << QString();
    block << text;
    if (at < lines.size() && !isBlank(lines.at(at)))
        block << QString();
    for (int i = 0; i < block.size(); ++i)
        lines.insert(at + i, block.at(i));
    return at + (blankBefore ? 1 : 0);
}

void removeSection(QStringList& lines, int headerLine)
{
    const BgSection section = sectionAt(lines, headerLine);
    if (section.headerLine < 0)
        return;
    int end = section.lastLine + 1;
    while (end < lines.size() && isBlank(lines.at(end)))
        ++end;
    lines.erase(lines.begin() + section.headerLine, lines.begin() + end);
}

} // namespace OFS::BgEdits
