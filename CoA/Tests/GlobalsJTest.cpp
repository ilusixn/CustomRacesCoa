#include "CoA/Source/Modules/GlobalsJ.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <Windows.h>
#include <direct.h>
#include <cstdio>

namespace {
    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::GlobalsJ::Reset();
        CoA::GlobalsJ::Init();
        return CoATest::NewState(true);
    }

    void WriteFile(const char* path, const char* text) {
        FILE* f = fopen(path, "w");
        fputs(text, f);
        fclose(f);
    }
}

COA_TEST(CustomWtf_MissingDirectoryReadsEmpty) {
    CoATest::TempCwd cwd;
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "", "ReadCustomWTF", { "s:Nothing" }) == std::vector<std::string>{ "" });
    lua_close(L);
}

COA_TEST(CustomWtf_EmptyFileReadsEmpty) {
    CoATest::TempCwd cwd;
    lua_State* L = Setup();
    CreateDirectoryA("WTF\\Custom", nullptr);
    WriteFile("WTF\\Custom\\Empty.json", "");
    CHECK(CoATest::Call(L, "", "ReadCustomWTF", { "s:Empty" }) == std::vector<std::string>{ "" });
    lua_close(L);
}

COA_TEST(CustomWtf_WriteCreatesDirectoryAndRoundTrips) {
    CoATest::TempCwd cwd;
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "", "WriteCustomWTF", { "s:Panel", "s:{\"a\":1}" }) == std::vector<std::string>{ "true" });
    CHECK(GetFileAttributesA("WTF\\Custom\\Panel.json") != INVALID_FILE_ATTRIBUTES);
    CHECK(CoATest::Call(L, "", "ReadCustomWTF", { "s:Panel" }) == std::vector<std::string>{ "{\"a\":1}" });
    lua_close(L);
}

COA_TEST(CustomWtf_DotDotNameIsNotSanitized) {
    CoATest::TempCwd cwd;
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "", "WriteCustomWTF", { "s:..\\Escaped", "s:x" }) == std::vector<std::string>{ "true" });
    CHECK(GetFileAttributesA("WTF\\Escaped.json") != INVALID_FILE_ATTRIBUTES);
    lua_close(L);
}

COA_TEST(CustomWtf_ClosedGateReturnsTheArgument) {
    CoATest::TempCwd cwd;
    lua_State* L = Setup();
    CoA::ClientState::SetActionProbe([](int32_t) { return false; });
    CHECK(CoATest::Call(L, "", "ReadCustomWTF", { "s:Panel" }) == std::vector<std::string>{ "Panel" });
    CoA::ClientState::SetActionProbe(nullptr);
    lua_close(L);
}

COA_TEST(KnownAddons_FromPacket) {
    lua_State* L = Setup();
    CoATest::Bytes b;
    b.U32(3).Str("Blizzard_AuctionUI").U8(1).Str("Ascension_Store").U8(1).Str("Plain").U8(0);
    CoATest::Deliver(CoA::GlobalsJ::OnKnownAddonList, 0x94E, b);

    CHECK((CoATest::Call(L, "", "GetKnownAddons") == std::vector<std::string>{ "Ascension_Store", "Plain" }));
    CHECK(CoATest::Call(L, "", "GetSecureAddons") == std::vector<std::string>{ "Ascension_Store" });
    CHECK(CoATest::Call(L, "", "IsKnownAddon", { "s:Blizzard_AuctionUI" }) == std::vector<std::string>{ "true" });
    CHECK(CoATest::Call(L, "", "IsKnownAddon", { "s:Other" }) == std::vector<std::string>{ "false" });
    lua_close(L);
}

COA_TEST(RegisterSavedCVar_BlockedForUnsecureAddon) {
    lua_State* L = Setup();
    CoATest::Bytes b;
    b.U32(2).Str("SecureOne").U8(1).Str("Plain").U8(0);
    CoATest::Deliver(CoA::GlobalsJ::OnKnownAddonList, 0x94E, b);

    lua_tainted = "Plain";
    CHECK(CoATest::Call(L, "", "RegisterSavedCVar", { "s:coaTestBlocked", "s:1" }) == std::vector<std::string>{ "false" });
    lua_tainted = "SecureOne";
    CHECK(CoATest::Call(L, "", "RegisterSavedCVar", { "s:coaTestSecure", "s:1", "b:1" }) == std::vector<std::string>{ "true" });
    lua_tainted = nullptr;

    CHECK(CVar::Lookup("coaTestBlocked") == nullptr);
    CVar* var = CVar::Lookup("coaTestSecure");
    CHECK(var && var->m_flags == 0x80000021);
    lua_close(L);
}

COA_TEST(CanPerformAction_0x19FollowsRunScript) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "", "CanPerformAction", { "n:25" }) == std::vector<std::string>{ "false" });
    CoA::GlobalsJ::SetInsideRunScript(true);
    CHECK(CoATest::Call(L, "", "CanPerformAction", { "n:25" }) == std::vector<std::string>{ "true" });
    CoA::GlobalsJ::SetInsideRunScript(false);
    CoA::ClientState::SetActionProbe([](int32_t action) { return action == 3; });
    CHECK(CoATest::Call(L, "", "CanPerformAction", { "n:3" }) == std::vector<std::string>{ "true" });
    CHECK(CoATest::Call(L, "", "CanPerformAction", { "n:4" }) == std::vector<std::string>{ "false" });
    CoA::ClientState::SetActionProbe(nullptr);
    lua_close(L);
}

COA_TEST(ErrorHandler_CountsStackWhileRunning) {
    lua_State* L = Setup();
    lua_register(L, "probe", [](lua_State* L) -> int {
        CoA::GlobalsJ::OnErrorHandler(L, 1);
        return 0;
    });
    // Inside probe: level 0 probe, 1 f, 2 the chunk.
    CHECK(luaL_dostring(L, "local function f() local r = probe(); return r end f()") == 0);
    CHECK(CoATest::Call(L, "", "GetErrorCallstackHeight") == std::vector<std::string>{ "3" });
    CoA::GlobalsJ::OnErrorHandler(L, 0);
    CHECK(CoATest::Call(L, "", "GetErrorCallstackHeight") == std::vector<std::string>{ "0" });
    lua_close(L);
}
