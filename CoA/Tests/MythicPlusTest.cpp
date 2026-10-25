#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/MythicPlus.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/MemoryBridge.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <ctime>

namespace {
    int32_t Week() {
        __time64_t now = _time64(nullptr);
        tm t{};
        _gmtime64_s(&t, &now);
        return static_cast<int32_t>(static_cast<float>(t.tm_yday) / 7.0f);
    }

    bool ReadTables(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "MythicKeystones.dbc")) {
            out = CoATest::Wdbc({ { 61, 7, 12 } }, "");
            return true;
        }

        if (strstr(path, "MythicAffixes.dbc")) {
            // id, week, level, +0xC..+0x38; this week's level-0xFE row names 103 (+0xC), 101 (+0x1C), 102 (+0x2C).
            std::vector<uint32_t> current(15, 0);
            current[0] = 1;
            current[1] = static_cast<uint32_t>(Week());
            current[2] = 0xFE;
            current[3] = 103;
            current[7] = 101;
            current[11] = 102;
            std::vector<uint32_t> other = current;
            other[0] = 2;
            other[1] = static_cast<uint32_t>(Week() + 1);
            other[3] = 999;
            out = CoATest::Wdbc({ current, other }, "");
            return true;
        }

        return false;
    }

    uint32_t ItemDisplay(uint32_t item) {
        return item == 61 || item == 62 ? 0x108B2 : item == 60 ? 5 : 0;
    }

    lua_State* Setup() {
        CoA::Dbc::SetFileReader(ReadTables);
        CoA::MemoryBridge::Reset();
        CoA::BridgeStore::Reset();
        CoA::BridgeStore::LoadTables();
        CoA::BridgeStore::BuildIndexes();
        CoA::ClientState::Probes().itemDisplayId = ItemDisplay;
        CoA::LuaReg::Reset();
        CoA::MythicPlus::Init();
        return CoATest::NewState(true);
    }

    void Teardown(lua_State* L) {
        lua_close(L);
        CoA::ClientState::Probes().itemDisplayId = nullptr;
        CoA::Dbc::SetFileReader(nullptr);
    }

    std::vector<std::string> Run(lua_State* L, const char* chunk) {
        std::vector<std::string> out;
        int base = lua_gettop(L);

        if (luaL_loadstring(L, chunk) != 0 || lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
            out.push_back(std::string("<error> ") + lua_tostring(L, -1));
            lua_settop(L, base);
            return out;
        }

        for (int i = base + 1; i <= lua_gettop(L); ++i) {
            out.push_back(lua_isnil(L, i) ? "nil" : lua_isboolean(L, i) ? (lua_toboolean(L, i) ? "true" : "false") : lua_tostring(L, i));
        }

        lua_settop(L, base);
        return out;
    }
}

COA_TEST(Mythic_IsItemKeystone) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "C_MythicPlus", "IsItemKeystone", { "n:61" }) == std::vector<std::string>{ "true" });
    // The keystone display, but not in the keystone table.
    CHECK(CoATest::Call(L, "C_MythicPlus", "IsItemKeystone", { "n:62" }) == std::vector<std::string>{ "false" });
    // Another display.
    CHECK(CoATest::Call(L, "C_MythicPlus", "IsItemKeystone", { "n:60" }) == std::vector<std::string>{ "false" });
    Teardown(L);
}

COA_TEST(Mythic_AffixesFromDbc) {
    lua_State* L = Setup();
    CHECK(Run(L, "local a = C_MythicPlus.GetCurrentAffixes() return #a, a[1], a[2], a[3]") == (std::vector<std::string>{ "3", "101", "102", "103" }));
    CHECK(Run(L, "local k = C_MythicPlus.GetKeystoneInfo(61) return k.keystoneLevel, k.dungeonID") == (std::vector<std::string>{ "12", "7" }));
    CHECK(CoATest::Call(L, "C_MythicPlus", "IsKeystoneActive") == std::vector<std::string>{ "false" });
    Teardown(L);
}

COA_TEST(Mythic_KeystonePatchUpdatesTheCachedRecord) {
    lua_State* L = Setup();
    CoATest::Bytes patch;
    patch.U32(61).U32(8).U32(15);
    CoATest::Deliver(CoA::MythicPlus::OnPatchKeystone, 0x56D, patch);
    CHECK(Run(L, "local k = C_MythicPlus.GetKeystoneInfo(61) return k.keystoneLevel, k.dungeonID") == (std::vector<std::string>{ "15", "8" }));
    Teardown(L);
}

COA_TEST(Mythic_RunStateFromPackets) {
    lua_State* L = Setup();
    CoATest::RecordSignals();
    CoATest::Bytes info;
    info.U32(77);
    CoATest::Deliver(CoA::MythicPlus::OnInstanceInfo, 0x9C1, info);
    CoATest::Bytes start;
    start.U64(0).U8(3).U32(61).U32(77);
    CoATest::Deliver(CoA::MythicPlus::OnStateChanged, 0x908, start);
    CoATest::Bytes timer;
    timer.U32(1500);
    CoATest::Deliver(CoA::MythicPlus::OnTimerUpdate, 0x90A, timer);
    CoATest::StopRecordingSignals();
    CHECK(CoATest::Call(L, "C_MythicPlus", "IsKeystoneActive") == std::vector<std::string>{ "true" });
    CHECK(Run(L, "local e = C_MythicPlus.GetActiveKeystoneTime() return e") == std::vector<std::string>{ "1.5" });
    CHECK(CoATest::Signals() == (std::vector<std::string>{ "MYTHIC_PLUS_STARTED", "MYTHIC_PLUS_TIMER_UPDATE:1.5" }));
    Teardown(L);
}
