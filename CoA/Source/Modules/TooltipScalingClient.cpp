// AscTooltipScaling.cpp's client half: the Deps, the GameTooltip methods and Has/Get/SetScript, and what the decomp
// sites ask with client objects.
#include "CoA/Source/Modules/TooltipScaling.h"
#include "CoA/Source/Modules/TooltipScalingClient.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/Frame/CSimpleRender.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/ChrClassesRec.h"
#include "WoW/Source/DB/Rec/DurabilityCostsRec.h"
#include "WoW/Source/DB/Rec/DurabilityQualityRec.h"
#include "WoW/Source/Object/ItemStats.h"
#include "WoW/Source/Object/ObjectClient/Item_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Trade_C.h"
#include "WoW/Source/Object/ObjectClient/UnitCombatLog_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/UIUtil/Tooltip.h"
#include "WoW/Source/Ui/GuildBankFrame.h"
#include "WoW/Source/Ui/LootFrame.h"
#include "WoW/Source/Ui/MailInfo.h"
#include "WoW/Source/Ui/PaperDollInfoFrame.h"
#include <cstring>



namespace {
    CGItem_C* s_lastItemObject = nullptr;   // 0x10BE24D0

    void Fill(const ItemStats_C* s, CoA::TooltipScaling::CacheItem& out) {
        out.cls = s->m_class;
        out.subClass = s->m_subClass;
        out.quality = s->m_quality;
        out.inventoryType = s->m_inventoryType;
        out.itemLevel = s->m_itemLevel;
        out.requiredLevel = s->m_requiredLevel;

        for (uint32_t i = 0; i < 10; ++i) {
            out.statType[i] = s->m_statType[i];
            out.statValue[i] = s->m_statValue[i];
        }

        memcpy(out.damageMin, s->m_damageMin, sizeof(out.damageMin));
        memcpy(out.damageMax, s->m_damageMax, sizeof(out.damageMax));
        memcpy(out.resistances, s->m_resistances, sizeof(out.resistances));
        out.delay = static_cast<uint32_t>(s->m_delay);
        out.randomProperty = s->m_randomProperty;
        out.block = s->m_block;
    }

    bool ItemCache(uint32_t item, CoA::TooltipScaling::CacheItem& out) {   // 0x67CA30(0xC5D828, item, 0, 0, 0, 0)
        const ItemStats_C* s = g_itemDBCache.GetRecord(static_cast<int32_t>(item), WOWGUID{ 0 }, nullptr, nullptr, false);

        if (!s) {
            return false;
        }

        Fill(s, out);
        return true;
    }

    bool PlayerInfo(CoA::TooltipScaling::Player& out) {   // 0x4038F0
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player) {
            return false;
        }

