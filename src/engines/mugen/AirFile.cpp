#include "engines/mugen/AirFile.h"

#include "core/code/CodeLine.h"
#include "engines/mugen/MugenOutline.h"

#include <QStringList>

namespace OFS::Mugen {

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
        frame.line = n;
        actions.last().frames << frame;
    }
    return actions;
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
