#ifndef COA_SOURCE_MODULES_MYSTICENCHANT_H
#define COA_SOURCE_MODULES_MYSTICENCHANT_H

#include <cstdint>
#include <string>
#include <vector>

// AscMysticEnchant.cpp: C_MysticEnchant, the original's subsystem at 0x102DD000..0x102F1200.
//
// Manager DAT_10BE3928 (accessor FUN_102E9C20): the known enchant spells (SMSG 0x5F9 / 0x5FA / 0x5FB), the slot enchants
// (0x5FD / 0x5FE), progress and level (0x5FC / 0x619 / 0x604), the pending collection reforges and applies (Lua), the
// inspect results (0x614), the "altar open" flag, the enchant container (QueryEnchants) and the filtered scroll list.
//
// MysticEnchant.dbc rows (0x7C bytes) are indexed by spell (+0x04) and by item (+0x18); a later row with the same key
// wins. +0x0C / +0x10 quality NAME (the +0x10 one for the stock "reborn" classes), +0x14 level, +0x1C worldforged,
// +0x20..+0x30 realm flags, +0x34 / +0x38 class mask, +0x3C..+0x48 spec tag NAMES, +0x4C / +0x58 / +0x64 / +0x70 the
// three class / tab / required AE / required TE.
//
// Every client read goes through Deps (MysticEnchantClient.cpp).
namespace CoA::MysticEnchant {
    // An item object the player can see, by its low guid. bag 0 = backpack (or not in a bag), 1..4 the equipped bags;
    // slot is 1-based (FUN_10111D30 / FUN_10111E20), 0 when the item is not in the backpack or a bag.
    struct Item {
        uint32_t low = 0;
        uint32_t entry = 0;
        uint32_t count = 0;   // ITEM_FIELD_STACK_COUNT (descriptor +0x38)
        uint8_t bag = 0;
        uint8_t slot = 0;
    };

    struct Deps {
        bool (*hasPlayer)() = nullptr;
        uint8_t (*playerClass)() = nullptr;                    // UNIT_FIELD_BYTES_0 byte 1
        uint32_t (*playerLevel)() = nullptr;                   // descriptor +0xD8
        uint32_t (*coinage)() = nullptr;                       // PLAYER_FIELD_COINAGE (+0x1248)
        bool (*casting)() = nullptr;                           // player +0xA60 != 0
        bool (*item)(uint32_t low, Item& out) = nullptr;       // ObjectPtr(item guid, 2)
        std::vector<Item> (*playerItems)() = nullptr;          // FUN_10308270: equipment, backpack, bag contents
        uint32_t (*tokenCount)(uint32_t entry) = nullptr;      // FUN_10308700: the currency-token slots 0x76..0x95
        uint32_t (*freeBagSlots)() = nullptr;                  // FUN_103085F0
        std::string (*itemName)(uint32_t low) = nullptr;       // CGItem_C::GetName (0x707C20)
        bool (*itemBuyPrice)(uint32_t entry, uint32_t& price) = nullptr;   // item cache record +0x20
        bool (*spellStacks)(uint32_t spell, uint32_t& stacks) = nullptr;   // Spell.dbc +0xC4 (false: no record)
        bool (*spellName)(uint32_t spell, std::string& name) = nullptr;    // Spell.dbc +0x220
        bool (*auraPoints)(uint32_t aura, int32_t& points) = nullptr;      // the player has the aura: its +0x144
        uint32_t (*investment)(uint32_t cls, uint32_t tab, bool te) = nullptr;   // AscCA active build (Plan 7)
        bool (*tokenGuid)(const char* token, uint64_t& guid) = nullptr;
        uint64_t (*activePlayerGuid)() = nullptr;
        bool (*playerClassByGuid)(uint64_t guid, uint8_t& cls) = nullptr;   // ObjectPtr(guid, TYPE_PLAYER)
        bool (*altarNearby)() = nullptr;                       // FUN_102E96E0
        std::string (*spellTextName)(uint32_t spell) = nullptr;
        std::string (*spellTextDescription)(uint32_t spell) = nullptr;
        void (*cacheItem)(uint32_t entry) = nullptr;           // AscAssetQuery::TryCacheItem (AssetQuery.cpp fills it)
    };

    Deps& Dependencies();

    // The exported queries other modules use (AscMysticEnchant.hpp).
    uint32_t SlotEnchant(uint32_t slot0);       // FUN_102e3fc0
    std::vector<uint32_t> Slots();              // FUN_102e9c20 +0x0C, for the preset save (SMSG 0x602)
    bool Slotted(uint32_t spell);               // FUN_102E9680
    bool Collected(uint32_t spell);             // FUN_102E96B0
    bool Worldforged(uint32_t spell);
    // The row of a spell (map 0x10BE04C8, FUN_100A0420) and its RE_QUALITY_* index (FUN_102E65D0), for AscBuildCreator.
    const uint8_t* RowOfSpell(uint32_t spell);
    uint32_t QualityOfRow(const uint8_t* row);

    // 0x10A42DD0 -> FUN_102EA1A0: a used game object; FUN_10A42F30's first two branches: a used item.
    void OnGameObjectUse(uint32_t entry);
    void OnItemUse(uint32_t entry);
    // 0x102EA2A0: at world entry, prefetch the untarnished scroll's item record (MysticEnchantClient.cpp calls it).
    void EnterWorld();

    // The player reads the preset checks share (AscMysticEnchantPreset.cpp), through Deps.
    bool IsAltar(uint32_t entry);            // FUN_102EA0C0's list of mystic altar game objects
    bool PlayerCasting();                    // player +0xA60 != 0; false without a player
    bool PlayerIsCoAClass();                 // FUN_100C5F50 on the player; false without a player
    uint32_t BagItemCount(uint32_t entry);   // FUN_10308470: the backpack and the equipped bags, not equipment

    void Init();
    // Tests only: the manager and the indexes forgotten.
    void Reset();
}

#endif
