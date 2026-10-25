// GLOBAL bindings, batch C (port of AscGlobalsC.cpp); the natives over client objects are in GlobalsCClient.cpp.
//   SMSG 0x720 (FUN_102d9f20 -> FUN_102d9dd0): 11 x u32 wildcard prestige record (words 8..10 are what
//   GetWildcardPrestigeInfo reports).
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"

namespace {
    // The ScreenLocations.dbc row's path (+4), or nil.
    int GetScreenLocationInfo(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        CoA::Dbc::Table& table = CoA::Dbc::Get("DBFilesClient\\ScreenLocations.dbc");

        if (const uint8_t* row = table.Row(id)) {
            CoA::Script::PushStr(L, table.Str(row, 4));
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    uint32_t s_prestige[11] = {};   // DAT_10be37fc..
    bool s_prestigeSet = false;     // DAT_10be3828

    int32_t OnPrestige(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {   // 0x720
        for (uint32_t& word : s_prestige) {
            word = CoA::Read::U32(msg);
        }

        s_prestigeSet = true;
        return 1;
    }

    void ResetPrestige() {   // FUN_102d9ef0 (every glue screen)
        s_prestigeSet = false;
    }

    int GetWildcardPrestigeInfo(lua_State* L) {
        if (!s_prestigeSet) {
            return CoA::Script::PushNils(L, 3);
        }

        for (uint32_t i = 8; i < 11; ++i) {
            lua_pushnumber(L, static_cast<double>(static_cast<int32_t>(s_prestige[i])));
        }

        return 3;
    }

    int SendEmpty(uint32_t opcode) {
        CoA::Packet(opcode).Send();
        return 0;
    }

    int ResetRaids(lua_State*) {
        return SendEmpty(0x620);
    }

    int ResetDungeons(lua_State*) {
        return SendEmpty(0x61F);
    }

    int PortGraveyard(lua_State*) {
        return SendEmpty(0x544);
    }

    int ResetWorldTime(lua_State*) {
        return SendEmpty(0x543);
    }

    // The item addon record's +0x3C (FUN_101a2690).
    int GetItemLevelInstant(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        if (const CoA::ItemAddon::Record* r = CoA::ItemAddon::Find(id)) {
            lua_pushinteger(L, static_cast<int>(r->fields[11]));
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetWildcardPrestigeInfo", GetWildcardPrestigeInfo),
        COA_NATIVE("", "ResetRaids", ResetRaids),
        COA_NATIVE("", "ResetDungeons", ResetDungeons),
        COA_NATIVE("", "PortGraveyard", PortGraveyard),
        COA_NATIVE("", "ResetWorldTime", ResetWorldTime),
        COA_NATIVE("", "GetItemLevelInstant", GetItemLevelInstant),
        COA_NATIVE("", "GetScreenLocationInfo", GetScreenLocationInfo),
    };
}

void CoA::GlobalsC::Init() {
    CoA::Lifecycle::OnGlueScreen(ResetPrestige);
    COA_PACKET(0x720, OnPrestige);
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
