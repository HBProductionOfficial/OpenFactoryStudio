#pragma once

#include "core/CoreExport.h"

#include <QtGlobal>

namespace OFS::ImageCache {

// Options > General > Cache type. "Memory" keeps every decoded sprite image. "Hard disk" keeps at
// most LowMemoryLimit in each cache and decodes the others again when needed.
inline constexpr qsizetype LowMemoryLimit = 64;

OFS_CORE_EXPORT void setLowMemory(bool on);
OFS_CORE_EXPORT bool lowMemory();

// True when a cache of `count` images must be emptied before it takes another.
inline bool full(qsizetype count)
{
    return lowMemory() && count >= LowMemoryLimit;
}

} // namespace OFS::ImageCache
