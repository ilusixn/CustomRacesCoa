// AscSmallApis.cpp's natives over client objects: C_Sun, C_Item, C_QuestLog and C_Taxi's node ids; and the client's
// server-time offset for C_LoyaltyPass.
#include "CoA/Source/Modules/AttachFixesClient.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/ObjectAddon.h"
#include "CoA/Source/Modules/SmallApis.h"
#include "CoA/Source/Modules/SmallApisClient.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Shared.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/ChrRacesRec.h"
#include "WoW/Source/DB/Rec/TaxiNodesRec.h"
#include "WoW/Source/Object/ObjectClient/Item_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/UnitCombatLog_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/QuestLog.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include "WoW/Source/Ui/TaxiMapFrame.h"
#include "WoW/Source/WorldClient/DayNight.h"
#include "WoW/Source/WorldClient/DayNight__DNInfo.h"
#include <string>

namespace {
    using CoA::Script::CheckNumber;
    using CoA::Script::PushBool;
    using CoA::Script::PushInt;
    using CoA::Script::PushStr;
    using CoA::Script::ToInt;

    // ---- C_Sun (FUN_1026a540 / 790 / 9e0) -------------------------------------------------------------------------
    // FUN_1026c920 NOPs DayNight::SetDirection's three sun-direction stores for good, then the binding writes the
    // component. Called as C_Sun:SetX(v): exactly two arguments, and only in the world.
    bool s_sunFrozen = false;

    int SetSun(lua_State* L, float C3Vector::*component) {
        if (lua_gettop(L) != 2 || !CGGameUI::s_inWorld) {
            return 0;
        }

        const float v = static_cast<float>(CheckNumber(L, 2));
        s_sunFrozen = true;
        DayNight::g_dnInfo.m_light1.m_dir.*component = v;
        return 0;
    }

    int SetX(lua_State* L) {
        return SetSun(L, &C3Vector::x);
    }

    int SetY(lua_State* L) {
        return SetSun(L, &C3Vector::y);
    }

    int SetZ(lua_State* L) {
        return SetSun(L, &C3Vector::z);
    }

    // ---- C_Item (FUN_100c71c0 / FUN_100cac80) -----------------------------------------------------------------------
    // The argument is an item GUID string (StringToGuid, then the item object).
    CGItem_C* ItemFromGuidString(const char* text) {
        return static_cast<CGItem_C*>(ClntObjMgrObjectPtr(StringToGuid(text), TYPE_ITEM, __FILE__, __LINE__));
    }

    // CanBind: not bound (CGItem_C::IsBound, 0x708520) and the item cache record's bonding set.
    int CanBind(lua_State* L) {
        std::string text;

        if (!CoA::Script::ReadString(L, text)) {
            return 0;
        }

        CGItem_C* item = ItemFromGuidString(text.c_str());
        ItemStats_C* rec = item ? g_itemDBCache.GetRecord(static_cast<int32_t>(item->m_obj->m_entryID), WOWGUID(), nullptr, nullptr, false) : nullptr;

        if (!rec) {
            lua_pushnil(L);
        } else {
            PushBool(L, !item->IsBound() && rec->m_bonding != 0);
        }

        return 1;
    }

    int IsBound(lua_State* L) {
        std::string text;

        if (!CoA::Script::ReadString(L, text)) {
            return 0;
        }

        CGItem_C* item = ItemFromGuidString(text.c_str());

        if (!item) {
            lua_pushnil(L);
        } else {
            PushBool(L, item->IsBound());
        }

        return 1;
    }

    // FUN_100ca700: the item's addon field 0 (FUN_1027f1f0 -> FUN_102cdbb0).
    int GetScalingLevel(lua_State* L) {
        std::string text;

        if (!CoA::Script::ReadString(L, text)) {
            return 0;
        }

        CGItem_C* item = ItemFromGuidString(text.c_str());

        if (!item) {
            lua_pushnil(L);
            return 1;
        }

        PushInt(L, static_cast<int32_t>(CoA::ObjectAddon::Item(item->m_obj->m_guid.m_guid)[0].value));
        return 1;
    }

    // ---- C_QuestLog ---------------------------------------------------------------------------------------------------
    QuestCache* QuestRec(uint32_t id) {
        return g_questDBCache.GetRecord(static_cast<int32_t>(id), WOWGUID(), nullptr, nullptr, false);
    }

    CreatureStats_C* CreatureRec(uint32_t id) {
        return g_creatureDBCache.GetRecord(static_cast<int32_t>(id), WOWGUID(), nullptr, nullptr, false);
    }

