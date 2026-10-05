#pragma once

#include "core/engine/EngineDebugSession.h"
#include "engines/mugen/debugger/DebugAddressMap.h"
#include "engines/mugen/debugger/ExternalDebugger.h"

#include <QDateTime>
#include <QHash>
#include <QPointer>
#include <QTimer>

#include <memory>

class QProcess;

namespace OFS {
class Project;
namespace Debug {
class ProcessMemory;
}
} // namespace OFS

namespace OFS::Mugen {

// Debug > Start on Engine for M.U.G.E.N: runs the engine on the project's run copy and watches its
// process through the address map for its version (data/engines/mugen/debug.xml).
class ExternalDebugSession : public EngineDebugSession
{
    Q_OBJECT

public:
    ExternalDebugSession(Project& project, QList<DebugAddressMap> maps, QObject* parent = nullptr);
    ~ExternalDebugSession() override;

    bool canDebug(const RunConfiguration& run) const override;
    bool start(Prepare prepare, QString* error) override;
    EngineLaunch launch() const override { return m_launch; }
    QDateTime startTime() const override { return m_started; }
    QString output() const override { return m_output; }

    Debug::SessionState debugState() const override { return m_state; }
    void setBreakpoints(Debug::BreakpointList* breakpoints) override;
    QList<Debug::Variable> debugVariables() const override;
    std::optional<Debug::Variable> debugVariable(const QString& path) const override;
    QList<Debug::StackFrame> debugStack() const override;
    bool perform(const QString& actionId) override;
    bool stepsByLine() const override { return false; }

    void poll();
    const ExternalDebugger* debugger() const { return m_debugger.get(); }

private:
    std::optional<DebugAddressMap> mapFor(const QString& executable) const;
    bool launchEngine(QString* error);
    void stopEngine();
    void engineEnded();
    void setState(Debug::SessionState state);
    void interrupted(const std::optional<ExternalDebugger::Location>& where);
    const QList<StatePlace>& statedefsOf(const QString& file);

    Project& m_project;
    QList<DebugAddressMap> m_maps;
    Prepare m_prepare;
    EngineLaunch m_launch;
    QDateTime m_started;
    QString m_output;
    QPointer<QProcess> m_process;
    std::unique_ptr<Debug::ProcessMemory> m_memory;
    std::unique_ptr<ExternalDebugger> m_debugger;
    QPointer<Debug::BreakpointList> m_breakpoints;
    Debug::SessionState m_state = Debug::SessionState::None;
    QTimer m_timer;
    int m_looks = 0;
    std::optional<std::int32_t> m_shownTime;
    bool m_restarting = false;
    struct FileStates
    {
        int revision = -1;
        QDateTime modified;
        QList<StatePlace> statedefs;
    };
    QHash<QString, FileStates> m_files;
};

} // namespace OFS::Mugen
