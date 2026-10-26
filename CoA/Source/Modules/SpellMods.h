#ifndef COA_SOURCE_MODULES_SPELLMODS_H
#define COA_SOURCE_MODULES_SPELLMODS_H

#include <array>
#include <cstdint>
#include <vector>

class CDataStore;

// AscSpellMods.cpp, the original's spell-modifier installer FUN_10324a90: server-driven spell modifiers, a few aura-type
// gates on casting, and the transient code patches around a spell cast. The client half (SpellModsClient.cpp) is what
// the decomp sites call; it converts the client's records and fills Deps.
//
//   SMSG 0x64F  handler_0x064F: u32 n, then n x 5 u32 -> Spell_C_SetCooldownLeft (0x805B10).
//   0x7FDC60    Packet_Group_32 (SMSG_SET_FLAT / _PCT_SPELL_MODIFIER) replaced by 0x10325B70.
//   0x7FD970    Spell_C_GetSpellModifiers replaced by 0x103288A0.
//   0x8012F0 / 0x7FF100  Spell_C_GetPowerCost / _GetPowerCostPerSecond: unit addon float scaling (0x10328CB0 / 0x10328DB0).
//   0x7FF180    SpellRec::GetCastTime: SpellAddon type-0x15D effects scale it (0x103284E0).
//   0x802C30    Spell_C::GetSpellRange: the min range zeroed under aura type 0x138 (0x103287F0).
//   0x71AF90 / 0x71B820  CGUnit_C::IsInMelee / IsInMeleeRange gates for Auto Shot (0x10324390 / FUN_10324300).
//   0x80CCE0    Spell_C_CastSpell: the cast's code patches around the original (FUN_103273F0).
//   0x806200    Spell_C_CancelSpell: a null cast link is never passed on; the 0x139 skip (FUN_10327240).
//   Lists       glue-screen clear (0x103243F0); the aura flag before 0x724820 / 0x71E930; two vetoes after 0x80B5D0.
//
// The original rewrites client bytes; each rewrite here is a switch the decomp site reads (Plan 13 ruling).
namespace CoA::SpellMods {
    // The Spell.dbc fields read here (the original reads them at the 32-bit offsets named).
    struct Spell {
        uint32_t id = 0;                // +0x000
        uint32_t attributesExC = 0;     // +0x01C
        uint32_t casterAuraSpell = 0;   // +0x060
        uint32_t targetAuraSpell = 0;   // +0x064
        uint32_t powerType = 0;         // +0x0A4
        uint32_t manaPerSecond = 0;     // +0x0B0
        uint32_t cumulativeAura = 0;    // +0x0C4
        uint32_t effectAura[3] = {};    // +0x17C
        uint32_t effectMisc[3] = {};    // +0x1B8
        uint32_t maskA[3] = {};         // +0x1E8
        uint32_t maskB[3] = {};         // +0x1F4
        uint32_t maskC[3] = {};         // +0x200
        uint32_t classSet = 0;          // +0x240
        uint32_t classMask[3] = {};     // +0x244
        uint32_t defenseType = 0;       // +0x254
        uint32_t schoolMask = 0;        // +0x284
    };

    struct Aura {
        uint64_t caster = 0;
        uint32_t spell = 0;
        uint16_t stack = 0;   // the word at +0xE
    };

    // The client questions; SpellModsClient.cpp installs them. Units are the client's CGUnit_C*.
    struct Deps {
        bool (*fetchSpell)(uint32_t spell, Spell& out) = nullptr;                                    // AscScript::FetchSpell
        std::vector<std::array<uint32_t, 3>> (*spellAffects)(uint32_t spell) = nullptr;              // AscBridgeStore
        std::vector<uint32_t> (*rankChain)(uint32_t firstRank) = nullptr;                            // AscSpellRank::Chain
        bool (*customAttr18)(uint32_t spell, uint32_t& value) = nullptr;                             // SpellCustomAttr +0x18
        void (*setCooldownLeft)(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t f, uint32_t g) = nullptr;
        bool (*affectedByAura)(void* unit, uint32_t type, uint32_t spell) = nullptr;                 // 0x7283A0
        bool (*hasAura)(void* unit, uint32_t spell) = nullptr;                                       // 0x7282A0
        bool (*isActivePlayer)(void* unit) = nullptr;                                                // 0x4CEE50
        uint32_t (*invSlotLow)(void* unit, uint32_t slot) = nullptr;                                 // FUN_1008d8a0
        bool (*inManastorm)() = nullptr;
        uint32_t (*autoRepeatingSpell)() = nullptr;                                                  // *0xD397D0
        uint32_t (*basePower)(void* unit, uint32_t powerType) = nullptr;                             // 0x71B770
        uint64_t (*unitGuid)(void* unit) = nullptr;
        void* (*activePlayer)() = nullptr;                                                           // 0x4038F0
        uint32_t (*auraCount)(void* unit) = nullptr;                                                 // 0x4F8850
        bool (*auraAt)(void* unit, uint32_t index, Aura& out) = nullptr;                             // 0x556E10
        uint32_t (*auraIndex)(void* unit, uint32_t spell) = nullptr;                                 // 0x7289C0
        const int32_t* (*auraAmounts)(uint64_t unit, uint64_t caster, uint32_t spell) = nullptr;     // AscAuraAmounts
        const uint8_t* (*spellAddon)(uint32_t spell) = nullptr;                                      // map 0x10BE0708
        int32_t (*castTime)(const void* rec) = nullptr;                                              // 0x7FF180(rec, 0, 0, 0)
        bool (*errorSpeech)() = nullptr;          // Sound_EnableErrorSpeech missing or its int value non-zero
        uint32_t (*castingSpellId)(void* unit) = nullptr;                                            // unit +0xA6C
        int32_t (*uiCastingSpell)(void* player) = nullptr;                                           // player +0xA60
        void (*setSpellRangeFlags)(uint32_t range, uint32_t flags) = nullptr;                        // SpellRange.dbc +0x14
        void (*setModalSkip)(bool on) = nullptr;                                                     // 0x80DB37 jne / jmp
        void (*fatal)(const char* message) = nullptr;                                                // AscCrashContext::Assert
    };

