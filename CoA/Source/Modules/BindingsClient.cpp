// AscBindings.cpp's natives over client objects (the rest are in Bindings.cpp): the glue-state SendWindowsFlash,
// SetPortraitToTexture and FillLocalizedClassList, and the world's gossip quests, client-side auras, camera, item set,
// trade skill, aura and unit-name natives.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Os/Gui.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/ItemRec.h"
#include "WoW/Source/DB/Rec/ItemSetRec.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/UIUtil/Camera.h"
#include "WoW/Source/Ui/GossipInfo.h"
#include "WoW/Source/Ui/GuildInfo.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include "WoW/Source/Ui/QuestLog.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"
#include <Windows.h>
#include <algorithm>
#include <string>
#include <vector>

// The client's own natives the DLL also registers on the glue state (GameUI_Scripts.cpp / ScriptEvents_Scripts.cpp).
int32_t Script_SetPortraitToTexture(lua_State* L);
int32_t Script_FillLocalizedClassList(lua_State* L);

namespace {
    // handler_SendWindowsFlash: the game window (OsGuiGetWindow(0)), FlashWindow(hwnd, FALSE).
    int SendWindowsFlash(lua_State*) {
        if (HWND hwnd = static_cast<HWND>(OsGuiGetWindow(0))) {
            FlashWindow(hwnd, FALSE);
        }

        return 0;
    }

    using CoA::Script::CheckNumber;
    using CoA::Script::PushBool;
    using CoA::Script::PushInt;
    using CoA::Script::PushStr;
    using CoA::Script::ToInt;

    // ---- gossip --------------------------------------------------------------------------------------------------
    // The client's available / active gossip-quest slots (0..0xFD); completableOnly keeps the quests whose log entry
    // is complete (CGQuestLog::GetQuestIndex, IsQuestCompleteEx(index - 1, 1)).
    std::vector<uint32_t> GossipQuests(bool active, bool completableOnly) {
        std::vector<uint32_t> ids;

        for (int32_t i = 0; i < 0xFE; ++i) {
            GossipQuestInfo* q = active ? CGGossipInfo::GetActiveQuest(i) : CGGossipInfo::GetAvailableQuest(i);

            if (!q) {
                break;
            }

            if (completableOnly) {
                const int32_t index = CGQuestLog::GetQuestIndex(static_cast<int32_t>(q->m_questID));

                if (index - 1 < 0 || !(CGQuestLog::IsQuestCompleteEx(static_cast<uint32_t>(index - 1), 1) & 0xFF)) {
                    continue;
                }
            }

            ids.push_back(q->m_questID);
        }

        return ids;
    }

    // CMSG_QUESTGIVER_ACCEPT_QUEST (0x189) / _COMPLETE_QUEST (0x18E) { u64 npc, u32 quest, u32 0 }.
    void SendQuestgiver(uint32_t opcode, uint32_t quest) {
        CoA::Packet(opcode).U64(CGGossipInfo::m_gossip.m_guid).U32(quest).U32(0).Send();
    }

    int AcceptAllAvailableGossipQuests(lua_State*) {
        for (uint32_t id : GossipQuests(false, false)) {
            SendQuestgiver(0x189, id);
        }

        return 0;
    }

    int CompleteAllActiveGossipQuests(lua_State*) {
        for (uint32_t id : GossipQuests(true, true)) {
            SendQuestgiver(0x18E, id);
        }

        return 0;
    }

    // One numeric argument; sent only when that quest is in the matching list.
    bool QuestArg(lua_State* L, uint32_t& id) {
        if (lua_gettop(L) != 1 || !lua_isnumber(L, 1)) {
            return false;
        }

        id = static_cast<uint32_t>(ToInt(lua_tonumber(L, 1)));
        return true;
    }

    bool Contains(const std::vector<uint32_t>& v, uint32_t id) {
        return std::find(v.begin(), v.end(), id) != v.end();
    }

    int AcceptAvailableGossipQuest(lua_State* L) {
        uint32_t id;

        if (QuestArg(L, id) && Contains(GossipQuests(false, false), id)) {
            SendQuestgiver(0x189, id);
        }

        return 0;
    }

