#pragma once

#include <QLatin1StringView>

namespace OFS::MugenType {

// Project type ids. Each definitions file is checked with the syntax file type of the same name.
inline constexpr QLatin1StringView Character{"char"};
inline constexpr QLatin1StringView Stage{"stage"};
inline constexpr QLatin1StringView Storyboard{"storyboard"};
inline constexpr QLatin1StringView Motif{"motif"}; // System screens
inline constexpr QLatin1StringView Fight{"fight"}; // Fight HUD & effects
inline constexpr QLatin1StringView Font{"font"};
inline constexpr QLatin1StringView Select{"select"};

} // namespace OFS::MugenType
