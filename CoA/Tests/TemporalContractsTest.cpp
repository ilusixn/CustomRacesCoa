// C_TemporalContracts (TemporalContracts.cpp): the empty type list's shapes, the readers over types (as the VM-protected
// filler would leave them), the queued activations and their 50 ms timer, CMSG 0x55B, the quest sort / category
// natives, and the resets.
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Modules/TemporalContracts.h"
#include "CoA/Source/Modules/QuestAddon.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Timer.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;
    using Packets = std::vector<std::vector<uint8_t>>;
    using CoA::TemporalContracts::ContractType;
    using CoA::TemporalContracts::QuestRecord;

    std::map<uint32_t, QuestRecord> s_quests;
    std::map<uint32_t, uint32_t> s_points;
    std::vector<std::pair<std::string, uint32_t>> s_completions;   // {type, remaining}, content type in .first's prefix
    std::vector<uint32_t> s_scheduled;
    std::vector<uint32_t> s_cancelled;
    CoA::Timer::Fn s_tick = nullptr;
    uint32_t s_nextTimer = 0;

    std::vector<uint32_t> QuestRow(uint32_t id, uint32_t group8, uint32_t group12, uint32_t sort) {
        std::vector<uint32_t> row(0x1D, 0);
        row[0] = id;
        row[2] = group8;
        row[3] = group12;
        row[0x1A] = sort;   // +0x68
        row[0x1C] = 7;      // +0x70
        return row;
    }

    bool ReadQuests(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "Quest.dbc")) {
            out = CoATest::Wdbc({ QuestRow(30, 5, 0, 3), QuestRow(31, 5, 0, 0), QuestRow(32, 0, 6, 0), QuestRow(33, 9, 0, 0) },
                std::string(1, '\0'));
            return true;
        }

        return false;
    }

    QuestRecord Rec(uint32_t money, uint32_t honor, uint32_t arena, std::vector<uint32_t> items, std::vector<uint32_t> amounts,
        uint32_t zone = 0) {
        QuestRecord r;
        r.zoneOrSort = zone;
        r.money = money;
        r.honor = honor;
        r.arena = arena;

        for (size_t i = 0; i < 4 && i < items.size(); ++i) {
            r.items[i] = items[i];
            r.amounts[i] = amounts[i];
        }

        return r;
    }

    void Realm(uint32_t ruleset, float honorScale) {
        CoATest::Bytes b;
        b.U32(1).U32(ruleset).F32(0).F32(honorScale).F32(0).U32(0).F32(0).F32(0).U32(0);

        for (int i = 0; i < 8; ++i) {
            b.U8(0);
        }

        b.Str("").Str("Realm").U8(0);
        CoATest::Deliver(CoA::RealmInfo::OnRealmInfo, 0x9BC, b);
    }

    lua_State* Start() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::ClientHooks::enterWorld.Clear();
        CoA::Dbc::SetFileReader(ReadQuests);
        CoA::BridgeStore::Reset();
        CoA::BridgeStore::LoadTables();
        CoA::BridgeStore::BuildIndexes();
        Realm(0, 1.5f);
        s_quests.clear();
        s_points.clear();
        s_completions.clear();
        s_scheduled.clear();
        s_cancelled.clear();
        s_tick = nullptr;
        s_nextTimer = 0;
        CoA::Timer::SetBackend(
            [](uint32_t ms, CoA::Timer::Fn fn, void*) {
                s_scheduled.push_back(ms);
                s_tick = fn;
                return ++s_nextTimer;
            },
            [](uint32_t id, CoA::Timer::Fn, void*) { s_cancelled.push_back(id); });
        CoA::TemporalContracts::Reset();
        CoA::TemporalContracts::Init();
        CoA::TemporalContracts::Deps& d = CoA::TemporalContracts::Dependencies();
        d.questRecord = [](uint32_t quest, QuestRecord& out) {
            auto it = s_quests.find(quest);

            if (it == s_quests.end()) {
                return false;
            }

            out = it->second;
            return true;
        };
        d.areaName = [](uint32_t id) { return id == 12 ? "Elwynn Forest" : static_cast<const char*>(nullptr); };
        d.questSortName = [](uint32_t id) { return id == 22 ? "Seasonal" : static_cast<const char*>(nullptr); };
        return CoATest::NewState(false);
    }

    void Stop(lua_State* L) {
        lua_close(L);
        CoATest::StopRecordingPackets();
        CoA::TemporalContracts::Reset();
        CoA::Timer::SetBackend(nullptr, nullptr);
        CoA::ClientHooks::enterWorld.Clear();
        CoA::BridgeStore::Reset();
        CoA::Dbc::SetFileReader(nullptr);
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

    Packets Ticks(int n) {
        CoATest::RecordPackets();

        for (int i = 0; i < n; ++i) {
            s_tick(nullptr, nullptr);
        }

        CoATest::StopRecordingPackets();
        return CoATest::SentPackets();
    }

    std::vector<uint8_t> Activation(const std::string& type, uint8_t flag, uint32_t count) {
        return CoATest::Expected(0x55B, CoATest::Bytes().U32(static_cast<uint32_t>(type.size())).Raw(type.data(),
            static_cast<uint32_t>(type.size())).U8(flag).U32(count));
    }

    ContractType Type(const char* name, uint32_t remaining, uint32_t money, uint32_t token, std::vector<uint32_t> groups,
        const char* content, const char* display = "Display") {
        ContractType t;
        t.name = name;
        t.remaining = remaining;
        t.moneyCost = money;
        t.tokenCost = token;
        t.groups = groups;
        t.minimumLevel = 10;
        t.displayName = display;
        t.contentType = content;
        return t;
    }

    // A table as "k=v" pairs in key order, nested tables as "k={...}".
    // (The test state has the base library only: no table.sort / table.concat.)
    const char* kDump =
        "local function dump(t) local keys = {} for k in pairs(t) do local i = #keys + 1 keys[i] = k "
        "while i > 1 and tostring(keys[i - 1]) > tostring(keys[i]) do keys[i - 1], keys[i] = keys[i], keys[i - 1] i = i - 1 end end "
        "local out = '' for i, k in ipairs(keys) do local v = t[k] if i > 1 then out = out .. ' ' end "
        "out = out .. tostring(k) .. '=' .. (type(v) == 'table' and ('{' .. dump(v) .. '}') or tostring(v)) end "
        "return out end ";
}

