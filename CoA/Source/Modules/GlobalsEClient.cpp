// AscGlobalsE.cpp's world natives (GetClassInfoByName / GetRaceInfo, also glue natives, are ported with the glue
// bindings): skill info, the world map area by name, a unit's primary stat marker and an inbox invoice's quantity.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/MailInfo.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include <cstdio>
#include <cstring>
#include <string>

namespace {
    // SkillLine name, then the player's rank / temp bonus / max through the player's skill slot, the profession flag
    // (category 11) and the description. Without a slot: name, nil x 3, profession, description.
    int GetSkillInfo(lua_State* L) {
        const int32_t id = CoA::Script::ToInt(lua_tonumber(L, 1));
        const SkillLineRec* row = g_skillLineDB.GetRecord(id);

        if (!row) {
            lua_pushnil(L);
            return 1;
        }

        const bool profession = row->m_categoryID == 0xB;

        if (CGPlayer_C* player = ClntObjMgrGetActivePlayerObj()) {
            const int32_t slot = player->GetSkillIndex(id);

            if (slot != -1) {
                const int32_t rank = static_cast<int32_t>(player->GetSkillValue(slot));
                const int32_t max = static_cast<int32_t>(player->GetSkillMaxRank(slot));
                const int32_t modifier = player->GetSkillModifier(slot);
                CoA::Script::PushStr(L, row->m_displayName);
                lua_pushinteger(L, rank);
                lua_pushinteger(L, modifier);
                lua_pushinteger(L, max);
                lua_pushboolean(L, profession);
                CoA::Script::PushStr(L, row->m_description);
                return 6;
            }
        }

        CoA::Script::PushStr(L, row->m_displayName);
        CoA::Script::PushNils(L, 3);
        lua_pushboolean(L, profession);
        CoA::Script::PushStr(L, row->m_description);
        return 6;
    }

    // WorldMapArea by AreaName, the first in id order; one string argument; an empty string finds nothing.
    int GetWorldMapAreaID(lua_State* L) {
        if (lua_gettop(L) != 1 || !lua_isstring(L, 1)) {
            lua_pushnil(L);
            return 1;
        }

        const std::string name = CoA::Script::CheckString(L, 1);

        if (!name.empty()) {
            for (int32_t id = g_worldMapAreaDB.m_minID; id <= g_worldMapAreaDB.m_maxID; ++id) {
                const WorldMapAreaRec* row = g_worldMapAreaDB.GetRecord(id);

                if (row && row->m_areaName && name == row->m_areaName) {
                    lua_pushinteger(L, row->m_id);
                    return 1;
                }
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // The first primary-stat marker aura the player unit has -> the stat number the original pairs with it; nil when
    // none.
    int GetUnitPrimaryStat(lua_State* L) {
        WOWGUID guid;

        if (Script_GetGUIDFromToken(CoA::Script::CheckString(L, 1), guid, false)) {
            if (CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(guid, TYPE_PLAYER, __FILE__, __LINE__))) {
                static const struct {
                    int32_t spell;
                    int32_t stat;
                } markers[] = { { 0xE913F, 1 }, { 0xE9140, 2 }, { 0xE9141, 3 }, { 0xE9142, 4 }, { 0xE914B, 6 } };

                for (const auto& m : markers) {
                    if (unit->HasAuraBySpellId(m.spell)) {
                        lua_pushinteger(L, m.stat);
                        return 1;
                    }
                }
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // Inbox item n: its body parsed as "%llX:%u:%u:%u:%u:%u:%u:%u" or "%llX:%u:%u:%u:%u:%u"; the quantity is the last
    // field. Nothing for an index past the inbox or a body that parses neither way.
    int GetInboxInvoiceQuantity(lua_State* L) {
        const uint32_t n = static_cast<uint32_t>(CoA::Script::ToInt(lua_tonumber(L, 1)));

        if (n == 0 || CGMailInfo::m_numInboxItems < n) {
            return 0;
        }

        const char* s = CGMailInfo::m_inbox.m_data[n - 1]->m_body;
        unsigned long long guid;
        unsigned a, b, c, d, e, f, qty;

        if (sscanf(s, "%llX:%u:%u:%u:%u:%u:%u:%u", &guid, &a, &b, &c, &d, &e, &f, &qty) != 8 &&
            sscanf(s, "%llX:%u:%u:%u:%u:%u", &guid, &a, &b, &c, &d, &qty) != 6) {
            return 0;
        }

        lua_pushinteger(L, static_cast<int32_t>(qty));
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetInboxInvoiceQuantity", GetInboxInvoiceQuantity),
        COA_NATIVE("", "GetSkillInfo", GetSkillInfo),
        COA_NATIVE("", "GetUnitPrimaryStat", GetUnitPrimaryStat),
        COA_NATIVE("", "GetWorldMapAreaID", GetWorldMapAreaID),
    };
}

void CoA::GlobalsEClient::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
