// AscSuperTrack.cpp (see SuperTrack.h).
#include "CoA/Source/Modules/SuperTrack.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "CoA/Source/Runtime/Timer.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
    struct Target {
        uint32_t id = 0;
        int32_t map = -1;
        float x = 0;
        float y = 0;
        float z = 0;
        float radius = 5.5f;
        uint32_t next = 0;
        uint32_t unk1c = 0;
    };

    struct Track {
        uint32_t quest = 0;
        Target target;
        bool fresh = false;   // +0x24 (0x10BCC664)
    };

    Track s_track;
    uint32_t s_lastQuest = 0;    // 0x10D3C1A8
    CVar* s_visitedCVar = nullptr;   // 0x10D3C1A4 "VisitedSuperTracks"
    uint32_t s_timers[3] = {};   // 0x10D3C18C / 190 / 194
    CoA::SuperTrack::Deps s_deps;
    const uint32_t kMaxListValue = 0xC4604;

    int32_t CurrentMap() {
        return s_deps.currentMap ? s_deps.currentMap() : -1;
    }

    bool PlayerPosition(float out[3]) {
        return s_deps.playerPosition && s_deps.playerPosition(out);
    }

    bool Tracking() {   // FUN_10217c80
        return s_track.target.x != 0.0f || s_track.target.y != 0.0f || s_track.target.z != 0.0f;
    }

    float TargetZ() {   // z, or the player's z + 2 when unset
        float z = s_track.target.z;
        float p[3];

        if (z == 0.0f && PlayerPosition(p)) {
            z = p[2] + 2.0f;
        }

        return z;
    }

    // FUN_10217850: 3-D distance from the player, 0 on another map.
    float Distance() {
        if (s_track.target.map != CurrentMap()) {
            return 0.0f;
        }

        float p[3] = { 0, 0, 0 };
        PlayerPosition(p);
        const float z = TargetZ();
        const float dz = p[2] - z;
        const float dx = p[0] - s_track.target.x;
        const float dy = p[1] - s_track.target.y;
        return std::sqrt(dz * dz + dx * dx + dy * dy);
    }

    // FUN_10217ac0: 0 off-map, 4 inside the radius, else 2.
    int32_t TargetState() {
        if (s_track.target.map != CurrentMap()) {
            return 0;
        }

        return Distance() <= s_track.target.radius ? 4 : 2;
    }

    void Changed() {
        CoA::Events::Signal("SUPER_TRACKING_CHANGED", nullptr);
    }

    CoA::Dbc::Table& SuperTrackDbc() {   // 0x10BDF618
        return CoA::Dbc::Get("DBFilesClient/SuperTrack.dbc");
    }

    CoA::Dbc::Table& QuestSuperTrackDbc() {   // 0x10BE01A0
        return CoA::Dbc::Get("DBFilesClient/QuestSuperTrack.dbc");
    }

    Target FromRow(const uint8_t* row) {   // a SuperTrack.dbc row IS a target (0x20 bytes)
        Target t;
        memcpy(&t, row, sizeof(Target));
        return t;
    }

    uint32_t ObjectiveComplete(uint32_t quest, uint32_t objective) {   // FUN_10194090
        return s_deps.objectiveComplete ? s_deps.objectiveComplete(quest, objective) : 0;
    }

    float DistanceTo(const float p[3], const uint8_t* row) {
        const float dx = p[0] - CoA::Dbc::Table::F32(row, 8);
        const float dy = p[1] - CoA::Dbc::Table::F32(row, 0xC);
        const float dz = p[2] - CoA::Dbc::Table::F32(row, 0x10);
        return std::sqrt(dz * dz + dx * dx + dy * dy);
    }

    // FUN_103345f0: the SuperTrack id to follow for QuestSuperTrack row `qrow`. Objective 0..3 = every objective whose
    // query comes back 0 contributes its 20-id list (+8 + k*0x50); -1 = the completion list (+0x148). Each listed id is
    // walked past already-visited chain nodes, then along the chain (only while both nodes carry +0x1C) to the node nearest
    // the player; nodes skipped on the way are marked visited. The first candidate is taken; later ones only on the current
    // map and nearer (compared by the distance of the node the walk started from).
    uint32_t BestSuperTrack(const uint8_t* qrow, int32_t objective) {
        CoA::Dbc::Table& st = SuperTrackDbc();
        float p[3] = { 0, 0, 0 };
        PlayerPosition(p);
        const uint8_t* lists[4] = {};
        const uint32_t quest = CoA::Dbc::Table::U32(qrow, 4);

        if (objective >= 0 && objective <= 3) {
            for (uint32_t k = 0; k < 4; ++k) {
                if (ObjectiveComplete(quest, k) == 0) {
                    lists[k] = qrow + 8 + k * 0x50;
                }
            }
        } else if (objective == -1) {
            lists[0] = qrow + 0x148;
        }

        uint32_t bestId = 0;
        float bestDistance = 3.4028235e38f;

        for (const uint8_t* list : lists) {
            if (!list) {
                continue;
            }

            for (uint32_t i = 0; i < 20; ++i) {
                const uint32_t id = CoA::Dbc::Table::U32(list, i * 4);
                const uint8_t* cur = id ? st.Row(id) : nullptr;

                if (!cur) {
                    continue;
                }

                for (;;) {
                    const uint32_t nextId = CoA::Dbc::Table::U32(cur, 0x18);
                    const uint8_t* next = nextId ? st.Row(nextId) : nullptr;

                    if (!next || !CoA::SuperTrack::Visited(CoA::Dbc::Table::U32(cur, 0))) {
                        break;
                    }

                    cur = next;
                }

                const float start = DistanceTo(p, cur);
                const uint8_t* best = cur;
                float nearest = start;
                std::vector<uint32_t> walked(1, CoA::Dbc::Table::U32(cur, 0));
                std::vector<uint32_t> skipped;

                if (CoA::Dbc::Table::U32(cur, 0x18)) {
                    const uint8_t* n = st.Row(CoA::Dbc::Table::U32(cur, 0x18));

                    while (n) {
                        if (!CoA::Dbc::Table::U32(n, 0x18) || !CoA::Dbc::Table::U32(best, 0x1C) || !CoA::Dbc::Table::U32(n, 0x1C)) {
                            break;
                        }

                        const float d = DistanceTo(p, n);

                        if (d < nearest) {
                            nearest = d;
                            best = n;
                            skipped = walked;
                        }

                        n = st.Row(CoA::Dbc::Table::U32(n, 0x18));

                        if (n) {
                            walked.push_back(CoA::Dbc::Table::U32(n, 0));
                        }

                        if (!CoA::Dbc::Table::U32(best, 0x18)) {
                            break;
                        }
                    }
                }

                for (uint32_t s : skipped) {
                    CoA::SuperTrack::MarkVisited(s);
                }

                if (bestId == 0 || (CoA::Dbc::Table::I32(best, 4) == CurrentMap() && start < bestDistance)) {
                    bestDistance = start;
                    bestId = CoA::Dbc::Table::U32(best, 0);
                }
            }
        }

        return bestId;
    }

    // FUN_10336440: the tracked quest's target -- its QuestSuperTrack row's objective / completion SuperTrack node, else
    // the quest POI (x, y as ints, z 0, radius 5.5; +0x1C is left as it was).
    bool ResolveQuestTarget() {
        if (!s_track.quest || !s_deps.questPoi) {
            return false;
        }

        bool complete = false;
        int32_t map = 99999;
        int32_t x = 0;
        int32_t y = 0;

        if (!s_deps.questPoi(s_track.quest, complete, map, x, y)) {
            return false;
        }

        CoA::Dbc::Table& qst = QuestSuperTrackDbc();

        for (uint32_t id = qst.MinId(); qst.Count() && id <= qst.MaxId(); ++id) {
            const uint8_t* qrow = qst.Row(id);

            if (!qrow || CoA::Dbc::Table::U32(qrow, 4) != s_track.quest) {
                continue;
            }

            const uint8_t* row = nullptr;

            if (!complete) {
                for (uint32_t k = 0; k < 4 && !row; ++k) {
                    if (ObjectiveComplete(s_track.quest, k) == 0) {
                        const uint32_t st = BestSuperTrack(qrow, static_cast<int32_t>(k));
                        row = st ? SuperTrackDbc().Row(st) : nullptr;
                    }
                }
            } else {
                row = SuperTrackDbc().Row(BestSuperTrack(qrow, -1));   // FUN_10334e90
            }

            if (row) {
                s_track.target = FromRow(row);
                return true;
            }

            break;
        }

        s_track.target.id = 0;
        s_track.target.map = map;
        s_track.target.x = static_cast<float>(x);
        s_track.target.y = static_cast<float>(y);
        s_track.target.z = 0;
        s_track.target.radius = 5.5f;
        s_track.target.next = 0;
        return true;
    }

    // ---- timers ----
    int32_t ResolveTick(const void*, void*) {   // 0x10336410
        ResolveQuestTarget();
        s_timers[0] = CoA::Timer::Schedule(1000, ResolveTick, nullptr);
        return 0;
    }

    int32_t ArrivalTick(const void*, void*) {   // 0x103362F0
        if (TargetState() == 4 && !CoA::SuperTrack::Visited(s_track.target.id)) {
            CoA::SuperTrack::MarkVisited(s_track.target.id);
        }

        s_timers[1] = CoA::Timer::Schedule(100, ArrivalTick, nullptr);
        return 0;
    }

    int32_t QuestChangeTick(const void*, void*) {   // 0x10336340: the list stored empty
        if (!s_track.fresh && s_lastQuest && s_lastQuest != s_track.quest && s_visitedCVar && s_deps.storeVisited) {
            s_deps.storeVisited({});
        }

        s_lastQuest = s_track.quest;
        s_track.fresh = false;
        s_timers[2] = CoA::Timer::Schedule(1000, QuestChangeTick, nullptr);
        return 0;
    }

    void StopTimers() {   // 0x103350C0 (glue Lua init)
        const CoA::Timer::Fn fns[3] = { ResolveTick, ArrivalTick, QuestChangeTick };

        for (int32_t i = 0; i < 3; ++i) {
            if (s_timers[i]) {
                CoA::Timer::Cancel(s_timers[i], fns[i], nullptr);
                s_timers[i] = 0;
            }
        }
    }

    void OnWorldLuaInit() {   // 0x103345A0: tracker cleared, fresh, no last quest
        s_track.quest = 0;
        s_track.target = Target{};
        s_track.fresh = true;
        s_lastQuest = 0;
    }

    // SMSG 0x740 SUPER_TRACKER_SET_POSITION (0x10334EC0): i32 map, f32 x, y, z; quest 0, id 0, radius 5.5, next 0 (+0x1C
    // untouched).
    int32_t OnSetPosition(void*, uint32_t, uint32_t, CDataStore* msg) {
        s_track.target.map = CoA::Read::I32(msg);
        s_track.target.x = CoA::Read::F32(msg);
        s_track.target.y = CoA::Read::F32(msg);
        s_track.target.z = CoA::Read::F32(msg);
        s_track.quest = 0;
        s_track.target.id = 0;
        s_track.target.radius = 5.5f;
        s_track.target.next = 0;
        Changed();
        return 1;
    }

    // FUN_10336140: (questID) -- tracker cleared to the quest, its target resolved, event fired.
    int SetSuperTrackedQuestID(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER })) {
            return 0;
        }

        const uint32_t quest = static_cast<uint32_t>(static_cast<int64_t>(CoA::Script::CheckNumber(L, 1)));
        const uint32_t keep = s_track.target.unk1c;   // +0x1C is not written
        s_track.quest = quest;
        s_track.target = Target{};
        s_track.target.unk1c = keep;
        ResolveQuestTarget();
        Changed();
        return 0;
    }

    // FUN_10335400: PositionFrame(bool) -- SuperTrackClient.cpp (a client flag and SetPoint's offset scaling).
    int PositionFrame(lua_State* L) {
        bool on;

        if (!CoA::Script::ReadBool(L, on)) {
            return 0;
        }

        if (s_deps.positionFrame) {
            s_deps.positionFrame(on);
        }

        return 0;
    }

    int ClearSuperTracker(lua_State*) {
        s_track.quest = 0;
        s_track.target = Target{};
        return 0;
    }

    int IsSuperTrackingAnything(lua_State* L) {   // FUN_10217c70
        CoA::Script::PushBool(L, s_track.target.map != -1);
        return 1;
    }

    int GetTargetState(lua_State* L) {
        CoA::Script::PushInt(L, TargetState());
        return 1;
    }

    int GetSuperTrackedWorldPosition(lua_State* L) {
        if (!Tracking()) {
            lua_pushnil(L);
            return 1;
        }

        float x = 0;
        float y = 0;
        float z = 0;
        float d = 0;

        if (s_track.target.map == CurrentMap()) {
            x = s_track.target.x;
            y = s_track.target.y;
            z = TargetZ();
            d = Distance();
        }

        CoA::Script::PushNum(L, x);
        CoA::Script::PushNum(L, y);
        CoA::Script::PushNum(L, z);
        CoA::Script::PushNum(L, d);
        return 4;
    }

    // FUN_102179e0: world -> screen through the world frame, then the client's pixel rounding.
    int GetSuperTrackedPosition(lua_State* L) {
        if (!Tracking()) {
            lua_pushnil(L);
            return 1;
        }

        float sx = 0;
        float sy = 0;
        float d = 0;

        if (s_track.target.map == CurrentMap()) {
            const float world[3] = { s_track.target.x, s_track.target.y, TargetZ() };

            if (s_deps.worldToScreen) {
                s_deps.worldToScreen(world, sx, sy);
            }

            d = Distance();
        }

        CoA::Script::PushNum(L, sx);
        CoA::Script::PushNum(L, sy);
        CoA::Script::PushNum(L, d);
        return 3;
    }

    // FUN_10335fb0: (x, y, z, map); radius 5.5.
    int SetSuperTrackedPosition(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TNUMBER, LUA_TNUMBER, LUA_TNUMBER })) {
            return 0;
        }

        Target t;
        t.x = static_cast<float>(CoA::Script::CheckNumber(L, 1));
        t.y = static_cast<float>(CoA::Script::CheckNumber(L, 2));
        t.z = static_cast<float>(CoA::Script::CheckNumber(L, 3));
        t.map = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 4));
        s_track.quest = 0;
        s_track.target = t;
        Changed();
        return 0;
    }

    // FUN_10335eb0: the current map and the corpse position.
    int SetSuperTrackedCorpse(lua_State* L) {
        bool unused;

        if (!CoA::Script::ReadBool(L, unused)) {
            return 0;
        }

        float pos[3] = { 0, 0, 0 };

        if (s_deps.corpsePosition) {
            s_deps.corpsePosition(pos);
        }

        Target t;
        t.map = CurrentMap();
        t.x = pos[0];
        t.y = pos[1];
        t.z = pos[2];
        s_track.quest = 0;
        s_track.target = t;
        Changed();
        return 0;
    }

    bool AcceptAnyValue(CVar*, const char*, const char*, void*) {   // 0x102A0990
        return true;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_SuperTrack", "ClearSuperTracker", ClearSuperTracker),
        COA_NATIVE("C_SuperTrack", "IsSuperTrackingAnything", IsSuperTrackingAnything),
        COA_NATIVE("C_SuperTrack", "GetTargetState", GetTargetState),
        COA_NATIVE("C_SuperTrack", "GetSuperTrackedWorldPosition", GetSuperTrackedWorldPosition),
        COA_NATIVE("C_SuperTrack", "GetSuperTrackedPosition", GetSuperTrackedPosition),
        COA_NATIVE("C_SuperTrack", "SetSuperTrackedPosition", SetSuperTrackedPosition),
        COA_NATIVE("C_SuperTrack", "SetSuperTrackedCorpse", SetSuperTrackedCorpse),
        COA_NATIVE("C_SuperTrack", "SetSuperTrackedQuestID", SetSuperTrackedQuestID),
        COA_NATIVE("C_SuperTrack", "PositionFrame", PositionFrame),
    };
}

