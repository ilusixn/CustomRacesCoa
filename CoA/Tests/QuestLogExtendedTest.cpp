#include "CoA/Source/Modules/QuestLogExtended.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "WoW/Source/Object/Player.h"
#include <vector>

namespace {
    namespace QL = CoA::QuestLogExtended;
    int s_changes = 0;

    void Setup() {
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::ClientHooks::afterEnterWorld.Clear();
        CoA::ClientHooks::afterUiReload.Clear();
        QL::Reset();
        s_changes = 0;
        QL::OnChanged([] { ++s_changes; });
        QL::Init();
    }

    void Add(CoATest::Bytes& b, uint8_t slot, uint32_t quest, uint32_t level = 0) {
        b.U8(slot).U32(quest).U32(1).U16(1).U16(2).U16(3).U16(4).U32(99).U32(level).U32(500);
    }

    void Feed(CoATest::Bytes& b) {
        CDataStore msg(b.data.data(), static_cast<uint32_t>(b.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(0x8F7, 0, &msg, result);
    }
}

COA_TEST(QuestLogExtended_Snapshot) {
    Setup();
    CoATest::Bytes b;
    b.U8(1).U8(1).U8(64).U8(2);
    Add(b, 30, 1234);
    Add(b, 63, 77, 42);
    Feed(b);
    CHECK(QL::Get(30) && QL::Get(30)->questId == 1234);
    CHECK(QL::Get(30) && QL::Get(30)->counters[3] == 4 && QL::Get(30)->time == 99 && QL::Get(30)->rewardXP == 500);
    CHECK(QL::Get(63) && QL::Get(63)->level == 42);
    CHECK(QL::LogSize() == 64);
    CHECK(QL::Get(31) == nullptr);
    CHECK(s_changes == 1);
}

COA_TEST(QuestLogExtended_DeltaKeepsOthers) {
    Setup();
    CoATest::Bytes a;
    a.U8(1).U8(1).U8(64).U8(2);
    Add(a, 30, 100);
    Add(a, 31, 101);
    Feed(a);
    CoATest::Bytes d;
    d.U8(1).U8(0).U8(64).U8(1);
    Add(d, 30, 0);
    Feed(d);
    CHECK(QL::Get(30) == nullptr);
    CHECK(QL::Get(31) && QL::Get(31)->questId == 101);
    CHECK(s_changes == 2);
}

COA_TEST(QuestLogExtended_FullClearsFirst) {
    Setup();
    CoATest::Bytes a;
    a.U8(1).U8(1).U8(64).U8(1);
    Add(a, 30, 100);
    Feed(a);
    CoATest::Bytes f;
    f.U8(1).U8(1).U8(64).U8(1);
    Add(f, 31, 101);
    Feed(f);
    CHECK(QL::Get(30) == nullptr);
    CHECK(QL::Get(31) && QL::Get(31)->questId == 101);
}

COA_TEST(QuestLogExtended_RejectsMalformed) {
    Setup();
    CoATest::Bytes a;
    a.U8(1).U8(1).U8(64).U8(1);
    Add(a, 30, 100);
    Feed(a);
    CHECK(s_changes == 1);

    CoATest::Bytes badSlot;
    badSlot.U8(1).U8(1).U8(50).U8(2);
    Add(badSlot, 31, 5);
    Add(badSlot, 70, 6);
    Feed(badSlot);

    CoATest::Bytes lowSlot;
    lowSlot.U8(1).U8(1).U8(50).U8(1);
    Add(lowSlot, 24, 6);
    Feed(lowSlot);

    CoATest::Bytes shortPacket;
    shortPacket.U8(1).U8(1).U8(50).U8(3);
    Add(shortPacket, 31, 5);
    Add(shortPacket, 32, 6);
    Feed(shortPacket);

    CoATest::Bytes badVersion;
    badVersion.U8(2).U8(1).U8(50).U8(1);
    Add(badVersion, 31, 5);
    Feed(badVersion);

    CHECK(QL::Get(30) && QL::Get(30)->questId == 100);
    CHECK(QL::Get(31) == nullptr && QL::Get(32) == nullptr);
    CHECK(QL::LogSize() == 64);
    CHECK(s_changes == 1);
}

COA_TEST(QuestLogExtended_NoPacketMeansNative) {
    Setup();
    CHECK(QL::LogSize() == 25);

    for (uint32_t i = 0; i < 70; ++i) {
        CHECK(QL::Get(i) == nullptr);
    }
}

COA_TEST(QuestLogExtended_ClearedOnGlue) {
    Setup();
    CoATest::Bytes a;
    a.U8(1).U8(1).U8(64).U8(1);
    Add(a, 30, 100);
    Feed(a);
    CoA::Lifecycle::OnGlueScreen(QL::Clear);
    CoA::Lifecycle::RunGlueScreen();
    CHECK(QL::Get(30) == nullptr);
    CHECK(QL::LogSize() == 25);
}

COA_TEST(QuestLogExtended_Hello) {
    Setup();
    CoATest::RecordPackets();
    QL::SendHello();
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().size() == 1);
    CHECK(CoATest::SentPackets().size() == 1 && CoATest::SentPackets()[0] == CoATest::Expected(0x8F6, CoATest::Bytes().U8(1).U8(64)));
}

COA_TEST(QuestLogExtended_ClampsLogSize) {
    Setup();
    CoATest::Bytes lo;
    lo.U8(1).U8(1).U8(0).U8(1);
    Add(lo, 30, 100);
    Feed(lo);
    CHECK(QL::LogSize() == 25);
    CHECK(QL::Get(30) && QL::Get(30)->questId == 100);
    CoATest::Bytes hi;
    hi.U8(1).U8(0).U8(200).U8(1);
    Add(hi, 31, 101);
    Feed(hi);
    CHECK(QL::LogSize() == 64);
    CHECK(QL::Get(31) && QL::Get(31)->questId == 101);
}

COA_TEST(QuestLogExtended_AccessorReadsOverflow) {
    Setup();
    CoATest::Bytes b;
    b.U8(1).U8(1).U8(64).U8(1);
    Add(b, 40, 777);
    Feed(b);
    const PlayerQuestLogEntry* e = QL::Mirror(40);
    CHECK(e && e->m_questId == 777 && e->m_state == 1 && e->m_counts[3] == 4 && e->m_time == 99);
    const PlayerQuestLogEntry* empty = QL::Mirror(30);
    CHECK(empty && empty->m_questId == 0 && empty->m_state == 0 && empty->m_counts[0] == 0 && empty->m_time == 0);
    CHECK(QL::Mirror(64) == nullptr);
    CHECK(QL::Mirror(24) == nullptr);

    // Held pointers stay distinct, and a read after a delta sees the new value.
    CHECK(QL::Mirror(40) != QL::Mirror(30));
    CoATest::Bytes d;
    d.U8(1).U8(0).U8(64).U8(1);
    Add(d, 40, 0);
    Feed(d);
    CHECK(QL::Mirror(40) && QL::Mirror(40)->m_questId == 0);
}

namespace {
    lua_State* s_state = nullptr;

