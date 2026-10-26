// AscSpellMods.cpp's tables, packets and detour logic (see SpellMods.h); the client half is SpellModsClient.cpp.
#include "CoA/Source/Modules/SpellMods.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/ObjectAddon.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include <cstring>
#include <string>
#include <unordered_map>

namespace {
    constexpr uint32_t kTableSize = 0x2AF60;
    using FamilyMap = std::unordered_map<uint32_t, std::unordered_map<uint8_t, int32_t>>;

    int32_t s_flat[kTableSize];   // 0x10BE44A8
    int32_t s_pct[kTableSize];    // 0x10C90228
    FamilyMap s_spellFlat;        // 0x10D3BFB4
    FamilyMap s_spellPct;         // 0x10D3BFD4
    bool s_auraFlag = false;      // 0x10BE44A0
    bool s_castWhileMoving = false;
    bool s_sheatheAnimates = false;
    bool s_activateSound = true;
    bool s_fizzleSound = true;
    CoA::SpellMods::Deps s_deps;

    using CoA::SpellMods::Spell;

    bool Fetch(uint32_t id, Spell& out) {
        return s_deps.fetchSpell && s_deps.fetchSpell(id, out);
    }

    bool Affected(void* unit, uint32_t type, uint32_t spell) {
        return s_deps.affectedByAura && s_deps.affectedByAura(unit, type, spell);
    }

    bool HasAura(void* unit, uint32_t spell) {
        return s_deps.hasAura && s_deps.hasAura(unit, spell);
    }

    bool IsPlayer(void* unit) {
        return s_deps.isActivePlayer && s_deps.isActivePlayer(unit);
    }

    uint32_t InvSlot(void* unit, uint32_t slot) {
        return s_deps.invSlotLow ? s_deps.invSlotLow(unit, slot) : 0;
    }

    uint32_t PendingSpell() {   // *DAT_10BCC4B0 (0xD397D0)
        return s_deps.autoRepeatingSpell ? s_deps.autoRepeatingSpell() : 0;
    }

    bool InManastorm() {
        return s_deps.inManastorm && s_deps.inManastorm();
    }

    bool Attr18(uint32_t spell, uint32_t& value) {
        value = 0;
        return s_deps.customAttr18 && s_deps.customAttr18(spell, value);
    }

    float UnitAddonFloat(void* unit, uint32_t index) {   // FUN_100d0a30
        const uint64_t guid = s_deps.unitGuid ? s_deps.unitGuid(unit) : 0;
        std::vector<CoA::ObjectAddon::Field>& fields = CoA::ObjectAddon::Unit(guid);
        index &= 0xFFFF;
        return index < fields.size() ? CoA::ObjectAddon::AsFloat(fields[index]) : 0.0f;
    }

    // FUN_10326ff0: the jne at 0x80DB37 becomes a jmp while on, and SpellRange.dbc rows 0x72 / 0x1BD get their flags
    // (+0x14) = 0 (on) / 2 (off).
    void SetAuraFlag(bool on) {
        if (s_deps.setModalSkip) {
            s_deps.setModalSkip(on);
        }

        if (s_deps.setSpellRangeFlags) {
            s_deps.setSpellRangeFlags(0x72, on ? 0 : 2);
            s_deps.setSpellRangeFlags(0x1BD, on ? 0 : 2);
        }

        s_auraFlag = on;
    }

    // FUN_10325020: an effect applies aura 0x138, and some effect has its three class masks zero and misc value 1.
    bool IsAuraFlagSpell(const Spell& rec) {
        bool aura = false;

        for (uint32_t i = 0; i < 3 && !aura; ++i) {
            aura = rec.effectAura[i] == 0x138;
        }

        if (!aura) {
            return false;
        }

        for (uint32_t i = 0; i < 3; ++i) {
            if (rec.maskA[i] == 0 && rec.maskB[i] == 0 && rec.maskC[i] == 0 && rec.effectMisc[i] == 1) {
                return true;
            }
        }

        return false;
    }