CoA::SuperTrack::Deps& CoA::SuperTrack::Dependencies() {
    return s_deps;
}

CVar* CoA::SuperTrack::VisitedCVar() {
    return s_visitedCVar;
}

bool CoA::SuperTrack::Visited(uint32_t id) {
    if (!s_deps.visited) {
        return false;
    }

    for (uint32_t v : s_deps.visited()) {
        if (v == id) {
            return true;
        }
    }

    return false;
}

void CoA::SuperTrack::MarkVisited(uint32_t id) {
    if (id >= kMaxListValue) {
        CoA::Log::Printf("AddSuperTrackToVisitedList.id >=  MAX_CVAR_INT_LIST_VALUE");
        return;
    }

    if (!s_deps.visited || !s_deps.storeVisited || Visited(id)) {
        return;
    }

    std::vector<uint32_t> ids = s_deps.visited();
    ids.push_back(id);

    if (ids.size() > 20) {
        ids.erase(ids.begin(), ids.end() - 20);
    }

    s_deps.storeVisited(ids);
}

void CoA::SuperTrack::Start() {
    s_timers[0] = CoA::Timer::Schedule(1000, ResolveTick, nullptr);
    s_timers[1] = CoA::Timer::Schedule(100, ArrivalTick, nullptr);
    s_timers[2] = CoA::Timer::Schedule(1000, QuestChangeTick, nullptr);
}

void CoA::SuperTrack::Init() {
    // FUN_10114540 with &0x10D3C1A4 as the output slot: no help, flags 0x21, default "", category 4.
    COA_CVAR("VisitedSuperTracks", "", 0x21, "", AcceptAnyValue, 4, false, &s_visitedCVar, true);
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_PACKET(0x740, OnSetPosition);
    CoA::LuaReg::OnWorldRegistered(OnWorldLuaInit);
    CoA::LuaReg::OnGlueRegistered(StopTimers);
    COA_EVENT("SUPER_TRACKING_CHANGED");
}

void CoA::SuperTrack::Reset() {
    s_track = Track();
    s_lastQuest = 0;

    for (uint32_t& t : s_timers) {
        t = 0;
    }
}
