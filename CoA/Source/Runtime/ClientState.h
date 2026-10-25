#ifndef COA_SOURCE_RUNTIME_CLIENTSTATE_H
#define COA_SOURCE_RUNTIME_CLIENTSTATE_H

// Client state the DLL reads directly (fixed exe globals in the original). The probes are installed by
// ClientStateClient.cpp; without one (tests) the answer is the default given.
#include <cstdint>

struct lua_State;

namespace CoA::ClientState {
    // CGGameUI::s_inWorld (0xBD0792); default false.
    bool InWorld();
    void SetInWorldProbe(bool (*probe)());

    // CGGameUI::CanPerformAction (0x5191C0); default true.
    bool CanPerformAction(int32_t action);
    void SetActionProbe(bool (*probe)(int32_t action));

    // The client's main Lua state (FrameScript_GetContext, 0x817DB0); default nullptr.
    lua_State* MainLuaState();
    void SetMainLuaStateProbe(lua_State* (*probe)());

    // The client directory with a trailing separator, as 0x4218B0 returns it; default "".
    const char* BasePath();
    void SetBasePathProbe(const char* (*probe)());

    void InstallClient();
}

#endif
