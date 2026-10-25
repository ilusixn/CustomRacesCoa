// AscSpellActivationOverlay.cpp (see SpellActivationOverlay.h).
#include "CoA/Source/Modules/SpellActivationOverlay.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "CoA/Source/Runtime/Timer.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace {
    CoA::Dbc::Table& Overlays() {
        return CoA::Dbc::Get("DBFilesClient/SpellActivationOverlays.dbc");
    }

    std::vector<uint32_t> s_shown;   // 0x10be43d4
    uint32_t s_timer = 0;            // 0x10be43c4
    CoA::SpellActivationOverlay::Deps s_deps;

    // PTR_s_TRIGGER_TYPE_NONE_10b3cb94, parsed by FUN_10215070
    const char* const kTriggerTypes[] = {
        "TRIGGER_TYPE_NONE", "TRIGGER_TYPE_STACKS", "TRIGGER_TYPE_AURA_EXISTS", "TRIGGER_TYPE_AURA_DOES_NOT_EXIST",
        "TRIGGER_TYPE_COOLDOWN_READY", "TRIGGER_TYPE_CHARGES", "TRIGGER_TYPE_MAX",
    };

    uint32_t TriggerType(const char* name) {
        for (uint32_t i = 0; i < 7; ++i) {
            if (name && strcmp(name, kTriggerTypes[i]) == 0) {
                return i;
            }
        }

        CoA::Log::Printf("Unexpected Value: %s", name ? name : "");
        return 0;
    }

    // FUN_10215370
    bool Compare(uint32_t op, int32_t have, int32_t want) {
        switch (op) {
            case 0: return have == want;
            case 1: return want < have;
            case 2: return have < want;
            case 3: return want <= have;
            case 4: return have <= want;
            default: return false;
        }
    }

    CoA::SpellActivationOverlay::Cooldown SpellCooldown(uint32_t spell) {
        return s_deps.cooldown ? s_deps.cooldown(spell) : CoA::SpellActivationOverlay::Cooldown();
    }

    uint32_t Now() {
        return s_deps.now ? s_deps.now() : 0;
    }

    // 0x1031f640
    bool Shown(uint32_t id) {
        CoA::Dbc::Table& t = Overlays();
        const uint8_t* row = t.Row(id);

        if (!row || !s_deps.hasPlayer || !s_deps.hasPlayer()) {
            return false;
        }

        // A row whose gating spell is on a real cooldown is hidden. (The original compares the smaller of the cooldown's
        // start time and duration against the spell's GCD + 50 ms.)
        uint32_t gate = CoA::Dbc::Table::U32(row, 0x1C);

        if (!gate) {
            gate = CoA::Dbc::Table::U32(row, 4);
        }

        if (gate) {
            const CoA::SpellActivationOverlay::Cooldown c = SpellCooldown(gate);

            if (c.enabled && static_cast<int32_t>(c.start) >= 1 && static_cast<int32_t>(c.duration) >= 1 && c.start + c.duration > Now()) {
                uint32_t recovery = 0;

                if (!s_deps.startRecovery || !s_deps.startRecovery(gate, recovery)) {
                    return false;
                }

                const uint32_t shortest = c.start < c.duration ? c.start : c.duration;

                if (shortest > recovery + 0x32) {
                    return false;
                }
            }
        }

        const uint32_t spell = CoA::Dbc::Table::U32(row, 4);

        switch (TriggerType(t.Str(row, 0x24))) {
            case 1:   // STACKS
            case 5: { // CHARGES
                uint32_t stacks = 0;

                if (!s_deps.auraStacks || !s_deps.auraStacks(spell, stacks)) {
                    return false;
                }

                return Compare(CoA::Dbc::Table::U32(row, 0x30), static_cast<int32_t>(stacks), CoA::Dbc::Table::I32(row, 0x2C));
            }
            case 2:
                return s_deps.hasAura && s_deps.hasAura(spell);
            case 3:
                return !(s_deps.hasAura && s_deps.hasAura(spell));
            case 4: {
                const CoA::SpellActivationOverlay::Cooldown c = SpellCooldown(spell);
                return c.duration + c.start <= Now();
            }
            default:
                return false;
        }
    }

    bool Contains(uint32_t id) {
        return std::find(s_shown.begin(), s_shown.end(), id) != s_shown.end();
    }

    // 0x1031fb60
    int32_t OnTimer(const void*, void*) {
        CoA::SpellActivationOverlay::Tick();
        CoA::Timer::Schedule(100, OnTimer, nullptr);
        return 0;
    }

    uint32_t FirstRank(uint32_t spell) {
        return s_deps.firstRank ? s_deps.firstRank(spell) : spell;
    }

    // handler_GetSpellActivationOverlayInfo: 8 values, the last nil when the dword at +0x28 is 0.
    int GetSpellActivationOverlayInfo(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        CoA::Dbc::Table& t = Overlays();
        const uint8_t* row = t.Row(id);

        if (!row) {
            return CoA::Script::PushNils(L, 8);
        }

        CoA::Script::PushInt(L, CoA::Dbc::Table::I32(row, 4));
        CoA::Script::PushStr(L, t.Str(row, 8));
        CoA::Script::PushInt(L, CoA::Dbc::Table::I32(row, 0x20));
        CoA::Script::PushNum(L, CoA::Dbc::Table::F32(row, 0x18));
        CoA::Script::PushNum(L, CoA::Dbc::Table::F32(row, 0xC));
        CoA::Script::PushNum(L, CoA::Dbc::Table::F32(row, 0x10));
        CoA::Script::PushNum(L, CoA::Dbc::Table::F32(row, 0x14));

        if (CoA::Dbc::Table::I32(row, 0x28)) {
            CoA::Script::PushInt(L, CoA::Dbc::Table::I32(row, 0x28));
        } else {
            lua_pushnil(L);
        }

        return 8;
    }

    // handler_IsSpellOverlayed: is any shown overlay's +0x1C spell the same first rank as arg 1.
    int IsSpellOverlayed(lua_State* L) {
        uint32_t spell;

        if (!CoA::Script::ReadNumber(L, spell)) {
            return 0;
        }

        const uint32_t base = FirstRank(spell);
        CoA::Dbc::Table& t = Overlays();

        for (uint32_t id : s_shown) {
            const uint8_t* row = t.Row(id);

            if (row && FirstRank(CoA::Dbc::Table::U32(row, 0x1C)) == base) {
                CoA::Script::PushBool(L, true);
                return 1;
            }
        }

        CoA::Script::PushBool(L, false);
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_SpellActivationOverlay", "GetSpellActivationOverlayInfo", GetSpellActivationOverlayInfo),
        COA_NATIVE("C_SpellActivationOverlay", "IsSpellOverlayed", IsSpellOverlayed),
    };
}

