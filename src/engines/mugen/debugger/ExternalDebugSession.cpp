#include "engines/mugen/debugger/ExternalDebugSession.h"

#include "core/GameTime.h"
#include "core/debug/ProcessMemory.h"
#include "core/editors/EditorInfo.h"
#include "core/engine/RunConfiguration.h"
#include "core/project/Project.h"
#include "core/project/ProjectFile.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTextDocument>

namespace OFS::Mugen {

namespace {

struct ExternalDebugSessionTexts
{
    Q_DECLARE_TR_FUNCTIONS(OFS::Mugen::ExternalDebugSessionTexts)
};

// The session looks at the engine twice a frame and refreshes the Debug panel ten times a second.
constexpr int LookIntervalMs = TickIntervalMs / 2;
constexpr int LooksPerRefresh = TicksPerSecond * 2 / 10;

} // namespace

ExternalDebugSession::ExternalDebugSession(Project& project, QList<DebugAddressMap> maps, QObject* parent)
    : EngineDebugSession(parent)
    , m_project(project)
    , m_maps(std::move(maps))
{
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(LookIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &ExternalDebugSession::poll);
}

ExternalDebugSession::~ExternalDebugSession()
{
    m_timer.stop();
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(3000);
        delete m_process;
    }
    m_debugger.reset();
    m_memory.reset();
    m_launch.copy.remove();
}

std::optional<DebugAddressMap> ExternalDebugSession::mapFor(const QString& executable) const
{
    return DebugAddressMap::forExecutable(m_maps, executable);
}

bool ExternalDebugSession::canDebug(const RunConfiguration& run) const
{
    return QFileInfo(run.executable).isFile() && mapFor(run.executable).has_value();
}

bool ExternalDebugSession::start(Prepare prepare, QString* error)
{
    if (m_process)
        return false;
    m_prepare = std::move(prepare);
    return launchEngine(error);
}

bool ExternalDebugSession::launchEngine(QString* error)
{
    EngineLaunch launch;
    if (!m_prepare || !m_prepare(&launch, error))
        return false;
    const std::optional<DebugAddressMap> map = mapFor(launch.program);
    if (!map) {
        launch.copy.remove();
        if (error)
            *error = ExternalDebugSessionTexts::tr("The debugger does not support this engine.");
        return false;
    }
    auto* process = new QProcess(this);
    process->setProgram(launch.program);
    process->setArguments(launch.arguments);
    process->setWorkingDirectory(launch.workingDir);
    process->setProcessChannelMode(QProcess::MergedChannels);
    m_output.clear();
    m_started = QDateTime::currentDateTime().addSecs(-1);
    process->start();
    if (!process->waitForStarted(10000)) {
        if (error)
            *error = process->errorString();
        delete process;
        launch.copy.remove();
        return false;
    }
    m_memory = Debug::ProcessMemory::open(process->processId(), error);
    if (!m_memory) {
        process->kill();
        process->waitForFinished(3000);
        delete process;
        launch.copy.remove();
        return false;
    }
    connect(process, &QProcess::readyRead, this,
            [this, process] { m_output += QString::fromLocal8Bit(process->readAll()); });
    connect(process, &QProcess::finished, this, [this, process] {
        if (process == m_process)
            engineEnded();
    });
    m_launch = launch;
    m_process = process;
    m_debugger = std::make_unique<ExternalDebugger>(*map);
    m_debugger->setMemory(m_memory.get());
    m_debugger->setBreakpoints(m_breakpoints);
    m_debugger->setProjectSlots(playerSlotsOf(launch.arguments, launch.copy.name));
    m_debugger->setResolvers(
        [this](const QString& file, int line) {
            std::optional<StatePlace> place = stateOfLine(statedefsOf(file), line);
            if (place)
                place->file = file;
            return place;
        },
        [this](int state) -> std::optional<StatePlace> {
            for (ProjectFile* file : m_project.files()) {
                if (file->editorId() != EditorId::States)
                    continue;
                for (const StatePlace& place : statedefsOf(file->path())) {
                    if (place.state == state)
                        return StatePlace{state, file->path(), place.line};
                }
            }
            return std::nullopt;
        });
    m_looks = 0;
    m_shownTime.reset();
    m_timer.start();
    setState(Debug::SessionState::Running);
    poll();
    Q_EMIT debugVariablesChanged();
    return true;
}

void ExternalDebugSession::stopEngine()
{
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished(5000);
    }
}

void ExternalDebugSession::engineEnded()
{
    m_timer.stop();
    if (m_process) {
        m_output += QString::fromLocal8Bit(m_process->readAll());
        m_process->deleteLater();
    }
    m_process = nullptr;
    if (m_debugger) {
        m_debugger->setMemory(nullptr);
        m_debugger->clearLocation();
    }
    m_memory.reset();
    // Remove the copy like Run in engine does. Keep the launch for its log.
    m_launch.copy.remove();
    m_launch.copy.files.clear();
    m_launch.copy.folders.clear();
    setState(Debug::SessionState::None);
    Q_EMIT debugLineChanged(QString(), 0);
    Q_EMIT finished();
    if (m_restarting) {
        m_restarting = false;
        QString error;
        if (!launchEngine(&error))
            Q_EMIT failed(error);
    }
}

