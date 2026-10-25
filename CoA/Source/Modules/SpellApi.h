#ifndef COA_SOURCE_MODULES_SPELLAPI_H
#define COA_SOURCE_MODULES_SPELLAPI_H

// AscSpellApi.cpp: C_Spell (handler_AppliesBuff .. handler_IsTrainerSpell, GetMaxLearnableRank, GetFirstRank) and
// FUN_10325130, the "is this effect positive" test SpellVisuals also uses. The original reads Spell.dbc records as
// dwords ([0] id, [3] mechanic, [4] attributes, [0x47+i] effect, [0x4A+i] die sides, [0x50+i] base points,
// [0x56+i] / [0x59+i] implicit targets, [0x5F+i] aura, [0x6E+i] misc value, [0x74+i] trigger spell, [0x85] icon,
// [0x90] family, [0x91] family flags, +0x9C spell level); SpellFields carries exactly those.
// HasRuneUI lives with character advancement (CANatives.cpp).
#include <cstdint>
#include <vector>

namespace CoA::SpellApi {
    struct SpellFields {
        uint32_t id = 0;
        uint32_t mechanic = 0;      // [3]
        uint32_t attributes = 0;    // [4]
        uint32_t effect[3] = {};
        uint32_t dieSides[3] = {};
        uint32_t basePoints[3] = {};
        uint32_t targetA[3] = {};
        uint32_t targetB[3] = {};
        uint32_t aura[3] = {};
        uint32_t miscValue[3] = {};
        uint32_t triggerSpell[3] = {};
        uint32_t iconId = 0;        // [0x85]
        uint32_t family = 0;        // [0x90]
        uint32_t familyFlags = 0;   // [0x91]
        uint32_t spellLevel = 0;    // +0x9C
    };

    // FUN_10325130: effect i of the spell is positive; `nested` stops the trigger-spell recursion.
    bool IsPositiveEffect(const SpellFields& s, uint32_t i, bool nested);

    // The 0x934 SKILL_LINE_ABILITY patch (ClientDbcPatch): FUN_10328a30(spell) -- 0 rebuilds the spell -> ability map,
    // otherwise the FIRST row naming the spell is mapped -- and FUN_103266c0, which erases the spell.
    void SkillLineAbilityUpdate(uint32_t spell);
    void SkillLineAbilityErase(uint32_t spell);

    struct Deps {
        bool (*spell)(uint32_t id, SpellFields& out) = nullptr;                    // FetchSpell
        uint32_t (*firstRank)(uint32_t spell) = nullptr;                           // AscSpellRank::FirstRank
        const std::vector<uint32_t>& (*chain)(uint32_t firstRank) = nullptr;       // AscSpellRank::Chain
        bool (*hasPlayer)() = nullptr;                                             // ActivePlayer() != null
        // SkillLineAbility.dbc (0xAD4598): the id range and a row's +8 spell / +4 skill line.
        void (*skillLineAbilityRange)(uint32_t& minId, uint32_t& maxId) = nullptr;
        bool (*skillLineAbility)(uint32_t id, uint32_t& spell, uint32_t& skillLine) = nullptr;
        bool (*skillLineCategory)(uint32_t skillLine, uint32_t& category) = nullptr;   // SkillLine.dbc (0xAD45E0) +4
    };

    Deps& Dependencies();
    void Init();
    void Reset();   // tests
}

#endif
