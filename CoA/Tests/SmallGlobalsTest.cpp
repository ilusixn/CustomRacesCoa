#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/Profiler.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Timer.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstdio>
#include <cstring>

namespace {
    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::Profiler::Reset();
        CoA::Profiler::Init();
        CoA::GlobalsH::Init();
        CoA::GlobalsC::Init();
        CoA::Misc::Init();
        return CoATest::NewState(true);
    }

    std::string ReadAll(const char* path) {
        std::string text;
        FILE* f = fopen(path, "r");

        if (f) {
            char buffer[512];
            size_t n;

            while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) {
                text.append(buffer, n);
            }

            fclose(f);
        }

        return text;
    }

    int s_scheduled = 0;
    int s_cancelled = 0;

    uint32_t FakeSchedule(uint32_t ms, CoA::Timer::Fn fn, void* param) {
        ++s_scheduled;
        return 100 + s_scheduled;
    }

    void FakeCancel(uint32_t id, CoA::Timer::Fn fn, void* param) {
        ++s_cancelled;
    }

    // ScreenLocations.dbc: {id, path} with one row id 3 -> "Interface\Glues\X".
    bool ReadScreenLocations(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "ScreenLocations.dbc") == nullptr) {
            return false;
        }

        const char strings[] = "\0Interface\\Glues\\X";
        uint32_t header[] = { 0x43424457, 1, 2, 8, sizeof(strings), 3, 1 };
        out.assign(reinterpret_cast<const uint8_t*>(header), reinterpret_cast<const uint8_t*>(header) + sizeof(header));
        out.insert(out.end(), strings, strings + sizeof(strings));
        return true;
    }
}

COA_TEST(Profiler_CountsOnlyWhileEnabled) {
    CoATest::TempCwd cwd;
    CoA::Logger::Init();
    lua_State* L = Setup();

    CoA::Profiler::Handled(0x3B, CoA::Profiler::Received(0x3B, 100));
    CHECK(CoA::Profiler::Count(0x3B) == 0);

    CoATest::Call(L, "", "SetOpcodeLoggingEnabled", { "b:1", "b:0" });
    CoA::Profiler::Handled(0x3B, CoA::Profiler::Received(0x3B, 100));
    CoA::Profiler::Handled(0x3B, CoA::Profiler::Received(0x3B, 300));
    CoA::Profiler::SendFinished(0x37, CoA::Profiler::SendStarted(0x37, 6));
    CHECK(CoA::Profiler::Count(0x3B) == 2);
    CHECK(CoA::Profiler::Bytes(0x3B) == 400);
    CHECK(CoA::Profiler::CurrentOpcode() == 0x3B);

    CoATest::Call(L, "", "LogOpcodes");
    CoA::Logger::Shutdown();
    std::string trace = ReadAll("Logs\\Trace.txt");
    CHECK(trace.find("=== Network Statistics (") != std::string::npos);
    CHECK(trace.find("SMSG_CHAR_ENUM (0x0000003B)") != std::string::npos);
    CHECK(trace.find("SENT CMSG_CHAR_ENUM (0x00000037)") != std::string::npos);
    // LogOpcodes resets the counters.
    CHECK(CoA::Profiler::Count(0x3B) == 0);
    lua_close(L);
}

COA_TEST(Profiler_PeriodicArmsBothTimers) {
    lua_State* L = Setup();
    s_scheduled = 0;
    s_cancelled = 0;
    CoA::Timer::SetBackend(FakeSchedule, FakeCancel);
    CoATest::Call(L, "", "SetOpcodeLoggingEnabled", { "b:1", "b:1" });
    CHECK(s_scheduled == 2);
    CoATest::Call(L, "", "SetSignalEventLoggingEnabled", { "b:0", "b:0" });
    CHECK(s_cancelled == 2);
    CHECK(s_scheduled == 2);
    // Not two booleans: nothing happens.
    CoATest::Call(L, "", "SetOpcodeLoggingEnabled", { "s:x" });
    CHECK(s_cancelled == 2);
    CoA::Timer::SetBackend(nullptr, nullptr);
    lua_close(L);
}

COA_TEST(GlobalsH_CharacterActivateSendsNonZeroIds) {
    lua_State* L = Setup();
    CoATest::RecordPackets();
    CoATest::Call(L, "", "CharacterActivate", { "n:5" });
    CoATest::Call(L, "", "CharacterActivate", { "n:0" });
    CoATest::Call(L, "", "CharacterDeactivate", { "n:7" });
    CoATest::StopRecordingPackets();
    CoATest::Bytes five;
    five.U32(5);
    CoATest::Bytes seven;
    seven.U32(7);
    CHECK(CoATest::SentPackets().size() == 2);
    CHECK(CoATest::SentPackets().size() == 2 && CoATest::SentPackets()[0] == CoATest::Expected(0x72E, five));
    CHECK(CoATest::SentPackets().size() == 2 && CoATest::SentPackets()[1] == CoATest::Expected(0x72F, seven));
    lua_close(L);
}

COA_TEST(GlobalsH_CallstackHeight) {
    lua_State* L = Setup();
    // Inside the native: level 0 the native, 1 f, 2 the chunk.
    CHECK(luaL_dostring(L, "local function f() local n = GetCallstackHeight(); return n end height = f()") == 0);
    lua_getglobal(L, "height");
    CHECK(lua_tonumber(L, -1) == 3);
    lua_close(L);
}

COA_TEST(GlobalsC_ScreenLocationInfo) {
    CoA::Dbc::SetFileReader(ReadScreenLocations);
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "", "GetScreenLocationInfo", { "n:3" }) == std::vector<std::string>{ "Interface\\Glues\\X" });
    CHECK(CoATest::Call(L, "", "GetScreenLocationInfo", { "n:4" }) == std::vector<std::string>{ "nil" });
    CHECK(CoATest::Call(L, "", "GetScreenLocationInfo", { "s:x" }).empty());
    CoA::Dbc::SetFileReader(nullptr);
    lua_close(L);
}

COA_TEST(Misc_IsAddOnsAllowedFollowsRealmInfo) {
    CoA::RealmInfo::Reset();
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "", "IsAddOnsAllowed") == std::vector<std::string>{ "false" });
    CoATest::Bytes b;
    b.U32(1).U32(0).F32(0).F32(0).F32(0).U32(0).F32(0).F32(0).U32(0);
    for (int i = 0; i < 8; ++i) {
        b.U8(0);
    }
    b.Str("").Str("r").U8(1);
    CoATest::Deliver(CoA::RealmInfo::OnRealmInfo, 0x9BC, b);
    CHECK(CoATest::Call(L, "", "IsAddOnsAllowed") == std::vector<std::string>{ "true" });
    lua_close(L);
}
