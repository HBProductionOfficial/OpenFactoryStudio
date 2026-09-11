#pragma once

#include "core/debug/Breakpoints.h"
#include "core/debug/DebugTypes.h"

#include <QHash>
#include <QPointer>

#include <optional>

namespace OFS::Mugen {

class EmulatorSession;

namespace Runtime {
class Character;
struct Controller;
} // namespace Runtime

// The Emulator's debugger. The workspace asks it before each line and each frame whether to stop.
class EmulatorDebugger
{
public:
    enum class Step
    {
        None,
        Into,
        Over, // the next line of the same character
        Out,  // the next line out of the current state (or of another character)
        Frame,
    };

    struct Location
    {
        int characterId = 0;
        QString characterName;
        int stateNo = 0;
        QString label;
        QString file;
        int line = 0;
    };

    void setBreakpoints(Debug::BreakpointList* breakpoints) { m_breakpoints = breakpoints; }
    void reset();
    void interrupt() { m_interrupt = true; }
    void resume(Step step);
    bool atLine(const EmulatorSession& session, const Runtime::Character& character,
                const Runtime::Controller& controller, int line);
    bool beforeFrame(const EmulatorSession& session);
    void afterFrame() { ++m_frames; }
    const std::optional<Location>& location() const { return m_location; }

    static QList<Debug::Variable> variables(const EmulatorSession& session);
    static std::optional<Debug::Variable> variable(const EmulatorSession& session, const QString& path);
    QList<Debug::StackFrame> stack(const EmulatorSession& session) const;

private:
    bool hit(const Debug::Breakpoint& breakpoint, const Runtime::Character* character);
    bool dataChanged(const EmulatorSession& session);

    QPointer<Debug::BreakpointList> m_breakpoints;
    Step m_step = Step::None;
    std::optional<Location> m_location;
    Location m_from;
    bool m_interrupt = false;
    int m_frames = 0;
    QHash<int, int> m_hits;
    QHash<QString, QString> m_dataValues;
};

} // namespace OFS::Mugen
