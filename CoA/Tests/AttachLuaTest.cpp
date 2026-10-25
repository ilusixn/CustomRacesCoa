#include "CoA/Source/Modules/AttachLua.h"
#include "CoA/Source/Modules/AttachMisc.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cstdio>
#include <cstring>
#include <string>

namespace {
    std::string ReadAll(const char* file) {
        std::string text;
        FILE* f = fopen(file, "rb");

        if (!f) {
            return text;
        }

        char buf[512];
        size_t n;

        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
            text.append(buf, n);
        }

        fclose(f);
        return text;
    }
}

// 0x4FBD20: strftime "%Y-%m-%d-%H.%M.%S" under Logs.
COA_TEST(AttachLua_CombatLogFileName) {
    SYSTEMTIME t = {};
    t.wYear = 2026;
    t.wMonth = 10;
    t.wDay = 5;
    t.wHour = 7;
    t.wMinute = 3;
    t.wSecond = 9;
    CHECK(CoA::AttachLua::CombatLogFileName(t) == "Logs\\2026-10-05-07.03.09 WoWCombatLog.txt");
}

// 0x7668C0: scriptProfile takes only "0"; nameplateShowEnemies posts its message.
COA_TEST(AttachMisc_ScriptProfileOnlyZero) {
    CHECK(strcmp(CoA::AttachMisc::CVarSetValue("scriptProfile", "1"), "0") == 0);
    CHECK(strcmp(CoA::AttachMisc::CVarSetValue("scriptProfile", "0"), "0") == 0);
    CHECK(strcmp(CoA::AttachMisc::CVarSetValue("gxApi", "d3d9"), "d3d9") == 0);
    CHECK(strcmp(CoA::AttachMisc::NameplateMessage("nameplateShowEnemies", "0"), "NAMEPLATES_MESSAGE_ALL_OFF") == 0);
    CHECK(strcmp(CoA::AttachMisc::NameplateMessage("nameplateShowEnemies", "1"), "NAMEPLATES_MESSAGE_ALL_ON_AUTO") == 0);
    CHECK(CoA::AttachMisc::NameplateMessage("scriptProfile", "1") == nullptr);
}

// 0x81AC90: an id past the table is refused with a Fatal line.
COA_TEST(AttachMisc_SignalEventPastTableRefused) {
    CoATest::TempCwd cwd;
    CoA::Logger::Init();
    CHECK(CoA::AttachMisc::AllowFormattedSignal(3, 722, true, true, nullptr, nullptr));
    CHECK(!CoA::AttachMisc::AllowFormattedSignal(800, 722, true, false, reinterpret_cast<const void*>(0x10), reinterpret_cast<const void*>(0xAB)));
    CoA::Logger::Shutdown();
    CHECK(ReadAll("Logs\\Fatal.txt") ==
        "Blocked invalid FrameScript signal event at 0x0081AC90: eventId=800, registeredEventCount=722, format=0x10, "
        "vaList=0xAB, hasEventData=true, hasEventObject=false\r\n");
}

// 0x403DE0: an empty CMSG 0x53B first.
COA_TEST(AttachMisc_LoginVerifyWorldSends53B) {
    CoATest::RecordPackets();
    CoA::AttachMisc::LoginVerifyWorld();
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().size() == 1);
    CHECK(CoATest::SentPackets()[0] == CoATest::Expected(0x53B, CoATest::Bytes()));
}

// 0x808200: which failures toggle the swing.
COA_TEST(AttachMisc_SpellFailedStopsSwing) {
    CHECK(CoA::AttachMisc::StopsSwing(0x400, 0, 0, 0x55));
    CHECK(CoA::AttachMisc::StopsSwing(0, 0x200, 0, 0x55));
    CHECK(CoA::AttachMisc::StopsSwing(0, 0, 0x100000, 0x55));
    CHECK(!CoA::AttachMisc::StopsSwing(0x400, 0, 0, 0x54));
    CHECK(!CoA::AttachMisc::StopsSwing(0x1, 0x1, 0x1, 0x55));
}