    // FUN_1021ae60: the quest's flags (+0x50): 0x400000 -> 2, 0x800000 -> 4, else 1.
    int GetQuestType(lua_State* L) {
        QuestCache* rec = QuestRec(static_cast<uint32_t>(ToInt(CheckNumber(L, 1))));
        int type = 1;

        if (rec) {
            if (rec->m_flags & 0x400000) {
                type = 2;
            } else if (rec->m_flags & 0x800000) {
                type = 4;
            }
        }

        PushInt(L, type);
        return 1;
    }

    // FUN_101940a0: every open logged quest (the client's list, not a header) with its slot in the player's log.
    template <class F>
    bool ForOpenLoggedQuest(CGPlayer_C* player, F fn) {
        for (int32_t i = 0; i < CGQuestLog::m_numQuest; ++i) {
            const QuestLogEntry& e = CGQuestLog::m_quests[i];

            if (e.m_isHeader) {
                continue;
            }

            for (uint32_t slot = 0; slot < 25; ++slot) {
                const PlayerQuestLogEntry& logged = player->m_player->m_playerQuestLog[slot];
                const uint32_t id = static_cast<uint16_t>(logged.m_questId);

                if (!id) {
                    break;
                }

                if (id == static_cast<uint32_t>(e.m_questId)) {
                    if (fn(id, logged)) {
                        return true;
                    }

                    break;
                }
            }
        }

        return false;
    }

    // FUN_1008dc50: an open logged quest needs `creature` killed and the count is not reached; *quest is set for every
    // matching objective.
    bool NeedsKill(CGPlayer_C* player, uint32_t creature, uint32_t* quest) {
        if (!CreatureRec(creature)) {
            return false;
        }

        return ForOpenLoggedQuest(player, [&](uint32_t id, const PlayerQuestLogEntry& slot) {
            QuestCache* q = QuestRec(id);

            if (!q) {
                return false;
            }

            for (uint32_t i = 0; i < 4; ++i) {
                if (static_cast<int32_t>(q->m_objectiveId[i]) == static_cast<int32_t>(creature)) {
                    if (quest) {
                        *quest = id;
                    }

                    if (q->m_objectiveRequiredCount[i] != slot.m_counts[i]) {
                        return true;
                    }
                }
            }

            return false;
        });
    }

    // FUN_1008da60: an open logged quest needs one of the creature's quest items and the bags hold a different count.
    bool NeedsItem(CGPlayer_C* player, uint32_t creature, uint32_t* quest) {
        CreatureStats_C* c = CreatureRec(creature);

        if (!c) {
            return false;
        }

        return ForOpenLoggedQuest(player, [&](uint32_t id, const PlayerQuestLogEntry&) {
            QuestCache* q = QuestRec(id);

            if (!q) {
                return false;
            }

            for (uint32_t j = 0; j < 6; ++j) {
                for (uint32_t u = 0; u < 6; ++u) {
                    const uint32_t item = c->m_questItem[j];

                    if (item != 0 && item == q->m_collectItemId[u]) {
                        if (quest) {
                            *quest = id;
                        }

                        if (static_cast<int32_t>(q->m_collectItemCount[u]) != CoA::AttachFixesClient::CountCarried(player, item)) {
                            return true;
                        }
                    }
                }
            }

            return false;
        });
    }

