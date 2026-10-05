#pragma once

#include "core/CoreExport.h"
#include "core/debug/DebugTarget.h"
#include "core/engine/Engine.h"

#include <QDateTime>
#include <QObject>

#include <functional>

namespace OFS {

struct RunConfiguration;

// Debug > Start on Engine: runs the project in the real engine and watches its process from
// outside. The engine that supplies the session knows where to look in the process.
class OFS_CORE_EXPORT EngineDebugSession : public QObject, public Debug::Target
{
    Q_OBJECT

public:
    using Prepare = std::function<bool(EngineLaunch* launch, QString* error)>;

    explicit EngineDebugSession(QObject* parent = nullptr);
    ~EngineDebugSession() override;

    virtual bool canDebug(const RunConfiguration& run) const = 0;
    virtual bool start(Prepare prepare, QString* error) = 0;
    virtual EngineLaunch launch() const = 0;
    virtual QDateTime startTime() const = 0;
    virtual QString output() const = 0;

Q_SIGNALS:
    void debugStateChanged();
    // line is 1-based. An empty file means none.
    void debugLineChanged(const QString& file, int line);
    void debugVariablesChanged();
    void finished();
    void failed(const QString& message);
};

} // namespace OFS
