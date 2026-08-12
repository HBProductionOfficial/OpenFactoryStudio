#include "engines/mugen/MugenVariables.h"

#include "core/code/CodeLine.h"
#include "engines/mugen/MugenOutline.h"

#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace OFS::Mugen {

namespace {

constexpr int IntegerSlots = 60;
constexpr int FloatSlots = 40;

struct Write
{
    bool isFloat = false;
    int slot = 0;
    int state = 0;
};

bool isVariableController(const QString& type)
{
    static const QStringList types{QStringLiteral("varset"), QStringLiteral("varadd"), QStringLiteral("varrandom"),
                                   QStringLiteral("parentvarset"), QStringLiteral("parentvaradd")};
    return types.contains(type.trimmed().toLower());
}

} // namespace

VariableUsage scanVariables(const QString& text)
{
    // "var(3) :=" and "fvar(3) :=" match. "sysvar(3) :=" does not.
    static const QRegularExpression assignment(QStringLiteral("(?<![\\w.$])(f?)var\\s*\\(\\s*(\\d+)\\s*\\)\\s*:="),
                                               QRegularExpression::CaseInsensitiveOption);
    QList<VariableUser> states;
    int current = -1;
    QList<Write> writes;
    QSet<QString> seen;
    auto record = [&](bool isFloat, qint64 slot) {
        if (current < 0 || slot < 0 || slot >= (isFloat ? FloatSlots : IntegerSlots))
            return;
        const QString key = QStringLiteral("%1:%2:%3").arg(current).arg(isFloat).arg(slot);
        if (seen.contains(key))
            return;
        seen.insert(key);
        writes << Write{isFloat, int(slot), current};
    };

    enum class Section
    {
        Other,
        StateDef,
        Controller,
    };
    Section section = Section::Other;
    QString controllerType;
    QList<CodeLine> controllerParams;
    auto finishController = [&] {
        if (section == Section::Controller && isVariableController(controllerType)) {
            for (const CodeLine& param : std::as_const(controllerParams)) {
                const QString key = param.key.toLower();
                bool ok = false;
                if (key == QLatin1String("v") || key == QLatin1String("fv")) {
                    const qint64 slot = param.value.trimmed().toLongLong(&ok);
                    if (ok)
                        record(key == QLatin1String("fv"), slot);
                } else if ((key == QLatin1String("var") || key == QLatin1String("fvar")) && !param.index.isEmpty()) {
                    const qint64 slot = param.index.trimmed().toLongLong(&ok);
                    if (ok)
                        record(key == QLatin1String("fvar"), slot);
                }
            }
        }
        controllerType.clear();
        controllerParams.clear();
    };

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int n = 0; n < lines.size(); ++n) {
        QString source = lines.at(n);
        if (source.endsWith(QLatin1Char('\r')))
            source.chop(1);
        const CodeLine line = CodeLine::parse(source);
        if (line.kind == CodeLine::Kind::Section) {
            finishController();
            const QString word = line.header.section(QLatin1Char(' '), 0, 0).section(QLatin1Char(','), 0, 0);
            if (word.compare(QLatin1String("Statedef"), Qt::CaseInsensitive) == 0) {
                VariableUser state;
                state.number = line.header.mid(word.size()).trimmed();
                if (n > 0) {
                    QString above = lines.at(n - 1);
                    if (above.endsWith(QLatin1Char('\r')))
                        above.chop(1);
                    state.name = commentName(above);
                }
                state.line = n;
                states << state;
                current = int(states.size()) - 1;
                section = Section::StateDef;
            } else if (word.compare(QLatin1String("State"), Qt::CaseInsensitive) == 0 && current >= 0) {
                section = Section::Controller;
            } else {
                section = Section::Other;
                current = -1;
            }
            continue;
        }
        if (section == Section::Other || line.kind != CodeLine::Kind::KeyValue)
            continue;
        if (line.key.compare(QLatin1String("type"), Qt::CaseInsensitive) == 0) {
            if (section == Section::StateDef)
                states[current].icon = stateTypeIcon(line.value);
            else
                controllerType = line.value;
            continue;
        }
        for (auto it = assignment.globalMatch(line.value); it.hasNext();) {
            const QRegularExpressionMatch m = it.next();
            record(!m.captured(1).isEmpty(), m.captured(2).toLongLong());
        }
        if (section == Section::Controller)
            controllerParams << line;
    }
    finishController();

    VariableUsage usage;
    usage.integers.resize(IntegerSlots);
    usage.floats.resize(FloatSlots);
    for (const Write& write : std::as_const(writes)) {
        QList<VariableUser>& users = write.isFloat ? usage.floats[write.slot] : usage.integers[write.slot];
        users << states.at(write.state);
    }
    return usage;
}

} // namespace OFS::Mugen