// The type list is filled by the VM-protected part only: every reader returns its empty shape.
COA_TEST(TemporalContracts_EmptyShapes) {
    lua_State* L = Start();
    const std::string d = kDump;
    CHECK((Run(L, d + "return dump(C_TemporalContracts.GetCompletableTemporalContracts())") == V{ "" }));
    CHECK((Run(L, d + "return dump(C_TemporalContracts.GetTemporalContractTypes())") == V{ "" }));
    CHECK((Run(L, d + "return dump(C_TemporalContracts.GetTemporalContractInfo('X'))") == V{ "moneyCost=0 quests={} remainingCompletions=0 tokenCost=0" }));
    CHECK((Run(L, "return C_TemporalContracts.GetTemporalContractInfo(), C_TemporalContracts.GetTemporalContractInfo('a', 'b')") == V{ "nil", "nil" }));
    CHECK((Run(L, "return C_TemporalContracts.GetCompletableQuestsByCategory('X'), C_TemporalContracts.GetCategoryData('X'), "
                  "C_TemporalContracts.GetCategoryName('X'), C_TemporalContracts.GetTotalContractTypeRewards('X')") == V{ "nil", "nil", "nil", "nil" }));
    CHECK((Run(L, d + "return dump(C_TemporalContracts.GetTotalContentTypeRewards('X'))") ==
        V{ "CallboardCachePoints=0 RewardAmount={} RewardArenaPoints=0 RewardHonor=0 RewardItems={} RewardMoney=0" }));
    CHECK((Run(L, d + "return dump(C_TemporalContracts.GetTotalContentTypeCost('X')), dump(C_TemporalContracts.GetTotalContractTypeCost('X'))") ==
        V{ "moneyCost=0 tokenCost=0", "moneyCost=0 tokenCost=0" }));
    CHECK((Run(L, "return C_TemporalContracts.GetTotalContentTypeRewards(), C_TemporalContracts.GetTotalContentTypeCost(1, 2), "
                  "C_TemporalContracts.GetTotalContractTypeCost()") == V{ "nil", "nil", "nil" }));
    // The Callboard view is not loaded (not ported): nothing to activate.
    CHECK((Run(L, "return C_TemporalContracts.ActivateAllTemporalContracts(true), C_TemporalContracts.ActivateAllTemporalContractsByContentType('Dungeon', false)") ==
        V{ "false", "false" }));
    Stop(L);
}

