#pragma once

#include "core/CoreExport.h"
#include "core/debug/DebugTarget.h"

#include <QStringList>
#include <QWidget>

namespace OFS {

// Workspace an engine draws itself, like the MUGEN Emulator. It also runs its debug sessions.
class OFS_CORE_EXPORT EngineWorkspace : public QWidget, public Debug::Target
{
    Q_OBJECT

public:
    using QWidget::QWidget;

    virtual void bindPanel(QWidget* panel) = 0;
    virtual void applySettings() {}
    virtual QStringList statusSections() const { return {}; }

Q_SIGNALS:
    void statusChanged();
    void debugStateChanged();
    // line is 1-based. An empty file means none.
    void debugLineChanged(const QString& file, int line);
    void debugVariablesChanged();
};

} // namespace OFS
