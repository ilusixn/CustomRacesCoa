// AscItemUse.cpp / AscGameObjectUse.cpp. The item-use detour 0x10A42F30 routes the used item's entry:
//
//   0                       straight to the tail
//   0xF25D0                 MYSTIC_SCROLL_USED("%u", entry)          (FUN_102EA3A0)
//   0x1B9271                MYSTIC_ENCHANT_UNLOCK_PRESET_USED         (FUN_102EA330)
//   0xBDF08                 DICE_OF_DESTINY_USED                      (FUN_10A31830)
//   a SkillCard.dbc item    SKILL_CARD_COLLECTION_ITEM_USED("%u", entry)   (the item -> card map 0x10BE06C0,
//   or kSkillCardItems      FUN_1031BC20; its fallback functor 0x10BE0704 is never set)
//   kScrollOfFortuneItems   SCROLL_OF_FORTUNE_USED                    (FUN_10A31980)
//   0x138060 / 0x138061     CASE_OF_FORTUNE_USED                      (FUN_10A317B0)
//   otherwise               the ItemAddon record (FUN_101A2610); none -> the tail. Its +0x14 flag 0x400000 ->
//                           MYSTIC_SCROLL_USED("%u", entry); then Appearance::ItemUsed with the record's name.
//   tail, every use         CollectorCache::ItemUsed (FUN_10194230), then ITEM_USED("%u", entry).
//
// The game-object-use detour 0x10A42DD0: the mystic-altar branch (FUN_102EA1A0 / FUN_102EA2C0), then
// GAMEOBJECT_USED("%u", entry); FrameXML's FelCommutationUtil toggles Fel Commutation from it.
#include "CoA/Source/Modules/ObjectUse.h"
#include "CoA/Source/Modules/Appearance.h"
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/MysticEnchant.h"
#include "CoA/Source/Modules/SkillCard.h"
#include "CoA/Source/Runtime/Events.h"
#include <algorithm>
#include <iterator>

namespace {
    constexpr uint32_t kMysticScroll = 0xF25D0;
    constexpr uint32_t kPresetUnlockToken = 0x1B9271;
    constexpr uint32_t kDiceOfDestiny = 0xBDF08;
    const uint32_t kSkillCardItems[13] = { 0xBE2F7, 0xBE2F8, 0xBE2F6, 0x17C73, 0xBDF0D, 0xBE2FA, 0xFC352, 0x17E5D,
        0x17C74, 0x17E4D, 0x17E4E, 0x17E4C, 0x17E5E };
    const uint32_t kScrollOfFortuneItems[60] = { 0x10CDBC, 0x10F4CC, 0x111BDC, 0x1142EC, 0x1169FC, 0x11910C, 0x11B81C,
        0x11DF2C, 0xB799A, 0xB799B, 0xB799C, 0xB799D, 0xB79A7, 0xB79A8, 0xB79A9, 0xB79AA, 0xB79AB, 0xB79AC, 0xB79AD,
        0xB79AE, 0x17E79, 0x17E7A, 0x17E7B, 0x17E7C, 0x17E7D, 0x17E7E, 0x17E7F, 0x17E80, 0x17E81, 0x17E82, 0x17E83,
        0x17E84, 0x17E85, 0x17E86, 0x17E87, 0x17E88, 0x17E89, 0x17E8A, 0x17E8B, 0x17E8C, 0x17E8D, 0x17E8E, 0x17E8F,
        0x17E90, 0x17E91, 0x17E92, 0x17E93, 0x17E94, 0x17E95, 0x17E96, 0x17E97, 0x17E98, 0x17E99, 0x17E9A, 0x17E9B,
        0x17E9C, 0x17E9D, 0x17E9E, 0x17E9F, 0x17EA0 };
    const uint32_t kCaseOfFortune[2] = { 0x138061, 0x138060 };
    constexpr uint32_t kMysticScrollFlag = 0x400000;

    template <size_t N>
    bool In(const uint32_t (&list)[N], uint32_t entry) {
        return std::find(std::begin(list), std::end(list), entry) != std::end(list);
    }

    void Route(uint32_t entry) {
        if (entry == kMysticScroll) {
            CoA::Events::Signal("MYSTIC_SCROLL_USED", "%u", entry);
        } else if (entry == kPresetUnlockToken) {
            CoA::Events::Signal("MYSTIC_ENCHANT_UNLOCK_PRESET_USED", nullptr);
        } else if (entry == kDiceOfDestiny) {
            CoA::Events::Signal("DICE_OF_DESTINY_USED", nullptr);
        } else if (CoA::SkillCard::ByItem(entry) || In(kSkillCardItems, entry)) {
            CoA::Events::Signal("SKILL_CARD_COLLECTION_ITEM_USED", "%u", entry);
        } else if (In(kScrollOfFortuneItems, entry)) {
            CoA::Events::Signal("SCROLL_OF_FORTUNE_USED", nullptr);
        } else if (In(kCaseOfFortune, entry)) {
            CoA::Events::Signal("CASE_OF_FORTUNE_USED", nullptr);
        } else if (const CoA::ItemAddon::Record* addon = CoA::ItemAddon::Find(entry)) {
            if (addon->fields[1] & kMysticScrollFlag) {
                CoA::Events::Signal("MYSTIC_SCROLL_USED", "%u", entry);
            }

            CoA::Appearance::ItemUsed(entry, addon->name ? addon->name : "");
        }
    }
}

void CoA::ItemUse::Used(uint32_t entry) {
    if (entry) {
        Route(entry);
    }

    CoA::CollectorCache::ItemUsed(entry);
    CoA::Events::Signal("ITEM_USED", "%u", entry);
}

void CoA::GameObjectUse::Used(uint32_t entry) {
    CoA::MysticEnchant::GameObjectUsed(entry);
    CoA::Events::Signal("GAMEOBJECT_USED", "%u", entry);
}

void CoA::ObjectUse::Init() {
    COA_EVENT("MYSTIC_SCROLL_USED");
    COA_EVENT("MYSTIC_ENCHANT_UNLOCK_PRESET_USED");
    COA_EVENT("DICE_OF_DESTINY_USED");
    COA_EVENT("SKILL_CARD_COLLECTION_ITEM_USED");
    COA_EVENT("SCROLL_OF_FORTUNE_USED");
    COA_EVENT("CASE_OF_FORTUNE_USED");
}
