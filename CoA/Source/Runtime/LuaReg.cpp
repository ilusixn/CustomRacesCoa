#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Log.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <vector>

// Port of AscBindings.cpp's registration (Register, SetMethod, SetField) and AscRegistry.cpp's luaopen_base detour.

namespace {
    struct ModuleEntry {
        const CoA::Binding* bindings;
        size_t count;
    };

    struct StateName {
        const char* ns;
        const char* name;
    };

#include "CoA/Source/Runtime/BindingStates.generated.inc"

    std::vector<ModuleEntry> s_modules;
    std::vector<void (*)()> s_glueRegistered;    // 0x10BE284C
    std::vector<void (*)()> s_worldRegistered;   // 0x10BE2A2C

    bool Listed(const StateName* list, size_t count, const CoA::Binding& b) {
        const char* ns = b.ns ? b.ns : "";

        for (size_t i = 0; i < count; ++i) {
            if (strcmp(list[i].ns, ns) == 0 && strcmp(list[i].name, b.name) == 0) {
                return true;
            }
        }

        return false;
    }

    bool InState(const CoA::Binding& b, bool glue) {
        if (b.ns && (b.ns[0] == '@' || b.ns[0] == '#')) {
            return true;
        }

        if (glue) {
            return Listed(kGlueBindings, sizeof(kGlueBindings) / sizeof(kGlueBindings[0]), b);
        }

        return !Listed(kGlueOnlyBindings, sizeof(kGlueOnlyBindings) / sizeof(kGlueOnlyBindings[0]), b);
    }

    // FUN_1030efe0: a method table kept in the registry under `mt` and as the global of the same name, __index
    // pointing at itself; created on first use.
    void SetMethod(lua_State* L, const char* mt, const char* name, lua_CFunction fn) {
        int top = lua_gettop(L);
        lua_getfield(L, LUA_REGISTRYINDEX, mt);

        if (lua_type(L, -1) != LUA_TTABLE) {
            lua_settop(L, top);
            lua_createtable(L, 0, 0);
            lua_pushvalue(L, -1);
            lua_setfield(L, LUA_REGISTRYINDEX, mt);
            lua_pushvalue(L, -1);
            lua_setfield(L, LUA_GLOBALSINDEX, mt);
            lua_pushvalue(L, -1);
            lua_setfield(L, -2, "__index");
        }

        lua_pushcclosure(L, fn, 0);
        lua_setfield(L, -2, name);
        lua_settop(L, top);
    }

    void SetField(lua_State* L, const char* ns, const char* name, lua_CFunction fn) {
        int top = lua_gettop(L);
        lua_getfield(L, LUA_GLOBALSINDEX, ns);

        if (lua_type(L, -1) != LUA_TTABLE) {
            lua_settop(L, top);
            lua_createtable(L, 0, 0);
        }

        lua_pushcclosure(L, fn, 0);
        lua_setfield(L, -2, name);
        lua_setfield(L, LUA_GLOBALSINDEX, ns);
        lua_settop(L, top);
    }

    // AscLua::RegisterGlobal.
    void SetGlobal(lua_State* L, const char* name, lua_CFunction fn) {
        lua_checkstack(L, 2);
        lua_pushcclosure(L, fn, 0);
        lua_setfield(L, LUA_GLOBALSINDEX, name);
    }

    void RegisterOne(lua_State* L, const CoA::Binding& b) {
        if (b.ns && b.ns[0] == '#') {
            return;
        }

        if (b.ns && b.ns[0] == '@') {
            SetMethod(L, b.ns + 1, b.name, b.fn);
        } else if (b.ns && *b.ns) {
            SetField(L, b.ns, b.name, b.fn);
        } else {
            SetGlobal(L, b.name, b.fn);
        }
    }
}

void CoA::LuaReg::AddModule(const CoA::Binding* bindings, size_t count) {
    s_modules.push_back({ bindings, count });
}

void CoA::LuaReg::Reset() {
    s_modules.clear();
    s_glueRegistered.clear();
    s_worldRegistered.clear();
}

void CoA::LuaReg::Register(lua_State* L, bool glue) {
    if (!L) {
        return;
    }

    size_t n = 0;

    for (const ModuleEntry& m : s_modules) {
        for (size_t i = 0; i < m.count; ++i) {
            if (InState(m.bindings[i], glue)) {
                RegisterOne(L, m.bindings[i]);
                ++n;
            }
        }
    }

    CoA::Log::Printf("LuaReg: %u natives registered (%s)", static_cast<unsigned>(n), glue ? "glue" : "world");
}

void CoA::LuaReg::OnGlueRegistered(void (*cb)()) {
    s_glueRegistered.push_back(cb);
}

void CoA::LuaReg::OnWorldRegistered(void (*cb)()) {
    s_worldRegistered.push_back(cb);
}

void CoA::LuaReg::RunGlueRegistered() {
    for (auto cb : s_glueRegistered) {
        cb();
    }
}

void CoA::LuaReg::RunWorldRegistered() {
    for (auto cb : s_worldRegistered) {
        cb();
    }
}

void CoA::LuaReg::ForEachWidgetMethod(const char* widget, void (*cb)(const char* name, lua_CFunction fn, void* ctx), void* ctx) {
    for (const ModuleEntry& m : s_modules) {
        for (size_t i = 0; i < m.count; ++i) {
            const CoA::Binding& b = m.bindings[i];

            if (b.ns && b.ns[0] == '#' && strcmp(b.ns + 1, widget) == 0) {
                cb(b.name, b.fn, ctx);
            }
        }
    }
}
