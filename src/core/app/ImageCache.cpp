#include "core/app/ImageCache.h"

#include <atomic>

namespace OFS::ImageCache {

namespace {

std::atomic<bool> g_lowMemory{false}; // read by background decoding too

} // namespace

void setLowMemory(bool on)
{
    g_lowMemory = on;
}

bool lowMemory()
{
    return g_lowMemory;
}

} // namespace OFS::ImageCache
