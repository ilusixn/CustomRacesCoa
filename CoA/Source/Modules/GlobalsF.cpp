// GLOBAL bindings, batch F; port of AscGlobalsF.cpp's request senders. The cursor / unit / event natives are in
// GlobalsFClient.cpp.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"

namespace {
    int32_t ArgInt(lua_State* L, int idx) {
        return CoA::Script::ToInt(lua_tonumber(L, idx));
    }

    int RequestRespawnAtCheckpoint(lua_State*) {
        CoA::Packet(0x774).U8(1).Send();
        return 0;
    }

    // CMSG 0x749 on, 0x74A off; true once sent.
    int ToggleDraftMode(lua_State* L) {
        bool enable;

        if (!CoA::Script::ReadBool(L, enable)) {
            return 0;
        }

        CoA::Packet(enable ? 0x749 : 0x74A).Send();
        lua_pushboolean(L, 1);
        return 1;
    }

    int SetLootSpecialization(lua_State* L) {
        CoA::Packet(0x679).U32(static_cast<uint32_t>(static_cast<int>(lua_tonumber(L, 1)))).Send();
        return 0;
    }

    int RequestChangeRuleset(lua_State* L) {
        CoA::Packet(0x53F).U8(static_cast<uint8_t>(ArgInt(L, 1))).Send();
        return 0;
    }

    int RequestSetDungeonDifficulty(lua_State* L) {
        CoA::Packet(0x773).U8(static_cast<uint8_t>(ArgInt(L, 1))).Send();
        return 0;
    }

    // (bracket, role): sent only with exactly two arguments.
    int JoinArenaWithRole(lua_State* L) {
        if (lua_gettop(L) == 2) {
            const uint32_t a = static_cast<uint32_t>(ArgInt(L, 1));
            const uint32_t b = static_cast<uint32_t>(ArgInt(L, 2));
            CoA::Packet(0x5C1).U32(a).U32(b).Send();
        }

        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "JoinArenaWithRole", JoinArenaWithRole),
        COA_NATIVE("", "RequestChangeRuleset", RequestChangeRuleset),
        COA_NATIVE("", "RequestRespawnAtCheckpoint", RequestRespawnAtCheckpoint),
        COA_NATIVE("", "RequestSetDungeonDifficulty", RequestSetDungeonDifficulty),
        COA_NATIVE("", "SetLootSpecialization", SetLootSpecialization),
        COA_NATIVE("", "ToggleDraftMode", ToggleDraftMode),
    };
}

void CoA::GlobalsF::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
