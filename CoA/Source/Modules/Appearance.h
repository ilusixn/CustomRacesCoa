#ifndef COA_SOURCE_MODULES_APPEARANCE_H
#define COA_SOURCE_MODULES_APPEARANCE_H

// AscAppearance.cpp: the Appearance subsystem -- C_Appearance (24), C_AppearanceCollection (12), C_AppearanceOutfit (6)
// and C_ItemSet's GetAppearances / GetItemSetNumCollected, transcribed from the original's AppearanceMgr (FUN_100c5eb0
// -> static 0x10BDB2A0, built by FUN_100c03a0). Appearance.cpp has the manager layout and the packets;
// AppearanceBrowser.cpp the collection browser, the outfits and the seasonal lists.
#include <cstdint>
#include <string>
#include <unordered_map>

namespace CoA::Appearance {
    struct ItemRecord {        // client Item.dbc (0xAD3D4C)
        uint32_t classId = 0;        // +4
        uint32_t subclassId = 0;     // +8
        uint32_t inventoryType = 0;  // +0x18
    };

    struct ItemSetRecord {     // client ItemSet.dbc
        uint32_t id = 0;             // +0
        std::string name;            // +4
        uint32_t items[17] = {};     // +8..+0x48
    };

    struct ItemInfo {          // what FUN_1020e9c0 / FUN_1020ef10 give the browser: the item's name and quality
        std::string name;
        uint32_t quality = 0;
    };

    // What Appearance reads from the client (AppearanceClient.cpp).
    struct Deps {
        bool (*hasPlayer)() = nullptr;                              // ActivePlayer() != null
        uint32_t (*playerClass)() = nullptr;                        // UNIT_FIELD_BYTES_0 byte 1, 0 unless a unit / player
        bool (*knowsSpell)(uint32_t spell) = nullptr;               // 0x7260E0 on the active player
        bool (*spellExists)(uint32_t spell) = nullptr;              // FetchSpell
        bool (*item)(uint32_t id, ItemRecord& out) = nullptr;
        bool (*itemSet)(uint32_t id, ItemSetRecord& out) = nullptr;
        void (*itemSetRange)(uint32_t& minId, uint32_t& maxId) = nullptr;   // ItemSet.dbc +0x10 / +0x0C
        bool (*creatureKnown)(uint32_t entry) = nullptr;            // FUN_100b8cc0 (the creature-template cache)
        // FUN_1020ef10: one batch over the items; false when it fails. FUN_1020e9c0: one item ("" when unknown).
        bool (*fetchItemInfo)(const uint32_t* items, uint32_t n, std::unordered_map<uint32_t, ItemInfo>& out) = nullptr;
        ItemInfo (*itemInfo)(uint32_t item) = nullptr;
        bool (*itemCamera)(uint32_t item, int32_t& cameraId) = nullptr;   // FUN_1021a020 -> the UICameras row's id
        // 0x74D120 then ObjectPtr(guid, 2): the item object's own GUID; false when no such item.
        bool (*itemGuid)(const char* guidText, uint64_t& itemGuid) = nullptr;
        bool (*isActivePlayer)(void* unit) = nullptr;               // 0x4CEE50 on the unit the aura hook is given
    };

    Deps& Dependencies();

    // FUN_10322c70's branch for the active player (SpellVisuals): the visual the applied appearance of the spell's
    // APPEARANCE_TYPE 4 category names; false when there is none.
    bool PlayerSpellVisual(uint32_t spell, uint32_t& visual);
    // The SMSG_PATCH_* refreshes: 0x693, 0x6EC, and 0x692 (a new Appearances.dbc row).
    void ItemAppearancesPatched();
    void ItemSetAppearancesPatched();
    void RebuildOutfits();
    // The item-use detour 0x10A42F30's last branch (ObjectUse.cpp): the item's ItemAppearances row names a collected
    // appearance; the first of its categories with a row fires UNLOCKED_APPEARANCE_ITEM_USED when its type is 2..6.
    void ItemUsed(uint32_t entry, const char* itemName);

    void Init();
    void Reset();   // tests: the manager, indexes, browsers and Deps back to their start
}

#endif
