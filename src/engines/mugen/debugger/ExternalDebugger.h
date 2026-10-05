#pragma once

#include "core/debug/Breakpoints.h"
#include "core/debug/DebugTypes.h"
#include "engines/mugen/debugger/DebugAddressMap.h"

#include <QByteArray>
#include <QHash>
#include <QPointer>
#include <QSet>

#include <functional>
#include <optional>

namespace OFS::Debug {
class ProcessMemory;
}

namespace OFS::Mugen {

struct StatePlace
{
    int state = 0;
    QString file;
    int line = 0; // from 1
};

QList<StatePlace> statedefs(const QString& text);
std::optional<StatePlace> stateOfLine(const QList<StatePlace>& statedefs, int line);
// The first two plain arguments are P1 and P2, "-p3 name" and "-p4 name" the others.
QSet<int> playerSlotsOf(const QStringList& arguments, const QString& name);

// Debugger for Debug > Start on Engine. It reads a running engine's memory through an address map
// and writes its pause flag. It cannot see the lines the engine runs, so a file/line breakpoint stops
// when a project player or helper enters the state the line belongs to.
class ExternalDebugger
{
public:
    struct Player
    {
        std::uint64_t address = 0;
        int slot = 0; // 0 to 3 are P1 to P4, then the helpers
        QByteArray bytes;
    };

    struct Location
    {
        int playerId = 0;
        QString playerName;
        StatePlace place;
    };

    struct Look
    {
        bool readable = false;
        std::optional<bool> paused;
        bool hit = false;
    };

    using LineResolver = std::function<std::optional<StatePlace>(const QString& file, int line)>;
    using StateResolver = std::function<std::optional<StatePlace>(int state)>;

    explicit ExternalDebugger(DebugAddressMap map);
    ~ExternalDebugger();

    const DebugAddressMap& map() const { return m_map; }
    void setMemory(Debug::ProcessMemory* memory) { m_memory = memory; }
    void setBreakpoints(Debug::BreakpointList* breakpoints) { m_breakpoints = breakpoints; }
    void setProjectSlots(const QSet<int>& slots) { m_projectSlots = slots; }
    void setResolvers(LineResolver lines, StateResolver states);
    void reset();

    Look look(bool checkBreakpoints);
    bool setPaused(bool paused);
    bool stepFrame(int timeoutMs = 500);
    void clearLocation() { m_location.reset(); }

    const std::optional<Location>& location() const { return m_location; }
    // For an Interrupt: the first project player in a state the project defines, else the first one.
    std::optional<Location> currentLocation() const;
    const QList<Player>& players() const { return m_players; }

    QList<Debug::Variable> variables() const;
    std::optional<Debug::Variable> variable(const QString& path) const;
    QList<Debug::StackFrame> stack() const;

    std::optional<std::int32_t> intField(const Player& player, const QString& name, int index = 0) const;
    std::optional<float> floatField(const Player& player, const QString& name, int index = 0) const;
    std::optional<std::int32_t> gameInt(const QString& name) const;
    const Player* playerById(int id) const;
    const Player* playerAt(std::uint64_t address) const;
    QString label(const Player& player) const;

private:
    void read();
    bool isProject(const Player& player) const;
    QList<const Player*> projectPlayers() const;
    QString dataValue(const QString& address) const;
    bool hit(const Debug::Breakpoint& breakpoint, const Player* player);
    bool conditionHolds(const QString& condition, const Player* player) const;
    Debug::Variable playerVariables(const Player& player) const;
    QString formatField(const Player* player, const QString& name, int index = 0) const;

    DebugAddressMap m_map;
    Debug::ProcessMemory* m_memory = nullptr;
    QPointer<Debug::BreakpointList> m_breakpoints;
    QSet<int> m_projectSlots;
    LineResolver m_lines;
    StateResolver m_states;

    std::uint64_t m_game = 0;
    QHash<QString, QByteArray> m_globals;
    QList<Player> m_players;
    std::optional<Location> m_location;
    std::optional<std::int32_t> m_lastTime;
    struct Seen
    {
        int state = 0;
        int time = 0;
    };
    QHash<QString, Seen> m_seen;
    QHash<int, int> m_hits;
    QHash<QString, QString> m_dataValues;
};

} // namespace OFS::Mugen