// The readers over a type list.
COA_TEST(TemporalContracts_Readers) {
    lua_State* L = Start();
    CoA::TemporalContracts::SetTypes({ Type("A", 2, 100, 3, { 5, 0, 6 }, "Dungeon", "Alpha"), Type("B", 0, 50, 1, { 9 }, "Dungeon"),
        Type("C", 4, 0, 2, { 6 }, "Raid"), Type("D", 1, 10, 1, { 9 }, "Raid") });
    s_quests[31] = Rec(500, 20, 7, { 900, 901 }, { 1, 2 });
    s_quests[32] = Rec(300, 10, 3, { 900 }, { 5 });
    s_quests[33] = Rec(40, 2, 1, {}, {});
    const std::string d = kDump;
    CHECK((Run(L, d + "return dump(C_TemporalContracts.GetCompletableTemporalContracts())") ==
        V{ "A={minimumLevel=10 moneyCost=100 name=Alpha quests={1=30 2=31 3=32} remainingCompletions=2 tokenCost=3} "
           "D={minimumLevel=10 moneyCost=10 name=Display quests={1=33} remainingCompletions=1 tokenCost=1}" }));
    CHECK((Run(L, d + "return dump(C_TemporalContracts.GetTemporalContractTypes())") == V{ "Dungeon={1=A 2=B} Raid={1=C 2=D}" }));
    CHECK((Run(L, d + "return dump(C_TemporalContracts.GetTemporalContractInfo('B'))") ==
        V{ "moneyCost=50 quests={1=33} remainingCompletions=0 tokenCost=1" }));
    CHECK((Run(L, d + "return dump(C_TemporalContracts.GetCompletableQuestsByCategory('A'))") == V{ "1=30 2=31 3=32" }));
    CHECK((Run(L, "return C_TemporalContracts.GetCategoryName('A')") == V{ "Alpha" }));
    // The first group with a cached quest (group 5: 30 is not cached, 31 is): its rewards, the honor scaled by 1.5.
    CHECK((Run(L, d + "return dump(C_TemporalContracts.GetCategoryData('A'))") ==
        V{ "category=Dungeon minimumLevel=10 moneyCost=100 name=Alpha remainingCompletions=2 rewards={CallboardCachePoints=0 "
           "RewardAmount={1=1 2=2 3=0 4=0} RewardArenaPoints=7 RewardHonor=30 RewardItems={1=900 2=901 3=0 4=0} RewardMoney=500} tokenCost=3" }));
    // Every priced type of the content type with completions left: one cached quest per group, times the completions,
    // the items accumulated.
    CHECK((Run(L, d + "return dump(C_TemporalContracts.GetTotalContentTypeRewards('Dungeon'))") ==
        V{ "CallboardCachePoints=0 RewardAmount={1=12 2=4} RewardArenaPoints=20 RewardHonor=90 RewardItems={1=900 2=901} RewardMoney=1600" }));
    // One type: its first cached quest times the completions.
    CHECK((Run(L, d + "return dump(C_TemporalContracts.GetTotalContractTypeRewards('A'))") ==
        V{ "CallboardCachePoints=0 RewardAmount={1=2 2=4} RewardArenaPoints=14 RewardHonor=60 RewardItems={1=900 2=901} RewardMoney=1000" }));
    CHECK((Run(L, d + "return dump(C_TemporalContracts.GetTotalContractTypeRewards('C'))") ==
        V{ "CallboardCachePoints=0 RewardAmount={} RewardArenaPoints=0 RewardHonor=0 RewardItems={} RewardMoney=0" }));   // unpriced
    CHECK((Run(L, d + "return dump(C_TemporalContracts.GetTotalContentTypeCost('Dungeon')), dump(C_TemporalContracts.GetTotalContentTypeCost('Raid'))") ==
        V{ "moneyCost=200 tokenCost=6", "moneyCost=10 tokenCost=1" }));
    CHECK((Run(L, d + "return dump(C_TemporalContracts.GetTotalContractTypeCost('A')), dump(C_TemporalContracts.GetTotalContractTypeCost('C'))") ==
        V{ "moneyCost=200 tokenCost=6", "moneyCost=0 tokenCost=0" }));
    // The Callboard cache points (AscQuestAddon) through their Dep.
    CoA::TemporalContracts::Dependencies().questAddonPoints = [](uint32_t quest, uint32_t& points) {
        points = quest == 31 ? 4 : 0;
        return quest == 31;
    };
    CHECK((Run(L, "return C_TemporalContracts.GetCategoryData('A').rewards.CallboardCachePoints, "
                  "C_TemporalContracts.GetTotalContractTypeRewards('A').CallboardCachePoints") == V{ "4", "8" }));
    Stop(L);
}

