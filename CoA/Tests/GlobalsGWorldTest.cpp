#include "CoA/Source/Modules/AttachPatches.h"
#include "CoA/Source/Modules/GlobalsG.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"

namespace {
    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::GlobalsG::Reset();
        CoA::GlobalsG::Init();
        return CoATest::NewState(false);
    }
}

// SetAFKTimer / SetAFKLogoutTimer: 0 turns the check off, anything else is the compare value.
COA_TEST(GlobalsG_AfkTimers) {
    lua_State* L = Setup();
    CHECK(CoA::GlobalsG::AfkEnabled() && CoA::GlobalsG::AfkTime() == 300000);
    CoATest::Call(L, "", "SetAFKTimer", { "n:0" });
    CHECK(!CoA::GlobalsG::AfkEnabled());
    CoATest::Call(L, "", "SetAFKTimer", { "n:600000" });
    CHECK(CoA::GlobalsG::AfkEnabled() && CoA::GlobalsG::AfkTime() == 600000);

    CoATest::Call(L, "", "SetAFKLogoutTimer", { "n:900000" });
    CHECK(CoA::AttachPatches::AfkLogoutEnabled() && CoA::GlobalsG::AfkLogoutTime() == 900000);
    CoATest::Call(L, "", "SetAFKLogoutTimer", { "n:0" });
    CHECK(!CoA::AttachPatches::AfkLogoutEnabled());
}

// SetNamePlateRange: clamped to 5..60 yards, squared; the scales to 0..1.
COA_TEST(GlobalsG_NamePlateRangeAndScale) {
    lua_State* L = Setup();
    CHECK(CoA::GlobalsG::NamePlateRangeSq() == 1681.0f);
    CoATest::Call(L, "", "SetNamePlateRange", { "n:80" });
    CHECK(CoA::GlobalsG::NamePlateRangeSq() == 3600.0f);
    CoATest::Call(L, "", "SetNamePlateRange", { "n:2" });
    CHECK(CoA::GlobalsG::NamePlateRangeSq() == 25.0f);
    CoATest::Call(L, "", "SetNamePlateScaleX", { "n:1.5" });
    CoATest::Call(L, "", "SetNamePlateScaleY", { "n:-1" });
    CHECK(CoA::GlobalsG::NamePlateWidth() == 1.0f && CoA::GlobalsG::NamePlateHeight() == 0.0f);
}
