// AscSpellTokens.cpp (see SpellTokens.h): the spell-description token and condition extensions.
#include "CoA/Source/Modules/SpellTokens.h"
#include "CoA/Source/Modules/AttachFixes.h"
#include "CoA/Source/Modules/AuraAmounts.h"
#include "CoA/Source/Modules/Modules.h"
#include "Engine/Source/Storm/H/storm.h"
#include <cstdio>
#include <cstring>
#include <unordered_set>

namespace {
    CoA::SpellTokens::Deps s_deps;
    uint64_t s_context = 0;   // 0x10D3C268's first 8 bytes: the unit of the last Script_GetAuraInfo

    // 0x10D3C4E0..0x10D3C56C, after the client's 0x8C names (FUN_10a11840).
    const char* const kExtraVariables[0x24] = {
        "w1", "w2", "w3", "W1", "W2", "W3", "ppl1", "ppl2", "ppl3", "PPL1", "PPL2", "PPL3", "ppl1", "ppl2", "ppl3",
        "power1", "power2", "power3", "power4", "power5", "power6", "power7",
        "POWER1", "POWER2", "POWER3", "POWER4", "POWER5", "POWER6", "POWER7",
        "br", "BR", "r1", "r2", "r3", "health", "HEALTH",
    };

    bool HasPlayer() {
        return s_deps.hasPlayer && s_deps.hasPlayer();
    }

    uint32_t PlayerU32(uint32_t offset) {
        return s_deps.playerU32 ? s_deps.playerU32(offset) : 0;
    }

    // An unsigned dword as the original converts it (int to double, +2^32 when negative, to float).
    float FromU32(uint32_t v) {
        return static_cast<float>(static_cast<double>(v));
    }

    // (level - base) of the player, the level capped at the spell's max level, times a per-level value; 0 without a
    // player or below the base level.
    float PerLevel(const CoA::SpellTokens::Spell& spell, float perLevel) {
        if (!HasPlayer()) {
            return 0.0f;
        }

        const uint32_t level = PlayerU32(0xD8);

        if (spell.baseLevel > level) {
            return 0.0f;
        }

        const uint32_t levels = (spell.maxLevel == 0 || level <= spell.maxLevel) ? level - spell.baseLevel : spell.maxLevel - spell.baseLevel;
        return FromU32(levels) * perLevel;
    }

    // FUN_1008d9c0: how many of the set's 17 items are equipped (each set entry counted once when any slot holds it).
    uint32_t EquippedFromSet(const std::vector<CoA::SpellTokens::Equipped>& equipped, const CoA::SpellTokens::ItemSet& set) {
        uint32_t n = 0;

        for (uint32_t item : set.items) {
            for (const CoA::SpellTokens::Equipped& e : equipped) {
                if (e.entry == item) {
                    ++n;
                    break;
                }
            }
        }

        return n;
    }
}

CoA::SpellTokens::Deps& CoA::SpellTokens::Dependencies() {
    return s_deps;
}

const char* CoA::SpellTokens::ExtraVariable(uint32_t index) {
    return index < sizeof(kExtraVariables) / sizeof(kExtraVariables[0]) ? kExtraVariables[index] : nullptr;
}

// 0x10B5DBC8 (written to 0x579852).
const char* CoA::SpellTokens::ConditionLetters() {
    return "sSaApP";
}

