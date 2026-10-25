#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/SuperTrack.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;
    std::vector<uint32_t> s_visited;
    int s_stores = 0;
    float s_player[3] = { 0, 0, 0 };
    int32_t s_map = 0;
    int s_rawOffsets = -1;

    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::SuperTrack::Reset();
        CoA::SuperTrack::Init();
        s_visited.clear();
        s_stores = 0;
        s_player[0] = s_player[1] = s_player[2] = 0;
        s_map = 0;
        s_rawOffsets = -1;
        CoA::SuperTrack::Deps& deps = CoA::SuperTrack::Dependencies();
        deps.playerPosition = [](float out[3]) {
            out[0] = s_player[0];
            out[1] = s_player[1];
            out[2] = s_player[2];
            return true;
        };
        deps.currentMap = [] { return s_map; };
        deps.visited = [] { return s_visited; };
        deps.storeVisited = [](const std::vector<uint32_t>& ids) {
            s_visited = ids;
            ++s_stores;
        };
        deps.positionFrame = [](bool on) { s_rawOffsets = on ? 1 : 0; };
        return CoATest::NewState(false);
    }

    V Signals(void (*fn)(lua_State*), lua_State* L) {
        CoATest::RecordSignals();
        fn(L);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }
}

// Setters fire SUPER_TRACKING_CHANGED; Clear resets to map -1 (nothing tracked); the world position is the target and its
// 3-D distance, z = the player's z + 2 when the target's is 0.
COA_TEST(SuperTrack_SetAndClear) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "C_SuperTrack", "IsSuperTrackingAnything")[0] == "false");
    CHECK(CoATest::Call(L, "C_SuperTrack", "GetSuperTrackedWorldPosition")[0] == "nil");
    CoATest::RecordSignals();
    CoATest::Call(L, "C_SuperTrack", "SetSuperTrackedPosition", { "n:3", "n:4", "n:0", "n:0" });
    CoATest::StopRecordingSignals();
    CHECK(CoATest::Signals() == V{ "SUPER_TRACKING_CHANGED" });
    CHECK(CoATest::Call(L, "C_SuperTrack", "IsSuperTrackingAnything")[0] == "true");
    CHECK((CoATest::Call(L, "C_SuperTrack", "GetSuperTrackedWorldPosition") == V{ "3", "4", "2", "5.385164738" }));
    // Default radius 5.5: the player at the target is inside it (state 4); another map gives 0.
    s_player[0] = 3;
    s_player[1] = 4;
    CHECK(CoATest::Call(L, "C_SuperTrack", "GetTargetState")[0] == "4");
    s_map = 1;
    CHECK(CoATest::Call(L, "C_SuperTrack", "GetTargetState")[0] == "0");
    CoATest::Call(L, "C_SuperTrack", "ClearSuperTracker");
    CHECK(CoATest::Call(L, "C_SuperTrack", "IsSuperTrackingAnything")[0] == "false");
    CoATest::Call(L, "C_SuperTrack", "PositionFrame", { "b:1" });
    CHECK(s_rawOffsets == 1);
}

// SMSG 0x740: i32 map, f32 x, y, z; quest 0, radius 5.5; the event.
COA_TEST(SuperTrack_Packet740) {
    lua_State* L = Setup();
    CoATest::Bytes b;
    b.U32(0).F32(10.0f).F32(0.0f).F32(5.0f);
    CDataStore msg(b.data.data(), static_cast<uint32_t>(b.data.size()));
    int32_t result = 0;
    CoATest::RecordSignals();
    CoA::Packets::Dispatch(0x740, 0, &msg, result);
    CoATest::StopRecordingSignals();
    CHECK(CoATest::Signals() == V{ "SUPER_TRACKING_CHANGED" });
    CHECK((CoATest::Call(L, "C_SuperTrack", "GetSuperTrackedWorldPosition") == V{ "10", "0", "5", "11.18033981" }));
    CHECK(CoATest::Call(L, "C_SuperTrack", "GetTargetState")[0] == "2");
}

// The visited list keeps the last 20 ids, skips ids already listed and refuses ids >= 0xC4604.
COA_TEST(SuperTrack_VisitedListCap) {
    Setup();

    for (uint32_t id = 1; id <= 25; ++id) {
        CoA::SuperTrack::MarkVisited(id);
    }

    CHECK(s_visited.size() == 20 && s_visited.front() == 6 && s_visited.back() == 25);
    const int stores = s_stores;
    CoA::SuperTrack::MarkVisited(25);
    CoA::SuperTrack::MarkVisited(0xC4604);
    CHECK(s_stores == stores);
    CHECK(CoA::SuperTrack::Visited(7) && !CoA::SuperTrack::Visited(5));
}

// The world Lua init clears the tracker (and marks it fresh); the first quest-change tick then does not clear the list.
COA_TEST(SuperTrack_GlueResets) {
    lua_State* L = Setup();
    CoATest::Call(L, "C_SuperTrack", "SetSuperTrackedPosition", { "n:3", "n:4", "n:0", "n:0" });
    CoA::LuaReg::RunWorldRegistered();
    CHECK(CoATest::Call(L, "C_SuperTrack", "IsSuperTrackingAnything")[0] == "false");
}
