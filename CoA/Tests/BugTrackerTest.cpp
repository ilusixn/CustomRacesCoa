// C_BugTracker / CreateBugReport (BugTracker.cpp): the setters and their limits, BUG_REPORT_MODIFIED in the world, CMSG 0x562
// byte-exact, the 2 s submit throttle, SMSG 0x565 / 0x566, the gate and the clears (glue screen, world entry).
#include "CoA/Source/Modules/BugTracker.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;
    using Packets = std::vector<std::vector<uint8_t>>;

    int64_t s_now = 0;

    void Realm(uint32_t field64) {
        CoATest::Bytes b;
        b.U32(1).U32(0).F32(0).F32(1).F32(0).U32(0).F32(0).F32(0).U32(0);

        for (int i = 0; i < 8; ++i) {
            b.U8(0);
        }

        b.Str("").Str("Realm").U8(0).U32(field64);
        CoATest::Deliver(CoA::RealmInfo::OnRealmInfo, 0x9BC, b);
    }

    lua_State* Start() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::ClientHooks::enterWorld.Clear();
        CoA::BugTracker::Reset();
        CoA::BugTracker::Init();
        CoA::BugTracker::Dependencies().now = [] { return s_now; };
        s_now = 1000;
        Realm(0x77);
        CoA::ClientState::SetActionProbe(nullptr);
        CoA::ClientState::SetInWorldProbe(nullptr);
        return CoATest::NewState(false);
    }

    void Stop(lua_State* L) {
        lua_close(L);
        CoATest::StopRecordingPackets();
        CoATest::StopRecordingSignals();
        CoA::ClientState::SetActionProbe(nullptr);
        CoA::ClientState::SetInWorldProbe(nullptr);
        CoA::ClientHooks::enterWorld.Clear();
        CoA::BugTracker::Reset();
        Realm(0);
    }

    V Call(lua_State* L, const char* name, const V& args = {}) {
        return CoATest::Call(L, "C_BugTracker", name, args);
    }

    std::vector<uint8_t> Report(uint32_t realm, uint32_t category, uint32_t priority, uint8_t isPublic, const std::string& title,
        const std::string& description) {
        CoATest::Bytes b;
        b.U32(realm).U32(category).U32(priority).U8(isPublic).U32(static_cast<uint32_t>(title.size()));
        b.Raw(title.data(), static_cast<uint32_t>(title.size())).U32(static_cast<uint32_t>(description.size()));
        b.Raw(description.data(), static_cast<uint32_t>(description.size()));
        return CoATest::Expected(0x562, b);
    }

    Packets Submit(lua_State* L, V* result = nullptr) {
        CoATest::RecordPackets();
        const V r = Call(L, "SubmitReport");
        CoATest::StopRecordingPackets();

        if (result) {
            *result = r;
        }

        return CoATest::SentPackets();
    }
}

// The report as loaded: category 9, priority 0, empty texts, private; the realm field is taken by the clears only.
COA_TEST(BugTracker_DefaultsAndSetters) {
    lua_State* L = Start();
    CHECK(Call(L, "GetReportCategory") == V{ "9" });
    CHECK(Call(L, "GetReportPriority") == V{ "0" });
    CHECK(Call(L, "GetReportTitle") == V{ "" });
    CHECK(Call(L, "GetReportDescription") == V{ "" });
    CHECK(Call(L, "IsReportPublic") == V{ "false" });
    CHECK(Call(L, "SetReportCategory", { "n:9" }) == V{ "true" });
    CHECK(Call(L, "SetReportCategory", { "n:10" }) == V{ "false" });
    CHECK(Call(L, "SetReportCategory", { "n:3.9" }) == V{ "true" });
    CHECK(Call(L, "GetReportCategory") == V{ "3" });
    CHECK(Call(L, "SetReportCategory", { "s:x" }) == V{ "false" });
    CHECK(Call(L, "SetReportPriority", { "n:3" }) == V{ "true" });
    CHECK(Call(L, "SetReportPriority", { "n:4" }) == V{ "false" });
    CHECK(Call(L, "GetReportPriority") == V{ "3" });
    CHECK(Call(L, "SetReportTitle", { "s:" }) == V{ "false" });
    CHECK(Call(L, "SetReportTitle", { "s:" + std::string(0x81, 'a') }) == V{ "false" });
    CHECK(Call(L, "SetReportTitle", { "s:" + std::string(0x80, 'a') }) == V{ "true" });
    CHECK(Call(L, "SetReportTitle", { "s:Crash" }) == V{ "true" });
    CHECK(Call(L, "GetReportTitle") == V{ "Crash" });
    CHECK(Call(L, "SetReportDescription", { "s:" + std::string(0x1001, 'd') }) == V{ "false" });
    CHECK(Call(L, "SetReportDescription", { "s:" + std::string(0x1000, 'd') }) == V{ "true" });
    CHECK(Call(L, "SetReportDescription", { "s:" }) == V{ "false" });
    CHECK(Call(L, "SetReportDescription", { "s:It broke" }) == V{ "true" });
    CHECK(Call(L, "GetReportDescription") == V{ "It broke" });
    CHECK(Call(L, "SetReportPublic", { "b:1" }) == V{ "true" });
    CHECK(Call(L, "IsReportPublic") == V{ "true" });
    CHECK(Call(L, "SetReportPublic", { "s:x" }) == V{ "false" });
    CHECK(Call(L, "SetReportPublic", {}) == V{ "false" });      // a boolean slot is required
    CHECK(Call(L, "SetReportPublic", { "nil" }) == V{ "true" });   // nil is a boolean slot's false
    CHECK(Call(L, "IsReportPublic") == V{ "false" });
    Stop(L);
}

