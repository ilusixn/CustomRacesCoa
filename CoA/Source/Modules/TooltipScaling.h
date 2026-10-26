#ifndef COA_SOURCE_MODULES_TOOLTIPSCALING_H
#define COA_SOURCE_MODULES_TOOLTIPSCALING_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class CDataStore;
struct lua_State;

// AscTooltipScaling.cpp: item-scaling tooltips (module init FUN_1027fb60, 22 hooks + SMSG 0x716).
//
// Items carry a level of their own (the item addon field, ObjectAddon::Item) or take one from context -- the trade
// target's slot, a loot roll, an auction entry, a guild bank slot, a quest's scaling row, the looted / inspected unit.
// FUN_1027eee0 (ScaledRecord) picks that level and asks the scaled item-stat store (ItemStats) for the record; the
// hooks feed it the context and let the tooltip, money line, feral attack power and repair cost read the scaled
// record instead of the item cache. The tooltip state ItemStats.h keeps (TooltipGuid / InspectedSlot / LootRollLevels)
// is shared with it.
//
//   0x6277F0 around   (sub_1027c870)  CGTooltip::SetItem: the context recorded for the call, two branches forced
//                                     (ClientHooks::options tooltipItemSet / itemComparison), all cleared after
//   0x61FEC0 replaced (FUN_1027b110)  AddLine: Classes / damage / stat / armor / item level / required level / DPS /
//                                     "Equip:" lines from the scaled record, else the tooltip's OnTextChanged
//   0x62D930 before   (FUN_1027cb00)  SetItemDelta: the comparison deltas from scaled records
//   0x61A0D0 before   (sub_10287600)  AddMoney: the scaled sell price
//   0x61A8F0 around   (sub_10287640)  GetFeralAttackPower from the scaled damage
//   0x708540 replaced (sub_1027ae80)  CGItem_C::GetRepairCost from the item's own level
//   0x754400 after    (sub_1027ade0)  CGBag_C::GetItemPointer: the item object it returns is kept
//   0x5E9250 after    (sub_1027ae00)  CanEquipItemInSlot also passes 5 levels under a scaled required level
//   0x61B4A0 after    (sub_1027c7f0)  #GameTooltip GetItemGUID / ClearLine
//   0x49EAB0 / 0x49EB70 / 0x49EC80    Has/Get/SetScript: a GameTooltip's "OnTextChanged" kept here (map 0x10BE2F80)
//   Set*Item context: SetQuestItem 0x62E670, SetQuestLogItem 0x62E790, SetTradeSkillItem 0x62EAE0,
//     SetTradePlayerItem 0x62EFF0, SetTradeTargetItem 0x62F1E0, SetInboxItem 0x62F9E0, SetAuctionItem 0x62FCF0,
//     SetLootRollItem 0x6300A0, SetGuildBankItem 0x6304A0, SetHyperlinkCompareItem 0x631B60
//   SMSG 0x716: the 19 inspected-slot item levels
// 0x627980 is only ever written with its stock jne (FUN_1027d4f0 restores it after each item), so nothing changes it.
namespace CoA::TooltipScaling {
    // The item-cache fields the module reads (0x67CA30's ItemStats_C, 32-bit offsets).
    struct CacheItem {
        uint32_t cls = 0;               // +0x04
        uint32_t subClass = 0;          // +0x08
        uint32_t quality = 0;           // +0x14
        uint32_t inventoryType = 0;     // +0x28
        uint32_t itemLevel = 0;         // +0x34
        uint32_t requiredLevel = 0;     // +0x38
        uint32_t statType[10] = {};     // +0x68
        uint32_t statValue[10] = {};    // +0x90
        uint32_t damageMin[2] = {};     // +0xC0 (float bits)
        uint32_t damageMax[2] = {};     // +0xC8
        uint32_t resistances[7] = {};   // +0xD8 (armor first)
        uint32_t delay = 0;             // +0xF4
        uint32_t randomProperty = 0;    // +0x19C
        uint32_t block = 0;             // +0x1A4
    };

    struct Player {
        uint64_t guid = 0;
        uint32_t level = 0;
        uint32_t cls = 0;               // UNIT_FIELD_BYTES_0 byte 1; 0 unless the object is a unit / player
    };