// Activations queue (one at a time: nothing is queued while one is pending); from world entry a 50 ms timer sends one
// per tick as CMSG 0x55B {u32 length, the name's bytes, u8 flag, u32 count}.
COA_TEST(TemporalContracts_ActivationQueue) {
    lua_State* L = Start();
    CHECK((Run(L, "return C_TemporalContracts.ActivateTemporalContract('Alpha', true, 3)") == V{ "true" }));
    CHECK((Run(L, "return C_TemporalContracts.ActivateTemporalContract('Beta', false)") == V{ "false" }));
    CoA::ClientHooks::enterWorld.Run();
    CHECK((s_scheduled == std::vector<uint32_t>{ 50 }));
    CHECK((Ticks(1) == Packets{ Activation("Alpha", 1, 3) }));
    CHECK((s_scheduled == std::vector<uint32_t>{ 50, 50 }));
    CHECK((Ticks(1).empty()));
    CHECK((Run(L, "return C_TemporalContracts.ActivateTemporalContract('Beta', nil, 'x')") == V{ "true" }));
    CHECK((Ticks(1) == Packets{ Activation("Beta", 0, 0) }));
    CHECK((Run(L, "return C_TemporalContracts.ActivateTemporalContract('Gamma', 2, 7.9)") == V{ "true" }));
    CHECK((Ticks(1) == Packets{ Activation("Gamma", 1, 7) }));
    CHECK((Run(L, "return C_TemporalContracts.ActivateTemporalContract('Gamma'), C_TemporalContracts.ActivateTemporalContract({}, true), "
                  "C_TemporalContracts.ActivateTemporalContract('Gamma', 'yes')") == V{ "false", "false", "false" }));
    CHECK((Run(L, "return C_TemporalContracts.ActivateTemporalContract(5, true)") == V{ "true" }));   // a number is a string
    CHECK((Ticks(1) == Packets{ Activation("5", 1, 0) }));
    Stop(L);
}

// Reproduced original behaviour: the tick re-arms itself without storing the id, so the glue-Lua cancel (FUN_103392f0)
// kills only the first timer, once; every world entry starts another chain.
COA_TEST(TemporalContracts_TimerChain) {
    lua_State* L = Start();
    CoA::LuaReg::RunGlueRegistered();
    CHECK(s_cancelled.empty());   // nothing armed yet
    CoA::ClientHooks::enterWorld.Run();
    Ticks(2);
    CoA::LuaReg::RunGlueRegistered();
    CHECK((s_cancelled == std::vector<uint32_t>{ 1 }));
    CoA::LuaReg::RunGlueRegistered();
    CHECK((s_cancelled == std::vector<uint32_t>{ 1 }));
    CoA::ClientHooks::enterWorld.Run();
    CoA::ClientHooks::enterWorld.Run();
    CHECK(s_scheduled.size() == 5);
    CoA::LuaReg::RunGlueRegistered();
    CHECK((s_cancelled == std::vector<uint32_t>{ 1, 5 }));
    Stop(L);
}

