#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/SpellActivationOverlay.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;

    std::set<uint32_t> s_auras;
    uint32_t s_stacks = 0;
    uint32_t s_now = 1000;
    CoA::SpellActivationOverlay::Cooldown s_cooldown;

    uint32_t Bits(float f) {
        uint32_t u;
        memcpy(&u, &f, 4);
        return u;
    }

    // SpellActivationOverlays.dbc: id, spell, texture, 4 floats, gate spell, int, trigger name, int, value, comparison.
    bool ReadOverlays(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "SpellActivationOverlays.dbc") == nullptr) {
            return false;
        }

        // String block "\0tex\0TRIGGER_TYPE_AURA_EXISTS\0TRIGGER_TYPE_STACKS\0TRIGGER_TYPE_COOLDOWN_READY\0"
        const std::string strings = std::string("tex") + '\0' + "TRIGGER_TYPE_AURA_EXISTS" + '\0' + "TRIGGER_TYPE_STACKS" + '\0' +
            "TRIGGER_TYPE_COOLDOWN_READY";
        const uint32_t tex = 1;
        const uint32_t aura = 5;
        const uint32_t stacks = 30;
        const uint32_t ready = 50;
        out = CoATest::Wdbc({
            { 1, 100, tex, Bits(1.0f), Bits(2.0f), Bits(3.0f), Bits(4.0f), 0, 7, aura, 0, 0, 0 },
            { 2, 200, tex, 0, 0, 0, 0, 0, 0, stacks, 9, 3, 3 },
            { 3, 300, tex, 0, 0, 0, 0, 0, 0, ready, 0, 0, 0 },
        }, strings);
        return true;
    }

    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::Lifecycle::Reset();
        CoA::Dbc::SetFileReader(ReadOverlays);
        CoA::SpellActivationOverlay::Reset();
        CoA::SpellActivationOverlay::Init();
        s_auras.clear();
        s_stacks = 0;
        s_now = 1000;
        s_cooldown = {};
        CoA::SpellActivationOverlay::Deps& deps = CoA::SpellActivationOverlay::Dependencies();
        deps.hasPlayer = [] { return true; };
        deps.hasAura = [](uint32_t spell) { return s_auras.count(spell) != 0; };
        deps.auraStacks = [](uint32_t spell, uint32_t& stacks) {
            stacks = s_stacks;
            return s_auras.count(spell) != 0;
        };
        deps.cooldown = [](uint32_t) { return s_cooldown; };
        deps.now = [] { return s_now; };
        deps.startRecovery = [](uint32_t, uint32_t& ms) {
            ms = 1500;
            return true;
        };
        deps.firstRank = [](uint32_t spell) { return spell == 301 ? 300u : spell; };
        return CoATest::NewState(false);
    }

    V Tick() {
        CoATest::RecordSignals();
        CoA::SpellActivationOverlay::Tick();
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }
}

// The 100 ms tick walks the rows in id order; additions are appended in walk order, not sorted.
COA_TEST(SpellActivationOverlay_ShowHide) {
    lua_State* L = Setup();
    CHECK(Tick() == V{ "SHOW_SPELL_ACTIVATION_OVERLAY:3" });   // spell 300 off cooldown
    s_auras.insert(100);
    s_auras.insert(200);
    s_stacks = 2;
    CHECK(Tick() == V{ "SHOW_SPELL_ACTIVATION_OVERLAY:1" });
    s_stacks = 3;
    CHECK(Tick() == V{ "SHOW_SPELL_ACTIVATION_OVERLAY:2" });
    s_auras.erase(100);
    CHECK(Tick() == V{ "HIDE_SPELL_ACTIVATION_OVERLAY:1" });
    // A gating spell on a real cooldown hides the row: the original compares the smaller of the cooldown's start time
    // and duration against the spell's GCD + 50 ms.
    s_now = 3000;
    s_cooldown = { 2000, 8000, 1 };
    CHECK((Tick() == V{ "HIDE_SPELL_ACTIVATION_OVERLAY:2", "HIDE_SPELL_ACTIVATION_OVERLAY:3" }));

    CHECK(CoATest::Call(L, "C_SpellActivationOverlay", "IsSpellOverlayed", { "n:301" })[0] == "false");
    s_cooldown = {};
    Tick();
    // IsSpellOverlayed compares each shown row's +0x1C column (0 here) after FirstRank, not its spell (+4).
    CHECK(CoATest::Call(L, "C_SpellActivationOverlay", "IsSpellOverlayed", { "n:301" })[0] == "false");
    CHECK(CoATest::Call(L, "C_SpellActivationOverlay", "IsSpellOverlayed", { "n:0" })[0] == "true");
}

// GetSpellActivationOverlayInfo: 8 values, the last nil when +0x28 is 0; nil x 8 without a row.
COA_TEST(SpellActivationOverlay_Info) {
    lua_State* L = Setup();
    CHECK((CoATest::Call(L, "C_SpellActivationOverlay", "GetSpellActivationOverlayInfo", { "n:1" }) ==
        V{ "100", "tex", "7", "4", "1", "2", "3", "nil" }));
    CHECK(CoATest::Call(L, "C_SpellActivationOverlay", "GetSpellActivationOverlayInfo", { "n:2" })[7] == "9");
    CHECK(CoATest::Call(L, "C_SpellActivationOverlay", "GetSpellActivationOverlayInfo", { "n:9" }).size() == 8);
    CoA::Dbc::SetFileReader(nullptr);
}

// The glue screen clears the shown list.
COA_TEST(SpellActivationOverlay_GlueResets) {
    Setup();
    Tick();
    CoA::Lifecycle::RunGlueScreen();
    CHECK(Tick() == V{ "SHOW_SPELL_ACTIVATION_OVERLAY:3" });
    CoA::Dbc::SetFileReader(nullptr);
}
