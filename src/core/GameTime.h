#pragma once

namespace OFS {

// MUGEN game time runs at 60 ticks a second.
inline constexpr int TicksPerSecond = 60;

// Rounded down because Qt timers count whole milliseconds.
inline constexpr int TickIntervalMs = 1000 / TicksPerSecond;

} // namespace OFS