    // FUN_10324870 / FUN_10324920
    int32_t SpellModifier(const FamilyMap& m, uint32_t spell, uint32_t op) {
        auto it = m.find(spell);

        if (it == m.end()) {
            return 0;
        }

        auto jt = it->second.find(static_cast<uint8_t>(op));
        return jt != it->second.end() ? jt->second : 0;
    }

    // FUN_10328ac0: every spell SpellAffect names for `spell` (its target itself, or when negative every rank of the
    // chain whose first rank is -target) gets value added to its entry for `op`.
    void AddSpellModifier(uint32_t op, int32_t value, uint32_t spell, bool flat) {
        Spell rec;

        if (!Fetch(spell, rec) || !s_deps.spellAffects) {
            return;
        }

        FamilyMap& m = flat ? s_spellFlat : s_spellPct;

        for (const auto& affect : s_deps.spellAffects(spell)) {
            const int32_t target = static_cast<int32_t>(affect[2]);
            std::vector<uint32_t> spells;

            if (target >= 0) {
                spells.push_back(static_cast<uint32_t>(target));
            } else if (s_deps.rankChain) {
                spells = s_deps.rankChain(static_cast<uint32_t>(-target));
            }

            for (uint32_t s : spells) {
                Spell affected;

                if (!Fetch(s, affected)) {
                    continue;
                }

                const int32_t total = value + SpellModifier(m, s, op);
                m[s][static_cast<uint8_t>(op)] = total;
            }
        }
    }

    // Unit addon float at defense type (+0x254) * 7 + 7 + power type (+0xA4), plus 1, times the value.
    int32_t ScaleBySchool(const Spell& rec, void* unit, int32_t r) {
        const float f = UnitAddonFloat(unit, rec.defenseType * 7 + 7 + rec.powerType);
        const float product = static_cast<float>((static_cast<double>(f) + 1.0) * static_cast<float>(r));
        return static_cast<int32_t>(product);
    }

    // FUN_10324270: the aura flag, or the pending spell passes aura-type check 0x138 on the unit.
    bool PendingSpellFlagged(void* unit) {
        if (s_auraFlag) {
            return true;
        }

        Spell rec;
        return Fetch(PendingSpell(), rec) && Affected(unit, 0x138, rec.id);
    }

    // The aura's spell addon effect slots (+0x50).
    uint32_t AddonEffect(const uint8_t* addon, uint32_t j) {
        uint32_t v;
        memcpy(&v, addon + 0x50 + j * 4, 4);
        return v;
    }
}

CoA::SpellMods::Deps& CoA::SpellMods::Dependencies() {
    return s_deps;
}

// handler_0x064F (7a guard: the count stops at the records the packet holds).
int32_t CoA::SpellMods::OnCooldowns(void*, uint32_t, uint32_t, CDataStore* msg) {
    uint32_t n = CoA::Read::U32(msg);
    const uint32_t whole = CoA::Read::Remaining(msg) / 20;

    if (n > whole) {
        n = whole;
    }

    for (; n != 0; --n) {
        const uint32_t v0 = CoA::Read::U32(msg);
        const uint32_t v1 = CoA::Read::U32(msg);
        const uint32_t v2 = CoA::Read::U32(msg);
        const uint32_t v3 = CoA::Read::U32(msg);
        const uint32_t v4 = CoA::Read::U32(msg);

        if (s_deps.setCooldownLeft) {
            s_deps.setCooldownLeft(v0, v1, v2, v3, v4 & 0x7FFFFFFF, v4 >> 31, 0);
        }
    }

    return 1;
}

// 0x10325B70. A packet shorter than its fields, or a class entry past the table, is ignored (7a guard; the original
// reads past the buffer and writes unchecked).
int32_t CoA::SpellMods::OnSpellModifier(void*, uint32_t opcode, uint32_t, CDataStore* msg) {
    const uint8_t isFamily = CoA::Read::U8(msg);
    const uint8_t bit = CoA::Read::U8(msg);

    if (isFamily) {
        const uint32_t value = CoA::Read::U32(msg);
        const uint32_t spell = CoA::Read::U32(msg);

        if (!CoA::Read::Failed(msg)) {
            AddSpellModifier(bit, static_cast<int32_t>(value), spell, opcode == 0x266);
        }

        return 1;
    }

    const uint8_t op = CoA::Read::U8(msg);
    const uint32_t value = CoA::Read::U32(msg);
    const uint32_t family = CoA::Read::U32(msg);
    const uint64_t index = static_cast<uint64_t>(bit) * 0x1F + static_cast<uint64_t>(family) * 0x11A0 + op;

    if (!CoA::Read::Failed(msg) && index < kTableSize) {
        (opcode == 0x266 ? s_flat : s_pct)[index] = static_cast<int32_t>(value);
    }

    return 1;
}

