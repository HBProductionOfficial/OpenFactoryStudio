#include "core/script/LuaSafety.h"

#include <sol/sol.hpp>

namespace OFS {

namespace {

constexpr int kHookInstructions = 10000;
constexpr int kMaxHooks = 20000; // 200 million instructions
thread_local int t_hooks = 0;

void runawayHook(lua_State* L, lua_Debug*)
{
    if (++t_hooks <= kMaxHooks)
        return;
    // From now on every instruction fails, so a pcall can't catch the error and go on.
    lua_sethook(L, runawayHook, LUA_MASKCOUNT, 1);
    luaL_error(L, "the script ran too long and was stopped");
}

} // namespace

void restrictLoading(sol::state& lua)
{
    // load() and loadfile() keep their own environment argument only when one is given.
    lua.safe_script(R"(
local textLoad = load
load = function(chunk, name, mode, ...)
    if select("#", ...) > 0 then
        return textLoad(chunk, name, "t", ...)
    end
    return textLoad(chunk, name, "t")
end
local textLoadFile = loadfile
loadfile = function(name, mode, ...)
    if select("#", ...) > 0 then
        return textLoadFile(name, "t", ...)
    end
    return textLoadFile(name, "t")
end
dofile = function(name)
    local chunk = assert(loadfile(name))
    return chunk()
end
if string then
    string.dump = nil
end
)",
                    sol::script_pass_on_error, "=ofs", sol::load_mode::text);
}

void noFinalizers(sol::state& lua)
{
    lua.safe_script(R"(
local setMeta = setmetatable
setmetatable = function(t, mt)
    if type(mt) == "table" and rawget(mt, "__gc") ~= nil then
        error("__gc can't be used here", 2)
    end
    return setMeta(t, mt)
end
)",
                    sol::script_pass_on_error, "=ofs", sol::load_mode::text);
}

void stopRunawayScript(lua_State* L, bool on)
{
    t_hooks = 0;
    if (on)
        lua_sethook(L, runawayHook, LUA_MASKCOUNT, kHookInstructions);
    else
        lua_sethook(L, nullptr, 0, 0);
}

} // namespace OFS
