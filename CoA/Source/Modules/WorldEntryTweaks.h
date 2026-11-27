#ifndef COA_SOURCE_MODULES_WORLDENTRYTWEAKS_H
#define COA_SOURCE_MODULES_WORLDENTRYTWEAKS_H

#include <cstdint>

// AscWorldEntryTweaks.cpp: the after-world-entry callbacks; each code rewrite is a switch the decomp site reads.
// See docs/2026-11-15-addon-watch-and-world-entry-notes.md (the anti-cheat and kill switch are not ported).
namespace CoA::WorldEntryTweaks {
    struct Deps {
        bool (*hasPlayer)() = nullptr;
        uint8_t (*playerClass)() = nullptr;
        // SpellShapeshiftForm row's flag 2 (+0xC) on or off.
        void (*setFormFlag)(uint32_t form, bool on) = nullptr;
    };

    Deps& Dependencies();

    // FUN_100c6380: classes 1..9 and 11.
    bool StockClass(uint8_t cls);

    // The callbacks in the original's order: FUN_10a5dc40, FUN_10a60440, FUN_10a73b70, FUN_10a50e70.
    void OnEnteredWorld();

    // Spell_C_HaveEquippedSpellItems: true skips the ammo check (0x809572 jmp; never turned back).
    bool SkipAmmoCheck();
    // CGPlayer_C::CanUseItem: true skips the proficiency check (0x6DC45F jmp; set on every entry).
    bool SkipProficiency();

    // Tests only.
    void Reset();

    void Init();
}

#endif
