#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <string>

namespace {
    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::GlobalsD::Init();
        return CoATest::NewState(false);
    }
}

// The item addon natives: nil without a record (and without exactly one numeric argument).
COA_TEST(GlobalsD_ItemAddonNativesEmptyStore) {
    lua_State* L = Setup();

    for (const char* name : { "GetItemQuality", "GetItemPvEPower", "GetItemPvPPower", "GetItemName", "GetItemFlavorText" }) {
        CHECK(CoATest::Call(L, "", name, { "n:12345" })[0] == "nil");
        CHECK(CoATest::Call(L, "", name)[0] == "nil");
    }
}
