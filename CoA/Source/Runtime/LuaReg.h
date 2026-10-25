#ifndef COA_SOURCE_RUNTIME_LUAREG_H
#define COA_SOURCE_RUNTIME_LUAREG_H

#include <cstddef>

struct lua_State;
typedef int (*lua_CFunction)(lua_State* L);

namespace CoA {
    // One Lua native (port of AscBindings::Binding). ns: "" for a global, a C_* namespace, "@Name" for a method of
    // the registry metatable Name (also _G.Name, __index = itself), or "#Type" for a widget method of
    // Frame / Minimap / Model / MovieFrame / GameTooltip (added to the client's method table, not here).
    struct Binding {
        const char* ns;
        const char* name;
        lua_CFunction fn;
    };
}

// A Binding table entry. Tools/coa/inventory.py counts these.
#define COA_NATIVE(ns, name, fn) CoA::Binding { (ns), (name), (fn) }

namespace CoA::LuaReg {
    // A module's bindings; called from the module's Init (AscBindings::Module).
    void AddModule(const CoA::Binding* bindings, size_t count);
    // Tests only: forget every module and callback.
    void Reset();

    // AscBindings::Register: the glue state gets only the natives the original registers there
    // (BindingStates.generated.inc, measured from a live capture); the world state gets all but the glue-only ones.
    // '@' and '#' entries are not in the capture: '@' goes into both states, '#' is never registered here.
    void Register(lua_State* L, bool glue);

    // FUN_102787e0's list (after the glue state gets its natives) and FUN_102783f0's (after the world state's).
    void OnGlueRegistered(void (*cb)());
    void OnWorldRegistered(void (*cb)());
    void RunGlueRegistered();
    void RunWorldRegistered();

    // Every "#<widget>" entry, for the widget module to add to the client's method table.
    void ForEachWidgetMethod(const char* widget, void (*cb)(const char* name, lua_CFunction fn, void* ctx), void* ctx);
}

// FUN_10278260, the DLL's detour on luaopen_base (0x855060): called at the end of luaopen_base for every new Lua
// state, before the client registers its own natives. Glue when CGlueMgr::m_suspended == 0 and the game is not
// initialized; world when CGlueMgr::m_suspended != 0.
void CoA_OnLuaBaseOpened(lua_State* L);

#endif
