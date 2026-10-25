#include "CoA/Source/Modules/Account.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lua.h"

namespace {
    int s_gmEffects = 0;
}

COA_TEST(Account_ParsesAccountInfo) {
    CoA::LuaReg::Reset();
    CoA::Account::Reset();
    CoATest::InitRealmModules();
    lua_State* L = CoATest::NewState(false);

    CoATest::Bytes b;
    b.U32(1).U32(3).U8(1).U8(0).U32(2);
    b.Str("Alpha").Str("MALE").U32(10).Str("MAGE").Str("Human").Str("Alliance");
    b.Str("Beta").Str("FEMALE").U32(20).Str("PRIEST").Str("Orc").Str("Horde");
    CoATest::Deliver(CoA::Account::OnAccountInfo, 0x9BB, b);

    CHECK(CoA::Account::GmLevel() == 3);
    CHECK(CoATest::Call(L, "C_AccountInfo", "GetGMLevel") == std::vector<std::string>{ "3" });
    CHECK(CoATest::Call(L, "C_AccountInfo", "IsNewcomer") == std::vector<std::string>{ "true" });
    CHECK(CoATest::Call(L, "C_AccountInfo", "IsGuide") == std::vector<std::string>{ "false" });
    CHECK(CoATest::Call(L, "C_AccountInfo", "GetNumCharacters") == std::vector<std::string>{ "2" });
    CHECK(CoATest::Call(L, "C_AccountInfo", "GetCharacterAtIndex", { "n:2" }) == std::vector<std::string>{ "table" });
    CHECK(CoATest::Call(L, "C_AccountInfo", "GetCharacterAtIndex", { "n:0" }).empty());
    CHECK(CoATest::Call(L, "C_AccountInfo", "GetCharacterAtIndex", { "n:3" }).empty());
    lua_close(L);
}

COA_TEST(Account_GmStatusAppliesEffectsOnce) {
    CoA::LuaReg::Reset();
    CoA::Account::Reset();
    CoATest::InitRealmModules();
    s_gmEffects = 0;
    CoA::Account::SetGmEffect([] { ++s_gmEffects; });

    CoATest::Bytes off;
    off.U8(0);
    CoATest::Deliver(CoA::Account::OnGMStatus, 0x5C0, off);
    CHECK(s_gmEffects == 0);

    CoATest::Bytes on;
    on.U8(1);
    CoATest::Deliver(CoA::Account::OnGMStatus, 0x5C0, on);
    CHECK(s_gmEffects == 1);

    lua_State* L = CoATest::NewState(false);
    CHECK(CoATest::Call(L, "C_AccountInfo", "IsGM") == std::vector<std::string>{ "true" });
    lua_close(L);
    CoA::Account::SetGmEffect(nullptr);
}
