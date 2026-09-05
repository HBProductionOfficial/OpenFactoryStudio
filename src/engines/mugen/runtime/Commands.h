#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <array>
#include <cstdint>
#include <vector>

namespace OFS::Mugen::Runtime {

// Directions are relative to the way the character faces (B back, F forward). N is no direction.
enum class Key : std::uint8_t
{
    U,
    D,
    B,
    F,
    UB,
    UF,
    DB,
    DF,
    N,
    a,
    b,
    c,
    x,
    y,
    z,
    s,
};

// What the player holds in one tick, with directions as on the screen.
struct InputState
{
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool a = false;
    bool b = false;
    bool c = false;
    bool x = false;
    bool y = false;
    bool z = false;
    bool s = false;
};

// A step key with its modifiers: "/" held, "~" released (after being held `chargeTime` ticks), "$"
// any direction that includes it.
struct StepKey
{
    Key key = Key::N;
    bool slash = false;
    bool tilde = false;
    bool dollar = false;
    int chargeTime = 0;
    bool operator==(const StepKey&) const = default;
};

// Keys pressed together ("a+b"). ">" means nothing else may be pressed in between.
struct CommandStep
{
    std::vector<StepKey> keys;
    bool greater = false;
    bool isSingleDirection() const;
    bool isDirToButton(const CommandStep& next) const;
};

// How long ago each key changed: positive while held, negative while released.
class InputBuffer
{
public:
    void reset();
    void update(const InputState& input, int facing);
    int state(const StepKey& key) const;
    int charge(const StepKey& key) const;
    bool holdingBack() const { return m_now[B] > 0; }

private:
    enum Raw
    {
        U,
        D,
        B,
        F,
        N,
        A,
        Bt,
        C,
        X,
        Y,
        Z,
        S,
        Count
    };
    std::array<int, Count> m_now{};
    std::array<int, Count> m_previous{};
};

class Command
{
public:
    static Command parse(const QString& name, const QString& text, int time, int bufferTime,
                         const std::array<Key, 7>& remap, QString* error = nullptr);

    QString name() const { return m_name; }
    const std::vector<CommandStep>& steps() const { return m_steps; }
    void step(const InputBuffer& input);
    bool active() const { return m_bufferTime > 0; }
    void clear();

private:
    bool greaterCheckFails(std::size_t i, const InputBuffer& input) const;

    QString m_name;
    std::vector<CommandStep> m_steps;
    std::vector<bool> m_completed;
    std::vector<int> m_stepTimers;
    std::vector<std::size_t> m_order;
    int m_maxTime = 15;
    int m_maxBufferTime = 1;
    int m_time = 0;
    int m_bufferTime = 0;
};

class CommandList
{
public:
    void load(const QString& text, QStringList* errors = nullptr);
    void add(const Command& command) { m_commands.push_back(command); }
    bool isEmpty() const { return m_commands.empty(); }
    QStringList names() const;

    void update(const InputState& input, int facing);
    bool active(const QString& name) const;
    bool holdingBack() const { return m_input.holdingBack(); }
    void clear();

private:
    InputBuffer m_input;
    std::vector<Command> m_commands;
};

} // namespace OFS::Mugen::Runtime
