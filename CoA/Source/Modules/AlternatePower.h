#ifndef COA_SOURCE_MODULES_ALTERNATEPOWER_H
#define COA_SOURCE_MODULES_ALTERNATEPOWER_H

#include <cstdint>

// AscAlternatePower.cpp: two small module inits and one callback of the attach init. Checked against the genuine DLL.
//
// FUN_10321bf0 (alternate power, once): the UnitAlternativePower* globals (UnitAddon.cpp), the event
// UNIT_POWER_ALTERNATIVE_UPDATE, a world-entry clear of the map 0x10BE4430 (nothing ever inserts into it, so not kept),
// and after every 0x6E7F50 (CGPlayer_C::PostInitActivePlayer, list 0x10BE2DB8) callbacks (LAB_10321bb0) on the player's
// addon fields 0 and 1 (FUN_102ce900, arg 4) that fire the event with field 0 (the power type).
//
// FUN_10322230 (the jump cast): after a spell is added to the spellbook (list 0x10BE2AEC, ClientHooks::afterAddKnownSpell)
// a spell whose SpellCustomAttr.dbc +0x14 has 0x1000 is remembered at 0x10BE4454 (nothing reads it) and one with 0x2000 at
// 0x10BE4458 (FUN_10322270). On key event 0x20 (FUN_103222b0, before 0x4943C0 in world) the player cancels that spell's
// aura when they have it (0x7282A0 / 0x802F80), else casts it on themself while falling (movement flag 0x1000, 0x80DA40).
// Its key-up / key-repeat registrations are the empty stub 0x1008D890.
//
// FUN_10a63e24's list callback 0x10A3AB50 (after every 0x6E7F50): the type callback for field 4 of kind-3 objects
// (0x10A4BAF0) -- 1 there zeroes a living unit's health descriptor (+0x60) and runs CGUnit_C::OnDeath (0x729220).
namespace CoA::AlternatePower {
    // The client questions; AlternatePowerClient.cpp installs them.
    struct Deps {
        uint64_t (*objectGuid)(void* object) = nullptr;     // object +8 -> the guid
        // 0x4D4DB0(guid, 8): a unit whose health (+0x60) is not 0 gets 0 and OnDeath (0x729220).
        void (*forceDeath)(uint64_t guid) = nullptr;
        bool (*hasPlayer)() = nullptr;                      // 0x4038F0
        bool (*hasAura)(uint32_t spell) = nullptr;          // 0x7282A0 on the player
        bool (*spellExists)(uint32_t spell) = nullptr;      // FetchSpell (0x100B2CB0)
        void (*cancelAura)(uint32_t spell) = nullptr;       // 0x802F80(rec, player, 0)
        bool (*falling)() = nullptr;                        // player +0xD8 (movement) +0x44 & 0x1000
        void (*castOnSelf)(uint32_t spell) = nullptr;       // 0x80DA40(spell, 0, player guid, 0)
    };

    Deps& Dependencies();

    void WatchAlternatePower(void* player);   // FUN_10321b60
    void RegisterForcedDeath(void* player);   // 0x10A3AB50
    void AddedSpell(uint32_t spell);          // FUN_10322270
    void KeyDown(const void* event);          // FUN_103222b0
    uint32_t FlagSpell();                     // 0x10BE4454
    uint32_t JumpSpell();                     // 0x10BE4458

    void Init();
    // Tests only: both spells 0, Deps forgotten.
    void Reset();
}

#endif