// The glue screen empties the types and the pending queue (the timer is left alone).
COA_TEST(TemporalContracts_GlueReset) {
    lua_State* L = Start();
    CoA::TemporalContracts::SetTypes({ Type("A", 2, 100, 3, { 5 }, "Dungeon") });
    Run(L, "C_TemporalContracts.ActivateTemporalContract('A', true)");
    CoA::ClientHooks::enterWorld.Run();
    CoA::Lifecycle::RunGlueScreen();
    CHECK(s_cancelled.empty());
    CHECK((Ticks(1).empty()));
    CHECK((Run(L, "return C_TemporalContracts.GetCategoryName('A')") == V{ "nil" }));
    CHECK((Run(L, "return C_TemporalContracts.ActivateTemporalContract('A', true)") == V{ "true" }));
    Stop(L);
}

// ActivateAll* hand each completion of the Callboard view to the queue (count 0), through the Dep.
COA_TEST(TemporalContracts_ActivateAll) {
    lua_State* L = Start();
    CoA::TemporalContracts::Dependencies().forEachCompletion = [](const std::string* content, void (*fn)(const std::string&, void*), void* ctx) {
        bool any = false;

        for (const auto& c : s_completions) {
            if (content && c.first.compare(0, content->size(), *content) != 0) {
                continue;
            }

            for (uint32_t i = 0; i < c.second; ++i) {
                fn(c.first, ctx);
                any = true;
            }
        }

        return any;
    };
    s_completions = { { "DungeonA", 2 }, { "RaidB", 1 } };
    CHECK((Run(L, "return C_TemporalContracts.ActivateAllTemporalContractsByContentType('Raid', true)") == V{ "true" }));
    CHECK((Run(L, "return C_TemporalContracts.ActivateAllTemporalContracts(false)") == V{ "false" }));   // one is pending
    CoA::ClientHooks::enterWorld.Run();
    CHECK((Ticks(2) == Packets{ Activation("RaidB", 1, 0) }));
    CHECK((Run(L, "return C_TemporalContracts.ActivateAllTemporalContracts(false)") == V{ "true" }));
    CHECK((Ticks(4) == Packets{ Activation("DungeonA", 0, 0), Activation("DungeonA", 0, 0), Activation("RaidB", 0, 0) }));
    CHECK((Run(L, "return C_TemporalContracts.ActivateAllTemporalContractsByContentType('Pvp', true)") == V{ "false" }));
    CHECK((Run(L, "return C_TemporalContracts.ActivateAllTemporalContracts(), C_TemporalContracts.ActivateAllTemporalContracts('x'), "
                  "C_TemporalContracts.ActivateAllTemporalContractsByContentType('Raid')") == V{ "false", "false", "false" }));
    Stop(L);
}

// GetQuestCategory: the built-in id ranges (the second name from expansion 1 on), else the type owning one of the
// quest's Quest.dbc groups (+8 / +0xC / +0x10), else nil.
COA_TEST(TemporalContracts_QuestCategory) {
    lua_State* L = Start();
    CHECK((Run(L, "return C_TemporalContracts.GetQuestCategory(17838), C_TemporalContracts.GetQuestCategory(81381), "
                  "C_TemporalContracts.GetQuestCategory(31)") == V{ "TBCZoneDailies", "VanillaDisenchantingWeekly", "nil" }));
    Realm(1, 1.0f);
    CHECK((Run(L, "return C_TemporalContracts.GetQuestCategory(81477), C_TemporalContracts.GetQuestCategory(81478)") ==
        V{ "TBCHighriskArmsDealer", "nil" }));
    CoA::TemporalContracts::SetTypes({ Type("A", 1, 1, 1, { 4 }, "X"), Type("B", 1, 1, 1, { 7, 6 }, "X"), Type("C", 1, 1, 1, { 5 }, "X") });
    CHECK((Run(L, "return C_TemporalContracts.GetQuestCategory(31), C_TemporalContracts.GetQuestCategory(32), "
                  "C_TemporalContracts.GetQuestCategory(99), C_TemporalContracts.GetQuestCategory('31x'), C_TemporalContracts.GetQuestCategory()") ==
        V{ "C", "B", "nil", "nil", "nil" }));
    Stop(L);
}

