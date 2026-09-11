#pragma once

#include <QList>
#include <QString>

namespace OFS::Debug {

// Interrupted means stopped by a breakpoint, Interrupt or a step.
enum class SessionState
{
    None,
    Running,
    Interrupted,
};

// A Debug panel value (a player's life, var(3)...) or a group of them, named by its path.
struct Variable
{
    QString path;
    QString name;
    QString value;
    QString type;
    QList<Variable> children;
};

struct StackFrame
{
    QString function;
    QString file; // absolute, with '/'
    int line = 0; // from 1
};

} // namespace OFS::Debug
