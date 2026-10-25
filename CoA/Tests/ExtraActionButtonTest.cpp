#include "CoA/Source/Modules/ExtraActionButton.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Timer.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;

    std::set<uint32_t> s_auras;
    std::set<uint32_t> s_items;
    uint32_t s_now = 1000;
    CoA::ExtraActionButton::Cooldown s_cooldown;

    // ExtraActionButtons.dbc: id, int, 3 strings, trigger name, argument, 2 ints, sort order.
    bool ReadButtons(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "ExtraActionButtons.dbc") == nullptr) {
            return false;
        }

        // "\0a\0b\0c\0TRIGGER_TYPE_AURA_EXISTS\0TRIGGER_TYPE_ITEM_IN_BAGS\0TRIGGER_TYPE_COOLDOWN_READY\0BOGUS"
        const std::string strings = std::string("a") + '\0' + "b" + '\0' + "c" + '\0' + "TRIGGER_TYPE_AURA_EXISTS" + '\0' +
            "TRIGGER_TYPE_ITEM_IN_BAGS" + '\0' + "TRIGGER_TYPE_COOLDOWN_READY" + '\0' + "BOGUS";
        const uint32_t a = 1, b = 3, c = 5, aura = 7, item = 32, ready = 58, bogus = 86;
        out = CoATest::Wdbc({
            { 1, 11, a, b, c, aura, 100, 0, 0, 9 },
            { 2, 22, a, b, c, item, 6948, 0, 0, 1 },
            { 3, 33, a, b, c, ready, 300, 0, 0, 5 },
            { 4, 44, a, b, c, bogus, 0, 0, 0, 0 },
        }, strings);
        return true;
    }

    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::Lifecycle::Reset();
        CoA::Dbc::SetFileReader(ReadButtons);
        CoA::ExtraActionButton::Reset();
        CoA::ExtraActionButton::Init();
        s_auras.clear();
        s_items.clear();
        s_now = 1000;
        s_cooldown = { 0, 5000, 1 };
        CoA::ExtraActionButton::Deps& deps = CoA::ExtraActionButton::Dependencies();
        deps.hasPlayer = [] { return true; };
        deps.hasAura = [](uint32_t spell) { return s_auras.count(spell) != 0; };
        deps.spellCooldown = [](uint32_t) { return s_cooldown; };
        deps.itemCooldown = [](uint32_t) { return s_cooldown; };
        deps.now = [] { return s_now; };
        deps.hasItem = [](uint32_t entry) { return s_items.count(entry) != 0; };
        return CoATest::NewState(false);
    }

    V Tick() {
        CoATest::RecordSignals();
        CoA::ExtraActionButton::Tick();
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }
}

// Removals fire in walk order; additions are sorted by the row's +0x24 order, then appended and fired.
COA_TEST(ExtraActionButton_AddRemoveSorted) {
    lua_State* L = Setup();
    CHECK(Tick().empty());
    s_auras.insert(100);
    s_items.insert(6948);
    s_now = 6000;   // spell 300 ready: start 0 + duration 5000 <= 6000
    CHECK((Tick() == V{ "EXTRA_ACTION_BUTTON_ADDED:2", "EXTRA_ACTION_BUTTON_ADDED:3", "EXTRA_ACTION_BUTTON_ADDED:1" }));
    CHECK(CoATest::Call(L, "C_ExtraActionButton", "GetNumExtraActionButtons") == V{ "3" });
    CHECK(CoATest::Call(L, "C_ExtraActionButton", "GetExtraActionButtonAtIndex", { "n:1" }) == V{ "2" });
    CHECK(CoATest::Call(L, "C_ExtraActionButton", "GetExtraActionButtonAtIndex", { "n:4" }) == V{ "nil" });
    CHECK(CoATest::Call(L, "C_ExtraActionButton", "GetExtraActionButtonAtIndex", { "n:0" }).empty());
    s_auras.clear();
    CHECK(Tick() == V{ "EXTRA_ACTION_BUTTON_REMOVED:1" });
    CoA::Dbc::SetFileReader(nullptr);
}

// GetExtraActionButtonInfo: (str +8, str +0xC, int +4, str +0x10), four nils without a row.
COA_TEST(ExtraActionButton_Info) {
    lua_State* L = Setup();
    CHECK((CoATest::Call(L, "C_ExtraActionButton", "GetExtraActionButtonInfo", { "n:2" }) == V{ "a", "b", "22", "c" }));
    CHECK((CoATest::Call(L, "C_ExtraActionButton", "GetExtraActionButtonInfo", { "n:9" }) == V{ "nil", "nil", "nil", "nil" }));
    CoA::Dbc::SetFileReader(nullptr);
}

// Without a player nothing triggers; the glue screen clears the list.
COA_TEST(ExtraActionButton_NoPlayerAndGlue) {
    lua_State* L = Setup();
    s_auras.insert(100);
    CoA::ExtraActionButton::Dependencies().hasPlayer = [] { return false; };
    CHECK(Tick().empty());
    CoA::ExtraActionButton::Dependencies().hasPlayer = [] { return true; };
    CHECK(Tick() == V{ "EXTRA_ACTION_BUTTON_ADDED:1" });
    CoA::Lifecycle::RunGlueScreen();
    CHECK(CoATest::Call(L, "C_ExtraActionButton", "GetNumExtraActionButtons") == V{ "0" });
    CoA::Dbc::SetFileReader(nullptr);
}

namespace {
    int s_scheduled = 0;
    int s_cancelled = 0;
}

// Reproduced original behaviour: each world entry arms another 100 ms chain; the glue stop cancels only the stored id.
COA_TEST(ExtraActionButton_EachWorldEntryAddsAChain) {
    Setup();
    s_scheduled = 0;
    s_cancelled = 0;
    CoA::Timer::SetBackend([](uint32_t, CoA::Timer::Fn, void*) { return static_cast<uint32_t>(++s_scheduled); },
        [](uint32_t, CoA::Timer::Fn, void*) { ++s_cancelled; });
    CoA::ExtraActionButton::Start();
    CoA::ExtraActionButton::Start();
    CHECK(s_scheduled == 2);
    CoA::ExtraActionButton::Stop();
    CHECK(s_cancelled == 1);
    CoA::Timer::SetBackend(nullptr, nullptr);
    CoA::Dbc::SetFileReader(nullptr);
}