void ExternalDebugSession::setState(Debug::SessionState state)
{
    if (m_state == state)
        return;
    m_state = state;
    Q_EMIT debugStateChanged();
}

void ExternalDebugSession::interrupted(const std::optional<ExternalDebugger::Location>& where)
{
    setState(Debug::SessionState::Interrupted);
    Q_EMIT debugLineChanged(where ? where->place.file : QString(), where ? where->place.line : 0);
    m_shownTime = m_debugger->gameInt(QStringLiteral("game_time"));
    Q_EMIT debugVariablesChanged();
}

void ExternalDebugSession::poll()
{
    if (!m_debugger || !m_memory || m_state == Debug::SessionState::None)
        return;
    const ExternalDebugger::Look look = m_debugger->look(m_state == Debug::SessionState::Running);
    if (look.hit) {
        interrupted(m_debugger->location());
        return;
    }
    // The engine's own Pause key interrupts the session, and resuming there lets it go on.
    if (look.paused && *look.paused && m_state == Debug::SessionState::Running) {
        interrupted(m_debugger->currentLocation());
        return;
    }
    if (look.paused && !*look.paused && m_state == Debug::SessionState::Interrupted) {
        m_debugger->clearLocation();
        setState(Debug::SessionState::Running);
        Q_EMIT debugLineChanged(QString(), 0);
    }
    if (m_state == Debug::SessionState::Running) {
        if (++m_looks % LooksPerRefresh == 0)
            Q_EMIT debugVariablesChanged();
    } else if (const std::optional<std::int32_t> time = m_debugger->gameInt(QStringLiteral("game_time"));
               time != m_shownTime) {
        m_shownTime = time;
        Q_EMIT debugVariablesChanged();
    }
}

void ExternalDebugSession::setBreakpoints(Debug::BreakpointList* breakpoints)
{
    m_breakpoints = breakpoints;
    if (m_debugger)
        m_debugger->setBreakpoints(breakpoints);
}

QList<Debug::Variable> ExternalDebugSession::debugVariables() const
{
    return m_debugger ? m_debugger->variables() : QList<Debug::Variable>();
}

std::optional<Debug::Variable> ExternalDebugSession::debugVariable(const QString& path) const
{
    return m_debugger ? m_debugger->variable(path) : std::nullopt;
}

QList<Debug::StackFrame> ExternalDebugSession::debugStack() const
{
    return m_debugger ? m_debugger->stack() : QList<Debug::StackFrame>();
}

bool ExternalDebugSession::perform(const QString& actionId)
{
    if (m_state == Debug::SessionState::None || !m_debugger)
        return false;
    const bool running = m_state == Debug::SessionState::Running;
    if (actionId == QLatin1String("debug_stop")) {
        stopEngine();
    } else if (actionId == QLatin1String("debug_restart")) {
        m_restarting = true;
        stopEngine();
    } else if (actionId == QLatin1String("debug_interrupt")) {
        if (running && m_debugger->setPaused(true))
            interrupted(m_debugger->currentLocation());
    } else if (actionId == QLatin1String("debug_continue") || actionId == QLatin1String("debug_start")) {
        if (!running && m_debugger->setPaused(false)) {
            m_debugger->clearLocation();
            setState(Debug::SessionState::Running);
            Q_EMIT debugLineChanged(QString(), 0);
        }
    } else if (actionId == QLatin1String("debug_step_frame")) {
        if (!running) {
            m_debugger->stepFrame();
            m_debugger->look(false);
            m_debugger->clearLocation();
            interrupted(m_debugger->currentLocation());
        }
    } else if (actionId == QLatin1String("debug_step_over") || actionId == QLatin1String("debug_step_into") ||
               actionId == QLatin1String("debug_step_out")) {
        // These need the lines a session runs, which the engine does not show.
    } else {
        return false;
    }
    return true;
}

const QList<StatePlace>& ExternalDebugSession::statedefsOf(const QString& file)
{
    FileStates& cached = m_files[file];
    if (auto* code = qobject_cast<CodeFile*>(m_project.file(file))) {
        const int revision = code->document()->revision();
        if (cached.revision != revision) {
            cached.revision = revision;
            cached.statedefs = statedefs(code->document()->toPlainText());
        }
        return cached.statedefs;
    }
    const QDateTime modified = QFileInfo(file).lastModified();
    if (cached.revision != -2 || cached.modified != modified) {
        cached.revision = -2;
        cached.modified = modified;
        QFile f(file);
        QString text;
        if (f.open(QIODevice::ReadOnly)) {
            QString encoding;
            QByteArray bom;
            text = CodeFile::decode(f.readAll(), &encoding, &bom);
        }
        cached.statedefs = statedefs(text);
    }
    return cached.statedefs;
}

} // namespace OFS::Mugen
