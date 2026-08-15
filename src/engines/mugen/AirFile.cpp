#include "engines/mugen/AirFile.h"

#include "core/code/CodeLine.h"
#include "engines/mugen/MugenOutline.h"

#include <QStringList>

#include <limits>

namespace OFS::Mugen {

QString AirFrame::text() const
{
    QString line = indent + QStringLiteral("%1,%2, %3,%4, %5").arg(group).arg(index).arg(x).arg(y).arg(time);
    if (!flip.isEmpty() || !extra.isEmpty())
        line += QStringLiteral(", ") + flip;
    for (const QString& field : extra)
        line += QStringLiteral(", ") + field;
    if (!comment.isEmpty())
        line += QLatin1Char(' ') + comment;
    return line;
}

QString AirAction::caption() const
{
    return name.isEmpty() ? QString::number(number) : QString::number(number) + QStringLiteral(" - ") + name;
}

bool AirAction::flipsHorizontally(int element) const
{
    return element >= 0 && element < frames.size() &&
           frames.at(element).flip.contains(QLatin1Char('H'), Qt::CaseInsensitive);
}

bool AirAction::flipsVertically(int element) const
{
    return element >= 0 && element < frames.size() &&
           frames.at(element).flip.contains(QLatin1Char('V'), Qt::CaseInsensitive);
}

QList<AirAction> readAnimations(const QString& text)
{
    QList<AirAction> actions;
    bool inAction = false;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int n = 0; n < lines.size(); ++n) {
        QString source = lines.at(n);
        if (source.endsWith(QLatin1Char('\r')))
            source.chop(1);
        const CodeLine line = CodeLine::parse(source);
        if (line.kind == CodeLine::Kind::Section) {
            inAction = false;
            const QStringList words = line.header.simplified().split(QLatin1Char(' '));
            if (words.size() != 3 || words[0].compare(QLatin1String("Begin"), Qt::CaseInsensitive) != 0 ||
                words[1].compare(QLatin1String("Action"), Qt::CaseInsensitive) != 0)
                continue;
            bool ok = false;
            AirAction action;
            action.number = words[2].toInt(&ok);
            if (!ok)
                continue;
            if (n > 0) {
                QString above = lines.at(n - 1);
                if (above.endsWith(QLatin1Char('\r')))
                    above.chop(1);
                action.name = commentName(above);
            }
            action.line = n;
            actions << action;
            inAction = true;
            continue;
        }
        if (!inAction || line.kind != CodeLine::Kind::Raw)
            continue;
        const QStringList fields = line.value.split(QLatin1Char(','));
        if (fields.size() < 5)
            continue;
        AirFrame frame;
        bool ok[5] = {};
        frame.group = fields[0].trimmed().toInt(&ok[0]);
        frame.index = fields[1].trimmed().toInt(&ok[1]);
        frame.x = fields[2].trimmed().toInt(&ok[2]);
        frame.y = fields[3].trimmed().toInt(&ok[3]);
        frame.time = fields[4].trimmed().toInt(&ok[4]);
        if (!(ok[0] && ok[1] && ok[2] && ok[3] && ok[4]))
            continue;
        if (fields.size() > 5)
            frame.flip = fields[5].trimmed();
        for (qsizetype f = 6; f < fields.size(); ++f)
            frame.extra << fields[f].trimmed();
        for (QChar c : source) {
            if (c != QLatin1Char(' ') && c != QLatin1Char('\t'))
                break;
            frame.indent += c;
        }
        if (line.commentStart >= 0)
            frame.comment = source.mid(line.commentStart).trimmed();
        frame.line = n;
        actions.last().frames << frame;
    }
    return actions;
}

qint64 elementStart(const AirAction& action, int element)
{
    qint64 tick = 0;
    for (int i = 0; i < element && i < action.frames.size(); ++i) {
        if (action.frames.at(i).time < 0)
            return Never;
        tick += action.frames.at(i).time;
    }
    return tick;
}

int elementAt(const AirAction& action, qint64 tick)
{
    if (action.frames.isEmpty())
        return -1;
    if (tick == Never)
        tick = std::numeric_limits<qint64>::max();
    int element = 0;
    qint64 start = 0;
    for (int i = 0; i < action.frames.size(); ++i) {
        if (start > tick)
            break;
        element = i;
        if (action.frames.at(i).time < 0)
            break; // an endless element, so the rest never start
        start += action.frames.at(i).time;
    }
    return element;
}

const AirAction* findAnimation(const QList<AirAction>& actions, int number)
{
    for (const AirAction& action : actions) {
        if (action.number == number)
            return &action;
    }
    return nullptr;
}

} // namespace OFS::Mugen