// FUN_10a10c30.
bool CoA::SpellTokens::Value(uint32_t op, const Spell& spell, const void* rec, float& out) {
    out = 0.0f;

    switch (op) {
        case 0x52:
        case 0x53:
            out = static_cast<float>(s_deps.modifyValue ? s_deps.modifyValue(rec, spell.maxTargets, 0x22) : spell.maxTargets);
            return true;

        case 0xA2: case 0xA3: case 0xA4: case 0xA5: case 0xA6: case 0xA7: {
            int32_t amount[3];

            if (CoA::AuraAmounts::LastForSpell(s_context, spell.id, amount)) {
                out = static_cast<float>(op == 0xA2 || op == 0xA5 ? amount[0] : op == 0xA3 || op == 0xA6 ? amount[1] : amount[2]);
            }

            return true;
        }

        case 0xA8: case 0xAB:
            out = PerLevel(spell, spell.realPointsPerLevel[0]);
            return true;

        case 0xA9: case 0xAC:
            out = PerLevel(spell, spell.realPointsPerLevel[1]);
            return true;

        case 0xAA: case 0xAD:
            out = PerLevel(spell, spell.realPointsPerLevel[2]);
            return true;

        case 0xAE: case 0xAF: case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4:
            if (HasPlayer()) {
                out = FromU32(PlayerU32(op * 4 - 0x260));
            }

            return true;

        case 0xB5: case 0xB6: case 0xB7: case 0xB8: case 0xB9: case 0xBA: case 0xBB:
            if (HasPlayer()) {
                out = FromU32(PlayerU32(op * 4 - 0x25C));
            }

            return true;

        case 0xBC:
        case 0xBD:
            if (HasPlayer() && s_deps.playerF32) {
                out = s_deps.playerF32(0x1000);
            }

            return true;

        case 0xBE: out = static_cast<float>(spell.miscValueB[0]); return true;
        case 0xBF: out = static_cast<float>(spell.miscValueB[1]); return true;
        case 0xC0: out = static_cast<float>(spell.miscValueB[2]); return true;

        case 0xC1:
            if (HasPlayer()) {
                out = FromU32(PlayerU32(0x60));
            }

            return true;

        case 0xC2:
            if (HasPlayer()) {
                out = FromU32(PlayerU32(0x80));
            }

            return true;

        default:
            return false;
    }
}

// FUN_10a12940.
void CoA::SpellTokens::SetAuraContext(uint64_t unit) {
    s_context = unit;
}

// FUN_10a12260.
bool CoA::SpellTokens::PlayerHasSpellBonus(uint32_t spell) {
    const std::vector<Equipped> equipped = s_deps.equipped ? s_deps.equipped() : std::vector<Equipped>();
    std::unordered_set<uint32_t> sets;

    for (const Equipped& e : equipped) {
        if (!e.hasProto) {
            continue;
        }

        if (e.itemSet) {
            sets.insert(e.itemSet);
        }

        for (uint32_t j = 0; j < 5; ++j) {
            if (!e.spells[j]) {
                break;
            }

            if (e.triggers[j] == 1 && e.spells[j] == spell) {
                return true;
            }
        }
    }

    for (uint32_t id : sets) {
        ItemSet set;

        if (!s_deps.itemSet || !s_deps.itemSet(id, set)) {
            continue;
        }

        const uint32_t count = EquippedFromSet(equipped, set);

        if (!count) {
            continue;
        }

        for (uint32_t k = 0; k < 8; ++k) {
            if (!set.spells[k]) {
                break;
            }

            if (set.thresholds[k] <= count && set.spells[k] == spell) {
                return true;
            }
        }
    }

    return false;
}

// FUN_10a136c0. The original copies the digits into a 12-byte stack buffer unterminated (and unbounded); here at most
// 11 digits, terminated (Ruling).
int32_t CoA::SpellTokens::Condition(const char* cursor) {
    if (!cursor) {
        return -1;
    }

    switch (*cursor) {
        case 'C':
        case 'c':
            return 0;

        case 'P':
        case 'p': {
            char digits[12] = {};

            for (uint32_t i = 0; i < sizeof(digits) - 1 && cursor[i + 1] >= '0' && cursor[i + 1] <= '9'; ++i) {
                digits[i] = cursor[i + 1];
            }

            const int32_t spell = SStrToInt(digits);
            // The original dereferences the active player unchecked (Ruling: false without one).
            return HasPlayer() && PlayerHasSpellBonus(static_cast<uint32_t>(spell)) ? 1 : 0;
        }

        default:
            return -1;
    }
}