// 0x103288A0. The original's false returns leave whatever its register held; every one has a zero low byte, which is
// what the client's callers test.
bool CoA::SpellMods::ModifierSum(uint32_t spell, uint32_t classSet, const uint32_t classMask[3], uint32_t attributesExC, uint32_t op,
    int32_t& flat, int32_t& pct) {
    if (classSet == 0 || ((attributesExC >> 29) & 1)) {
        flat = 0;
        pct = 100;
        return false;
    }

    pct = 0;
    flat = 0;

    for (uint32_t bit = 0; bit < 96; ++bit) {
        if (!(classMask[bit >> 5] & (1u << (bit & 0x1F)))) {
            continue;
        }

        const uint64_t index = static_cast<uint64_t>(classSet) * 0x11A0 + bit * 0x1F + (op & 0xFF);
        flat += index < kTableSize ? s_flat[index] : 0;
        pct += index < kTableSize ? s_pct[index] : 0;
    }

    flat += SpellModifier(s_spellFlat, spell, op);
    pct += SpellModifier(s_spellPct, spell, op);

    if (flat == 0 && pct == 0) {
        return false;
    }

    const int32_t v = pct + 100;
    pct = v < 0 ? 0 : v;
    return true;
}

bool CoA::SpellMods::PowerPerSecondReplaced(const Spell& spell, void* unit, int32_t& out) {
    uint32_t attr = 0;

    if (!unit || !Attr18(spell.id, attr) || !(attr & 0x20000)) {
        return false;
    }

    const uint32_t v = s_deps.basePower ? s_deps.basePower(unit, spell.powerType) : 0;
    out = static_cast<int32_t>((spell.manaPerSecond * v) / 100);
    return true;
}

int32_t CoA::SpellMods::AfterPowerPerSecond(const Spell& spell, void* unit, int32_t r) {
    if (r && unit && spell.defenseType < 4 && spell.powerType < 7) {
        r = ScaleBySchool(spell, unit, r);
    }

    return r;
}

int32_t CoA::SpellMods::AfterPowerCost(const Spell& spell, const void* rec, void* unit, int32_t r) {
    if (r && unit) {
        if (spell.defenseType <= 3 && spell.powerType < 7) {
            r = ScaleBySchool(spell, unit, r);

            if (r == 0) {
                return 0;
            }
        }
    } else {
        if (r == 0) {
            return 0;
        }

        if (!unit) {
            return r;
        }
    }

    // 0x7FF180 is called through its hooked entry, as the original does.
    if ((s_deps.castTime ? s_deps.castTime(rec) : 0) == 0 && spell.powerType == 0) {
        const float f = UnitAddonFloat(unit, 6);   // FUN_102cdcf0 +0x48
        return static_cast<int32_t>((1.0f - f) * static_cast<float>(r));
    }

    return r;
}

