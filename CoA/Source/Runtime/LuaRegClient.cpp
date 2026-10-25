#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Log.h"
#include "WoW/Source/Client.h"
#include "WoW/Source/Glue/CGlueMgr.h"

// FUN_10278260 (AscRegistry.cpp:LuaSetupDetour), run at the end of luaopen_base for every new Lua state:
//   CGlueMgr::m_suspended (0xB6AA2C) == 0 and the game not initialized (0xB2F9A0) -> glue natives, then the glue list
//   CGlueMgr::m_suspended != 0                                             -> world natives, then the world list
void CoA_OnLuaBaseOpened(lua_State* L) {
    if (CGlueMgr::m_suspended == 0) {
        if (ClientGameInitialized() == 0) {
            CoA::Log::Printf("Lua state %p: glue natives", static_cast<void*>(L));
            CoA::LuaReg::Register(L, true);
            CoA::LuaReg::RunGlueRegistered();
        }
    } else {
        CoA::Log::Printf("Lua state %p: world natives", static_cast<void*>(L));
        CoA::LuaReg::Register(L, false);
        CoA::LuaReg::RunWorldRegistered();
    }
}