    struct Deps {
        bool (*itemCache)(uint32_t item, CacheItem& out) = nullptr;           // 0x67CA30(0xC5D828, item, 0, 0, 0, 0)
        bool (*player)(Player& out) = nullptr;                                 // 0x4038F0
        bool (*lootUnitLevel)(uint32_t& level) = nullptr;                     // CGLootInfo::m_lootObj (0xBFA8D8), mask 8
        bool (*inspectTargetExists)() = nullptr;                               // CGCharacterInfo::m_inspectTarget, mask 0x10
        uint32_t (*tradeLevel)(uint32_t slot) = nullptr;                      // *(0xCA0DE0 + slot * 0x50)
        bool (*itemEntry)(uint64_t guid, uint32_t& entry) = nullptr;          // an item object (mask 2): descriptor +0xC
        const char* (*globalString)(const char* key, int32_t flag) = nullptr; // 0x819D40(key, -1, flag)
        // ChrClasses in id order (min .. max, absent rows skipped): {row byte 0, name +0x10}.
        std::vector<std::pair<uint8_t, const char*>> (*classes)() = nullptr;
        // DurabilityCosts row index level - 1, dword column + 1 (FUN_100b2c50); false without a row.
        bool (*durabilityCost)(uint32_t level, uint32_t column, uint32_t& cost) = nullptr;
        // DurabilityQuality row index quality * 2 + 1, its float (FUN_100b2c70); false without a row.
        bool (*durabilityQuality)(uint32_t quality, float& multiplier) = nullptr;
    };

    Deps& Dependencies();

    const uint8_t* ScaledRecord();   // FUN_1027eee0

    // sub_1027c870: before CGTooltip::SetItem (skipLock may be cleared) and after it.
    void BeforeSetItem(uint32_t item, const uint64_t* requester, const uint64_t* itemGuid, int32_t& skipLock);
    void AfterSetItem();

    // FUN_1027b110: what AddLine does with a line.
    struct Line {
        enum Kind {
            Pass,      // the tooltip's OnTextChanged (if any), then the client with `left` / right
            Rewrite,   // the client with `left` (and right) as given, no OnTextChanged
            Drop,      // nothing
        };
        Kind kind = Pass;
        bool hasLeft = false;
        std::string left;
        // The required-level line colours both texts: 0 keeps the caller's, 1 the tooltip text colour (0xAD2D30), 2 red
        // (0xAD2D34).
        int colour = 0;
    };
    Line AddLine(const char* left, const char* right, uint32_t leftColour);

    // FUN_1027cb00: the deltas written into the client's comparison summary (dword view, 0 the float DPS).
    void CompareDeltas(int32_t* summary);
    uint32_t Money(uint32_t money);   // sub_10287600
    // sub_10287640: true when the module computed the feral attack power itself; damage is the scaled average either way.
    bool FeralAttackPower(const CacheItem& item, float& damage, int32_t& result);
    // sub_1027ae80.
    uint32_t RepairCost(uint32_t entry, uint64_t guid, uint32_t durability, uint32_t maxDurability);
    // sub_1027ae00, after a 0 from the client: 1 when the kept item object's level - 5 reaches the player's level.
    int32_t CanEquip(int32_t result, uint32_t requiredLevel, bool haveLastItem, uint64_t lastItemGuid);

    // The Set*Item context hooks.
    void SetQuestContext(uint32_t quest);    // sub_10283fe0 / sub_10284040 with *0xC0D65C / *0xC23AD8
    void SetTradeSkill(bool on);             // sub_102840a0 (the flag nothing reads)
    void BeginTrade(lua_State* L);           // FUN_1027b070 / FUN_1027b0c0: slot = argument 2 - 1
    void EndTrade(bool clearFlag);           // only the target's clears the trade flag
    void SetInboxItem(uint32_t itemGuidLow); // sub_1027afc0
    void BeforeAuctionItem(lua_State* L);    // FUN_10283e00 (ItemStats' auction Deps)
    void BeginLootRoll(lua_State* L);        // FUN_10283f90
    void EndLootRoll();
    void SetGuildBankLevel(uint32_t level);  // FUN_10283f20
    // sub_1027af70: false when a guild banker is open (the client's call is skipped and 0 returned).
    bool BeginHyperlinkCompare(bool bankerOpen);
    void EndHyperlinkCompare();

    // GetItemGUID's GUID (0x10BE2EB8).
    uint64_t TooltipGuid();

    // The GameTooltip OnTextChanged refs (0x10BE2F80).
    bool OnTextChanged(const void* tooltip, int& ref);
    void SetOnTextChanged(const void* tooltip, int ref);

    int32_t OnInspectLevels(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);   // SMSG 0x716

    // Tests only.
    uint32_t InspectLevel(uint32_t slot);
    void Init();
    void Reset();
}

#endif
