#pragma once

#include "engines/mugen/runtime/Commands.h"

#include <cstdint>

namespace OFS::Mugen {

namespace Runtime {
class Character;
class Match;
} // namespace Runtime

// The Emulator panel's training options, as MUGEN's training menu has them.
struct TrainingOptions
{
    enum class Control
    {
        Cooperative, // the dummy does what the other options say
        AI,
        Manual,
    };
    enum class Mode
    {
        Stand,
        Crouch,
        Jump,
        WJump,
    };
    enum class Guard
    {
        None,
        Auto,
    };
    enum class Distance
    {
        Any,
        Close,
        Medium,
        Far,
    };
    Control control = Control::Cooperative;
    Mode mode = Mode::Stand;
    Guard guard = Guard::None;
    Distance distance = Distance::Any;
    int buttonJam = 0; // 1 to 7: A, B, C, X, Y, Z, Start mashed
    int aiLevel = 4;
};

// MUGEN's training mode: life and power come back and nobody is knocked out.
class Training
{
public:
    void setOptions(const TrainingOptions& options) { m_options = options; }
    const TrainingOptions& options() const { return m_options; }
    void reset(std::uint32_t seed = 1);

    Runtime::InputState dummyInput(const Runtime::Match& match, const Runtime::Character& dummy,
                                   const Runtime::InputState& keys);
    // Life comes back after a second out of hit states, power after a second with control.
    void refill(const Runtime::Match& match, Runtime::Character& player, int side);

private:
    int random(int max);

    TrainingOptions m_options;
    int m_lifeTimer[2] = {0, 0};
    int m_powerTimer[2] = {0, 0};
    int m_jamDelay = 0;
    std::uint32_t m_seed = 1;
    // x 1 is forward and y 1 is down.
    int m_aiX = 0;
    int m_aiY = 0;
    int m_aiHold = 0;
};

} // namespace OFS::Mugen