int32_t CoA::SpellMods::AfterCastTime(const Spell* spell, int32_t result) {
    uint32_t r = static_cast<uint32_t>(result);
    void* player = s_deps.activePlayer ? s_deps.activePlayer() : nullptr;

    if (!player || !s_deps.auraCount || !s_deps.auraAt) {
        return result;
    }

    const uint64_t playerGuid = s_deps.unitGuid ? s_deps.unitGuid(player) : 0;

    for (uint32_t i = 0; i < s_deps.auraCount(player); ++i) {
        Aura aura;

        if (!s_deps.auraAt(player, i, aura)) {
            continue;
        }

        Spell auraRec;

        if (!Fetch(aura.spell, auraRec)) {
            continue;
        }

        const uint8_t* addon = s_deps.spellAddon ? s_deps.spellAddon(aura.spell) : nullptr;

        if (!addon) {
            continue;
        }

        const int32_t* amount = s_deps.auraAmounts ? s_deps.auraAmounts(playerGuid, aura.caster, aura.spell) : nullptr;

        if (!amount) {
            continue;
        }

        for (uint32_t j = 0; j < 3; ++j) {
            // A null record (the client's early 0 return) has no school mask (the original dereferences it).
            if (AddonEffect(addon, j) == 0x15D && spell && (spell->schoolMask & auraRec.effectMisc[j])) {
                const float scale = 100.0f - static_cast<float>(amount[j]);
                const float value = static_cast<float>(static_cast<double>(r)) * scale / 100.0f;
                r = static_cast<uint32_t>(static_cast<int64_t>(value));   // FUN_10ae6220
            }
        }
    }

    return static_cast<int32_t>(r);
}

void CoA::SpellMods::AfterSpellRange(void* unit, uint32_t spell, float* minRange) {
    if (!unit || !minRange || *minRange <= 0.0f) {
        return;
    }

    if (!s_auraFlag) {
        Spell rec;

        if (!Fetch(spell, rec) || !Affected(unit, 0x138, rec.id)) {
            return;
        }
    }

    *minRange = 0.0f;
}

bool CoA::SpellMods::InMeleeOverride(void* unit) {
    if (PendingSpell() != 0x4B) {
        return false;
    }

    if (InManastorm() || PendingSpellFlagged(unit)) {
        return true;
    }

    return IsPlayer(unit) && InvSlot(unit, 0xF) == 0;
}

bool CoA::SpellMods::MeleeRangeVeto(void* unit) {
    if (IsPlayer(unit) && InvSlot(unit, 0xF) == 0 && InvSlot(unit, 0x11) != 0) {
        return true;
    }

    if (PendingSpell() != 0x4B) {
        return false;
    }

    if (InManastorm() || PendingSpellFlagged(unit)) {
        return true;
    }

    return IsPlayer(unit) && InvSlot(unit, 0xF) == 0;
}

// FUN_103273F0's part before the original. A null caster patches nothing (the original dereferences it).
void CoA::SpellMods::BeforeCastSpell(void* unit, uint32_t spell) {
    if (!unit) {
        return;
    }

    Spell rec;

    if (Fetch(spell, rec)) {
        bool family5Plain = false;

        if (Affected(unit, 0x139, rec.id)) {
            s_castWhileMoving = true;
        }

        for (uint32_t i = 0; i < 3; ++i) {
            if (rec.effectAura[i] == 0x4E) {
                s_sheatheAnimates = true;
            }
        }

        if (rec.classSet == 5) {
            if ((rec.classMask[0] & 0x64) || (rec.classMask[1] & 0xC0) || (rec.classMask[2] & 0x200)) {
                if (HasAura(unit, 0x7B9B)) {
                    s_castWhileMoving = true;
                }
            } else {
                family5Plain = true;
            }
        }

        if (!family5Plain && rec.classSet == 3 && (rec.classMask[0] & 0x400015) && HasAura(unit, 0x7B9B)) {
            s_castWhileMoving = true;
        }
    }

    if (s_deps.castingSpellId && s_deps.castingSpellId(unit) != 0) {
        const bool on = !s_deps.errorSpeech || s_deps.errorSpeech();
        s_activateSound = on;
        s_fizzleSound = on;
    }
}

// The cast patch and 0x7370AF back to stock; 0x80D6EF / 0x808323 are left as set.
void CoA::SpellMods::AfterCastSpell() {
    s_castWhileMoving = false;
    s_sheatheAnimates = false;
}

void CoA::SpellMods::NullCancelFromStopCasting() {
    void* player = s_deps.activePlayer ? s_deps.activePlayer() : nullptr;

    if (!player) {
        return;
    }

    const int32_t casting = s_deps.uiCastingSpell ? s_deps.uiCastingSpell(player) : 0;
    const std::string message = "Spell_C_CancelSpell: spellCastLink = nullptr but GetUICastingSpellID = " + std::to_string(casting) + ".";

    if (s_deps.fatal) {
        s_deps.fatal(message.c_str());
    }
}