// FUN_10a11310 / FUN_10a11290.
std::string CoA::SpellTokens::SchoolName(uint32_t mask) {
    struct School {
        uint32_t mask;
        const char* key;
        const char* text;
    };

    static const School kSchools[] = {
        { 1, "STRING_SCHOOL_PHYSICAL", "Physical" }, { 2, "STRING_SCHOOL_HOLY", "Holy" },
        { 3, "STRING_SCHOOL_HOLYSTRIKE", "Holystrike" }, { 4, "STRING_SCHOOL_FIRE", "Fire" },
        { 5, "STRING_SCHOOL_FLAMESTRIKE", "Flamestrike" }, { 6, "STRING_SCHOOL_HOLYFIRE", "Holyfire" },
        { 8, "STRING_SCHOOL_NATURE", "Nature" }, { 9, "STRING_SCHOOL_STORMSTRIKE", "Stormstrike" },
        { 10, "STRING_SCHOOL_HOLYSTORM", "Holystorm" }, { 0xC, "STRING_SCHOOL_FIRESTORM", "Firestorm" },
        { 0x10, "STRING_SCHOOL_FROST", "Frost" }, { 0x11, "STRING_SCHOOL_FROSTSTRIKE", "Froststrike" },
        { 0x12, "STRING_SCHOOL_HOLYFROST", "Holyfrost" }, { 0x14, "STRING_SCHOOL_FROSTFIRE", "Frostfire" },
        { 0x18, "STRING_SCHOOL_FROSTSTORM", "Froststorm" }, { 0x1C, "STRING_SCHOOL_ELEMENTAL", "Elemental" },
        { 0x20, "STRING_SCHOOL_SHADOW", "Shadow" }, { 0x21, "STRING_SCHOOL_SHADOWSTRIKE", "Shadowstrike" },
        { 0x22, "STRING_SCHOOL_SHADOWLIGHT", "Shadowlight" }, { 0x24, "STRING_SCHOOL_SHADOWFLAME", "Shadowflame" },
        { 0x28, "STRING_SCHOOL_SHADOWSTORM", "Shadowstorm" }, { 0x30, "STRING_SCHOOL_SHADOWFROST", "Shadowfrost" },
        { 0x40, "STRING_SCHOOL_ARCANE", "Arcane" }, { 0x41, "STRING_SCHOOL_SPELLSTRIKE", "Spellstrike" },
        { 0x42, "STRING_SCHOOL_DIVINE", "Divine" }, { 0x44, "STRING_SCHOOL_SPELLFIRE", "Spellfire" },
        { 0x48, "STRING_SCHOOL_SPELLSTORM", "Spellstorm" }, { 0x50, "STRING_SCHOOL_SPELLFROST", "Spellfrost" },
        { 0x60, "STRING_SCHOOL_SPELLSHADOW", "Spellshadow" }, { 0x7C, "STRING_SCHOOL_CHROMATIC", "Chromatic" },
        { 0x7E, "STRING_SCHOOL_MAGIC", "Magic" }, { 0x7F, "STRING_SCHOOL_CHAOS", "Chaos" },
    };

    if (mask == 0) {
        return std::string();
    }

    const char* key = "STRING_SCHOOL_UNKNOWN";
    const char* text = "Unknown";

    for (const School& school : kSchools) {
        if (school.mask == mask) {
            key = school.key;
            text = school.text;
            break;
        }
    }

    const char* global = s_deps.globalText ? s_deps.globalText(key) : nullptr;
    return global ? global : text;
}

// FUN_10a12780.
void CoA::SpellTokens::ReplaceSchool(char* text, uint32_t size, uint32_t schoolMask) {
    if (!text || !size) {
        return;
    }

    std::string s(text);
    bool replaced = false;
    size_t from = 0;

    for (;;) {
        const size_t at = s.size() < 3 || s.size() - 3 < from ? std::string::npos : s.find("$sc", from);

        if (at == std::string::npos) {
            break;
        }

        const std::string name = SchoolName(schoolMask);
        s.replace(at, 3, name);
        from = at + name.size();
        replaced = true;
    }

    if (replaced) {
        snprintf(text, size, "%s", s.c_str());
    }
}

// FUN_10324a10(spell, 0x40) from FUN_10a11130.
bool CoA::SpellTokens::HidesCastTime(uint32_t spell) {
    return (CoA::AttachFixes::CustomAttr(spell, 0x14) & 0x40) != 0;
}

void CoA::SpellTokens::Reset() {
    s_deps = Deps();
    s_context = 0;
}

void CoA::SpellTokens::Init() {
}