// GetQuestSortIDs: [the cached quest's ZoneOrSort when asked and set,] the Quest.dbc row's non-zero sort ids; nothing
// while either record is missing. GetQuestSortText: AreaTable for a positive id, QuestSort for a negative one, else
// "Unknown".
COA_TEST(TemporalContracts_QuestSort) {
    lua_State* L = Start();
    s_quests[30] = Rec(0, 0, 0, {}, {}, 12);
    s_quests[99] = Rec(0, 0, 0, {}, {}, 12);
    const std::string d = kDump;
    CHECK((Run(L, d + "return dump(C_TemporalContracts.GetQuestSortIDs(30, true))") == V{ "1=12 2=3 3=7" }));
    CHECK((Run(L, d + "return dump(C_TemporalContracts.GetQuestSortIDs(30, false))") == V{ "1=3 2=7" }));
    CHECK((Run(L, "return C_TemporalContracts.GetQuestSortIDs(31, true)").empty()));   // not cached
    CHECK((Run(L, "return C_TemporalContracts.GetQuestSortIDs(99, true)").empty()));   // no Quest.dbc row
    CHECK((Run(L, "return C_TemporalContracts.GetQuestSortIDs('x', true)").empty()));
    CHECK((Run(L, "return C_TemporalContracts.GetQuestSortText(12), C_TemporalContracts.GetQuestSortText(-22), C_TemporalContracts.GetQuestSortText(13), "
                  "C_TemporalContracts.GetQuestSortText(-23), C_TemporalContracts.GetQuestSortText(0)") ==
        V{ "Elwynn Forest", "Seasonal", "Unknown", "Unknown", "Unknown" }));
    CHECK((Run(L, "return C_TemporalContracts.GetQuestSortText('x')").empty()));
    Stop(L);
}

// Every C_TemporalContracts native, for the world state.
COA_TEST(TemporalContracts_NativesRegistered) {
    lua_State* L = Start();
    CHECK((Run(L, "local n = 0 for k, v in pairs(C_TemporalContracts) do n = n + 1 end return n") == V{ "16" }));
    Stop(L);
}

// The real questAddonPoints (CoA::QuestAddon, Plan 13): no points while quest 31's record is fetched (CMSG 0x731), then
// the SMSG 0x732 points, times the completions in the totals.
COA_TEST(TemporalContracts_QuestAddonPoints) {
    lua_State* L = Start();
    CoA::TemporalContracts::SetTypes({ Type("A", 2, 100, 3, { 5, 0, 6 }, "Dungeon", "Alpha") });
    s_quests[31] = Rec(500, 20, 7, { 900, 901 }, { 1, 2 });
    CoA::QuestAddon::Reset();
    CoA::QuestAddon::Init();
    CoATest::RecordPackets();
    CHECK((Run(L, "return C_TemporalContracts.GetCategoryData('A').rewards.CallboardCachePoints") == V{ "0" }));
    const std::vector<uint8_t> query = CoATest::Expected(0x731, CoATest::Bytes().U32(31));
    CHECK(std::find(CoATest::SentPackets().begin(), CoATest::SentPackets().end(), query) != CoATest::SentPackets().end());
    CoATest::Bytes b;
    b.U32(31).U32(4).U32(0);
    CoATest::Deliver(CoA::QuestAddon::OnResponse, 0x732, b);
    CHECK((Run(L, "return C_TemporalContracts.GetCategoryData('A').rewards.CallboardCachePoints, "
                  "C_TemporalContracts.GetTotalContractTypeRewards('A').CallboardCachePoints") == V{ "4", "8" }));
    CoA::QuestAddon::Reset();
    Stop(L);
}