// Every setter and the clear fire BUG_REPORT_MODIFIED, in the world only.
COA_TEST(BugTracker_ModifiedEvent) {
    lua_State* L = Start();
    CoATest::RecordSignals();
    Call(L, "SetReportCategory", { "n:1" });
    CHECK(CoATest::Signals().empty());
    CoA::ClientState::SetInWorldProbe([] { return true; });
    Call(L, "SetReportCategory", { "n:1" });
    Call(L, "SetReportCategory", { "n:11" });   // refused: no event
    Call(L, "SetReportPriority", { "n:1" });
    Call(L, "SetReportTitle", { "s:t" });
    Call(L, "SetReportDescription", { "s:d" });
    Call(L, "SetReportPublic", { "b:0" });
    Call(L, "ClearReport");
    CHECK(CoATest::Signals() == V(6, "BUG_REPORT_MODIFIED"));
    CoATest::Signals().clear();
    CoA::Lifecycle::RunGlueScreen();
    CoA::ClientHooks::enterWorld.Run();
    CHECK(CoATest::Signals() == V(2, "BUG_REPORT_MODIFIED"));
    CoATest::StopRecordingSignals();
    Stop(L);
}

// SubmitReport: CMSG 0x562 with the realm field taken at the last clear; refused while invalid or within 2 s.
COA_TEST(BugTracker_Submit) {
    lua_State* L = Start();
    V r;
    CHECK(Submit(L, &r).empty());   // no title
    CHECK(r == V{ "false" });
    Call(L, "SetReportTitle", { "s:Title" });
    CHECK(Submit(L).empty());       // no description
    Call(L, "SetReportDescription", { "s:Desc" });
    // Never cleared since load: the realm field is still 0.
    CHECK(Submit(L, &r) == Packets{ Report(0, 9, 0, 0, "Title", "Desc") });
    CHECK(r == V{ "true" });
    s_now = 1001;
    CHECK(Submit(L, &r).empty());   // 1 s later
    CHECK(r == V{ "false" });
    s_now = 1002;
    CHECK(Submit(L) == Packets{ Report(0, 9, 0, 0, "Title", "Desc") });
    Call(L, "ClearReport");
    CHECK(Call(L, "GetReportTitle") == V{ "" });
    Call(L, "SetReportTitle", { "s:T" });
    Call(L, "SetReportDescription", { "s:D" });
    Call(L, "SetReportCategory", { "n:2" });
    Call(L, "SetReportPriority", { "n:1" });
    Call(L, "SetReportPublic", { "b:1" });
    // The clear forgot the last submit: no throttle, and the realm field is now 0x77.
    CHECK(Submit(L) == Packets{ Report(0x77, 2, 1, 1, "T", "D") });
    Stop(L);
}