CoA::SpellActivationOverlay::Deps& CoA::SpellActivationOverlay::Dependencies() {
    return s_deps;
}

void CoA::SpellActivationOverlay::Tick() {
    CoA::Dbc::Table& t = Overlays();

    for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
        const bool on = Shown(id);

        if (!Contains(id)) {
            if (on) {
                s_shown.push_back(id);
                CoA::Events::Signal("SHOW_SPELL_ACTIVATION_OVERLAY", "%u", id);
            }
        } else if (!on) {
            s_shown.erase(std::remove(s_shown.begin(), s_shown.end(), id), s_shown.end());
            CoA::Events::Signal("HIDE_SPELL_ACTIVATION_OVERLAY", "%u", id);
        }
    }
}

void CoA::SpellActivationOverlay::Start() {
    s_timer = CoA::Timer::Schedule(100, OnTimer, nullptr);
}

void CoA::SpellActivationOverlay::Stop() {
    s_shown.clear();

    if (s_timer) {
        CoA::Timer::Cancel(s_timer, OnTimer, nullptr);
        s_timer = 0;
    }
}

void CoA::SpellActivationOverlay::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    CoA::Lifecycle::OnGlueScreen(Stop);
    COA_EVENT("SHOW_SPELL_ACTIVATION_OVERLAY");
    COA_EVENT("HIDE_SPELL_ACTIVATION_OVERLAY");
}

void CoA::SpellActivationOverlay::Reset() {
    s_shown.clear();
    s_timer = 0;
}
