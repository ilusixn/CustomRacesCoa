// AscGlobalsC.cpp's natives over client objects: the player's position and melee haste, the party conversion and a
// spell's stack size through the player's modifiers.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/Object/ObjectClient/Item_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/PartyFrame.h"

// WoW/Source/WorldClient/Map.cpp (0xADFBC4).
extern int32_t s_mapId;

namespace {
    // x, y, z (the object's GetPosition, vtable +0x2C) and the client's map id.
    int GetCurrentPlayerPosition(lua_State* L) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        const int32_t map = s_mapId;

        if (!player) {
            return 0;
        }

        const C3Vector pos = player->GetPosition();
        lua_pushnumber(L, pos.x);
        lua_pushnumber(L, pos.y);
        lua_pushnumber(L, pos.z);
        lua_pushinteger(L, map);
        return 4;
    }

    // CMSG 0x525, only while the party has at most 4 members (CGPartyInfo::GetNumPartyMembers, 0x52BD10).
    int ConvertToParty(lua_State*) {
        if (static_cast<uint8_t>(CGPartyInfo::GetNumPartyMembers()) <= 4) {
            CoA::Packet(0x525).Send();
        }

        return 0;
    }

    // CumulativeAura (+0xC4) through the player's spell modifiers (Spell_C_GetSpellModifiers, SPELLMOD 0x1F).
    int GetSpellMaxStack(lua_State* L) {
        SpellRec rec;

        if (!g_spellDB.GetRecord(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)), &rec)) {
            return 0;
        }

        int32_t add = 0;
        int32_t pct = 100;
        Spell_C_GetSpellModifiers(&rec, 0x1F, add, pct);
        int32_t stack = rec.m_cumulativeAura;

        if (add != 0 || pct != 0) {
            stack = ((add + stack) * pct) / 100;
        }

        lua_pushinteger(L, stack);
        return 1;
    }

    // The main-hand weapon's delay (item cache +0xF4, 2000 without one) over UNIT_FIELD_BASEATTACKTIME.
    int GetMeleeHaste(lua_State* L) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player) {
            lua_pushnumber(L, 0);
            return 1;
        }

        const int32_t attackTime = static_cast<int32_t>(player->m_unit->m_unitFieldBaseattacktime[0]);
        int32_t delay = 2000;

        if (CGItem_C* item = static_cast<CGItem_C*>(ClntObjMgrObjectPtr(player->m_player->m_playerFieldInvSlotHead[15], TYPE_ITEM, __FILE__, __LINE__))) {
            if (ItemStats_C* rec = g_itemDBCache.GetRecord(static_cast<int32_t>(item->m_obj->m_entryID), WOWGUID(), nullptr, nullptr, false)) {
                delay = rec->m_delay;
            }
        }

        lua_pushnumber(L, static_cast<double>(static_cast<float>(delay) / static_cast<float>(attackTime)));
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "ConvertToParty", ConvertToParty),
        COA_NATIVE("", "GetCurrentPlayerPosition", GetCurrentPlayerPosition),
        COA_NATIVE("", "GetMeleeHaste", GetMeleeHaste),
        COA_NATIVE("", "GetSpellMaxStack", GetSpellMaxStack),
    };
}

void CoA::GlobalsCClient::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