    int CompleteActiveGossipQuest(lua_State* L) {
        uint32_t id;

        if (QuestArg(L, id) && Contains(GossipQuests(true, true), id)) {
            SendQuestgiver(0x18E, id);
        }

        return 0;
    }

    // FUN_100d8740 / FUN_100d8ae0 / FUN_100d9290: the ids as a 1-based table; nothing when empty.
    int PushIdList(lua_State* L, const std::vector<uint32_t>& ids) {
        if (ids.empty()) {
            return 0;
        }

        lua_createtable(L, 0, 0);

        for (size_t i = 0; i < ids.size(); ++i) {
            lua_pushinteger(L, static_cast<int>(i + 1));
            lua_pushinteger(L, static_cast<int>(ids[i]));
            lua_settable(L, -3);
        }

        return 1;
    }

    int GetActiveGossipQuestIds(lua_State* L) {
        return PushIdList(L, GossipQuests(true, false));
    }

    int GetAvailableGossipQuestIds(lua_State* L) {
        return PushIdList(L, GossipQuests(false, false));
    }

    int GetCompleteGossipQuestIds(lua_State* L) {
        return PushIdList(L, GossipQuests(true, true));
    }

    // ---- client-side auras -----------------------------------------------------------------------------------------
    // DAT_10bdb230/4: a DLL-global guid that is never written -- the empty guid.
    const uint64_t kNoCaster = 0;

    // FUN_100de1a0: a complete SMSG_AURA_UPDATE (0x496) the server is asked to echo back (CMSG_ECHO 0x532). Flags: 0x88
    // base (no caster | negative), +0x20 (duration sent) unless DurationIndex is 21 or AttributesExE bit 10 is set, one
    // bit per non-zero Effect.
    void ApplyClientsideAura(uint32_t spellId, uint8_t slot, uint64_t target, uint64_t caster) {
        SpellRec rec;

        if (!g_spellDB.GetRecord(static_cast<int32_t>(spellId), &rec)) {
            return;
        }

        uint8_t flags = 0x88;

        if (rec.m_durationIndex != 0x15) {
            flags = static_cast<uint8_t>((((static_cast<uint32_t>(rec.m_attributesExE) >> 10) & 1) ^ 1) * 0x20 + 0x88);
        }

        if (rec.m_effect[0]) {
            flags |= 1;
        }

        if (rec.m_effect[1]) {
            flags |= 2;
        }

        if (rec.m_effect[2]) {
            flags |= 4;
        }

        CoA::Packet p(0x532);
        p.U16(0x496).PackedGuid(target).U8(slot).U32(spellId).U8(flags).U8(static_cast<uint8_t>(rec.m_spellLevel)).U8(1);

        if (!(flags & 8)) {
            p.PackedGuid(caster);
        }

        if (flags & 0x20) {
            p.U32(5000).U32(5000);
        }

        p.Send();
    }

    // FUN_100d5400(L, target, caster): arg1 = apply?, arg2 = slot (stored as 0xFE - n), arg3 = spell. Removal is the
    // same SMSG_AURA_UPDATE with spell 0.
    void ClientsideAuraOn(lua_State* L, uint64_t target, uint64_t caster) {
        const int apply = lua_toboolean(L, 1);
        const uint8_t slot = static_cast<uint8_t>(-2 - static_cast<int8_t>(ToInt(CheckNumber(L, 2))));

        if (apply) {
            ApplyClientsideAura(static_cast<uint32_t>(ToInt(CheckNumber(L, 3))), slot, target, caster);
            return;
        }

        CoA::Packet(0x532).U16(0x496).PackedGuid(target).U8(slot).U32(0).Send();
    }

    int ClientsideAura(lua_State* L) {
        ClientsideAuraOn(L, ClntObjMgrGetActivePlayer().m_guid, kNoCaster);
        return 0;
    }

    // On the player's current target (UNIT_FIELD_TARGET).
    int ClientsideAuraTarget(lua_State* L) {
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player) {
            return 0;
        }

