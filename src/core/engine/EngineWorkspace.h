#pragma once

#include "core/CoreExport.h"
#include "core/debug/DebugTypes.h"

#include <QStringList>
#include <QWidget>

#include <optional>

namespace OFS {

namespace Debug {
class BreakpointList;
}

class OFS_CORE_EXPORT EngineWorkspace : public QWidget
{
    Q_OBJECT

public:
    using QWidget::QWidget;

    virtual void bindPanel(QWidget* panel) = 0;
    virtual void applySettings() {}
    virtual QStringList statusSections() const { return {}; }
    virtual bool perform(const QString& actionId)
    {
        Q_UNUSED(actionId)
        return false;
    }

    virtual Debug::SessionState debugState() const { return Debug::SessionState::None; }
    virtual void setBreakpoints(Debug::BreakpointList* breakpoints) { Q_UNUSED(breakpoints) }
    virtual QList<Debug::Variable> debugVariables() const { return {}; }
    virtual std::optional<Debug::Variable> debugVariable(const QString& path) const
    {
        Q_UNUSED(path)
        return std::nullopt;
    }
    virtual QList<Debug::StackFrame> debugStack() const { return {}; }

Q_SIGNALS:
    void statusChanged();
    void debugStateChanged();
    // line is 1-based. An empty file means none.
    void debugLineChanged(const QString& file, int line);
    void debugVariablesChanged();
};

} // namespace OFS
