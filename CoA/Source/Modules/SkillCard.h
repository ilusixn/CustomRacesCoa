#ifndef COA_SOURCE_MODULES_SKILLCARD_H
#define COA_SOURCE_MODULES_SKILLCARD_H

#include <cstdint>
#include <string>

class CDataStore;
struct lua_State;

// AscSkillCardCollection.cpp / AscSkillCardSlots.cpp: skill cards.
//
// Cards: SkillCard.dbc (container 0x10BDF4F8, rows 0x30: +0 id, +4 card id, +8 rank, +0xC item, +0x10 spell, +0x14 type,
// +0x18 expansion, +0x1C class, +0x20 quality (strings), +0x24 wildcard, +0x28 draft, +0x2C starter), patched per row by
// SMSG_PATCH_SKILL_CARD 0x647 (FUN_101e2f10), indexed by (card id, rank) in the map 0x10BE0678.
//
// C_SkillCardCollection (0x1031BDD0..0x1031DC30), collection manager FUN_1031bb20 -> 0x10BE3FE0 (ctor FUN_10316ac0):
//   +0x00 bonus sealed-pack progress    +0x04 collected {card id, progress, rank} (0xC each)
//   +0x10 pending cards {name, a, b}    +0x20 type -> SkillCardEntryContainer (types 0 1 4 5 6 7)
//   +0x3C 24 sealed-card purchase counts  +0x9C / +0xA0 the purchase in flight (type 0x18 = none, amount)
// Module init 0x1031B9E0: SMSG 0x648 list / 0x649 collected / 0x653 / 0x654 / 0x655 pending / 0x657 claim result / 0x66B
// sealed result / 0x6DE booster result / 0x66C SealedCardCosts row patch / 0x647 card patch; glue reset 0x10317DF0;
// SMSG_GAME_MODE_CHANGED resets the containers (FUN_1031bd50).
//
// C_SkillCard (0x102F5290..0x102F79A0), slot manager FUN_102f5140 -> 0x10BE3AC4: spec -> type (SKILL_CARD_* 0..8) -> slots
// of 9 bytes {u32 card id, u32 rank, u8 blocked}. Module init 0x102F50C0: SMSG 0x623 SET_SKILL_CARD_RESULT, 0x624 the whole
// list, 0x625 one slot; glue reset 0x102F41C0. CMSG 0x622 {u32 spec, str type, u32 slot, u32 card} (card 0 = remove).
//
// Checked against the genuine DLL: the filter reader (FUN_10314150 / lua_toarray FUN_103162d0 / FUN_103166c0), the
// filter query (FUN_1031dc30), the container's argument / text / populate-key slots (FUN_10214bf0 / FUN_102149a0 /
// FUN_10214e50), ByItem (FUN_102142a0), the collect path (FUN_10317ef0), the slot packet (FUN_102f4660), Blocked
// (FUN_102f51a0) and AnyUnblocked (FUN_102f4f70).
namespace CoA::SkillCard {
    struct Card {
        uint32_t id = 0;
        uint32_t cardId = 0;
        uint32_t rank = 0;
        uint32_t item = 0;
        uint32_t spell = 0;
        std::string type;
        std::string expansion;
        std::string cls;
        std::string quality;
        uint32_t wildcard = 0;
        uint32_t draft = 0;
        uint32_t starter = 0;
    };

    // The client questions; SkillCardClient.cpp installs them.
    struct Deps {
        bool (*hasPlayer)() = nullptr;                                 // 0x4038F0 found the active player
        uint8_t (*playerClass)() = nullptr;                            // FUN_100a0390: unit / player +0x5D, else 0
        uint32_t (*playerLevel)() = nullptr;                           // descriptor +0xD8
        uint32_t (*playerMoney)() = nullptr;                           // descriptor +0x1248
        uint32_t (*heldCount)(uint32_t item) = nullptr;                // FUN_10308470 + FUN_10308570
        bool (*spellName)(uint32_t spell, std::string& name) = nullptr;   // FetchSpell (0x100B2CB0), Spell +0x220
        std::string (*spellDescription)(uint32_t spell) = nullptr;     // FUN_101ac910(spell, 1, 1, 0)
    };

    Deps& Dependencies();

    const Card* ByItem(uint32_t item);                        // FUN_102142a0
    const Card* ByCardRank(uint32_t cardId, uint32_t rank);   // the (card, rank) map 0x10BE0678
    int QualityIndex(const std::string& quality, int fallback);   // FUN_101a33d0 (SKILL_CARD_COMMON..MAX)
    bool CollectedAtLeast(uint32_t cardId, uint32_t rank);    // FUN_1031bbf0
    uint32_t CollectedRank(uint32_t cardId);                  // FUN_103195a0
    void PushCard(lua_State* L, const Card& card);            // FUN_1009bc60
    int TypeIndex(const std::string& type);                   // FUN_10214060 (-1 when unknown)
    const char* TypeName(uint32_t type);                      // FUN_102f3fd0 (SKILL_CARD_* incl. MAX)
    bool AnyUnblocked(uint32_t spec);                         // FUN_102f4f70 (SkillCardSlots.cpp)

    // C_SkillCardCollection's packets.
    int32_t OnPatchCard(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);         // 0x647
    int32_t OnList(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);              // 0x648
    int32_t OnCollected(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);         // 0x649
    int32_t OnPendingList(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);       // 0x653
    int32_t OnPendingAdded(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);      // 0x654
    int32_t OnPendingRemoved(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);    // 0x655
    int32_t OnClaimResult(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);       // 0x657
    int32_t OnPurchaseResult(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);    // 0x66B
    int32_t OnCostPatch(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);         // 0x66C
    int32_t OnBoosterResult(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);     // 0x6DE
    void OnGlueScreen();                                                                       // 0x10317DF0

    // The collection module init (0x1031B9E0); also fills CustomStore's skill-card Deps and Wildcard's
    // skillCardUnblocked.
    void Init();
    // Tests only: the manager as constructed, the card store forgotten (reloaded from SkillCard.dbc on next use), Deps
    // forgotten.
    void Reset();
}

// C_SkillCard (SkillCardSlots.cpp).
namespace CoA::SkillCardSlots {
    int32_t OnSetResult(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);   // 0x623
    int32_t OnList(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);        // 0x624
    int32_t OnUpdate(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);      // 0x625
    void OnGlueScreen();                                                                 // 0x102F41C0

    void Init();   // 0x102F50C0
    // Tests only: no slots.
    void Reset();
}

#endif