        out.guid = player->m_obj->m_guid.m_guid;
        out.level = player->m_unit->m_unitFieldLevel;
        // The class byte only for a unit / player object (+0x14 type 3 / 4).
        out.cls = (player->m_typeId == ID_UNIT || player->m_typeId == ID_PLAYER) ? player->m_unit->m_unitFieldBytes0[1] : 0;
        return true;
    }

    bool LootUnitLevel(uint32_t& level) {   // CGLootInfo::m_lootObj (0xBFA8D8), mask 8
        CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(CGLootInfo::m_lootObj, TYPE_UNIT, __FILE__, __LINE__));

        if (!unit) {
            return false;
        }

        level = unit->m_unit->m_unitFieldLevel;
        return true;
    }

    bool InspectTargetExists() {   // CGCharacterInfo::m_inspectTarget (0xC24220), mask 0x10
        return ClntObjMgrObjectPtr(CGCharacterInfo::m_inspectTarget, TYPE_PLAYER, __FILE__, __LINE__) != nullptr;
    }

    uint32_t TradeLevel(uint32_t slot) {   // *(0xCA0DE0 + slot * 0x50): the trade target's slot, its +0x30 dword
        return Trade_C_GetTargetSlotDword30(slot);
    }

    bool ItemEntry(uint64_t guid, uint32_t& entry) {   // mask 2, descriptor +0xC
        WOWGUID g;
        g.m_guid = guid;
        CGObject_C* item = ClntObjMgrObjectPtr(g, TYPE_ITEM, __FILE__, __LINE__);

        if (!item) {
            return false;
        }

        entry = static_cast<uint32_t>(item->m_obj->m_entryID);
        return true;
    }

    const char* GlobalString(const char* key, int32_t flag) {   // 0x819D40(key, -1, flag)
        return FrameScript_GetText(key, -1, static_cast<FRAMESCRIPT_GENDER>(flag));
    }

    std::vector<std::pair<uint8_t, const char*>> Classes() {   // FUN_100b13c0 over ChrClasses min .. max
        std::vector<std::pair<uint8_t, const char*>> out;

        for (int32_t id = g_chrClassesDB.m_minID; id <= g_chrClassesDB.m_maxID; ++id) {
            if (const ChrClassesRec* rec = g_chrClassesDB.GetRecord(id)) {
                out.push_back({ static_cast<uint8_t>(rec->m_id), rec->m_name });
            }
        }

        return out;
    }

    // FUN_100b2c50: DurabilityCosts row index level - 1; the dword at column + 1. Ruling: the original reads past the
    // table and the row unchecked; here an absent row is none and a column past the row's 29 costs is 0.
    bool DurabilityCost(uint32_t level, uint32_t column, uint32_t& cost) {
        if (level == 0 || static_cast<int32_t>(level - 1) >= g_durabilityCostsDB.m_numRecords) {
            return false;
        }

        const DurabilityCostsRec* rec = g_durabilityCostsDB.GetRecordByIndex(static_cast<int32_t>(level - 1));

        if (!rec) {
            return false;
        }

        cost = column < 21 ? static_cast<uint32_t>(rec->m_weaponSubClassCost[column])
            : column < 29 ? static_cast<uint32_t>(rec->m_armorSubClassCost[column - 21]) : 0;
        return true;
    }

    bool DurabilityQuality(uint32_t quality, float& multiplier) {   // FUN_100b2c70: row index quality * 2 + 1
        const int32_t index = static_cast<int32_t>(quality * 2 + 1);

        if (index < 0 || index >= g_durabilityQualityDB.m_numRecords) {
            return false;
        }

        const DurabilityQualityRec* rec = g_durabilityQualityDB.GetRecordByIndex(index);

        if (!rec) {
            return false;
        }

        multiplier = rec->m_data;
        return true;
    }

    // FUN_10281690 GameTooltip:GetItemGUID(): the tooltip item's GUID through the client's formatter (0x74D0D0).
    int GetItemGUID(lua_State* L) {
        char buffer[32] = {};
        WOWGUID guid;
        guid.m_guid = CoA::TooltipScaling::TooltipGuid();
        GUIDToHexString(guid, buffer);
        CoA::Script::PushStr(L, buffer);
        return 1;
    }

    // FUN_10281460 GameTooltip:ClearLine(line): both font strings of that line emptied (SetText 0x483910). Ruling: the
    // original indexes the arrays unchecked; a line past them does nothing here.
    int ClearLine(lua_State* L) {
        lua_rawgeti(L, 1, 0);
        CGTooltip* tip = static_cast<CGTooltip*>(lua_touserdata(L, -1));
        lua_settop(L, -2);
        const int32_t line = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 2));

        if (line > 0 && tip) {
            const uint32_t i = static_cast<uint32_t>(line - 1);

            if (i < tip->m_leftStrings.m_count && tip->m_leftStrings.m_data[i]) {
                tip->m_leftStrings.m_data[i]->SetText("", 1);
            }

            if (i < tip->m_rightStrings.m_count && tip->m_rightStrings.m_data[i]) {
                tip->m_rightStrings.m_data[i]->SetText("", 1);
            }
        }

        return 0;
    }

    // The frame (self[0]) when it is a GameTooltip and argument 2 is "OnTextChanged". Ruling: the original calls IsA
    // on a nil self[0]; here that is no tooltip.
    void* TooltipOnTextChanged(lua_State* L) {
        lua_rawgeti(L, 1, 0);
        CSimpleFrame* obj = static_cast<CSimpleFrame*>(lua_touserdata(L, -1));
        lua_settop(L, -2);

        if (!obj || !obj->IsA(CGTooltip::GetObjectType()) || !lua_isstring(L, 2)) {
            return nullptr;
        }

        const char* what = lua_tolstring(L, 2, nullptr);
        return what && strcmp(what, "OnTextChanged") == 0 ? obj : nullptr;
    }

    // FUN_1027e0a0 / FUN_1027dfb0 / FUN_1027e1a0.
    int32_t FrameScript(int32_t which, lua_State* L) {
        void* obj = TooltipOnTextChanged(L);

        if (!obj) {
            return -1;
        }

        int ref = 0;

        if (which == 0) {   // HasScript
            if (CoA::TooltipScaling::OnTextChanged(obj, ref)) {
                lua_pushnumber(L, 1.0);
                return 1;
            }

            return -1;
        }

        if (which == 1) {   // GetScript
            if (CoA::TooltipScaling::OnTextChanged(obj, ref)) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
                return 1;
            }

            return -1;
        }

        // SetScript: whatever is on top becomes the ref (the old one is not released).
        CoA::TooltipScaling::SetOnTextChanged(obj, luaL_ref(L, LUA_REGISTRYINDEX));
        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("#GameTooltip", "GetItemGUID", GetItemGUID),
        COA_NATIVE("#GameTooltip", "ClearLine", ClearLine),
    };
}

