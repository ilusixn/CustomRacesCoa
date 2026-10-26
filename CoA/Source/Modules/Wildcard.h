#ifndef COA_SOURCE_MODULES_WILDCARD_H
#define COA_SOURCE_MODULES_WILDCARD_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// AscWildcardRolls.cpp / AscWildcardSets.cpp / AscWildcardRapid.cpp: C_Wildcard (61 natives) and the two managers
// behind it.
//
// WildcardModeMgr (accessor FUN_10a315d0, static 0x10D3D0D8; module init 0x10a31460 -- glue reset FUN_10a2f950,
// enter-world FUN_10a318b0, 22 events, 8 packet handlers; SMSG 0x6F4's own init 0x10a3a270):
//   +0x08 / +0x58   two server counters per spec (SMSG 0x6EE / 0x6EF), the quick-roll thresholds' counts
//   +0xA8 / +0xF8 / +0x148   repurchasable rolls / ability rolls / talent rolls per spec
//   +0x198          the reveal records (0x2C each, SMSG 0x621): {entry, first-rank spell, ranks gained, pre-roll rank,
//                   stop code, the 1.5 s reveal timer}; CA reads them (CoA::CA::WildcardReveals)
//   +0x1A4          spellbook adds (0x542030) deferred while their spell's reveal runs, by entry
//   DAT_10d3d0a4 / a8 / ac   the roll-ready poll's timer, its last answer, the tick until which it polls every 50 ms
//   DAT_10d3d0b0    an unlearn (CMSG 0x61D) is outstanding
// Each per-spec array is indexed by FUN_1016f580 unchecked: spec -1 (no spec yet) reads the dword before it.
//
// WildcardMgr (accessor FUN_10a349d0, static 0x10D3D318; module init 0x10a34970 -- SMSG 0x6F0, glue reset 0x10a33940,
// enter-world 0x10a33c90, the CA lists learned / unlearned / spec changed):
//   +0x00 / +0x0C   the desired / undesired roll lists, 8-byte entries {u32 id, u8 kind, u8 is-ability, u8 is-talent}
//                   (kind: Ability 1, Talent 2, TalentAbility 4 -- CharacterAdvancement rows; Tag 5 -- SpellTagTypes
//                   rows; Suggestion 6 -- id 1 "suggested abilities" / 2 "suggested talents")
//   +0x18 / +0xF8   the desired / undesired entry browsers (WildcardEntryContainerBase)
//   +0x1D8          the quick-roll configs {u8 kind, u32 threshold, u32 max} (SMSG 0x6F0)
//   +0x1E8          the rapid-rolling session; +0x268 the session counter; +0x270 the phase a cancel was requested in
//
// Every client question goes through CoA::Wildcard::Deps (WildcardClient.cpp).
namespace CoA::Wildcard {
    struct Deps {
        bool (*hasPlayer)() = nullptr;                                   // 0x4038F0 found the active player
        uint32_t (*playerLevel)() = nullptr;                             // descriptor +0xD8
        uint32_t (*playerMoney)() = nullptr;                             // descriptor +0x1248 (PLAYER_FIELD_COINAGE)
        uint32_t (*itemCount)(uint32_t entry) = nullptr;                 // FUN_10308570: the currency-token slots
        uint32_t (*tickCount)() = nullptr;                               // GetTickCount (0x10b1907c)
        uint32_t (*firstRank)(uint32_t spell) = nullptr;                 // AscSpellRank::FirstRank (FUN_102163f0)
        // The spellbook add itself (0x542030 through the detour's trampoline).
        void (*addKnownSpell)(uint32_t spell, uint32_t notify, uint32_t replace) = nullptr;
        // AscSkillCard::AnyUnblocked (FUN_102f4f70): a slotted, unblocked skill card in the spec. Not ported yet; unset
        // answers as the original does with no cards (false).
        bool (*skillCardUnblocked)(uint32_t spec) = nullptr;
    };

    Deps& Dependencies();

    // The two managers' module inits (0x10a31460 + 0x10a3a270, 0x10a34970); also fills the CA Deps that ask them.
    void Init();
    // Tests only: both managers back to their constructed state, Deps forgotten.
    void Reset();
}

// The mode manager (WildcardRolls.cpp).
namespace CoA::WildcardRolls {
    uint32_t SpecCounter(int which);   // FUN_10a2fe60 (0: +0x08) / FUN_10a2fe70 (1: +0x58) for the active spec
    bool RevealsPending();             // +0x198 not empty
    bool AtMaxLevel();                 // FUN_1008e380 on the active player
    void ArmPoll();                    // FUN_10a31760: 50 ms polls for a second, then 1 s; WILDCARD_ROLL_READY on the edge
    uint32_t StartingCount();          // FUN_10154f50
    bool CanShowStartingChoice();      // FUN_10a2f920
    bool CanAffordRoll(bool talent);   // FUN_1016f390, then FUN_10155120 / FUN_10155160

    // AscRuntime.cpp's detour 0x10275ED0 on 0x542030 (the spellbook add): false while the module's own add runs (the
    // client body proceeds); otherwise the add is deferred (FUN_10a31b00) or made (FUN_102781e0) and true returned.
    bool AddKnownSpellHook(uint32_t spell, uint32_t notify, uint32_t replace);
}

// The WildcardMgr's desired / undesired lists (WildcardSets.cpp).
namespace CoA::WildcardSets {
    // FUN_10a34d10: {entry, kind} is on the undesired list (the suggestion manager's filter).
    bool IsUndesired(uint32_t id, uint8_t kind);
}

// The rapid-rolling session of the WildcardMgr (WildcardRapid.cpp).
namespace CoA::WildcardRapid {
    bool Active();              // FUN_10a34ce0: phase 1..5
    bool InFlight();            // FUN_10a34cb0: a cancel pending or phase 1..3
    void MarkRerollPending();   // FUN_10a2f910: an unlearn request (CMSG 0x61D) is outstanding
    bool TakeRerollPending();   // read and clear it (SMSG 0x61E)
    // FUN_10a33b10 (world entry, the glue reset): the session back to its default (id 0, stop code
    // STOP_RAPID_ROLLING_NONE) and the cancel phase (+0x270) to 0.
    void ResetSession();

    // The session's part of WildcardRolls.cpp's handlers: true when the session consumed it.
    bool OnUnlearnResult(const std::string& result);   // FUN_10a35c20 (SMSG 0x61E)
    bool OnRollResult(const std::string& result);      // FUN_10a359c0 (SMSG 0x644)
    void OnEntryLearned(uint32_t entry, uint32_t newRank, uint32_t preRoll, const std::string& stop);   // FUN_10a355d0
    void OnRevealDone(uint32_t entry);                 // FUN_10a35870
}

#endif