// CreateBugReport(category, priority, title, description, public): sets every field and sends, unthrottled.
COA_TEST(BugTracker_CreateBugReport) {
    lua_State* L = Start();
    CoATest::RecordPackets();
    CHECK(CoATest::Call(L, "", "CreateBugReport", { "n:4", "n:2", "s:Hi", "s:There", "b:1" }) == V{ "true" });
    CHECK(CoATest::Call(L, "", "CreateBugReport", { "n:4", "n:2", "s:Hi", "s:There", "b:0" }) == V{ "true" });
    CHECK(CoATest::Call(L, "", "CreateBugReport", { "n:10", "n:2", "s:Hi", "s:There", "b:0" }) == V{ "false" });
    CHECK(CoATest::Call(L, "", "CreateBugReport", { "n:9", "n:4", "s:Hi", "s:There", "b:0" }) == V{ "false" });
    CHECK(CoATest::Call(L, "", "CreateBugReport", { "n:9", "n:3", "s:", "s:There", "b:0" }) == V{ "false" });
    CHECK(CoATest::Call(L, "", "CreateBugReport", { "n:9", "n:3", "s:Hi", "s:", "b:0" }) == V{ "false" });
    CHECK(CoATest::Call(L, "", "CreateBugReport", { "n:9", "n:3", "s:Hi", "s:There" }) == V{ "false" });   // the boolean is required
    CHECK(CoATest::Call(L, "", "CreateBugReport", { "s:x", "n:3", "s:Hi", "s:There", "b:0" }) == V{ "false" });
    CoATest::StopRecordingPackets();
    CHECK((CoATest::SentPackets() == Packets{ Report(0, 4, 2, 1, "Hi", "There"), Report(0, 4, 2, 0, "Hi", "There") }));
    CHECK(Call(L, "GetReportCategory") == V{ "4" });
    CHECK(Call(L, "IsReportPublic") == V{ "false" });
    // A failed create leaves the report.
    CHECK(Call(L, "GetReportTitle") == V{ "Hi" });
    Stop(L);
}

// SMSG 0x565: u32 length + bytes; SMSG 0x566: u32 id, the report cleared first.
COA_TEST(BugTracker_Results) {
    lua_State* L = Start();
    Call(L, "SetReportTitle", { "s:T" });
    CoATest::RecordSignals();
    CoATest::Bytes err;
    err.Name("Too long");
    CDataStore m1(err.data.data(), static_cast<uint32_t>(err.data.size()));
    int32_t result = 0;
    CoA::Packets::Dispatch(0x565, 0, &m1, result);
    CoA::ClientState::SetInWorldProbe([] { return true; });
    CoATest::Bytes ok;
    ok.U32(4242);
    CDataStore m2(ok.data.data(), static_cast<uint32_t>(ok.data.size()));
    CoA::Packets::Dispatch(0x566, 0, &m2, result);
    CoATest::StopRecordingSignals();
    CHECK((CoATest::Signals() == V{ "CREATE_BUG_REPORT_ERROR:Too long", "BUG_REPORT_MODIFIED", "CREATE_BUG_REPORT_SUCCESS:4242" }));
    CHECK(Call(L, "GetReportTitle") == V{ "" });
    Stop(L);
}

// The gate: nothing answers, nothing changes.
COA_TEST(BugTracker_Gate) {
    lua_State* L = Start();
    CoA::ClientState::SetActionProbe([](int32_t action) { return action != 0x11; });
    CoATest::RecordPackets();
    for (const char* n : { "ClearReport", "SubmitReport", "GetReportCategory", "GetReportPriority", "GetReportTitle", "GetReportDescription",
             "IsReportPublic" }) {
        CHECK(Call(L, n).empty());
    }
    CHECK(Call(L, "SetReportCategory", { "n:1" }).empty());
    CHECK(Call(L, "SetReportPriority", { "n:1" }).empty());
    CHECK(Call(L, "SetReportTitle", { "s:a" }).empty());
    CHECK(Call(L, "SetReportDescription", { "s:a" }).empty());
    CHECK(Call(L, "SetReportPublic", { "b:1" }).empty());
    CHECK(CoATest::Call(L, "", "CreateBugReport", { "n:4", "n:2", "s:Hi", "s:There", "b:1" }).empty());
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().empty());
    CoA::ClientState::SetActionProbe(nullptr);
    CHECK(Call(L, "GetReportCategory") == V{ "9" });
    CHECK(Call(L, "GetReportTitle") == V{ "" });
    Stop(L);
}

// The glue screen and the world entry clear the report (and the throttle).
COA_TEST(BugTracker_Clears) {
    lua_State* L = Start();
    Call(L, "SetReportTitle", { "s:T" });
    Call(L, "SetReportCategory", { "n:1" });
    CoA::Lifecycle::RunGlueScreen();
    CHECK(Call(L, "GetReportTitle") == V{ "" });
    CHECK(Call(L, "GetReportCategory") == V{ "9" });
    Call(L, "SetReportTitle", { "s:T" });
    Call(L, "SetReportDescription", { "s:D" });
    CHECK(Submit(L) == Packets{ Report(0x77, 9, 0, 0, "T", "D") });
    CoA::ClientHooks::enterWorld.Run();
    CHECK(Call(L, "GetReportDescription") == V{ "" });
    Call(L, "SetReportTitle", { "s:T" });
    Call(L, "SetReportDescription", { "s:D" });
    CHECK(Submit(L).size() == 1);   // the same second: the clear reset the throttle
    Stop(L);
}