    // FUN_1021ac30: (unit token). A unit with a quest-giver state: its name, -1 and its marker model's file name.
    // Otherwise its creature entry (the cache record's id, +0x6C) against the quest log: "objective" or "collect" and
    // the quest id. Else nil.
    int GetUnitQuestInfo(lua_State* L) {
        const char* s = lua_tolstring(L, 1, nullptr);
        const std::string token = s ? s : "";
        CGPlayer_C* player = token.empty() ? nullptr : ClntObjMgrGetActivePlayerObj();

        if (player) {
            const WOWGUID guid = GetGuidByKeyword(token.c_str());
            CGUnit_C* unit = guid.m_guid ? static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(guid, TYPE_UNIT, __FILE__, __LINE__)) : nullptr;

            if (unit) {
                const uint32_t state = unit->m_questStatus;

                if (state != 0) {
                    const char* name;

                    switch (state) {
                        case 1: name = "unavailable"; break;
                        case 2: name = "trivial"; break;
                        case 5: name = "incomplete"; break;
                        case 6: case 9: case 10: name = "handin"; break;
                        case 8: name = "pickup"; break;
                        default: name = ""; break;
                    }

                    PushStr(L, name);
                    lua_pushinteger(L, -1);
                    CM2Model* icon = unit->m_interactIcon;
                    PushStr(L, icon && icon->m_shared ? icon->m_shared->m_fileName : nullptr);
                    return 3;
                }

                const uint32_t entry = unit->m_obj->m_entryID;

                if (CreatureRec(entry)) {
                    uint32_t quest = 0;
                    const char* kind = nullptr;

                    if (NeedsKill(player, entry, &quest)) {
                        kind = "objective";
                    } else if (NeedsItem(player, entry, &quest)) {
                        kind = "collect";
                    }

                    if (kind) {
                        PushStr(L, kind);
                        lua_pushinteger(L, static_cast<int>(quest));
                        return 2;
                    }
                }
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // FUN_1021af00: the player's race -> ChrRaces +0x1C (1 -> 3, 7 -> 2, else 0); the first ZoneStory row (ids min
    // .. max-1: the max id is never visited) for the zone whose +4 is 0 or that side: +8 and the string at +0x10.
    int GetZoneStoryInfo(lua_State* L) {
        if (CGPlayer_C* player = ClntObjMgrGetActivePlayerObj()) {
            const int32_t zone = ToInt(CheckNumber(L, 1));
            const uint8_t race = (player->m_typeId == ID_UNIT || player->m_typeId == ID_PLAYER) ? player->m_unit->m_unitFieldBytes0[0] : 0;
            int32_t side = 0;

            if (ChrRacesRec* chrRace = g_chrRacesDB.GetRecord(race)) {
                side = chrRace->m_baseLanguage == 1 ? 3 : chrRace->m_baseLanguage == 7 ? 2 : 0;
            }

            CoA::Dbc::Table& t = CoA::Dbc::Get("DBFilesClient/ZoneStory.dbc");

            for (uint32_t id = t.MinId(); t.Loaded() && id < t.MaxId(); ++id) {
                const uint8_t* row = t.Row(id);

                if (!row || CoA::Dbc::Table::I32(row, 0xC) != zone) {
                    continue;
                }

                const int32_t f = CoA::Dbc::Table::I32(row, 4);

                if (f != 0 && f != side) {
                    continue;
                }

                PushInt(L, CoA::Dbc::Table::I32(row, 8));
                PushStr(L, t.Str(row, 0x10));
                return 2;
            }
        }

        return CoA::Script::PushNils(L, 2);
    }

    // ---- C_Taxi (FUN_10217ce0 / d20) ------------------------------------------------------------------------------------
    int GetCurrentTaxiNodeID(lua_State* L) {
        PushInt(L, TaxiMapCurrentNodeId());
        return 1;
    }

    // The 1-based taxi map slot's TaxiNodes row id (CGTaxiMap::m_nodes, m_numNodes); nil without one.
    int GetTaxiNodeID(lua_State* L) {
        const uint32_t slot = static_cast<uint32_t>(ToInt(CheckNumber(L, 1))) - 1;

        if (slot < static_cast<uint32_t>(CGTaxiMap::m_numNodes) && slot < CGTaxiMap::m_nodes.Count()) {
            if (TaxiNodesRec* node = CGTaxiMap::m_nodes.m_data[slot].m_rec) {
                PushInt(L, node->m_id);
                return 1;
            }
        }

        lua_pushnil(L);
        return 1;
    }

    int32_t TimeOffset() {
        return CGQuestLog::m_timeOffset;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_Sun", "SetX", SetX),
        COA_NATIVE("C_Sun", "SetY", SetY),
        COA_NATIVE("C_Sun", "SetZ", SetZ),
        COA_NATIVE("C_Item", "CanBind", CanBind),
        COA_NATIVE("C_Item", "IsBound", IsBound),
        COA_NATIVE("C_Item", "GetScalingLevel", GetScalingLevel),
        COA_NATIVE("C_QuestLog", "GetQuestType", GetQuestType),
        COA_NATIVE("C_QuestLog", "GetUnitQuestInfo", GetUnitQuestInfo),
        COA_NATIVE("C_QuestLog", "GetZoneStoryInfo", GetZoneStoryInfo),
        COA_NATIVE("C_Taxi", "GetCurrentTaxiNodeID", GetCurrentTaxiNodeID),
        COA_NATIVE("C_Taxi", "GetTaxiNodeID", GetTaxiNodeID),
    };
}

bool CoA::SmallApisClient::SunFrozen() {
    return s_sunFrozen;
}

void CoA::SmallApisClient::Init() {
    CoA::SmallApis::SetTimeOffsetSource(TimeOffset);
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
