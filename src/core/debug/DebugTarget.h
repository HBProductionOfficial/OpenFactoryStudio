#pragma once

#include "core/debug/DebugTypes.h"

#include <QList>
#include <QString>
#include <QtGlobal>

#include <optional>

namespace OFS::Debug {

class BreakpointList;

// What the Debug menu, panel and watch lists work with: the Emulator or a Start on Engine session.
class Target
{
public:
    virtual ~Target() = default;

    virtual SessionState debugState() const { return SessionState::None; }
    virtual void setBreakpoints(BreakpointList* breakpoints) { Q_UNUSED(breakpoints) }
    virtual QList<Variable> debugVariables() const { return {}; }
    virtual std::optional<Variable> debugVariable(const QString& path) const
    {
        Q_UNUSED(path)
        return std::nullopt;
    }
    virtual QList<StackFrame> debugStack() const { return {}; }
    // Handles window actions like Interrupt, Continue and the steps. False for the others.
    virtual bool perform(const QString& actionId)
    {
        Q_UNUSED(actionId)
        return false;
    }
    // A session that cannot see the lines it runs only steps by frame.
    virtual bool stepsByLine() const { return true; }
};

} // namespace OFS::Debug