    double Limit(const char* name = "MAX_QUESTLOG_QUESTS") {
        lua_getfield(s_state, LUA_GLOBALSINDEX, name);
        const double v = lua_isnumber(s_state, -1) ? lua_tonumber(s_state, -1) : -1.0;
        lua_pop(s_state, 1);
        return v;
    }
}

// The interface's MAX_QUESTLOG_QUESTS and MAX_WATCHABLE_QUESTS follow the server's log size; 25 after a relog without one.
COA_TEST(QuestLogExtended_InterfaceLimit) {
    Setup();
    CoA::LuaReg::Reset();
    s_state = CoATest::NewState(false);
    CoA::ClientState::SetMainLuaStateProbe([] { return s_state; });
    lua_pushnumber(s_state, 25);
    lua_setfield(s_state, LUA_GLOBALSINDEX, "MAX_QUESTLOG_QUESTS");
    lua_pushnumber(s_state, 25);
    lua_setfield(s_state, LUA_GLOBALSINDEX, "MAX_WATCHABLE_QUESTS");

    CoATest::Bytes b;
    b.U8(1).U8(1).U8(64).U8(1);
    Add(b, 30, 100);
    Feed(b);
    CHECK(Limit() == 64);
    CHECK(Limit("MAX_WATCHABLE_QUESTS") == 64);

    CoA::Lifecycle::RunGlueScreen();
    CoA::ClientHooks::afterEnterWorld.Run();
    CHECK(Limit() == 25);
    CHECK(Limit("MAX_WATCHABLE_QUESTS") == 25);

    // A /reload rebuilds the interface with FrameXML's 25; the limit is set again.
    Feed(b);
    lua_pushnumber(s_state, 25);
    lua_setfield(s_state, LUA_GLOBALSINDEX, "MAX_QUESTLOG_QUESTS");
    lua_pushnumber(s_state, 25);
    lua_setfield(s_state, LUA_GLOBALSINDEX, "MAX_WATCHABLE_QUESTS");
    CoA::ClientHooks::afterUiReload.Run();
    CHECK(Limit() == 64);
    CHECK(Limit("MAX_WATCHABLE_QUESTS") == 64);

    CoA::ClientState::SetMainLuaStateProbe(nullptr);
    lua_close(s_state);
    s_state = nullptr;
    CoA::ClientHooks::afterEnterWorld.Clear();
    CoA::ClientHooks::afterUiReload.Clear();
}

// The store sends no QUEST_LOG_UPDATE of its own: the refresh's CGQuestLog::Update(1) sends the one per packet.
COA_TEST(QuestLogExtended_SignalsUpdate) {
    Setup();
    CoATest::Bytes a;
    a.U8(1).U8(1).U8(64).U8(1);
    Add(a, 30, 100);
    Feed(a);
    CoATest::RecordSignals();
    CoATest::Bytes d;
    d.U8(1).U8(0).U8(64).U8(1);
    Add(d, 31, 101);
    Feed(d);
    CoATest::StopRecordingSignals();
    CHECK(s_changes == 2);
    CHECK(CoATest::Signals().empty());
}

// The refresh reloads the saved watches on the login snapshot only: the first packet after a clear.
COA_TEST(QuestLogExtended_FirstSinceClear) {
    Setup();
    static std::vector<bool> firsts;
    firsts.clear();
    QL::OnChanged([] { firsts.push_back(QL::FirstSinceClear()); });
    CoATest::Bytes a;
    a.U8(1).U8(1).U8(64).U8(1);
    Add(a, 30, 100);
    Feed(a);
    CHECK(!QL::FirstSinceClear());
    CoATest::Bytes d;
    d.U8(1).U8(0).U8(64).U8(1);
    Add(d, 31, 101);
    Feed(d);
    Feed(a);
    QL::Clear();
    Feed(d);
    CHECK(firsts == std::vector<bool>({ true, false, false, true }));

    // A malformed packet after a clear is not applied and leaves the next one first.
    QL::Clear();
    CoATest::Bytes bad;
    bad.U8(2).U8(1).U8(64).U8(1);
    Add(bad, 30, 100);
    Feed(bad);
    Feed(a);
    CHECK(firsts.size() == 5 && firsts[4]);
}

// OnUpdateQuest's per-slot decisions. A quest still in another slot has moved (promotion from slot 40 into native
// slot 3, in either packet order): no "Quest accepted", no POI removal.
COA_TEST(QuestLogExtended_DecideSlot) {
    uint32_t ids[QL::kMaxSlots] = {};
    ids[3] = 500;
    ids[40] = 500;
    // The descriptor gives slot 3 quest 500 while slot 40 still holds it.
    QL::SlotDecision d = QL::DecideSlot(ids, 3, 0, 0, 0);
    CHECK(!d.accepted && !d.removePoi);
    // Then 0x8F7 empties slot 40.
    ids[40] = 0;
    d = QL::DecideSlot(ids, 40, 0, 500, 0);
    CHECK(!d.accepted && !d.removePoi);

    // A new quest, and a quest that leaves the log.
    uint32_t log[QL::kMaxSlots] = {};
    log[30] = 600;
    d = QL::DecideSlot(log, 30, 0, 0, 0);
    CHECK(d.accepted && !d.removePoi);
    d = QL::DecideSlot(log, 30, 0, 599, 0);
    CHECK(d.accepted && d.removePoi);
    log[30] = 0;
    d = QL::DecideSlot(log, 30, 0, 600, 0);
    CHECK(!d.accepted && d.removePoi);

    // The same quest: a failure lifted is accepted again; a counter or state change is not.
    log[30] = 600;
    d = QL::DecideSlot(log, 30, 0, 600, 2);
    CHECK(d.accepted && !d.removePoi);
    d = QL::DecideSlot(log, 30, 1, 600, 0);
    CHECK(!d.accepted && !d.removePoi);
    d = QL::DecideSlot(log, 30, 2, 600, 2);
    CHECK(!d.accepted);

    // A timed quest 800 pinned to native slot 3: its quest 500 moves to slot 30 (0x8F7 first), then the descriptor
    // gives slot 3 the timed quest. 500 is never accepted or dropped; 800 is accepted.
    uint32_t pin[QL::kMaxSlots] = {};
    pin[3] = 500;
    pin[30] = 500;
    d = QL::DecideSlot(pin, 30, 0, 0, 0);
    CHECK(!d.accepted && !d.removePoi);
    pin[3] = 800;
    d = QL::DecideSlot(pin, 3, 0, 500, 0);
    CHECK(d.accepted && !d.removePoi);
}

// The refresh gets each changed slot with its entry before the packet; the first packet after a clear fills the log
// as a login does, so it reports none.
COA_TEST(QuestLogExtended_ChangedSlots) {
    Setup();
    static std::vector<QL::Change> seen;
    seen.clear();
    QL::OnChanged([] { seen = QL::Changes(); });
    CoATest::Bytes a;
    a.U8(1).U8(1).U8(64).U8(2);
    Add(a, 30, 100);
    Add(a, 31, 101);
    Feed(a);
    CHECK(seen.empty());

    // A full resend with slot 30 unchanged, 31 emptied and 32 new.
    CoATest::Bytes f;
    f.U8(1).U8(1).U8(64).U8(2);
    Add(f, 30, 100);
    Add(f, 32, 102);
    Feed(f);
    CHECK(seen.size() == 2);
    CHECK(seen.size() == 2 && seen[0].slot == 31 && seen[0].old.questId == 101);
    CHECK(seen.size() == 2 && seen[1].slot == 32 && seen[1].old.questId == 0);
}
