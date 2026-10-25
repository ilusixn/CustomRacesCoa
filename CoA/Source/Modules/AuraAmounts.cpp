// AscAuraAmounts.cpp's map and packets (see AuraAmounts.h); the armor penetration native is in AuraAmountsClient.cpp.
#include "CoA/Source/Modules/AuraAmounts.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <unordered_map>

namespace {
    struct Entry {
        uint64_t caster;
        uint32_t spell;
        int32_t amount[3];
    };

    std::unordered_map<uint64_t, std::vector<Entry>> s_units;   // 0x10d3c280
    CoA::AuraAmounts::Deps s_deps;

    bool Effects(uint32_t spell, CoA::AuraAmounts::SpellEffects& out) {
        return s_deps.spellEffects && s_deps.spellEffects(spell, out);
    }

    bool HasEffect(const CoA::AuraAmounts::SpellEffects& rec, int32_t aura, int32_t misc) {   // FUN_10a117f0
        for (int32_t j = 0; j < 3; ++j) {
            if (rec.aura[j] == aura && rec.misc[j] == misc) {
                return true;
            }
        }

        return false;
    }

    void SignalForUnit(const char* event, uint64_t guid) {
        if (!s_deps.tokens) {
            return;
        }

        for (const std::string& token : s_deps.tokens(guid)) {
            CoA::Events::Signal(event, "%s", token.c_str());
        }
    }

    // FUN_10a13440
    void AmountsChanged(uint64_t guid, const CoA::AuraAmounts::SpellEffects& rec) {
        if (HasEffect(rec, 0x45, 0x7F) || HasEffect(rec, 0x61, 0x7F)) {
            SignalForUnit("UNIT_ABSORB_AMOUNT_CHANGED", guid);
        } else if (HasEffect(rec, 0x12D, 0x7F)) {
            SignalForUnit("UNIT_HEAL_ABSORB_AMOUNT_CHANGED", guid);
        }
    }

    // FUN_10a12470: false stops a 0x674 run (unknown spell).
    bool ReadRecord(uint64_t unit, CDataStore* msg) {
        const uint32_t slot = CoA::Read::U32(msg);
        const uint64_t caster = CoA::Read::U64(msg);
        const uint32_t spell = CoA::Read::U32(msg);
        CoA::AuraAmounts::SpellEffects rec;

        if (!Effects(spell, rec)) {
            return false;
        }

        std::vector<Entry>& list = s_units[unit];
        auto same = [&](const Entry& e) { return e.caster == caster && e.spell == spell; };

        if (slot == 0xFFFFFFFF) {
            list.erase(std::remove_if(list.begin(), list.end(), same), list.end());
        } else {
            int32_t amount[3];

            for (int32_t j = 0; j < 3; ++j) {
                amount[j] = static_cast<int32_t>(CoA::Read::U32(msg));
            }

            auto it = std::find_if(list.begin(), list.end(), same);

            if (it != list.end()) {
                std::copy(amount, amount + 3, it->amount);
            } else {
                const uint64_t player = s_deps.activePlayer ? s_deps.activePlayer() : 0;

                if (player != unit && !(s_deps.acceptsSpell && s_deps.acceptsSpell(spell, unit))) {
                    return true;
                }

                list.push_back({ caster, spell, { amount[0], amount[1], amount[2] } });
            }
        }

        AmountsChanged(unit, rec);
        return true;
    }

    int32_t OnAuraUpdateAddon(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x673
        const uint64_t unit = CoA::Read::U64(msg);
        ReadRecord(unit, msg);
        return 1;
    }

    // (The original loops while its read position differs from the size; this stops at the end.)
    int32_t OnAuraUpdateAllAddon(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x674
        const uint64_t unit = CoA::Read::U64(msg);

        while (CoA::Read::Remaining(msg) > 0) {
            if (!ReadRecord(unit, msg) || CoA::Read::Failed(msg)) {
                break;
            }
        }

        return 1;
    }

