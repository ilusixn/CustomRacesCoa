#include "CoA/Source/Runtime/Script.h"
#include "CoA/Source/Runtime/Log.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

namespace {
    const char* const s_expected[] = {
        "nil", "boolean", "lightuserdata", "number", "string", "table", "function", "userdata", "thread",
    };
}

bool CoA::Script::ValidateInput(lua_State* L, std::initializer_list<int> types) {
    return CoA::Script::ValidateInput(L, types.begin(), types.size());
}

// FUN_10309e50. Trailing nil slots are dropped from the required count but still type-checked. The original also
// logs the addon, function and object names; the index and expected type are what identify the call.
bool CoA::Script::ValidateInput(lua_State* L, const int* types, size_t count) {
    int required = static_cast<int>(count);

    while (required > 0 && types[required - 1] == LUA_TNIL) {
        --required;
    }

    int top = lua_gettop(L);

    if (top < required) {
        CoA::Log::Printf("Script::ValidateInput Invalid argument count. Expected %d, got %d.", required, top);
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        int idx = static_cast<int>(i) + 1;
        int want = types[i];
        int type = lua_type(L, idx);
        bool ok;

        switch (want) {
            case LUA_TNIL:
                ok = type <= LUA_TNIL;
                break;
            case LUA_TBOOLEAN:
                ok = type == LUA_TNIL || type == LUA_TBOOLEAN || type == LUA_TNUMBER;
                break;
            case LUA_TNUMBER:
                ok = lua_isnumber(L, idx) != 0;
                break;
            case LUA_TSTRING:
                ok = lua_isstring(L, idx) != 0;
                break;
            case LUA_TLIGHTUSERDATA:
            case LUA_TTABLE:
            case LUA_TFUNCTION:
            case LUA_TUSERDATA:
            case LUA_TTHREAD:
                ok = type == want;
                break;
            default:
                ok = false;
                break;
        }

        if (!ok) {
            CoA::Log::Printf("Script::ValidateInput Invalid argument type at index %d. Expected %s.", idx,
                (want >= 0 && want <= LUA_TTHREAD) ? s_expected[want] : "unknown");
            return false;
        }
    }

    return true;
}

double CoA::Script::CheckNumber(lua_State* L, int idx) {
    return luaL_checknumber(L, idx);
}

const char* CoA::Script::CheckString(lua_State* L, int idx) {
    return luaL_checklstring(L, idx, nullptr);
}

bool CoA::Script::ReadNumber(lua_State* L, uint32_t& out) {
    if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER })) {
        return false;
    }

    out = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)));
    return true;
}

bool CoA::Script::ReadString(lua_State* L, std::string& out) {
    if (!CoA::Script::ValidateInput(L, { LUA_TSTRING })) {
        return false;
    }

    out.assign(CoA::Script::CheckString(L, 1));
    return true;
}

bool CoA::Script::ReadInt2(lua_State* L, int32_t& a, int32_t& b) {
    if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TNUMBER })) {
        return false;
    }

    a = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1));
    b = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 2));
    return true;
}

bool CoA::Script::ReadBool(lua_State* L, bool& out) {
    if (!CoA::Script::ValidateInput(L, { LUA_TBOOLEAN })) {
        return false;
    }

    out = lua_toboolean(L, 1) != 0;
    return true;
}

void CoA::Script::PushStr(lua_State* L, const char* s) {
    lua_pushstring(L, s ? s : "");
}

void CoA::Script::PushInt(lua_State* L, int32_t v) {
    lua_pushinteger(L, v);
}

void CoA::Script::PushNum(lua_State* L, double v) {
    lua_pushnumber(L, v);
}

void CoA::Script::PushBool(lua_State* L, bool v) {
    lua_pushboolean(L, v ? 1 : 0);
}

int CoA::Script::PushNils(lua_State* L, int n) {
    for (int i = 0; i < n; ++i) {
        lua_pushnil(L);
    }

    return n;
}

void CoA::Script::SetField(lua_State* L, const char* key, int32_t value) {
    lua_pushstring(L, key);
    lua_pushinteger(L, value);
    lua_settable(L, -3);
}

void CoA::Script::ToBoolMap(lua_State* L, int idx, std::map<std::string, bool>& out) {
    out.clear();

    if (lua_type(L, idx) != LUA_TTABLE) {
        CoA::Log::Printf("FrameScript::lua_totable Value at index %d is not a table", idx);
        return;
    }

    lua_pushnil(L);

    while (lua_next(L, idx)) {
        if (!lua_isstring(L, -2)) {
            CoA::Log::Printf("FrameScript::lua_totable Invalid lua key type");
            lua_settop(L, -3);
            out.clear();
            return;
        }

        const int vt = lua_type(L, -1);

        if (vt != LUA_TNIL && vt != LUA_TBOOLEAN && vt != LUA_TNUMBER) {
            CoA::Log::Printf("FrameScript::lua_totable Invalid lua value type");
            lua_settop(L, -3);
            out.clear();
            return;
        }

        const bool value = lua_toboolean(L, -1) != 0;
        out[lua_tolstring(L, -2, nullptr)] = value;
        lua_settop(L, -2);
    }
}

void CoA::Script::ToStringArray(lua_State* L, int idx, std::vector<std::string>& out) {
    out.clear();

    if (lua_type(L, idx) != LUA_TTABLE) {
        CoA::Log::Printf("FrameScript::lua_toarray Value at index %d is not a table", idx);
        return;
    }

    if (idx < 0) {
        idx = lua_gettop(L) + 1 + idx;
    }

    lua_pushnil(L);

    while (lua_next(L, idx)) {
        if (!lua_isstring(L, -1)) {
            lua_settop(L, -3);
            CoA::Log::Printf("FrameScript::lua_toarray Invalid lua value type");
            out.clear();
            return;
        }

        out.emplace_back(lua_tolstring(L, -1, nullptr));
        lua_settop(L, -2);
    }
}
