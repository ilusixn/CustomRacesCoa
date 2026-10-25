// AscSpellApi.cpp (see SpellApi.h).
#include "CoA/Source/Modules/SpellApi.h"
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <unordered_map>

using CoA::SpellApi::SpellFields;
using CoA::Script::CheckNumber;
using CoA::Script::PushBool;
using CoA::Script::PushInt;
using CoA::Script::ReadNumber;
using CoA::Script::ToInt;
using CoA::Script::ValidateInput;

namespace {
    CoA::SpellApi::Deps s_deps;

    bool Fetch(uint32_t id, SpellFields& s) {
        s = SpellFields();
        return s_deps.spell && s_deps.spell(id, s);
    }

    uint32_t U32(const uint8_t* p, uint32_t off) {
        uint32_t v;
        memcpy(&v, p + off, 4);
        return v;
    }

    // FUN_10324e80: effect i applies an aura (effect 6/27/35/65/119/128/129/143/190 with an aura set).
    bool AppliesAura(const SpellFields& s, uint32_t i) {
        switch (s.effect[i]) {
            case 0x06: case 0x1B: case 0x23: case 0x41: case 0x77: case 0x80: case 0x81: case 0x8F: case 0xBE:
                return s.aura[i] != 0;
            default:
                return false;
        }
    }

    // FUN_10325970: neither implicit target is an enemy-style target.
    bool FriendlyTargets(const SpellFields& s, uint32_t i) {
        for (uint32_t t : { s.targetA[i], s.targetB[i] }) {
            switch (t) {
                case 2: case 6: case 0xF: case 0x10: case 0x18: case 0x1C: case 0x35: case 0x68:
                    return false;
            }
        }

        return true;
    }

    int32_t Amount(const SpellFields& s, uint32_t i) {
        return static_cast<int32_t>(s.basePoints[i] + s.dieSides[i]);
    }
}

// FUN_10325130
bool CoA::SpellApi::IsPositiveEffect(const SpellFields& s, uint32_t i, bool nested) {
    if (s.attributes & 0x4000000) {
        return false;
    }

    const uint32_t id = s.id;

    if (const uint8_t* attr = CoA::CA::SpellCustomAttrRow(id)) {   // FUN_102a7f10
        const uint32_t bit = i == 0 ? 0x1000 : i == 1 ? 0x2000 : i == 2 ? 0x4000 : 0;

        if (bit && (U32(attr, 8) & bit)) {
            return false;
        }
    }

    switch (s.family) {
        case 0:
            switch (id) {
                case 0xD634: case 0x878C: case 0x721E: case 0xF223: case 0xF224:
                    return false;
                case 0x789D: case 0x119F: case 0x7E38: case 0x920C: case 0x95AE: case 0xC4A8: case 0xF114: case 0xF126: case 0xF17B:
                case 0xF18A: case 0xF388: case 0x10DA7F:
                    return true;
            }

            break;
        case 3:
            if (s.familyFlags == 0x2000 || s.iconId == 0x2D) {
                return true;
            }

            if (s.familyFlags == 0x800) {
                return false;
            }

            break;
        case 6:
            switch (id) {
                case 0x1182C1: case 0xFD4C: case 0xFD88: case 0xB9E1: case 0x11C62C: case 0x11C668:
                    return true;
                case 0xE40D: case 0x11ACED:
                    return false;
            }

            if (s.familyFlags & 0x800000) {
                return i == 0;
            }

            break;
        case 8:
            switch (id) {
                case 0x16A74: case 0x7FAC: case 0x1433: case 0x1A76: case 0x7F85: case 0xE288: case 0xE289: case 0x16A73: case 0x114865:
                case 0xEED7E: case 0x10DD13: case 0x10E356: case 0x11488C: case 0x11AB68: case 0x11AB69:
                    return true;
            }

            break;
        case 9:
            if (id == 0x851A || id == 0x114DFA) {
                return true;
            }

            break;
        case 0xB:
            if (id == 0x77F4) {
                return false;
            }

            break;
    }

    if (s.mechanic == 0x1D) {
        return true;
    }

    for (uint32_t k = 0; k < 3; ++k) {
        if (AppliesAura(s, k)) {
            if (s.aura[k] == 0x10) {
                return true;
            }

            if (s.aura[k] == 0x56) {
                return false;
            }
        }
    }

    switch (s.effect[i]) {
        case 3:
            if (id == 0x6F19) {
                return false;
            }

            break;
        case 6:
        case 0x80:
            switch (s.aura[i]) {
                case 0x122: case 0x39:
                    if (Amount(s, i) > 0) {
                        return true;
                    }

                    break;
                case 3:
                    if (s.targetA[i] == 1) {
                        return false;
                    }

                    break;
                case 0xC:
                    if (i == 0 && s.effect[1] == 0 && s.effect[2] == 0) {
                        return false;
                    }

                    break;
                case 0xD: case 0x1D: case 0x1E: case 0x31: case 0x4F: case 0x76: case 0x87: case 0x146:
                    if (Amount(s, i) < 0) {
                        return false;
                    }

                    break;
                case 0xE:
                    if (Amount(s, i) > 0) {
                        return false;
                    }

                    break;
                case 0x17: case 0xE3:
                    if (!nested) {
                        SpellFields t;

                        if (Fetch(s.triggerSpell[i], t)) {
                            for (uint32_t k = 0; k < 3; ++k) {
                                if (t.effect[k] != 0 && FriendlyTargets(t, k) && !IsPositiveEffect(t, k, true)) {
                                    return false;
                                }
                            }
                        }
                    }

                    break;
                case 0x1A: case 0x1B: case 0x35: case 0x44: case 0x59: case 0x5F:
                    return false;
                case 0x21:
                    if (s.targetA[i] != 1) {
                        return false;
                    }

                    break;
                case 0x3C:
                    return id == 0x60A4;
                case 0x4D:
                    switch (s.miscValue[i]) {
                        case 0x10: case 0x13: case 0x15: case 0x19:
                            return false;
                    }

                    break;
                case 0x6B: case 0x6C:
                    if (s.miscValue[i] == 0xE && Amount(s, i) > 0 && !nested) {
                        uint32_t k = 0;

                        while (k < 3 && !(k != i && IsPositiveEffect(s, k, true))) {
                            ++k;
                        }

                        if (k == 3) {
                            return false;
                        }
                    }

                    break;
                case 0x6D:
                    return true;
                case 0x13A:
                    return false;
            }

            break;
        case 0xA: case 0x24: case 0x2C: case 0x88: case 0x89:
            return true;
        case 0x81:
            return false;
        default:
            break;
    }

    if (!FriendlyTargets(s, i)) {
        return false;
    }

    if (!nested && s.aura[i] == 0 && s.triggerSpell[i] != 0) {
        SpellFields t;

        if (Fetch(s.triggerSpell[i], t)) {
            for (uint32_t k = 0; k < 3; ++k) {
                if (!IsPositiveEffect(t, k, true)) {
                    return false;
                }
            }
        }
    }

    return true;
}

