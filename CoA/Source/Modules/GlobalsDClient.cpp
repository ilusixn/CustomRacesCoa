// AscGlobalsD.cpp's natives over client objects: item icons, spell cooldowns / durations / absorbs, container and
// inventory GUIDs, quest completion and the Map / AreaTable name lookups.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/AreaTableRec.h"
#include "WoW/Source/DB/Rec/ItemDisplayInfoRec.h"
#include "WoW/Source/DB/Rec/ItemRec.h"
#include "WoW/Source/DB/Rec/MapRec.h"
#include "WoW/Source/DB/Rec/SpellDurationRec.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Object/ObjectClient/Container_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/UnitCombatLog_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/QuestLog.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include <string>
#include <vector>

namespace {
    using CoA::Script::CheckString;
    using CoA::Script::PushStr;
    using CoA::Script::ReadNumber;
    using CoA::Script::ToInt;

    void PushGuid(lua_State* L, const WOWGUID& guid) {   // FUN_10287550 -> GUIDToHexString (0x74D0D0)
        char buffer[32] = {};
        GUIDToHexString(guid, buffer);
        lua_pushstring(L, buffer);
    }

    // Item.dbc's display id (+0x14) -> ItemDisplayInfo (0x4CFD90), inventory icon (+0x14).
    int GetItemIconInstant(lua_State* L) {
        if (lua_gettop(L) == 1 && lua_isnumber(L, 1)) {
            if (ItemRec* item = g_itemDB.GetRecord(ToInt(lua_tonumber(L, 1)))) {
                ItemDisplayInfoRec display;

                if (g_itemDisplayInfoDB.GetRecord(item->m_displayInfoID, &display)) {
                    PushStr(L, display.m_inventoryIcon[0]);
                    return 1;
                }
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // max(RecoveryTime, CategoryRecoveryTime), then StartRecoveryTime (+0x238), as unsigned numbers.
    int GetSpellBaseCooldown(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(id), &rec)) {
            return CoA::Script::PushNils(L, 2);
        }

        uint32_t cd = static_cast<uint32_t>(rec.m_categoryRecoveryTime);

        if (cd < static_cast<uint32_t>(rec.m_recoveryTime)) {
            cd = static_cast<uint32_t>(rec.m_recoveryTime);
        }

        lua_pushnumber(L, static_cast<double>(cd));
        lua_pushnumber(L, static_cast<double>(static_cast<uint32_t>(rec.m_startRecoveryTime)));
        return 2;
    }

    // The SpellDuration row of DurationIndex: its base duration.
    int GetSpellBaseDuration(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        SpellRec rec;

        if (g_spellDB.GetRecord(static_cast<int32_t>(id), &rec)) {
            if (SpellDurationRec* row = g_spellDurationDB.GetRecord(rec.m_durationIndex)) {
                lua_pushinteger(L, row->m_duration);
                return 1;
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // SCHOOL_ABSORB (69): true and the first such effect's misc value; MANA_SHIELD (97): true.
    int IsAbsorbSpell(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(id), &rec)) {
            lua_pushnil(L);
            return 1;
        }

        for (int i = 0; i < 3; ++i) {
            if (rec.m_effectAura[i] == 0x45) {
                lua_pushboolean(L, 1);
                int32_t v;

                if (rec.m_effectAura[0] == 0x45) {
                    v = rec.m_effectMiscValue[0];
                } else if (rec.m_effectAura[1] == 0x45) {
                    v = rec.m_effectMiscValue[1];
                } else {
                    v = rec.m_effectAura[2] == 0x45 ? rec.m_effectMiscValue[2] : 0;
                }

                lua_pushinteger(L, v);
                return 2;
            }
        }

        for (int i = 0; i < 3; ++i) {
            if (rec.m_effectAura[i] == 0x61) {
                lua_pushboolean(L, 1);
                return 1;
            }
        }

        lua_pushboolean(L, 0);
        return 1;
    }

    // (bag, slot): bag 0 is the backpack (PLAYER_FIELD_PACK_SLOT), else the bag in the player's inventory slot 18 + bag
    // and its CONTAINER_FIELD_SLOT; bags outside 1..4 (the original reads past the inventory slots) give nil.
    int GetContainerItemGUID(lua_State* L) {
        if (CGPlayer_C* player = ClntObjMgrGetActivePlayerObj()) {
            const int32_t bag = ToInt(lua_tonumber(L, 1));
            const uint32_t slot = static_cast<uint32_t>(ToInt(lua_tonumber(L, 2)) - 1);

            if (slot < 0x24) {
                const WOWGUID* guid = nullptr;

                if (bag == 0) {
                    // Slots past the 16 backpack slots read on into the bank slots (contiguous), as the original.
                    guid = &(&player->m_player->m_playerFieldPackSlot1[0])[slot];
                } else if (bag > 0 && bag < 5) {
                    CGContainer_C* container = static_cast<CGContainer_C*>(ClntObjMgrObjectPtr(player->m_player->m_playerFieldInvSlotHead[18 + bag], TYPE_CONTAINER, __FILE__, __LINE__));

                    if (container) {
                        guid = &container->m_container->m_containerFieldSlot1[slot];
                    }
                }

                if (guid && guid->m_guid) {
                    PushGuid(L, *guid);
                    return 1;
                }
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // (unit, slot 1..22): a player's PLAYER_FIELD_INV_SLOT_HEAD.
    int GetInventoryItemGUID(lua_State* L) {
        const std::string token = CheckString(L, 1);
        const uint32_t slot = static_cast<uint32_t>(ToInt(lua_tonumber(L, 2)) - 1);

        if (slot < 0x16) {
            const WOWGUID guid = GetGuidByKeyword(token.c_str());
            CGObject_C* object = guid.m_guid ? ClntObjMgrObjectPtr(guid, TYPE_UNIT, __FILE__, __LINE__) : nullptr;

            if (object && object->m_typeId == ID_PLAYER) {
                const WOWGUID item = static_cast<CGPlayer_C*>(object)->m_player->m_playerFieldInvSlotHead[slot];

                if (item.m_guid) {
                    PushGuid(L, item);
                    return 1;
                }
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // In the world: the quest's log slot (descriptor +0x278), CGQuestLog::GetQuestIndex, IsQuestCompleteEx(index - 1, 1).
    int IsQuestComplete(lua_State* L) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (CGGameUI::s_inWorld && player) {
            const int32_t id = ToInt(lua_tonumber(L, 1));

            for (uint32_t i = 0; i < 0x19; ++i) {
                const int32_t q = static_cast<int32_t>(player->m_player->m_playerQuestLog[i].m_questId);

                if (q != 0 && q == id) {
                    const int32_t index = CGQuestLog::GetQuestIndex(id);
                    lua_pushboolean(L, static_cast<char>(CGQuestLog::IsQuestCompleteEx(static_cast<uint32_t>(index - 1), 1)) != 0);
                    return 1;
                }
            }
        }

        lua_pushboolean(L, 0);
        return 1;
    }

    // Map.dbc ids 0 .. max-1 whose MapName equals the argument exactly.
    int GetMapID(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TSTRING })) {
            return 0;
        }

        const std::string name = CheckString(L, 1);

        for (int32_t i = 0; i < g_mapDB.m_maxID; ++i) {
            if (MapRec* row = g_mapDB.GetRecord(i)) {
                if (row->m_mapName && name == row->m_mapName) {
                    lua_pushinteger(L, row->m_id);
                    return 1;
                }
            }
        }

        lua_pushnil(L);
        return 1;
    }

    int GetAreaIdByName(lua_State* L) {
        const std::string name = CheckString(L, 1);

        for (int32_t i = g_areaTableDB.m_minID; i < g_areaTableDB.m_maxID + 1; ++i) {
            if (AreaTableRec* row = g_areaTableDB.GetRecord(i)) {
                if (row->m_areaName && name == row->m_areaName) {
                    lua_pushinteger(L, row->m_id);
                    return 1;
                }
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // The names of the AreaTable rows with flag 0x2000, collected once (DAT_10d3d74b / DAT_10d3d7fc).
    int IsAreaHidden(lua_State* L) {
        static bool built = false;
        static std::vector<std::string> hidden;

        if (!built) {
            for (int32_t i = g_areaTableDB.m_minID; i < g_areaTableDB.m_maxID + 1; ++i) {
                if (AreaTableRec* row = g_areaTableDB.GetRecord(i)) {
                    if (row->m_flags & 0x2000) {
                        hidden.emplace_back(row->m_areaName ? row->m_areaName : "");
                    }
                }
            }

            built = true;
        }

        const std::string name = CheckString(L, 1);
        bool found = false;

        for (const std::string& s : hidden) {
            if (s == name) {
                found = true;
                break;
            }
        }

        lua_pushboolean(L, found);
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetAreaIdByName", GetAreaIdByName),
        COA_NATIVE("", "GetContainerItemGUID", GetContainerItemGUID),
        COA_NATIVE("", "GetInventoryItemGUID", GetInventoryItemGUID),
        COA_NATIVE("", "GetItemIconInstant", GetItemIconInstant),
        COA_NATIVE("", "GetMapID", GetMapID),
        COA_NATIVE("", "GetSpellBaseCooldown", GetSpellBaseCooldown),
        COA_NATIVE("", "GetSpellBaseDuration", GetSpellBaseDuration),
        COA_NATIVE("", "IsAbsorbSpell", IsAbsorbSpell),
        COA_NATIVE("", "IsAreaHidden", IsAreaHidden),
        COA_NATIVE("", "IsQuestComplete", IsQuestComplete),
    };
}

void CoA::GlobalsDClient::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