        ClientsideAuraOn(L, player->m_unit->m_unitFieldTarget.m_guid, kNoCaster);
        return 0;
    }

    // FUN_100d0160: every visible UNIT whose guid carries the creature entry given in arg 4, with the player as caster.
    // Guids whose high word is a player / item / pet-less type carry no entry (0).
    int32_t AuraCreatureVisit(WOWGUID guid, void* param) {
        lua_State* L = static_cast<lua_State*>(param);

        if (!ClntObjMgrObjectPtr(guid, TYPE_UNIT, __FILE__, __LINE__)) {
            return 1;
        }

        const uint32_t want = static_cast<uint32_t>(ToInt(CheckNumber(L, 4)));
        const uint32_t lo = static_cast<uint32_t>(guid.m_guid);
        const uint32_t hi = static_cast<uint32_t>(guid.m_guid >> 32);
        uint32_t entry = 0;

        if (hi > 0x0FFFFFFF) {
            const uint32_t t = hi >> 16;
            const bool noEntry = t == 0xF101 || t == 0x1FC0 || t == 0 || t == 8000 || t == 0x1F50 || t == 0x4000 || t == 0xF100;

            if (!noEntry) {
                entry = (lo >> 24) | ((hi & 0xFFFF) << 8);
            }
        }

        if (entry == want) {
            ClientsideAuraOn(L, guid.m_guid, ClntObjMgrGetActivePlayer().m_guid);
        }

        return 1;
    }

    int ClientsideAuraCreature(lua_State* L) {
        ClntObjMgrEnumVisibleObjects(AuraCreatureVisit, L);
        return 0;
    }

    // ---- client passthroughs -----------------------------------------------------------------------------------------
    // The client's own CMSG_GUILD_PERMISSIONS sender (CGGuildInfo::QueryPermissions, 0x5CAA40).
    int GuildQueryPermissions(lua_State*) {
        CGGuildInfo::QueryPermissions();
        return 0;
    }

    // The active camera's field of view (CGWorldFrame::GetActiveCamera, 0x4F5960).
    int SetCameraFoV(lua_State* L) {
        if (CGCamera* camera = CGWorldFrame::GetActiveCamera()) {
            camera->m_fov = static_cast<float>(CheckNumber(L, 1));
        }

        return 0;
    }

    // The client's ItemSet.dbc name.
    int GetSetName(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        ItemSetRec* row = g_itemSetDB.GetRecord(static_cast<int32_t>(id));

        if (!row) {
            lua_pushnil(L);
            return 1;
        }

        PushStr(L, row->m_name);
        return 1;
    }

    // ---- C_TradeSkill -------------------------------------------------------------------------------------------------
    // For each CREATE_ITEM (24), CREATE_ITEM_2 (157) or CREATE_RANDOM_ITEM (59) effect, Spell_C_GetMinMaxPoints gives the
    // count. The original never settable()s the per-effect table into the outer one, so what it returns is the top of
    // the stack: the last effect's {ItemID, MinCount, MaxCount}. Reproduced as-is.
    int GetCraftedItem(lua_State* L) {
        SpellRec rec;
        int found = 0;

        if (g_spellDB.GetRecord(ToInt(CheckNumber(L, 1)), &rec)) {
            lua_createtable(L, 0, 0);
            lua_checkstack(L, 2);

            for (uint32_t i = 0; i < 3; ++i) {
                const int32_t effect = rec.m_effect[i];

                if (effect != 0x18 && effect != 0x9D && effect != 0x3B) {
                    continue;
                }

                float mn = 0;
                float mx = 0;
                Spell_C_GetMinMaxPoints(&rec, i, &mn, &mx, 0, 0, 0, 0);
                const uint32_t lo = static_cast<uint32_t>(mn);
                const uint32_t hi = static_cast<uint32_t>(mx);
                PushInt(L, static_cast<int32_t>(i + 1));
                lua_createtable(L, 0, 0);
                lua_checkstack(L, 2);
                CoA::Script::SetField(L, "ItemID", rec.m_effectItemType[i]);
                CoA::Script::SetField(L, "MinCount", static_cast<int32_t>(lo > 1 ? lo : 1));
                CoA::Script::SetField(L, "MaxCount", static_cast<int32_t>(hi > 1 ? hi : 1));
                ++found;
            }

            if (found) {
                return 1;
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // {[i] = {ItemID, Count}} for each non-zero Reagent[i], or nil.
    int GetReagentItems(lua_State* L) {
        SpellRec rec;
        int found = 0;

        if (g_spellDB.GetRecord(ToInt(CheckNumber(L, 1)), &rec)) {
            lua_createtable(L, 0, 0);
            lua_checkstack(L, 2);

            for (uint32_t i = 0; i < 8; ++i) {
                if (!rec.m_reagent[i]) {
                    continue;
                }

                PushInt(L, static_cast<int32_t>(i + 1));
                lua_createtable(L, 0, 0);
                lua_checkstack(L, 2);
                CoA::Script::SetField(L, "ItemID", rec.m_reagent[i]);
                CoA::Script::SetField(L, "Count", rec.m_reagentCount[i]);
                lua_settable(L, -3);
                ++found;
            }

            if (found) {
                return 1;
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // ---- C_Aura.UnitHasAura -------------------------------------------------------------------------------------------
    // FUN_10326dc0: {string, number}; a player or unit object, then CGUnit_C::HasAuraBySpellId. Nothing for a bad unit.
    int UnitHasAura(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TSTRING, LUA_TNUMBER })) {
            return 0;
        }

        const std::string token = CoA::Script::CheckString(L, 1);
        const int32_t spellId = ToInt(CheckNumber(L, 2));
        WOWGUID guid;

        if (!Script_GetGUIDFromToken(token.c_str(), guid, false)) {
            return 0;
        }

        CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(guid, TYPE_PLAYER, __FILE__, __LINE__));

        if (!unit) {
            unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(guid, TYPE_UNIT, __FILE__, __LINE__));
        }

        if (!unit) {
            return 0;
        }

        PushBool(L, unit->HasAuraBySpellId(spellId));
        return 1;
    }

    // ---- UnitNameBadge / UnitNameTag ------------------------------------------------------------------------------------
    // A unit token resolved to a player: its PLAYER_FLAGS; false (nothing read) otherwise.
    bool PlayerFlagsOf(lua_State* L, uint32_t& flags) {
        const std::string token = CoA::Script::CheckString(L, 1);
        WOWGUID guid;

        if (!Script_GetGUIDFromToken(token.c_str(), guid, false)) {
            return false;
        }

        CGPlayer_C* player = static_cast<CGPlayer_C*>(ClntObjMgrObjectPtr(guid, TYPE_PLAYER, __FILE__, __LINE__));

        if (!player) {
            return false;
        }

        flags = player->m_player->m_playerFlags;
        return true;
    }

    // FUN_10a73690: AFK (flag 2) -> the CHAT_FLAG_AFK text, else DND (4) -> CHAT_FLAG_DND.
    int UnitNameBadge(lua_State* L) {
        uint32_t flags = 0;

        if (PlayerFlagsOf(L, flags) && (flags & 6)) {
            const char* text = FrameScript_GetText((flags & 2) ? "CHAT_FLAG_AFK" : "CHAT_FLAG_DND", -1, static_cast<FRAMESCRIPT_GENDER>(1));
            lua_pushstring(L, text ? text : "");
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    // FUN_10a738b0: GM (flag 8) -> "<GM>", else developer (0x8000) -> "<Dev>".
    int UnitNameTag(lua_State* L) {
        uint32_t flags = 0;

        if (PlayerFlagsOf(L, flags) && (flags & 8)) {
            lua_pushstring(L, "<GM>");
        } else if (flags & 0x8000) {
            lua_pushstring(L, "<Dev>");
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    // ---- C_UICamera.GetItemCameraID ---------------------------------------------------------------------------------------
    // FUN_1021a020: weapons (Item class 2) match UICameraAppearanceWeapons on (2, subclass, inventory type); armor (4)
    // matches UICameraAppearanceChrRaces on (race, sex, inventory type). The first matching weapon row decides even if
    // its camera is missing.
    const uint8_t* ItemCamera(uint32_t itemId) {
        ItemRec* item = g_itemDB.GetRecord(static_cast<int32_t>(itemId));

        if (!item) {
            return nullptr;
        }

        CoA::Dbc::Table& cameras = CoA::Dbc::Get("DBFilesClient/UICameras.dbc");

        if (item->m_classID == 2) {
            CoA::Dbc::Table& w = CoA::Dbc::Get("DBFilesClient/UICameraAppearanceWeapons.dbc");

            for (uint32_t id = w.MinId(); w.Loaded() && id <= w.MaxId(); ++id) {
                const uint8_t* r = w.Row(id);

                if (r && CoA::Dbc::Table::I32(r, 4) == 2 && CoA::Dbc::Table::I32(r, 8) == item->m_subclassID && CoA::Dbc::Table::I32(r, 0xC) == item->m_inventoryType) {
                    return cameras.Row(CoA::Dbc::Table::U32(r, 0x10));
                }
            }
        } else if (item->m_classID == 4 && ClntObjMgrGetActivePlayerObj()) {
            CoA::Dbc::Table& c = CoA::Dbc::Get("DBFilesClient/UICameraAppearanceChrRaces.dbc");

            for (uint32_t id = c.MinId(); c.Loaded() && id <= c.MaxId(); ++id) {
                const uint8_t* r = c.Row(id);

                if (!r || CoA::Dbc::Table::U32(r, 4) != ClientServices::GetCharacterRace()) {
                    continue;
                }

                if (CoA::Dbc::Table::U32(r, 8) != ClientServices::GetCharacterSex() || CoA::Dbc::Table::I32(r, 0xC) != item->m_inventoryType) {
                    continue;
                }

                return cameras.Row(CoA::Dbc::Table::U32(r, 0x10));
            }
        }

        return nullptr;
    }

    int GetItemCameraID(lua_State* L) {
        uint32_t itemId;

        if (!CoA::Script::ReadNumber(L, itemId)) {
            return 0;
        }

        if (const uint8_t* camera = ItemCamera(itemId)) {
            PushInt(L, CoA::Dbc::Table::I32(camera, 0));
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "AcceptAllAvailableGossipQuests", AcceptAllAvailableGossipQuests),
        COA_NATIVE("", "AcceptAvailableGossipQuest", AcceptAvailableGossipQuest),
        COA_NATIVE("", "ClientsideAura", ClientsideAura),
        COA_NATIVE("", "ClientsideAuraCreature", ClientsideAuraCreature),
        COA_NATIVE("", "ClientsideAuraTarget", ClientsideAuraTarget),
        COA_NATIVE("", "CompleteActiveGossipQuest", CompleteActiveGossipQuest),
        COA_NATIVE("", "CompleteAllActiveGossipQuests", CompleteAllActiveGossipQuests),
        COA_NATIVE("", "GetActiveGossipQuestIds", GetActiveGossipQuestIds),
        COA_NATIVE("", "GetAvailableGossipQuestIds", GetAvailableGossipQuestIds),
        COA_NATIVE("", "GetCompleteGossipQuestIds", GetCompleteGossipQuestIds),
        COA_NATIVE("", "GuildQueryPermissions", GuildQueryPermissions),
        COA_NATIVE("", "SetCameraFoV", SetCameraFoV),
        // The original's names end in a space (strings 0x10B60B04 / 0x10B60AF4).
        COA_NATIVE("", "UnitNameBadge ", UnitNameBadge),
        COA_NATIVE("", "UnitNameTag ", UnitNameTag),
        COA_NATIVE("C_Aura", "UnitHasAura", UnitHasAura),
        COA_NATIVE("C_ItemSet", "GetSetName", GetSetName),
        COA_NATIVE("C_TradeSkill", "GetCraftedItem", GetCraftedItem),
        COA_NATIVE("C_TradeSkill", "GetReagentItems", GetReagentItems),
        COA_NATIVE("C_UICamera", "GetItemCameraID", GetItemCameraID),
        COA_NATIVE("", "SendWindowsFlash", SendWindowsFlash),
        // handler_SetPortraitToTexture tail-calls the client's own native (0x516970); it is registered directly.
        COA_NATIVE("", "SetPortraitToTexture", Script_SetPortraitToTexture),
        // 0x101912E0 is `mov eax, 0x60A510; jmp eax`: the glue state gets the client's own world native (glue-only in
        // the capture; the world state has the client's registration already).
        COA_NATIVE("", "FillLocalizedClassList", Script_FillLocalizedClassList),
    };
}

void CoA::BindingsClient::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
