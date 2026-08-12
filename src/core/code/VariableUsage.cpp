#include "core/code/VariableUsage.h"

#include <QCoreApplication>
#include <QStringList>

namespace OFS {

QString VariableUser::caption() const
{
    return name.isEmpty() ? number : number + QStringLiteral(" - ") + name;
}

namespace {

void addSlots(Outline& outline, const QString& title, const QList<QList<VariableUser>>& slotUsers)
{
    OutlineNode group;
    group.text = title;
    for (qsizetype slot = 0; slot < slotUsers.size(); ++slot) {
        OutlineNode row;
        row.text = QString::number(slot);
        for (const VariableUser& user : slotUsers.at(slot)) {
            OutlineNode state;
            state.text = user.caption();
            state.icon = user.icon;
            state.line = user.line;
            row.children << state;
        }
        group.children << row;
    }
    outline << group;
}

QString entry(const VariableUser& user, VariableLogOptions::Format format)
{
    switch (format) {
    case VariableLogOptions::Format::Name:
        return QLatin1Char(' ') + (user.name.isEmpty() ? user.number : user.name);
    case VariableLogOptions::Format::NumberAndName:
        return user.caption();
    case VariableLogOptions::Format::Number:
        break;
    }
    return user.number;
}

void addSection(QString& log, const QString& title, const QString& function,
                const QList<QList<VariableUser>>& slotUsers, const VariableLogOptions& options)
{
    const QString dashes(22, QLatin1Char('-'));
    log += QLatin1Char(';') + dashes + QLatin1Char(' ') + title + QLatin1Char(' ') + dashes + QLatin1Char('\n');
    log += QLatin1Char('\n');
    const int width = int(QStringLiteral("%1(%2)").arg(function).arg(qMax<qsizetype>(slotUsers.size() - 1, 0)).size());
    for (qsizetype slot = 0; slot < slotUsers.size(); ++slot) {
        const QList<VariableUser>& users = slotUsers.at(slot);
        if (users.isEmpty() && options.onlyUsed)
            continue;
        const QString name = QStringLiteral("%1(%2)").arg(function).arg(slot);
        QStringList entries;
        for (const VariableUser& user : users)
            entries << entry(user, options.format);
        const QString head = QLatin1Char(';') + name.rightJustified(width) + QStringLiteral(":= ");
        if (options.onePerLine && entries.size() > 1) {
            const QString indent = QLatin1Char(';') + QString(head.size() - 1, QLatin1Char(' '));
            log += head + entries.join(QLatin1Char('\n') + indent) + QLatin1Char('\n');
        } else {
            log += head + entries.join(QStringLiteral(", ")) + QLatin1Char('\n');
        }
    }
    log += QLatin1Char('\n');
}

} // namespace

Outline variableOutline(const VariableUsage& usage)
{
    Outline outline;
    addSlots(outline, QCoreApplication::translate("OFS::VariableUsage", "Integer"), usage.integers);
    addSlots(outline, QCoreApplication::translate("OFS::VariableUsage", "Float"), usage.floats);
    return outline;
}

QString variableLog(const VariableUsage& usage, const VariableLogOptions& options)
{
    QString log;
    if (options.integers)
        addSection(log, QStringLiteral("Integer variable list"), QStringLiteral("var"), usage.integers, options);
    if (options.floats)
        addSection(log, QStringLiteral("Float variable list"), QStringLiteral("fvar"), usage.floats, options);
    return log;
}

} // namespace OFS
