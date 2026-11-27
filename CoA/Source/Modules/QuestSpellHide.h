#ifndef COA_SOURCE_MODULES_QUESTSPELLHIDE_H
#define COA_SOURCE_MODULES_QUESTSPELLHIDE_H

#include <cstdint>

// AscQuestSpellHide.cpp (FUN_10112620): spell 0xE4E2D's effects and visual are hidden while quest 0x1D0BB4 is logged.
namespace CoA::QuestSpellHide {
    constexpr uint32_t kSpell = 0xE4E2D;
    constexpr uint32_t kQuest = 0x1D0BB4;

    struct Deps {
        bool (*questInLog)(uint32_t quest) = nullptr;   // FUN_101940a0 over the client's quest log
        uint32_t (*effectSpell)(void* effect) = nullptr;   // CEffect +0x18
    };

    Deps& Dependencies();

    bool HiddenSpell(uint32_t spell);   // FUN_10112560
    bool RemoveEffect(void* object, void* effect);   // 0x10112650
    bool HideVisual(uint32_t kind, uint32_t guidLo, uint32_t guidHi, uint32_t spell);   // 0x10112640

    // Tests only.
    void Reset();

    void Init();
}

#endif