    Deps& Dependencies();

    // ---- packets -----------------------------------------------------------------------------------------------
    // handler_0x064F.
    int32_t OnCooldowns(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
    // 0x10325B70, replacing Packet_Group_32 (0x266 = flat): u8 isFamily, u8 bit; isFamily: u32 value, u32 spell; else
    // u8 op, u32 value, u32 family -> table [bit * 0x1F + family * 0x11A0 + op].
    int32_t OnSpellModifier(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);

    // ---- detours -----------------------------------------------------------------------------------------------
    // 0x103288A0, replacing Spell_C_GetSpellModifiers: the sum for (spell, op) over both tables and the per-spell maps.
    bool ModifierSum(uint32_t spell, uint32_t classSet, const uint32_t classMask[3], uint32_t attributesExC, uint32_t op,
        int32_t& flat, int32_t& pct);
    // 0x10328DB0 before Spell_C_GetPowerCostPerSecond: true when SpellCustomAttr +0x18 & 0x20000 replaces the cost.
    bool PowerPerSecondReplaced(const Spell& spell, void* unit, int32_t& out);
    // 0x10328DB0 after it: the unit addon float scaling.
    int32_t AfterPowerPerSecond(const Spell& spell, void* unit, int32_t result);
    // 0x10328CB0 after Spell_C_GetPowerCost; rec is the client's record, for the cast-time call.
    int32_t AfterPowerCost(const Spell& spell, const void* rec, void* unit, int32_t result);
    // 0x103284E0 after SpellRec::GetCastTime.
    int32_t AfterCastTime(const Spell* spell, int32_t result);
    // 0x103287F0 after Spell_C::GetSpellRange (the third argument, the min range).
    void AfterSpellRange(void* unit, uint32_t spell, float* minRange);
    // 0x10324390 before CGUnit_C::IsInMelee: true returns 1.
    bool InMeleeOverride(void* unit);
    // FUN_10324300 before CGUnit_C::IsInMeleeRange: true returns 0.
    bool MeleeRangeVeto(void* unit);
    // FUN_103273F0 before / after Spell_C_CastSpell.
    void BeforeCastSpell(void* unit, uint32_t spell);
    void AfterCastSpell();
    // FUN_10327240 over Spell_C_CancelSpell from Spell_C_StopCasting (0x809AB9) with a null cast link: the assert.
    void NullCancelFromStopCasting();
    // From Spell_C_CancelCastingSpellByMovement (0x8066DA): true keeps the cast (aura type 0x139 on the player).
    bool KeepCastOnMove(uint32_t spell);

    // ---- lists -------------------------------------------------------------------------------------------------
    bool AuraFlagOn(const Spell& aura);    // 0x10325AA0, before 0x724820
    bool AuraFlagOff(const Spell& aura);   // 0x10325B50, before 0x71E930
    bool StackRequirement(void* unit, const Spell& spell);   // 0x103267E0, after 0x80B5D0
    bool AuraRequirement(void* unit, const Spell& spell);    // 0x10326780, after 0x80B5D0
    void ClearModifiers();                 // 0x103243F0, every glue screen

    // ---- the switches the decomp sites read ---------------------------------------------------------------------
    bool CastWhileMoving();   // FUN_10323d50: 0x73A0D0..0x73A0EF (SPELL_FAILED_MOVING) NOPed
    bool SheatheAnimates();   // 0x7370AF je -> jmp (CGUnit_C::SetSheatheState always animates)
    bool ActivateSound();     // 0x80D6EF..0x80D711 stock (Spell_C_CastSpell's activate sound)
    bool FizzleSound();       // 0x808323 je stock (Spell_C_SpellFailed's fizzle sound)
    bool AuraFlag();          // 0x10BE44A0

    // Tests only: a table entry, the per-spell maps' entry, and reset of everything.
    int32_t FlatAt(uint32_t index);
    int32_t PctAt(uint32_t index);
    int32_t SpellEntry(bool flat, uint32_t spell, uint32_t op);
    void Reset();

    void Init();
}

#endif
