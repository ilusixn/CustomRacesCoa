// C_Callboard.GetView / C_CallboardCache (Callboard.cpp, CallboardCache.cpp): SMSG 0x589 / 0x58A / 0x730, the view
// tables, the view surviving glue screens, ForEachCompletion wired into C_TemporalContracts.ActivateAll*, the cache
// level resolution (current / upcoming / next and their events), the points natives and short packets.
#include "CoA/Source/Modules/Callboard.h"
#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/TemporalContracts.h"
#include "CoA/Source/Modules/Token.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;

    uint32_t s_level = 0;

    lua_State* Start() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::ClientHooks::enterWorld.Clear();
        CoA::GameEvents::Reset();
        CoA::Token::Reset();
        CoA::Token::Init();
        CoA::TemporalContracts::Reset();
        CoA::Callboard::Reset();
        CoA::Callboard::Init();
        CoA::CallboardCache::Reset();
        CoA::CallboardCache::Init();
        s_level = 0;
        return CoATest::NewState(false);
    }

    void Stop(lua_State* L) {
        lua_close(L);
        CoATest::StopRecordingSignals();
        CoA::Callboard::Reset();
        CoA::CallboardCache::Reset();
        CoA::TemporalContracts::Reset();
        CoA::Token::Reset();
        CoA::GameEvents::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::ClientHooks::enterWorld.Clear();
    }

    V Dispatch(uint32_t opcode, CoATest::Bytes& b) {
        CoATest::RecordSignals();
        CDataStore msg(b.data.data(), static_cast<uint32_t>(b.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }

    V Run(lua_State* L, const std::string& chunk) {
        V out;
        const int base = lua_gettop(L);

        if (luaL_loadstring(L, chunk.c_str()) != 0 || lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
            out.push_back(std::string("<error> ") + lua_tostring(L, -1));
            lua_settop(L, base);
            return out;
        }

        for (int i = base + 1; i <= lua_gettop(L); ++i) {
            out.push_back(lua_isnil(L, i) ? "nil" : lua_isboolean(L, i) ? (lua_toboolean(L, i) ? "true" : "false")
                : lua_istable(L, i) ? "table" : lua_tostring(L, i));
        }

        lua_settop(L, base);
        return out;
    }

    const std::string kDump =
        "local function dump(t) local keys = {} for k in pairs(t) do local i = #keys + 1 keys[i] = k "
        "while i > 1 and tostring(keys[i - 1]) > tostring(keys[i]) do keys[i - 1], keys[i] = keys[i], keys[i - 1] i = i - 1 end end "
        "local out = '' for i, k in ipairs(keys) do local v = t[k] if i > 1 then out = out .. ' ' end "
        "out = out .. tostring(k) .. '=' .. (type(v) == 'table' and ('{' .. dump(v) .. '}') or tostring(v)) end "
        "return out end ";

    struct Reward {
        uint32_t money, honor, arena, points;
        std::vector<std::pair<uint32_t, uint32_t>> items;
    };

    struct Sub {
        const char* type;
        const char* name;
        uint32_t remaining, money, token, level;
        Reward reward;
    };

    struct Cat {
        const char* content;
        const char* name;
        uint32_t remaining, money, token;
        Reward reward;
        std::vector<Sub> subs;
    };

    void PutReward(CoATest::Bytes& b, const Reward& r) {
        b.U32(r.money).U32(r.honor).U32(r.arena).U32(r.points).U32(static_cast<uint32_t>(r.items.size()));

        for (const auto& i : r.items) {
            b.U32(i.first).U32(i.second);
        }
    }

    V View(const std::vector<Cat>& cats) {
        CoATest::Bytes b;
        b.U32(static_cast<uint32_t>(cats.size()));

        for (const Cat& c : cats) {
            b.Str(c.content).Str(c.name).U32(c.remaining).U32(c.money).U32(c.token);
            PutReward(b, c.reward);
            b.U32(static_cast<uint32_t>(c.subs.size()));

            for (const Sub& s : c.subs) {
                b.Str(s.type).Str(s.name).U32(s.remaining).U32(s.money).U32(s.token).U32(s.level);
                PutReward(b, s.reward);
            }
        }

        return Dispatch(0x589, b);
    }

    void Collect(const std::string& type, void* ctx) {
        std::string& s = *static_cast<std::string*>(ctx);
        s += (s.empty() ? "" : ",") + type;
    }

    std::string Completions(const std::string* content, bool* ran = nullptr) {
        std::string s;
        const bool r = CoA::Callboard::ForEachCompletion(content, Collect, &s);

        if (ran) {
            *ran = r;
        }

        return s;
    }

    struct Level {
        uint32_t event, threshold, info, points;
    };

    void Levels(const std::vector<Level>& levels) {
        CoATest::Bytes b;
        b.U32(static_cast<uint32_t>(levels.size()));

        for (const Level& l : levels) {
            b.U32(0xAAAA).U32(0xBBBB).U32(l.event).U32(l.threshold).U32(l.info).U32(l.points);
        }

        Dispatch(0x730, b);
    }

    void Event(uint16_t id, bool active) {
        CoATest::Bytes b;
        uint8_t raw[0x34] = {};
        memcpy(raw, &id, 2);
        raw[0x13] = active ? 1 : 0;
        b.Raw(raw, sizeof(raw));
        CoATest::Deliver(CoA::GameEvents::OnGameEventInfo, 0x9BD, b);
    }

    V Info(lua_State* L) {
        V r = Run(L, "return C_CallboardCache.GetCallboardCacheInfo()");
        V p = Run(L, "return C_CallboardCache.GetCurrentPoints()");
        V l = Run(L, "return C_CallboardCache.GetItemLevelInfo()");
        r.insert(r.end(), p.begin(), p.end());
        r.insert(r.end(), l.begin(), l.end());
        return r;
    }
}

// GetView: nil until 0x589, then {Categories = {...}} with every field; 0x589 replaces it and fires
// CALLBOARD_VIEW_UPDATED; 0x58A fires CALLBOARD_VIEW_REQUEST_RESULT; glue screens and world entries keep the view.
COA_TEST(Callboard_ViewAndEvents) {
    lua_State* L = Start();
    CHECK(Run(L, "return C_Callboard.GetView()") == V{ "nil" });
    CHECK(View({ { "Dungeon", "Dungeons", 3, 10, 20, { 1, 2, 3, 4, { { 50, 2 }, { 51, 1 } } },
        { { "Heroic", "Heroics", 2, 5, 6, 70, { 7, 8, 9, 10, {} } }, { "Normal", "Normals", 0, 0, 0, 60, { 0, 0, 0, 0, { { 52, 3 } } } } } },
        { "Raid", "Raids", 1, 0, 0, { 0, 0, 0, 0, {} }, {} } }) == V{ "CALLBOARD_VIEW_UPDATED" });
    CHECK(Run(L, kDump + "return dump(C_Callboard.GetView())") == V{
        "Categories={1={ContentType=Dungeon MoneyCost=10 Name=Dungeons RemainingCompletions=3 "
        "Rewards={CallboardCachePoints=4 Items={1={Amount=2 ItemID=50} 2={Amount=1 ItemID=51}} RewardArenaPoints=3 RewardHonor=2 RewardMoney=1} "
        "SubCategories={1={MinimumLevel=70 MoneyCost=5 Name=Heroics RemainingCompletions=2 "
        "Rewards={CallboardCachePoints=10 Items={} RewardArenaPoints=9 RewardHonor=8 RewardMoney=7} TokenCost=6 Type=Heroic} "
        "2={MinimumLevel=60 MoneyCost=0 Name=Normals RemainingCompletions=0 "
        "Rewards={CallboardCachePoints=0 Items={1={Amount=3 ItemID=52}} RewardArenaPoints=0 RewardHonor=0 RewardMoney=0} TokenCost=0 Type=Normal}} "
        "TokenCost=20} "
        "2={ContentType=Raid MoneyCost=0 Name=Raids RemainingCompletions=1 "
        "Rewards={CallboardCachePoints=0 Items={} RewardArenaPoints=0 RewardHonor=0 RewardMoney=0} SubCategories={} TokenCost=0}}" });

    CoA::Lifecycle::RunGlueScreen();
    CoA::ClientHooks::enterWorld.Run();
    CHECK(Run(L, "return #C_Callboard.GetView().Categories") == V{ "2" });
    CHECK(View({}) == V{ "CALLBOARD_VIEW_UPDATED" });
    CHECK(Run(L, kDump + "return dump(C_Callboard.GetView())") == V{ "Categories={}" });

    CoATest::Bytes r;
    r.Str("CALLBOARD_VIEW_REQUEST_RESULT_OK");
    CHECK(Dispatch(0x58A, r) == V{ "CALLBOARD_VIEW_REQUEST_RESULT:CALLBOARD_VIEW_REQUEST_RESULT_OK" });
    CoATest::Bytes e;
    CHECK(Dispatch(0x58A, e) == V{ "CALLBOARD_VIEW_REQUEST_RESULT:" });
    Stop(L);
}

// ForEachCompletion: false before the view; every sub-category's Type once per remaining completion, the categories'
// own counts ignored; a content type matches ContentType or Name; installed as TemporalContracts' Dep.
COA_TEST(Callboard_ForEachCompletion) {
    lua_State* L = Start();
    CHECK(CoA::TemporalContracts::Dependencies().forEachCompletion == CoA::Callboard::ForEachCompletion);
    bool ran = true;
    CHECK(Completions(nullptr, &ran) == "");
    CHECK(!ran);
    View({ { "Dungeon", "Dungeons", 9, 0, 0, {}, { { "A", "a", 2, 0, 0, 0, {} }, { "B", "b", 0, 0, 0, 0, {} }, { "C", "c", 1, 0, 0, 0, {} } } },
        { "Raid", "Raids", 0, 0, 0, {}, { { "R", "r", 3, 0, 0, 0, {} } } }, { "Pvp", "Arena", 5, 0, 0, {}, {} } });
    CHECK(Completions(nullptr, &ran) == "A,A,C,R,R,R");
    CHECK(ran);
    const std::string dungeon = "Dungeon";
    const std::string raids = "Raids";
    const std::string arena = "Arena";
    const std::string none = "Nothing";
    CHECK(Completions(&dungeon) == "A,A,C");
    CHECK(Completions(&raids) == "R,R,R");
    CHECK(Completions(&arena, &ran) == "");
    CHECK(!ran);
    CHECK(Completions(&none, &ran) == "");
    CHECK(!ran);

    // Through C_TemporalContracts: the queue takes them (true), and is then busy.
    CoA::TemporalContracts::Init();
    lua_close(L);
    L = CoATest::NewState(false);
    CHECK(Run(L, "return C_TemporalContracts.ActivateAllTemporalContractsByContentType('Arena', true)") == V{ "false" });
    CHECK(Run(L, "return C_TemporalContracts.ActivateAllTemporalContractsByContentType('Raid', true)") == V{ "true" });
    CHECK(Run(L, "return C_TemporalContracts.ActivateAllTemporalContracts(true)") == V{ "false" });   // queue pending
    Stop(L);
}

// Short packets: the category, sub-category and reward-item counts stop at the packet's end.
COA_TEST(Callboard_ShortPackets) {
    lua_State* L = Start();
    CoATest::Bytes b;
    b.U32(500).Str("Dungeon").Str("D").U32(1).U32(0).U32(0).U32(0).U32(0).U32(0).U32(0).U32(900).U32(1).U32(2);
    CHECK(Dispatch(0x589, b) == V{ "CALLBOARD_VIEW_UPDATED" });
    CHECK(Run(L, kDump + "return dump(C_Callboard.GetView().Categories[1].Rewards.Items)") == V{ "1={Amount=2 ItemID=1}" });
    CHECK(Run(L, "return #C_Callboard.GetView().Categories, #C_Callboard.GetView().Categories[1].SubCategories") == (V{ "1", "0" }));
    CoATest::Bytes e;
    CHECK(Dispatch(0x589, e) == V{ "CALLBOARD_VIEW_UPDATED" });
    CHECK(Run(L, "return #C_Callboard.GetView().Categories") == V{ "0" });
    Stop(L);
}

// The cache levels: the first eligible entry at or below the level is current, the next eligible one next; while none
// is current each entry above the level is upcoming; upcoming / next only with the current entry's event; entries of
// inactive events are skipped.
COA_TEST(CallboardCache_Levels) {
    lua_State* L = Start();
    CoA::CallboardCache::Dependencies().cacheLevel = [] { return s_level; };
    CHECK(Info(L) == (V{ "nil", "nil", "0", "nil", "0", "nil", "nil" }));
    Levels({ { 0, 100, 11, 110 }, { 0, 50, 12, 120 }, { 0, 0, 13, 130 } });
    s_level = 10;   // 100: upcoming, 50: upcoming, 0: current
    CHECK(Info(L) == (V{ "13", "12", "0", "130", "10", "0", "50" }));
    s_level = 60;   // 100 upcoming, 50 current (0 is next)
    CHECK(Info(L) == (V{ "12", "11", "0", "120", "60", "50", "100" }));
    s_level = 50;   // a threshold equal to the level is current
    CHECK(Info(L) == (V{ "12", "11", "0", "120", "50", "50", "100" }));
    s_level = 200;  // 100 current
    CHECK(Info(L) == (V{ "11", "nil", "0", "110", "200", "100", "nil" }));

    // Events: an inactive event's entries are skipped; upcoming with another event than current is dropped.
    Levels({ { 7, 100, 21, 210 }, { 0, 50, 22, 220 }, { 7, 0, 23, 230 } });
    s_level = 60;
    CHECK(Info(L) == (V{ "22", "nil", "0", "220", "60", "50", "nil" }));
    Event(7, true);
    CHECK(Info(L) == (V{ "22", "nil", "0", "220", "60", "50", "nil" }));   // upcoming (event 7) dropped: current is 0
    s_level = 10;
    CHECK(Info(L) == (V{ "23", "nil", "0", "230", "10", "0", "nil" }));   // the last upcoming (50, event 0) is dropped
    s_level = 5;
    Levels({ { 0, 10, 31, 310 } });
    CHECK(Info(L) == (V{ "nil", "nil", "0", "nil", "5", "nil", "nil" }));   // upcoming without a current is dropped

    // GetCurrentPoints: the Callboard cache points token (index 0x3C) first.
    CoATest::Bytes t;
    t.U32(1).Str("TOKEN_TYPE_CALLBOARD_CACHE_POINTS").U32(77).U32(0);
    Dispatch(0x66F, t);
    CHECK(Run(L, "return C_CallboardCache.GetCurrentPoints()") == (V{ "77", "nil" }));

    // A new table replaces the old one.
    Levels({});
    CHECK(Info(L) == (V{ "nil", "nil", "77", "nil", "5", "nil", "nil" }));
    Stop(L);
}

// GetPointsForQuest: the quest addon record's points through the Dep; nil without one; a non-number returns nothing.
COA_TEST(CallboardCache_PointsForQuest) {
    lua_State* L = Start();
    CHECK(Run(L, "return C_CallboardCache.GetPointsForQuest(5)") == V{ "nil" });
    CoA::CallboardCache::Dependencies().questAddonPoints = [](uint32_t quest, uint32_t& points) {
        points = quest * 2;
        return quest != 9;
    };
    CHECK(Run(L, "return C_CallboardCache.GetPointsForQuest(5)") == V{ "10" });
    CHECK(Run(L, "return C_CallboardCache.GetPointsForQuest(9)") == V{ "nil" });
    CHECK(Run(L, "return C_CallboardCache.GetPointsForQuest(7.9)") == V{ "14" });
    CHECK(Run(L, "return select('#', C_CallboardCache.GetPointsForQuest('x'))") == V{ "0" });
    Stop(L);
}

// Short 0x730: the count stops at the packet's end; a cut entry keeps what was read.
COA_TEST(CallboardCache_ShortPacket) {
    lua_State* L = Start();
    CoA::CallboardCache::Dependencies().cacheLevel = [] { return s_level; };
    CoATest::Bytes b;
    b.U32(99).U32(0).U32(0).U32(0).U32(0).U32(41).U32(410).U32(5);   // then a cut entry (the next one)
    Dispatch(0x730, b);
    CHECK(Info(L) == (V{ "41", "nil", "0", "410", "0", "0", "nil" }));
    Stop(L);
}