    // FUN_10a12970 / FUN_10a12b60 / FUN_10a12d40: the unit's entries summed per effect, for the effects `pick`
    // selects. nil for a bad token; 0 when the token names no unit.
    template <class Pick>
    int SumAmounts(lua_State* L, Pick pick) {
        const std::string token = CoA::Script::CheckString(L, 1);
        uint64_t guid = 0;

        if (!s_deps.tokenGuid || !s_deps.tokenGuid(token.c_str(), guid)) {
            lua_pushnil(L);
            return 1;
        }

        int32_t sum = 0;

        if (s_deps.unitExists && s_deps.unitExists(guid)) {
            for (const Entry& e : s_units[guid]) {
                CoA::AuraAmounts::SpellEffects rec;

                if (!Effects(e.spell, rec)) {
                    continue;
                }

                for (int32_t j = 0; j < 3; ++j) {
                    if (pick(rec, j)) {
                        sum += e.amount[j];
                    }
                }
            }
        }

        CoA::Script::PushInt(L, sum);
        return 1;
    }

    int UnitGetTotalAbsorbs(lua_State* L) {
        return SumAmounts(L, [](const CoA::AuraAmounts::SpellEffects& r, int32_t j) {
            return (r.aura[j] == 0x45 || r.aura[j] == 0x61) && r.misc[j] == 0x7F;
        });
    }

    int UnitGetTotalHealAbsorbs(lua_State* L) {
        return SumAmounts(L, [](const CoA::AuraAmounts::SpellEffects& r, int32_t j) { return r.aura[j] == 0x12D && r.misc[j] == 0x7F; });
    }

    int UnitGetTotalSchoolAbsorbs(lua_State* L) {
        return SumAmounts(L, [](const CoA::AuraAmounts::SpellEffects& r, int32_t j) { return r.aura[j] == 0x45; });
    }

    // FUN_10a10620 (every glue screen): the per-unit map emptied.
    void ClearUnits() {
        s_units.clear();
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "UnitGetTotalAbsorbs", UnitGetTotalAbsorbs),
        COA_NATIVE("", "UnitGetTotalHealAbsorbs", UnitGetTotalHealAbsorbs),
        COA_NATIVE("", "UnitGetTotalSchoolAbsorbs", UnitGetTotalSchoolAbsorbs),
    };
}

CoA::AuraAmounts::Deps& CoA::AuraAmounts::Dependencies() {
    return s_deps;
}

const int32_t* CoA::AuraAmounts::Find(uint64_t unit, uint64_t caster, uint32_t spell) {
    auto it = s_units.find(unit);

    if (it == s_units.end()) {
        return nullptr;
    }

    for (const Entry& e : it->second) {
        if (e.caster == caster && e.spell == spell) {
            return e.amount;
        }
    }

    return nullptr;
}

bool CoA::AuraAmounts::LastForSpell(uint64_t unit, uint32_t spell, int32_t out[3]) {
    auto it = s_units.find(unit);

    if (it == s_units.end()) {
        return false;
    }

    bool found = false;

    for (const Entry& e : it->second) {
        if (e.spell == spell) {
            out[0] = e.amount[0];
            out[1] = e.amount[1];
            out[2] = e.amount[2];
            found = true;
        }
    }

    return found;
}

void CoA::AuraAmounts::Forget(uint64_t unit) {
    s_units.erase(unit);
}

void CoA::AuraAmounts::Init() {
    s_units.clear();
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    CoA::Lifecycle::OnGlueScreen(ClearUnits);
    COA_PACKET(0x673, OnAuraUpdateAddon);
    COA_PACKET(0x674, OnAuraUpdateAllAddon);
    COA_EVENT("UNIT_ABSORB_AMOUNT_CHANGED");
    COA_EVENT("UNIT_HEAL_ABSORB_AMOUNT_CHANGED");
}
