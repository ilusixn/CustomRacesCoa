#ifndef COA_SOURCE_MODULES_CUSTOMSTORE_H
#define COA_SOURCE_MODULES_CUSTOMSTORE_H

#include <cstdint>

// AscCustomStore.cpp: C_CustomStore (11, 0x101A5DE0..0x101A6C00), the server-defined NPC / game-object stores.
//
// Manager FUN_101a5c30 -> 0x10BCA5D0: +0x00 store types (0x3C each: u32 id, u32 creature entry, u32 game-object entry,
// str, str -- SMSG 0x6BD), +0x0C page size (0x12 by default), +0x10 items (0x40 each: id, type, item entry, money,
// 5 token items, 5 token counts, game event, achievement -- SMSG 0x6BA), +0x1C the CustomStoreDataEntryContainer
// (vtable 0x10B3A54C) over the items. Module init 0x101A5950: glue reset 0x101A3EB0; SMSG 0x6BA
// QUERY_CUSTOM_STORE_RESULT, 0x6BC PURCHASE_CUSTOM_STORE_ITEM_RESULT, 0x6BD the store types. CMSG 0x6B9 {u32 store}
// queries, 0x6BB {u32 item, u32 count} buys.
//
// The purchase check (FUN_101a3a20), the store check (FUN_101a63a0) and the type / query handlers were checked against
// the genuine DLL. Every client question goes through Deps (CustomStoreClient.cpp).
namespace CoA::CustomStore {
    // AscSkillCard::ByItem (FUN_102142a0): the skill card an item teaches.
    struct SkillCard {
        uint32_t cardId = 0;
        uint32_t rank = 0;
        uint32_t quality = 0;
        uint32_t spell = 0;
    };

    struct Deps {
        bool (*hasPlayer)() = nullptr;                                    // 0x4038F0 found the active player
        uint32_t (*playerMoney)() = nullptr;                              // descriptor +0x1248
        uint32_t (*heldCount)(uint32_t item) = nullptr;                   // FUN_10308470 + FUN_10308570
        bool (*nearby)(uint32_t entry, bool gameObject) = nullptr;        // FUN_101a3d50 / FUN_101a3bf0 (5.5 yd)
        bool (*achievementDone)(uint32_t id) = nullptr;                   // FUN_103087e0 over 0xACFDB0
        bool (*spellFamily)(uint32_t spell, uint32_t& family) = nullptr;  // FetchSpell, Spell +0x240
        // AscSkillCard (not ported yet): unset answers as the original does with no skill card for the item.
        bool (*skillCardByItem)(uint32_t item, SkillCard& out) = nullptr;               // FUN_102142a0
        bool (*skillCardCollectedAtLeast)(uint32_t cardId, uint32_t rank) = nullptr;
        int32_t (*skillCardQualityIndex)(uint32_t quality) = nullptr;                   // QualityIndex(q, 5)
    };

    Deps& Dependencies();

    void Init();
    // Tests only: the manager as constructed (no types, no items, page size 0x12), Deps forgotten.
    void Reset();
}

#endif
