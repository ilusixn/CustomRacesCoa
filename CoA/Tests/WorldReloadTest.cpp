#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <set>
#include <string>

namespace {
    // Every Lua function reachable as _G.name or _G.ns.name.
    std::set<std::string> Functions(lua_State* L) {
        std::set<std::string> out;
        lua_pushnil(L);

        while (lua_next(L, LUA_GLOBALSINDEX)) {
            if (lua_type(L, -2) == LUA_TSTRING) {
                const std::string key = lua_tostring(L, -2);

                if (lua_type(L, -1) == LUA_TFUNCTION) {
                    out.insert(key);
                } else if (lua_type(L, -1) == LUA_TTABLE && key != "_G") {
                    lua_pushnil(L);

                    while (lua_next(L, -2)) {
                        if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TFUNCTION) {
                            out.insert(key + "." + lua_tostring(L, -2));
                        }

                        lua_pop(L, 1);
                    }
                }
            }

            lua_pop(L, 1);
        }

        return out;
    }
}

// World, then glue (logout), then world again: every world native is on the last world state too.
COA_TEST(WorldReload_NativesBackAfterGlue) {
    CoA::LuaReg::Reset();
    CoA::Packets::Clear();
    CoA::Misc::Init();
    CoA::GlobalsB::Init();
    CoA::GlobalsC::Init();
    CoA::GlobalsD::Init();
    CoA::GlobalsG::Init();
    CoA::GlobalsH::Init();
    CoA::GlobalsJ::Init();
    CoA::SmallApis::Init();
    CoA::Bindings::Init();

    lua_State* first = CoATest::NewState(false);
    const std::set<std::string> world = Functions(first);
    lua_State* glue = CoATest::NewState(true);
    const std::set<std::string> glueFunctions = Functions(glue);
    lua_State* again = CoATest::NewState(false);
    const std::set<std::string> reloaded = Functions(again);

    CHECK(world.count("FindScriptEvents") == 1);   // a world-only native
    CHECK(glueFunctions.count("FindScriptEvents") == 0);
    CHECK(world.size() > glueFunctions.size());
    CHECK(reloaded == world);
    lua_close(first);
    lua_close(glue);
    lua_close(again);
}
