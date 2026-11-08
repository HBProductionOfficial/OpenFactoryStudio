#pragma once

#include "core/CoreExport.h"

struct lua_State;

namespace sol {
class state;
}

namespace OFS {

// Lua doesn't check precompiled chunks, so a crafted one could reach memory outside the sandbox.
// After this only source text loads: load, loadfile and dofile take text only, and string.dump is
// gone. Chunks given to the state must be loaded in text mode too.
OFS_CORE_EXPORT void restrictLoading(sol::state& lua);

// Lua doesn't watch finalizers (__gc) for runaway code, so a script that must stop can't set them.
OFS_CORE_EXPORT void noFinalizers(sol::state& lua);

// Stops a script that runs too long, so an endless loop can't hang the program. Call
// stopRunawayScript(L, false) when the script is done. Use noFinalizers() with it.
OFS_CORE_EXPORT void stopRunawayScript(lua_State* L, bool on = true);

} // namespace OFS