bool CoA::SpellMods::KeepCastOnMove(uint32_t spell) {
    void* player = s_deps.activePlayer ? s_deps.activePlayer() : nullptr;
    Spell rec;
    return player && Fetch(spell, rec) && Affected(player, 0x139, rec.id);
}

bool CoA::SpellMods::AuraFlagOn(const Spell& aura) {
    if (IsAuraFlagSpell(aura)) {
        SetAuraFlag(true);
    }

    return true;
}

bool CoA::SpellMods::AuraFlagOff(const Spell& aura) {
    if (IsAuraFlagSpell(aura)) {
        SetAuraFlag(false);
    }

    return true;
}

// 0x103267E0: a spell flagged SpellCustomAttr +0x18 & 0x2000 with a required caster aura spell needs the unit's aura of
// that spell at (the word at +0xE) >= the required spell's StackAmount. An unknown required spell or a missing aura vetoes.
bool CoA::SpellMods::StackRequirement(void* unit, const Spell& rec) {
    const uint32_t required = rec.casterAuraSpell;

    if (required == 0 && rec.targetAuraSpell == 0) {
        return true;
    }

    uint32_t attr = 0;

    if (!Attr18(rec.id, attr) || !(attr & 0x2000) || required == 0 || !unit) {
        return true;
    }

    Spell req;

    if (!Fetch(required, req)) {
        return false;
    }

    const uint32_t slot = s_deps.auraIndex ? s_deps.auraIndex(unit, required) : 0xFFFFFFFF;
    Aura aura;

    if (!s_deps.auraAt || !s_deps.auraAt(unit, slot, aura)) {
        return false;
    }

    return aura.stack >= req.cumulativeAura;
}

// 0x10326780: SpellCustomAttr +0x18 & 0x4000 needs aura 0xC3849 on the unit.
bool CoA::SpellMods::AuraRequirement(void* unit, const Spell& rec) {
    uint32_t attr = 0;

    if (!Attr18(rec.id, attr) || !(attr & 0x4000) || !unit) {
        return true;
    }

    return HasAura(unit, 0xC3849);
}

void CoA::SpellMods::ClearModifiers() {
    memset(s_flat, 0, sizeof(s_flat));
    memset(s_pct, 0, sizeof(s_pct));
    s_spellFlat.clear();   // FUN_101cf290
    s_spellPct.clear();
}

bool CoA::SpellMods::CastWhileMoving() {
    return s_castWhileMoving;
}

bool CoA::SpellMods::SheatheAnimates() {
    return s_sheatheAnimates;
}

bool CoA::SpellMods::ActivateSound() {
    return s_activateSound;
}

bool CoA::SpellMods::FizzleSound() {
    return s_fizzleSound;
}

bool CoA::SpellMods::AuraFlag() {
    return s_auraFlag;
}

int32_t CoA::SpellMods::FlatAt(uint32_t index) {
    return index < kTableSize ? s_flat[index] : 0;
}

int32_t CoA::SpellMods::PctAt(uint32_t index) {
    return index < kTableSize ? s_pct[index] : 0;
}

int32_t CoA::SpellMods::SpellEntry(bool flat, uint32_t spell, uint32_t op) {
    return SpellModifier(flat ? s_spellFlat : s_spellPct, spell, op);
}

void CoA::SpellMods::Reset() {
    ClearModifiers();
    s_auraFlag = false;
    s_castWhileMoving = false;
    s_sheatheAnimates = false;
    s_activateSound = true;
    s_fizzleSound = true;
    s_deps = Deps();
}

// FUN_10324a90. The aura-flag and usable-action list entries are registered by SpellModsClient.cpp (they take the
// client's records); the two code replacements and the detours are the decomp sites' calls.
void CoA::SpellMods::Init() {
    COA_PACKET(0x64F, CoA::SpellMods::OnCooldowns);
    CoA::Lifecycle::OnGlueScreen(CoA::SpellMods::ClearModifiers);
}
