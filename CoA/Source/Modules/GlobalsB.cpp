// Small GLOBAL bindings over the client's caches, DBC containers and descriptors, plus the packets that feed them;
// port of AscGlobalsB.cpp. The natives over client objects are in GlobalsBClient.cpp.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <vector>

namespace {
    // SMSG 0x76F CHARACTER_SELECTION_SORT_ORDER (FUN_100d3a00): a C string.
    std::string s_sortOrder;      // DAT_10bc94ac
    bool s_sortOrderSet = false;  // DAT_10bdb5fe

    int32_t OnSortOrder(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        s_sortOrder = CoA::Read::Str(msg);
        s_sortOrderSet = true;
        return 1;
    }

    int GetCharacterSelectionSortOrder(lua_State* L) {
        if (!s_sortOrderSet) {
            lua_pushnil(L);
        } else {
            CoA::Script::PushStr(L, s_sortOrder.c_str());
        }

        return 1;
    }

    // Realm-info object +0x40 then +0x18.
    int GetRealmMaintenance(lua_State* L) {
        const CoA::RealmInfo::Info& r = CoA::RealmInfo::Get();
        CoA::Script::PushBool(L, r.gates[0] != 0 && r.flag14 != 0);
        return 1;
    }

    // SMSG 0x768 HONORABLE_COMBAT_ZONES (FUN_100d3b30): u32 n x u32 zone.
    std::vector<uint32_t> s_honorableZones;   // DAT_10bdb5f0
    bool s_honorableZonesSet = false;         // DAT_10bdb5ee

    int32_t OnHonorableZones(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        s_honorableZones.clear();

        for (uint32_t n = CoA::Read::U32(msg); n; --n) {
            s_honorableZones.push_back(CoA::Read::U32(msg));
        }

        s_honorableZonesSet = true;
        CoA::Events::Signal("HONORABLE_COMBAT_ZONES_PAYLOAD", nullptr);
        return 1;
    }

    int GetHonorableCombatZones(lua_State* L) {
        if (!s_honorableZonesSet) {
            lua_pushnil(L);
            return 1;
        }

        lua_createtable(L, static_cast<int>(s_honorableZones.size()), 0);
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < s_honorableZones.size(); ++i) {
            lua_pushnumber(L, static_cast<double>(i + 1));
            lua_pushinteger(L, static_cast<int>(s_honorableZones[i]));
            lua_settable(L, -3);
        }

        return 1;
    }

    // SMSG 0x66D LFG_UPDATE_CUSTOM (FUN_100d3f80): u32 dungeon; CURRENT_LFG_DUNGEON_ID_CHANGED on a change.
    uint32_t s_lfgDungeon = 0;   // DAT_10bdb640

    int32_t OnLfgUpdateCustom(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        const uint32_t id = CoA::Read::U32(msg);
        const bool changed = id != s_lfgDungeon;
        s_lfgDungeon = id;

        if (changed) {
            CoA::Events::Signal("CURRENT_LFG_DUNGEON_ID_CHANGED", nullptr);
        }

        return 1;
    }

    // The first DungeonEncounterExtra row (by id) whose +0xC is the current LFG dungeon.
    int GetLFGBoss(lua_State* L) {
        if (s_lfgDungeon != 0) {
            CoA::Dbc::Table& t = CoA::Dbc::Get("DBFilesClient\\DungeonEncounterExtra.dbc");

            for (uint32_t id = t.MinId(); id <= t.MaxId() && t.Loaded(); ++id) {
                const uint8_t* row = t.Row(id);

                if (row && CoA::Dbc::Table::U32(row, 0xC) == s_lfgDungeon) {
                    lua_pushinteger(L, static_cast<int>(CoA::Dbc::Table::U32(row, 0)));
                    return 1;
                }
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // FUN_10005da0's static list.
    int Ascension_GetArenaBracketCutoffs(lua_State* L) {
        static const int32_t kCutoffs[6] = { 0, 1400, 1700, 2000, 2200, 2400 };

        for (int32_t c : kCutoffs) {
            lua_pushinteger(L, c);
        }

        return 6;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "Ascension_GetArenaBracketCutoffs", Ascension_GetArenaBracketCutoffs),
        COA_NATIVE("", "GetHonorableCombatZones", GetHonorableCombatZones),
        COA_NATIVE("", "GetLFGBoss", GetLFGBoss),
        COA_NATIVE("", "GetRealmMaintenance", GetRealmMaintenance),
        COA_NATIVE("", "GetCharacterSelectionSortOrder", GetCharacterSelectionSortOrder),
    };
}

void CoA::GlobalsB::Init() {
    COA_PACKET(0x76F, OnSortOrder);
    COA_PACKET(0x768, OnHonorableZones);
    COA_PACKET(0x66D, OnLfgUpdateCustom);
    COA_EVENT("HONORABLE_COMBAT_ZONES_PAYLOAD");
    COA_EVENT("CURRENT_LFG_DUNGEON_ID_CHANGED");
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