bool CoA::TooltipScalingClient::BeforeAddLine(CGTooltip* tooltip, const char*& left, const char*& right, CImVector& leftColor,
    CImVector& rightColor, std::string& leftBuffer, std::string& rightBuffer) {
    uint32_t colour;
    memcpy(&colour, &leftColor, 4);
    const CoA::TooltipScaling::Line line = CoA::TooltipScaling::AddLine(left, right, colour);

    if (line.kind == CoA::TooltipScaling::Line::Drop) {
        return true;
    }

    leftBuffer = line.left;
    left = line.hasLeft ? leftBuffer.c_str() : nullptr;

    if (line.kind == CoA::TooltipScaling::Line::Rewrite) {
        if (line.colour) {
            // s_tooltipTextColor (0xAD2D30) / s_tooltipRedColor (0xAD2D34), both texts.
            const uint32_t c = line.colour == 2 ? 0xFFFF2020u : 0xFFFFFFFFu;
            memcpy(&leftColor, &c, 4);
            memcpy(&rightColor, &c, 4);
        }

        return false;
    }

    // No rewrite: the tooltip's OnTextChanged(self, left, right) may replace either text.
    int ref = 0;
    lua_State* L = FrameScript_GetContext();
    const int top = lua_gettop(L);

    if (CoA::TooltipScaling::OnTextChanged(tooltip, ref)) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        lua_rawgeti(L, LUA_REGISTRYINDEX, tooltip->m_luaObjectRef);
        lua_pushstring(L, left ? left : "");
        lua_pushstring(L, right ? right : "");

        if (lua_pcall(L, 3, 2, 0) == 0) {
            if (lua_type(L, -2) != LUA_TNIL) {
                const char* s = lua_tolstring(L, -2, nullptr);
                leftBuffer = s ? s : "";
                left = s ? leftBuffer.c_str() : nullptr;
            }

            if (lua_type(L, -1) != LUA_TNIL) {
                const char* s = lua_tolstring(L, -1, nullptr);
                rightBuffer = s ? s : "";
                right = s ? rightBuffer.c_str() : nullptr;
            }
        }
    }

    lua_settop(L, top);
    return false;
}

bool CoA::TooltipScalingClient::FeralAttackPower(const ItemStats_C* stats, float& damage, int32_t& result) {
    CoA::TooltipScaling::CacheItem item;
    Fill(stats, item);
    return CoA::TooltipScaling::FeralAttackPower(item, damage, result);
}

int32_t CoA::TooltipScalingClient::RepairCost(CGItem_C* item) {
    return static_cast<int32_t>(CoA::TooltipScaling::RepairCost(static_cast<uint32_t>(item->m_obj->m_entryID), item->m_obj->m_guid.m_guid,
        item->m_item->m_itemFieldDurability, item->m_item->m_itemFieldMaxdurability));
}

void CoA::TooltipScalingClient::KeepItemObject(CGItem_C* item) {
    s_lastItemObject = item;
}

bool CoA::TooltipScalingClient::AfterCanEquip(const ItemStats_C* stats, bool result) {
    // The kept object's GUID is read as the original reads it (the object may have gone since).
    const uint64_t guid = s_lastItemObject ? s_lastItemObject->m_obj->m_guid.m_guid : 0;
    return CoA::TooltipScaling::CanEquip(result ? 1 : 0, stats->m_requiredLevel, s_lastItemObject != nullptr, guid) != 0;
}

// sub_1027afc0: with a number in argument 3, the mail's attachment item GUID. Ruling: the original indexes the inbox
// and dereferences GetAttachment's result unchecked; here a missing message or attachment leaves the GUID alone.
void CoA::TooltipScalingClient::BeforeInboxItem(lua_State* L) {
    if (!lua_isnumber(L, 3)) {
        return;
    }

    const int32_t mail = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 2));
    const int32_t attachment = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 3));
    const uint32_t index = static_cast<uint32_t>(mail - 1);

    if (index >= CGMailInfo::m_inbox.m_count) {
        return;
    }

    if (MailboxMessageInfo* m = CGMailInfo::m_inbox.m_data[index]) {
        if (const MailAttachmentInfo* a = GetAttachment(m, static_cast<uint32_t>(attachment - 1))) {
            CoA::TooltipScaling::SetInboxItem(a->m_itemGuid);
        }
    }
}

void CoA::TooltipScalingClient::BeforeGuildBankItem(lua_State* L) {   // FUN_10283f20: the slot's +0x28
    const int32_t tab = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 2));
    const int32_t slot = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 3));

    if (const GuildBankSlotInfo* item = CGGuildBankInfo::GetGuildBankItem(static_cast<uint32_t>(tab - 1), static_cast<uint32_t>(slot - 1))) {
        CoA::TooltipScaling::SetGuildBankLevel(item->m_flags);
    }
}

void CoA::TooltipScalingClient::Init() {
    CoA::TooltipScaling::Deps& d = CoA::TooltipScaling::Dependencies();
    d.itemCache = ItemCache;
    d.player = PlayerInfo;
    d.lootUnitLevel = LootUnitLevel;
    d.inspectTargetExists = InspectTargetExists;
    d.tradeLevel = TradeLevel;
    d.itemEntry = ItemEntry;
    d.globalString = GlobalString;
    d.classes = Classes;
    d.durabilityCost = DurabilityCost;
    d.durabilityQuality = DurabilityQuality;
    g_frameScriptHook = FrameScript;
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