namespace {
    bool AllPositive(const SpellFields& s) {   // FUN_10325940
        for (uint32_t i = 0; i < 3; ++i) {
            if (!CoA::SpellApi::IsPositiveEffect(s, i, true)) {
                return false;
            }
        }

        return true;
    }

    // handler_AppliesBuff / AppliesDebuff: an aura-applying effect, positive or not.
    int Applies(lua_State* L, bool buff) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        SpellFields s;
        const bool found = Fetch(id, s);
        const bool aura = AppliesAura(s, 0) || AppliesAura(s, 1) || AppliesAura(s, 2);
        const bool positive = found && AllPositive(s);
        PushBool(L, buff ? (aura && positive) : (aura && !positive));
        return 1;
    }

    int AppliesBuff(lua_State* L) {
        return Applies(L, true);
    }

    int AppliesDebuff(lua_State* L) {
        return Applies(L, false);
    }

    int IsHarmfulSpell(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        SpellFields s;
        PushBool(L, Fetch(id, s) && !AllPositive(s));
        return 1;
    }

    int IsHelpfulSpell(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        SpellFields s;
        PushBool(L, Fetch(id, s) && AllPositive(s));
        return 1;
    }

    // FUN_10325090: a fixed list.
    int IsImmunitySpell(lua_State* L) {
        switch (static_cast<uint32_t>(ToInt(CheckNumber(L, 1)))) {
            case 0xB17E: case 0x3FE: case 0x282: case 0x2C6: case 0x48D7: case 0x83FA: case 0xC4D0A: case 0xC392B: case 0xC4BEB:
            case 0xC4DA6: case 0xC4DF2:
                PushBool(L, true);
                break;
            default:
                PushBool(L, false);
        }

        return 1;
    }

    // handler_IsTrainerSpell: any NPCTrainer.dbc row with +4 == spell.
    int IsTrainerSpell(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        CoA::Dbc::Table& t = CoA::Dbc::Get("DBFilesClient\\NPCTrainer.dbc");
        bool found = false;

        for (uint32_t r = t.MinId(); t.Loaded() && r <= t.MaxId() && !found; ++r) {
            if (const uint8_t* row = t.Row(r)) {
                found = U32(row, 4) == id;
            }
        }

        PushBool(L, found);
        return 1;
    }

    uint32_t FirstRank(uint32_t spell) {
        return s_deps.firstRank ? s_deps.firstRank(spell) : spell;
    }

    int GetFirstRank(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        PushInt(L, static_cast<int32_t>(FirstRank(id)));
        return 1;
    }

    // FUN_10328a30(0) into the map at 0x10D3C000: spell (SkillLineAbility +8) -> its row's skill line (+4), over
    // every row in id order (a later row replaces an earlier one).
    std::unordered_map<uint32_t, uint32_t> s_slaBySpell;   // 0x10D3C000
    bool s_slaBuilt = false;

    template <class Fn>
    void ForEachSkillLineAbility(Fn fn) {
        uint32_t minId = 0;
        uint32_t maxId = 0;

        if (!s_deps.skillLineAbilityRange || !s_deps.skillLineAbility) {
            return;
        }

        s_deps.skillLineAbilityRange(minId, maxId);

        for (uint32_t id = minId; id < maxId + 1; ++id) {
            uint32_t spell;
            uint32_t line;

            if (s_deps.skillLineAbility(id, spell, line) && !fn(spell, line)) {
                return;
            }
        }
    }

    void BuildSkillLineAbility() {
        s_slaBuilt = true;
        s_slaBySpell.clear();
        ForEachSkillLineAbility([](uint32_t spell, uint32_t line) {
            s_slaBySpell[spell] = line;
            return true;
        });
    }

    // FUN_10324550: (spellId, level) -> the highest rank of the spell's chain whose spellLevel (Spell.dbc +0x9C) the
    // level reaches. A rank other than the spell itself needs the spell to be a class skill (SkillLine category 7);
    // nil otherwise, and without a player.
    int GetMaxLearnableRank(lua_State* L) {
        if (!ValidateInput(L, { LUA_TNUMBER, LUA_TNUMBER })) {
            return 0;
        }

        const uint32_t spell = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
        const uint32_t level = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));

        if (!s_deps.hasPlayer || !s_deps.hasPlayer()) {
            lua_pushnil(L);
            return 1;
        }

        static const std::vector<uint32_t> none;
        const std::vector<uint32_t> chain = s_deps.chain ? s_deps.chain(FirstRank(spell)) : none;
        uint32_t best = 0;

        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            SpellFields rec;

            if (Fetch(*it, rec) && level >= rec.spellLevel) {
                best = *it;
                break;
            }
        }

        if (best && best != spell) {
            if (!s_slaBuilt) {
                BuildSkillLineAbility();
            }

            auto sla = s_slaBySpell.find(spell);
            uint32_t category = 0;

            if (sla == s_slaBySpell.end() || !s_deps.skillLineCategory || !s_deps.skillLineCategory(sla->second, category) || category != 7) {
                best = 0;
            }
        }

        if (best) {
            PushInt(L, static_cast<int32_t>(best));
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_Spell", "GetMaxLearnableRank", GetMaxLearnableRank),
        COA_NATIVE("C_Spell", "AppliesBuff", AppliesBuff),
        COA_NATIVE("C_Spell", "AppliesDebuff", AppliesDebuff),
        COA_NATIVE("C_Spell", "IsHarmfulSpell", IsHarmfulSpell),
        COA_NATIVE("C_Spell", "IsHelpfulSpell", IsHelpfulSpell),
        COA_NATIVE("C_Spell", "IsImmunitySpell", IsImmunitySpell),
        COA_NATIVE("C_Spell", "IsTrainerSpell", IsTrainerSpell),
        COA_NATIVE("C_Spell", "GetFirstRank", GetFirstRank),
    };
}

void CoA::SpellApi::SkillLineAbilityUpdate(uint32_t spell) {
    if (spell == 0) {
        BuildSkillLineAbility();
        return;
    }

    if (!s_slaBuilt) {
        BuildSkillLineAbility();
    }

    ForEachSkillLineAbility([spell](uint32_t rowSpell, uint32_t line) {
        if (rowSpell != spell) {
            return true;
        }

        s_slaBySpell[spell] = line;
        return false;
    });
}

void CoA::SpellApi::SkillLineAbilityErase(uint32_t spell) {
    if (!s_slaBuilt) {
        BuildSkillLineAbility();
    }

    s_slaBySpell.erase(spell);
}

CoA::SpellApi::Deps& CoA::SpellApi::Dependencies() {
    return s_deps;
}

void CoA::SpellApi::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}

void CoA::SpellApi::Reset() {
    s_deps = Deps();
    s_slaBySpell.clear();
    s_slaBuilt = false;
}
