#ifndef COA_SOURCE_MODULES_SPELLTOKENS_H
#define COA_SOURCE_MODULES_SPELLTOKENS_H

#include <cstdint>
#include <string>
#include <vector>

// AscSpellTokens.cpp, the original's spell-description extensions (installer FUN_10a11840):
//
//   0x576B63 / 0x576B7C  LookupMathVariable searches the DLL's table 0x10D3C2B0: the client's 0x8C names, then 0x24
//                        more (count 0xB0). A name's op is its index + 22 (MATHOP_STAT_STRENGTH); "ppl1".."ppl3" appear
//                        twice, so the second copies (ops 0xAE..0xB0) are unreachable and every later name's op is 3
//                        past the case the evaluator gives it -- kept as shipped (power5 reads POWER6, ...).
//   0x5782D0  MathState::GetVariableValue: FUN_10a10c30 evaluates ops 0x52 / 0x53 and 0xA2..0xC2 itself and pushes
//             the value on the math stack; any other op goes to the client.
//   0x608390  Script_GetAuraInfo: after it, its info (the unit GUID first) is the description context (0x10D3C268)
//             (FUN_10a12940).
//   0x5797F0  SpellParserParseConditional: C... is refused (false); P<n> is whether the player's equipped items or
//             item-set bonuses carry spell n; anything else goes to the client (FUN_10a136c0). The cursor is not moved.
//   0x579852  the '!' branch's condition letters "sSaA" become "sSaApP".
//   0x57AAF0  SpellParserParseSubText: before it, every "$sc" in the output buffer becomes the spell's school name
//             (FUN_10a12780).
//   0x6238A0  CGTooltip::SetSpell: while it runs for a spell with SpellCustomAttr +0x14 0x40, the jne at 0x62442C is
//             a jmp -- the cast-time / cooldown line block (AddSpellCastTimeLine) is skipped (FUN_10a11130).
namespace CoA::SpellTokens {
    // The Spell.dbc fields read here (32-bit offsets).
    struct Spell {
        uint32_t id = 0;                      // +0x000
        uint32_t maxLevel = 0;                // +0x094
        uint32_t baseLevel = 0;               // +0x098
        float realPointsPerLevel[3] = {};     // +0x134
        int32_t miscValueB[3] = {};           // +0x1C4
        int32_t maxTargets = 0;               // +0x250
        uint32_t schoolMask = 0;              // +0x284
    };

    // One equipped item, as FUN_10a12260 reads it.
    struct Equipped {
        uint32_t entry = 0;          // the item object's entry (descriptor +0xC)
        bool hasProto = false;       // the item cache has its record
        uint32_t itemSet = 0;        // proto +0x1A8
        uint32_t spells[5] = {};     // proto +0x100
        uint32_t triggers[5] = {};   // proto +0x114
    };

    struct ItemSet {
        uint32_t items[17] = {};      // ItemSet.dbc +0x08
        uint32_t spells[8] = {};      // +0x4C
        uint32_t thresholds[8] = {};  // +0x6C
    };

    struct Deps {
        bool (*hasPlayer)() = nullptr;                                       // 0x4038F0 non-null
        uint32_t (*playerU32)(uint32_t offset) = nullptr;                   // the active player's descriptor dword
        float (*playerF32)(uint32_t offset) = nullptr;
        int32_t (*modifyValue)(const void* rec, int32_t value, int32_t op) = nullptr;   // 0x7FDB50(rec, op, &value)
        std::vector<Equipped> (*equipped)() = nullptr;                      // the 19 equipment slots' items
        bool (*itemSet)(uint32_t id, ItemSet& out) = nullptr;               // ItemSet.dbc (0xAD3EFC)
        const char* (*globalText)(const char* key) = nullptr;               // FrameScript_GetText(key, -1, 0)
    };

    Deps& Dependencies();

    // The DLL's variable table: 0xB0 names, the client's first.
    constexpr uint32_t kVariableCount = 0xB0;
    constexpr uint32_t kClientVariables = 0x8C;
    // The 0x24 names after the client's.
    const char* ExtraVariable(uint32_t index);
    // 0x10B5DBC8, the '!' branch's letters.
    const char* ConditionLetters();

    // FUN_10a10c30: false for an op the client evaluates. `rec` is the client's record (for modifyValue).
    bool Value(uint32_t op, const Spell& spell, const void* rec, float& out);
    // FUN_10a12940 after Script_GetAuraInfo.
    void SetAuraContext(uint64_t unit);

    // FUN_10a136c0: -1 when the client decides, else the condition (0 / 1).
    int32_t Condition(const char* cursor);
    // FUN_10a12260.
    bool PlayerHasSpellBonus(uint32_t spell);

    // FUN_10a11310 / FUN_10a11290.
    std::string SchoolName(uint32_t mask);
    // FUN_10a12780 on the output buffer.
    void ReplaceSchool(char* text, uint32_t size, uint32_t schoolMask);

    // FUN_10a11130's test: SetSpell skips its cast-time line for this spell.
    bool HidesCastTime(uint32_t spell);

    void Reset();
    void Init();
}

#endif
