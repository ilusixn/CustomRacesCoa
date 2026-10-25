// AscGlobalsB.cpp's client natives: the glue-state LoadWDBCache / SaveWDBCache and the world natives over the client's
// caches, DBC tables and the player's descriptors. A null cache record returns NO values (the original's null in eax
// is the return count); a missing DBC row pushes nil.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/Frame/CSimpleMovieFrame.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/AchievementRec.h"
#include "WoW/Source/DB/Rec/AreaTableRec.h"
#include "WoW/Source/DB/Rec/ItemRec.h"
#include "WoW/Source/DB/Rec/MapRec.h"
#include "WoW/Source/DB/Rec/SkillLineAbilityRec.h"
#include "WoW/Source/DB/Rec/SkillLineRec.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/UnitCombatLog_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/QuestLog.h"
#include <cmath>

namespace {
    using CoA::Script::CheckNumber;
    using CoA::Script::PushBool;
    using CoA::Script::PushStr;
    using CoA::Script::ReadNumber;
    using CoA::Script::ToInt;

    int32_t ArgInt(lua_State* L, int idx) {
        return ToInt(CheckNumber(L, idx));
    }

    ItemStats_C* ItemRecord(uint32_t id) {   // 0x67CA30 on the item cache
        return g_itemDBCache.GetRecord(static_cast<int32_t>(id), WOWGUID(), nullptr, nullptr, false);
    }

    QuestCache* QuestRecord(uint32_t id) {   // 0x67DE90 on the quest cache
        return g_questDBCache.GetRecord(static_cast<int32_t>(id), WOWGUID(), nullptr, nullptr, false);
    }

    int LoadWDBCache(lua_State*) {   // DBCache_Initialize (0x635520)
        DBCache_Initialize();
        return 0;
    }

    int SaveWDBCache(lua_State*) {   // DBCache_Destroy (0x635540)
        DBCache_Destroy();
        return 0;
    }

    int GetItemSetID(lua_State* L) {
        ItemStats_C* rec = ItemRecord(static_cast<uint32_t>(ArgInt(L, 1)));
        lua_pushinteger(L, rec ? static_cast<int32_t>(rec->m_itemSet) : 0);
        return 1;
    }

    int GetItemTemplateBonding(lua_State* L) {
        ItemStats_C* rec = ItemRecord(static_cast<uint32_t>(ArgInt(L, 1)));

        if (!rec) {
            return 0;
        }

        lua_pushinteger(L, static_cast<int32_t>(rec->m_bonding));
        return 1;
    }

    int GetItemWeaponSpeed(lua_State* L) {
        double v = 0.0;

        if (lua_gettop(L) == 1 && lua_isnumber(L, 1)) {
            if (ItemStats_C* rec = ItemRecord(static_cast<uint32_t>(ArgInt(L, 1)))) {
                v = static_cast<double>(rec->m_delay);
            }
        }

        lua_pushnumber(L, v);
        return 1;
    }

    int GetQuestScaling(lua_State* L) {
        QuestCache* rec = QuestRecord(static_cast<uint32_t>(ArgInt(L, 1)));

        if (!rec) {
            return 0;
        }

        PushBool(L, ((rec->m_flags >> 24) & 1) != 0);
        return 1;
    }

    int GetTitleForQuestID(lua_State* L) {   // GetQuestTitleText (0x5DEC70)
        PushStr(L, GetQuestTitleText(ArgInt(L, 1)));
        return 1;
    }

    int CanShowAchievementToast(lua_State* L) {
        AchievementRec* row = g_achievementDB.GetRecord(ArgInt(L, 1));
        PushBool(L, row && (row->m_flags & 0x400) == 0);
        return 1;
    }

    ItemRec* ItemRow(lua_State* L) {
        return lua_gettop(L) == 1 && lua_isnumber(L, 1) ? g_itemDB.GetRecord(ArgInt(L, 1)) : nullptr;
    }

