#ifndef COA_SOURCE_MODULES_EXTRAACTIONBUTTON_H
#define COA_SOURCE_MODULES_EXTRAACTIONBUTTON_H

#include <cstdint>

// AscExtraActionButton.cpp: C_ExtraActionButton, the original's subsystem at 0x10236840..0x10236f00. Not packet-driven:
// world entry (0x10236b00) arms a 100 ms timer (0x10236ca0) whose tick (0x10236cd0) walks ExtraActionButtons.dbc,
// evaluates each row's trigger (0x10236840) and keeps the ordered list of active buttons (0x10be24f8), firing
// EXTRA_ACTION_BUTTON_ADDED / _REMOVED ("%u", id). Removals fire in walk order; additions are sorted by the row's order
// (+0x24) first. The glue screen (0x10236a20) clears the list and cancels the timer. HIDE_ALL_EXTRA_ACTION_BUTTONS is
// registered but never fired. The client questions go through Deps (ExtraActionButtonClient.cpp), which also starts the
// timer at world entry.
//
// ExtraActionButtons.dbc row: +0x04 int, +0x08 / +0x0C / +0x10 strings, +0x14 trigger type NAME, +0x18 trigger argument
// (spell / item id), +0x24 sort order.
namespace CoA::ExtraActionButton {
    struct Cooldown {
        uint32_t start = 0;
        uint32_t duration = 0;
        uint8_t enabled = 0;
    };

    struct Deps {
        bool (*hasPlayer)() = nullptr;
        bool (*hasAura)(uint32_t spell) = nullptr;       // the player has an aura of the spell (0x7282A0)
        Cooldown (*spellCooldown)(uint32_t spell) = nullptr;   // Spell_C_GetSpellCooldown(spell, 0, ...)
        Cooldown (*itemCooldown)(uint32_t item) = nullptr;     // Spell_C_GetItemCooldown(item, ...)
        uint32_t (*now)() = nullptr;                     // OsGetAsyncTimeMs
        bool (*hasItem)(uint32_t entry) = nullptr;       // FUN_10308270: equipped, backpack or bag item of the entry
    };

    Deps& Dependencies();

    // 0x10236cd0: one walk of the table.
    void Tick();
    // 0x10236b00 / 0x10236a20: arm the 100 ms timer / clear the list and cancel it.
    void Start();
    void Stop();

    void Init();
    // Tests only.
    void Reset();
}

#endif
