// AscAppearance.cpp's client half: the active player (class, spellbook), Spell.dbc, Item.dbc and ItemSet.dbc records, the
// creature-template cache (BridgeStore) and item text (ItemAddon), the item camera (C_UICamera's FUN_1021a020), a GUID string's item
// and the unit test the aura hook needs.
#include "CoA/Source/Modules/Appearance.h"
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/Modules.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/ItemRec.h"
#include "WoW/Source/DB/Rec/ItemSetRec.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Object/ObjectClient/Item_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

WOWGUID StringToGuid(const char* string);   // 0x74D120 (UnitCombatLog_C.cpp)

namespace {
    using CoA::Appearance::ItemInfo;

    bool HasPlayer() {   // ActivePlayer()
        return ClntObjMgrGetActivePlayerObj() != nullptr;
    }

    uint32_t PlayerClass() {   // UNIT_FIELD_BYTES_0 byte 1
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? player->m_unit->m_unitFieldBytes0[1] : 0;
    }

    bool KnowsSpell(uint32_t spell) {   // 0x7260E0
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player && player->IsSpellKnown(static_cast<int32_t>(spell));
    }

    bool SpellExists(uint32_t spell) {
        SpellRec rec;
        return g_spellDB.GetRecord(static_cast<int32_t>(spell), &rec) != 0;
    }

    bool Item(uint32_t id, CoA::Appearance::ItemRecord& out) {   // Item.dbc (0xAD3D4C)
        const ItemRec* row = g_itemDB.GetRecord(static_cast<int32_t>(id));

        if (!row) {
            return false;
        }

        out.classId = static_cast<uint32_t>(row->m_classID);
        out.subclassId = static_cast<uint32_t>(row->m_subclassID);
        out.inventoryType = static_cast<uint32_t>(row->m_inventoryType);
        return true;
    }

    bool ItemSet(uint32_t id, CoA::Appearance::ItemSetRecord& out) {
        const ItemSetRec* row = g_itemSetDB.GetRecord(static_cast<int32_t>(id));

        if (!row) {
            return false;
        }

        out.id = static_cast<uint32_t>(row->m_id);
        out.name = row->m_name ? row->m_name : "";

        for (uint32_t k = 0; k < 17; ++k) {
            out.items[k] = static_cast<uint32_t>(row->m_itemID[k]);
        }

        return true;
    }

    void ItemSetRange(uint32_t& minId, uint32_t& maxId) {   // ItemSet.dbc +0x10 / +0x0C
        minId = static_cast<uint32_t>(g_itemSetDB.m_minID);
        maxId = static_cast<uint32_t>(g_itemSetDB.m_maxID);
    }

    bool CreatureKnown(uint32_t entry) {   // FUN_100b8cc0
        return CoA::BridgeStore::CreatureByEntry(entry) != nullptr;
    }

    bool FetchItemInfo(const uint32_t* items, uint32_t n, std::unordered_map<uint32_t, ItemInfo>& out) {   // FUN_1020ef10
        std::unordered_map<uint32_t, CoA::ItemAddon::Info> info;

        if (!CoA::ItemAddon::FetchInfo(items, n, info)) {
            return false;
        }

        for (const auto& i : info) {
            out[i.first] = { i.second.name, i.second.quality };
        }

        return true;
    }

    ItemInfo GetItemInfo(uint32_t item) {   // FUN_1020e9c0
        const CoA::ItemAddon::Info info = CoA::ItemAddon::GetInfo(item);
        return { info.name, info.quality };
    }

    bool ItemCamera(uint32_t item, int32_t& cameraId) {   // FUN_1021a020, the UICameras row's id
        const uint8_t* row = CoA::BindingsClient::ItemCamera(item);

        if (!row) {
            return false;
        }

        memcpy(&cameraId, row, 4);
        return true;
    }

    // 0x74D120 then ObjectPtr(guid, 2): the item object's own GUID.
    bool ItemGuid(const char* text, uint64_t& itemGuid) {
        const WOWGUID guid = StringToGuid(text);
        const CGItem_C* item = guid.m_guid ? static_cast<const CGItem_C*>(ClntObjMgrObjectPtr(guid, TYPE_ITEM, __FILE__, __LINE__)) : nullptr;

        if (!item) {
            return false;
        }

        itemGuid = item->m_obj->m_guid.m_guid;
        return true;
    }

    bool IsActivePlayer(void* unit) {   // 0x4CEE50
        return static_cast<CGUnit_C*>(unit)->IsActivePlayer();
    }
}

void CoA::AppearanceClient::Init() {
    CoA::Appearance::Deps& d = CoA::Appearance::Dependencies();
    d.hasPlayer = HasPlayer;
    d.playerClass = PlayerClass;
    d.knowsSpell = KnowsSpell;
    d.spellExists = SpellExists;
    d.item = Item;
    d.itemSet = ItemSet;
    d.itemSetRange = ItemSetRange;
    d.creatureKnown = CreatureKnown;
    d.fetchItemInfo = FetchItemInfo;
    d.itemInfo = GetItemInfo;
    d.itemCamera = ItemCamera;
    d.itemGuid = ItemGuid;
    d.isActivePlayer = IsActivePlayer;
}