    int GetItemClassID(lua_State* L) {
        if (ItemRec* row = ItemRow(L)) {
            lua_pushinteger(L, row->m_classID);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetItemSubClassID(lua_State* L) {
        if (ItemRec* row = ItemRow(L)) {
            lua_pushinteger(L, row->m_subclassID);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetItemInventoryType(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        if (ItemRec* row = g_itemDB.GetRecord(static_cast<int32_t>(id))) {
            lua_pushinteger(L, row->m_inventoryType);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetActiveMapName(lua_State* L) {
        if (MapRec* row = g_mapDB.GetRecord(CGGameUI::m_iCurrentMapID)) {
            PushStr(L, row->m_mapName);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetMapName(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        if (MapRec* row = g_mapDB.GetRecord(static_cast<int32_t>(id))) {
            PushStr(L, row->m_mapName);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetAreaName(lua_State* L) {
        if (AreaTableRec* row = g_areaTableDB.GetRecord(ArgInt(L, 1))) {
            PushStr(L, row->m_areaName);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    // The player's keyring slots 1..31 (descriptor +0x7B8 + slot * 8), as GUIDToHexString (0x74D0D0).
    int GetKeyringItemGUID(lua_State* L) {
        if (CGPlayer_C* player = ClntObjMgrGetActivePlayerObj()) {
            const int32_t slot = ArgInt(L, 1);

            if (static_cast<uint32_t>(slot - 1) < 0x1F) {
                const WOWGUID guid = player->m_player->m_playerFieldKeyringSlot1[slot - 1];

                if (guid.m_guid) {
                    char buffer[32] = {};
                    GUIDToHexString(guid, buffer);
                    lua_pushstring(L, buffer);
                    return 1;
                }
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // The quest's slot in the player's log (descriptor +0x278, 25 x 0x14), then CGQuestLog::GetQuestIndex (0x5DEEB0).
    int GetQuestLogIndexByID(lua_State* L) {
        const int32_t quest = ArgInt(L, 1);
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player) {
            return 0;
        }

        for (uint32_t i = 0; i < 25; ++i) {
            const int32_t q = static_cast<int32_t>(player->m_player->m_playerQuestLog[i].m_questId);

            if (q != 0 && q == quest) {
                lua_pushinteger(L, CGQuestLog::GetQuestIndex(quest));
                return 1;
            }
        }

        lua_pushinteger(L, 0);
        return 1;
    }

    // |1 - UNIT_MOD_CAST_SPEED| x 100, rounded up, never negative.
    int GetSpellHaste(lua_State* L) {
        double v = 0.0;

        if (CGPlayer_C* player = ClntObjMgrGetActivePlayerObj()) {
            const float mod = player->m_unit->m_unitModCastSpeed;
            const int32_t i = static_cast<int32_t>(std::ceil(static_cast<double>(std::fabs(1.0f - mod) * 100.0f)));
            v = static_cast<double>(i < 0 ? 0 : i);
        }

        lua_pushnumber(L, v);
        return 1;
    }

    int IsPassiveSpellID(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        SpellRec rec;
        const bool found = g_spellDB.GetRecord(static_cast<int32_t>(id), &rec) != 0;
        PushBool(L, found && ((static_cast<uint32_t>(rec.m_attributes) >> 6) & 1) != 0);
        return 1;
    }

    // CastSpecialSpell(spell): a profession spell (an effect 0xA2, or a SkillLineAbility row whose SkillLine category
    // is 11) is cast as CMSG_CAST_SPELL 0x12E with the DLL's own cast counter and the DLL-global target guid
    // DAT_10bdb230 (never written: empty).
    uint8_t s_castCount = 0;   // DAT_10bde438

    int CastSpecialSpell(lua_State* L) {
        const uint32_t spell = static_cast<uint32_t>(ArgInt(L, 1));
        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec)) {
            return 0;
        }

        bool special = false;

        for (uint32_t i = 0; i < 3 && !special; ++i) {
            special = rec.m_effect[i] == 0xA2;
        }

        for (int32_t id = g_skillLineAbilityDB.m_minID; !special && id <= g_skillLineAbilityDB.m_maxID; ++id) {
            SkillLineAbilityRec* sla = g_skillLineAbilityDB.GetRecord(id);

            if (!sla || static_cast<uint32_t>(sla->m_spell) != spell) {
                continue;
            }

            SkillLineRec* line = g_skillLineDB.GetRecord(sla->m_skillLine);
            special = line && line->m_categoryID == 0xB;
        }

        if (!special) {
            return 0;
        }

        // The packed guid of the empty target: mask byte 0.
        CoA::Packet(0x12E).U8(s_castCount).U32(spell).U8(0).U32(0).U8(0).Send();
        s_castCount = static_cast<uint8_t>(s_castCount % 0xFF + 1);
        return 0;
    }

    // MovieFrame:SetVolume(v) (FUN_101aeca0): v clamped to [0, 1], then SESound::SetVolume (0x879710). The original
    // calls it with the frame as `this`, so the sound's m_internal is read from the frame's vtable slot and the
    // write lands in the client's code; here it goes to the frame's own sound (CSimpleMovieFrame::m_sound, +0x29C).
    int SetVolume(lua_State* L) {
        lua_rawgeti(L, 1, 0);
        CSimpleMovieFrame* frame = static_cast<CSimpleMovieFrame*>(lua_touserdata(L, -1));
        lua_settop(L, -2);
        float v = static_cast<float>(CheckNumber(L, 2));

        if (v < 0.0f) {
            v = 0.0f;
        }

        if (v > 1.0f) {
            v = 1.0f;
        }

        if (frame) {
            frame->m_sound.SetVolume(v);
        }

        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "LoadWDBCache", LoadWDBCache),
        COA_NATIVE("", "SaveWDBCache", SaveWDBCache),
        COA_NATIVE("", "CanShowAchievementToast", CanShowAchievementToast),
        COA_NATIVE("", "CastSpecialSpell", CastSpecialSpell),
        COA_NATIVE("", "GetActiveMapName", GetActiveMapName),
        COA_NATIVE("", "GetAreaName", GetAreaName),
        COA_NATIVE("", "GetItemClassID", GetItemClassID),
        COA_NATIVE("", "GetItemInventoryType", GetItemInventoryType),
        COA_NATIVE("", "GetItemSetID", GetItemSetID),
        COA_NATIVE("", "GetItemSubClassID", GetItemSubClassID),
        COA_NATIVE("", "GetItemTemplateBonding", GetItemTemplateBonding),
        COA_NATIVE("", "GetItemWeaponSpeed", GetItemWeaponSpeed),
        COA_NATIVE("", "GetKeyringItemGUID", GetKeyringItemGUID),
        COA_NATIVE("", "GetMapName", GetMapName),
        COA_NATIVE("", "GetQuestLogIndexByID", GetQuestLogIndexByID),
        COA_NATIVE("", "GetQuestScaling", GetQuestScaling),
        COA_NATIVE("", "GetSpellHaste", GetSpellHaste),
        COA_NATIVE("", "GetTitleForQuestID", GetTitleForQuestID),
        COA_NATIVE("", "IsPassiveSpellID", IsPassiveSpellID),
        COA_NATIVE("#MovieFrame", "SetVolume", SetVolume),
    };
}

void CoA::GlobalsBClient::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
