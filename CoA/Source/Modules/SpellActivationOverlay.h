#ifndef COA_SOURCE_MODULES_SPELLACTIVATIONOVERLAY_H
#define COA_SOURCE_MODULES_SPELLACTIVATIONOVERLAY_H

#include <cstdint>

// AscSpellActivationOverlay.cpp: C_SpellActivationOverlay, the original's subsystem at 0x1031f580..0x1031fcc0. World entry
// (0x1031f940) arms a 100 ms timer (0x1031fb60) whose tick (0x1031fb90) walks SpellActivationOverlays.dbc, evaluates each
// row (0x1031f640) and keeps the list of shown overlays (0x10be43d4), firing SHOW_ / HIDE_SPELL_ACTIVATION_OVERLAY
// ("%u", id); additions are appended in walk order, not sorted. The glue screen (0x1031f860) clears it and cancels the
// timer. The client questions go through Deps (SpellActivationOverlayClient.cpp), which also starts the timer at world
// entry.
//
// SpellActivationOverlays.dbc row: +0x04 spell, +0x08 texture str, +0x0C..+0x18 floats, +0x1C spell whose cooldown gates
// the overlay (0 = +0x04), +0x20 int, +0x24 trigger type NAME, +0x28 int, +0x2C stack / charge value, +0x30 comparison.
namespace CoA::SpellActivationOverlay {
    struct Cooldown {
        uint32_t start = 0;
        uint32_t duration = 0;
        uint8_t enabled = 0;
    };

    struct Deps {
        bool (*hasPlayer)() = nullptr;
        bool (*hasAura)(uint32_t spell) = nullptr;                        // the player has an aura of the spell
        bool (*auraStacks)(uint32_t spell, uint32_t& stacks) = nullptr;   // the player's aura of the spell: its +0x0E word
        Cooldown (*cooldown)(uint32_t spell) = nullptr;                   // Spell_C_GetSpellCooldown(spell, 0, ...)
        uint32_t (*now)() = nullptr;                                      // OsGetAsyncTimeMs
        bool (*startRecovery)(uint32_t spell, uint32_t& ms) = nullptr;    // Spell.dbc StartRecoveryTime (+0x238)
        uint32_t (*firstRank)(uint32_t spell) = nullptr;                  // AscSpellRank::FirstRank
    };

    Deps& Dependencies();

    // 0x1031fb90: one walk of the table.
    void Tick();
    // 0x1031f940 / 0x1031f860: arm the 100 ms timer / clear the list and cancel it.
    void Start();
    void Stop();

    void Init();
    // Tests only.
    void Reset();
}

#endif
