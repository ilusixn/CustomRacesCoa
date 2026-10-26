// AscClientGuards.cpp (see ClientGuards.h).
#include "CoA/Source/Modules/ClientGuards.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/lua-5.1/src/lualib.h"
#include <cstdint>
#include <cstring>

namespace {
    CoA::ClientGuards::Deps s_deps;

    // FUN_10a4c0c0.
    void SkipBaseMipStore() {
        if (s_deps.skipBaseMipStore) {
            s_deps.skipBaseMipStore();
        }
    }
}

CoA::ClientGuards::Deps& CoA::ClientGuards::Dependencies() {
    return s_deps;
}

int CoA::ClientGuards::ToBoolean(lua_State* L) {
    const int type = lua_type(L, 1);

    if (type == LUA_TBOOLEAN) {
        lua_pushboolean(L, lua_toboolean(L, 1) != 0);
        return 1;
    }

    if (type == LUA_TNUMBER) {
        const double d = luaL_checknumber(L, 1);
        lua_pushboolean(L, static_cast<int32_t>(d) != 0);
        return 1;
    }

    const char* s = luaL_checklstring(L, 1, nullptr);
    const bool yes = *s && (strcmp(s, "true") == 0 || strcmp(s, "1") == 0 || strcmp(s, "on") == 0 || strcmp(s, "t") == 0);
    lua_pushboolean(L, yes);
    return 1;
}

void CoA::ClientGuards::Init() {
    // FUN_10a60cd0: FUN_10a4c0c0 on the after-0x52A980 list.
    CoA::ClientHooks::afterUiReload.Add(SkipBaseMipStore);
    // AscRuntime::Detour(0x8540D0), as the Engine pointer luaopen_string reads.
    g_luaStringToBoolean = CoA::ClientGuards::ToBoolean;
}
