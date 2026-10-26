#include "CoA/Source/Modules/LootLockout.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;
    using Packets = std::vector<std::vector<uint8_t>>;

    const uint64_t kPlayer = 0x0000000000000101ull;
    const uint64_t kOther = 0x0000000000000202ull;
    int64_t s_now = 1000;
    std::map<uint32_t, CoA::LootLockout::Encounter> s_encounters;
    uint32_t s_min = 0;
    uint32_t s_max = 0;
    bool s_unit = true;

    bool ReadExtra(const char* path, std::vector<uint8_t>& out) {
        if (!strstr(path, "DungeonEncounterExtra.dbc")) {
            return false;
        }

        // id, entry (+4), +8, +0xC
        out = CoATest::Wdbc({ { 3, 900, 0, 0 }, { 5, 777, 0, 0 }, { 6, 777, 0, 0 } }, std::string(1, '\0'));
        return true;
    }

    void Add(uint32_t id, int32_t map, int32_t difficulty, int32_t order, const char* name, uint32_t icon) {
        CoA::LootLockout::Encounter e;
        e.id = id;
        e.map = map;
        e.difficulty = difficulty;
        e.order = order;
        e.name = name;
        e.icon = icon;
        s_encounters[id] = e;
    }

    void Start() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::LootLockout::Reset();
        CoA::LootLockout::Init();
        CoA::Dbc::SetFileReader(ReadExtra);
        CoA::Dbc::Get("DBFilesClient\\DungeonEncounterExtra.dbc").Unload();
        s_now = 1000;
        s_unit = true;
        s_encounters.clear();
        // Ids 1..4 on map 33 difficulty 0 (heroic = 1), 4 is the container's max id.
        Add(1, 33, 0, 1, "Boss One", 7);
        Add(2, 33, 0, 2, nullptr, 99);
        Add(3, 33, 1, 3, "Heroic Three", 7);
        Add(4, 33, 0, 4, "Four", 7);
        s_min = 1;
        s_max = 4;
        CoA::LootLockout::Deps& d = CoA::LootLockout::Dependencies();
        d.tokenGuid = [](const char* token) -> uint64_t {
            return !strcmp(token, "player") ? kPlayer : !strcmp(token, "target") ? kOther : 0;
        };
        d.encounter = [](uint32_t id, CoA::LootLockout::Encounter& out) {
            auto it = s_encounters.find(id);

            if (it == s_encounters.end()) {
                return false;
            }

            out = it->second;
            return true;
        };
        d.encounterMinId = [] { return s_min; };
        d.encounterMaxId = [] { return s_max; };
        d.spellIcon = [](uint32_t id) -> const char* { return id == 7 ? "Interface\\Icons\\Boss" : nullptr; };
        d.mapInstanceType = [](uint32_t map, int32_t& type) {
            if (map == 0) {
                return false;
            }

            type = map >= 500 ? 2 : 1;
            return true;
        };
        d.unitEntry = [](const char* token, uint32_t& entry) {
            if (!s_unit) {
                return false;
            }

            entry = !strcmp(token, "target") ? 777 : 1;
            return true;
        };
        d.now = [] { return s_now; };
    }

    void Stop() {
        CoATest::StopRecordingPackets();
        CoATest::StopRecordingSignals();
        CoA::Dbc::Get("DBFilesClient\\DungeonEncounterExtra.dbc").Unload();
        CoA::Dbc::SetFileReader(nullptr);
        CoA::LootLockout::Reset();
        CoA::Lifecycle::Reset();
        CoA::Packets::Clear();
        CoA::LuaReg::Reset();
    }

    void Deliver(uint32_t opcode, CoATest::Bytes b) {
        CDataStore msg(b.data.data(), static_cast<uint32_t>(b.data.size()));
        int32_t result = 0;
        CHECK(CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result));
        CHECK(result == 1);
    }

    // Runs a chunk; results as strings (tables "table").
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
                : lua_istable(L, i) ? "table" : lua_isfunction(L, i) ? "function" : lua_tostring(L, i));
        }

        lua_settop(L, base);
        return out;
    }
}

// SMSG 0x9B4 appends every {encounter, expiry} without clearing; 0x9B5 appends one; 0x9B6 erases the first match only.
// FUN_102948d0 answers the first entry for the encounter: seconds left, 0 once expired or past 2038 (time >> 32).
COA_TEST(LootLockout_PacketsAndSecondsRemaining) {
    Start();
    Deliver(0x9B4, CoATest::Bytes().U64(kPlayer).U32(2).U32(1).U32(1600).U32(2).U32(1100));
    CHECK(CoA::LootLockout::SecondsRemaining(kPlayer, 1) == 600);
    CHECK(CoA::LootLockout::SecondsRemaining(kPlayer, 2) == 100);
    CHECK(CoA::LootLockout::SecondsRemaining(kOther, 1) == 0);
    Deliver(0x9B4, CoATest::Bytes().U64(kPlayer).U32(1).U32(1).U32(5000));   // appended after the first lock for 1
    CHECK(CoA::LootLockout::SecondsRemaining(kPlayer, 1) == 600);
    Deliver(0x9B6, CoATest::Bytes().U64(kPlayer).U32(1));                     // the first one goes
    CHECK(CoA::LootLockout::SecondsRemaining(kPlayer, 1) == 4000);
    Deliver(0x9B6, CoATest::Bytes().U64(kPlayer).U32(1));
    CHECK(CoA::LootLockout::SecondsRemaining(kPlayer, 1) == 0);
    Deliver(0x9B6, CoATest::Bytes().U64(kOther).U32(1));                      // unknown unit: nothing
    Deliver(0x9B5, CoATest::Bytes().U64(kOther).U32(3).U32(1001));
    CHECK(CoA::LootLockout::SecondsRemaining(kOther, 3) == 1);
    s_now = 1001;
    CHECK(CoA::LootLockout::SecondsRemaining(kOther, 3) == 0);               // expiry <= now
    s_now = (1ll << 32) + 5;
    CHECK(CoA::LootLockout::SecondsRemaining(kPlayer, 2) == 0);              // the high half set
    s_now = 1000;
    // A short 0x9B4 reads zeros past the end and the count loop stops when the packet is used up.
    Deliver(0x9B4, CoATest::Bytes().U64(kOther).U32(1000).U32(4).U32(2000));
    CHECK(CoA::LootLockout::SecondsRemaining(kOther, 4) == 1000);
    Deliver(0x9B5, CoATest::Bytes().U64(kOther).U32(2));                      // expiry reads 0
    CHECK(CoA::LootLockout::SecondsRemaining(kOther, 2) == 0);
    Stop();
}

// SMSG 0x6FE: the binds are emptied first; only QUERY_INSTANCE_BINDS_OK reads the u32 n x 12-byte list; the event
// carries ("%b%s", ok, result) either way.
COA_TEST(LootLockout_InstanceBinds) {
    Start();
    lua_State* L = CoATest::NewState(false);
    CoATest::RecordSignals();
    Deliver(0x6FE, CoATest::Bytes().Str("QUERY_INSTANCE_BINDS_OK").U32(4).U32(9).U32(33).U32(0).U32(9).U32(533).U32(1)
        .U32(9).U32(34).U32(1).U32(9).U32(0).U32(1));
    CHECK((CoATest::Signals() == V{ "QUERY_INSTANCE_BINDS_RESULT:true,QUERY_INSTANCE_BINDS_OK" }));
    // Dungeons (type != 2) at difficulty 2 -> raw 1: map 34; map 0 has no Map row.
    CHECK((Run(L, "local t = C_LootLockout.ListInstanceBinds(false, 2) local s = '' for i, v in ipairs(t) do s = s .. i .. '=' .. v .. ' ' end return s")
        == V{ "1=34 " }));
    CHECK((Run(L, "local t = ListInstanceBinds(true, 2) return t[1], t[2]") == V{ "533", "nil" }));
    CHECK((Run(L, "local t = ListInstanceBinds(false, 1) return t[1], t[2]") == V{ "33", "nil" }));
    CHECK((Run(L, "return ListInstanceBinds(false, 0)") == V{}));
    CHECK((Run(L, "return ListInstanceBinds(1, 1)") == V{ "table" }));    // a number passes a boolean slot (true)
    CHECK((Run(L, "local t = ListInstanceBinds(1, 2) return t[1]") == V{ "533" }));
    CHECK((Run(L, "return ListInstanceBinds('x', 1)") == V{}));
    CoATest::Signals().clear();
    Deliver(0x6FE, CoATest::Bytes().Str("QUERY_INSTANCE_BINDS_UNKNOWN").U32(1).U32(9).U32(33).U32(0));
    CHECK((CoATest::Signals() == V{ "QUERY_INSTANCE_BINDS_RESULT:false,QUERY_INSTANCE_BINDS_UNKNOWN" }));
    CHECK((Run(L, "return #ListInstanceBinds(false, 1)") == V{ "0" }));
    Deliver(0x6FE, CoATest::Bytes().Str("QUERY_INSTANCE_BINDS_OK").U32(9).U32(1).U32(34).U32(0));   // short list
    CHECK((Run(L, "local t = ListInstanceBinds(false, 1) return #t, t[1]") == V{ "1", "34" }));
    lua_close(L);
    Stop();
}

// QueryInstanceBinds: CMSG 0x6FD, true. ResetInstanceDifficulty(map, difficulty): {number, number}; CMSG 0x58C {u32 map,
// u8 difficulty - 1}; a difficulty whose low byte is 0 (0, 256) sends nothing and returns nothing.
COA_TEST(LootLockout_Cmsg) {
    Start();
    lua_State* L = CoATest::NewState(false);
    CoATest::RecordPackets();
    CHECK((CoATest::Call(L, "C_LootLockout", "QueryInstanceBinds") == V{ "true" }));
    CHECK((CoATest::Call(L, "", "ResetInstanceDifficulty", { "n:533", "n:3.9" }) == V{ "true" }));
    CHECK((CoATest::Call(L, "", "ResetInstanceDifficulty", { "n:533", "n:0" }) == V{}));
    CHECK((CoATest::Call(L, "", "ResetInstanceDifficulty", { "n:533", "n:256" }) == V{}));
    CHECK((CoATest::Call(L, "", "ResetInstanceDifficulty", { "n:533", "n:258" }) == V{ "true" }));
    CHECK((CoATest::Call(L, "", "ResetInstanceDifficulty", { "n:533" }) == V{}));
    CHECK((CoATest::SentPackets() == Packets{ CoATest::Expected(0x6FD, CoATest::Bytes()),
        CoATest::Expected(0x58C, CoATest::Bytes().U32(533).U8(2)), CoATest::Expected(0x58C, CoATest::Bytes().U32(533).U8(1)) }));
    lua_close(L);
    Stop();
}

// GetEncounterData(token, id): exactly a string and a number, else nil. Name ("" for none), map, difficulty + 1, the
// icon path (the question mark without a SpellIcon row), order, seconds remaining. An unknown encounter returns 6
// having pushed nothing: the caller receives the six stack slots ending at the top (its locals below the call, the
// function and the two arguments).
COA_TEST(LootLockout_GetEncounterData) {
    Start();
    lua_State* L = CoATest::NewState(false);
    Deliver(0x9B5, CoATest::Bytes().U64(kPlayer).U32(1).U32(1300));
    CHECK((CoATest::Call(L, "C_LootLockout", "GetEncounterData", { "s:player", "n:1.7" })
        == V{ "Boss One", "33", "1", "Interface\\Icons\\Boss", "1", "300" }));
    CHECK((CoATest::Call(L, "", "GetEncounterData", { "s:target", "n:2" })
        == V{ "", "33", "1", "Interface\\Icons\\inv_misc_questionmark", "2", "0" }));
    CHECK((CoATest::Call(L, "", "GetEncounterData", { "s:player" }) == V{ "nil" }));
    CHECK((CoATest::Call(L, "", "GetEncounterData", { "s:player", "n:1", "n:1" }) == V{ "nil" }));
    CHECK((CoATest::Call(L, "", "GetEncounterData", { "s:player", "s:x" }) == V{ "nil" }));
    CHECK((CoATest::Call(L, "", "GetEncounterData", { "n:5", "s:1" }) == V{ "Boss One", "33", "1", "Interface\\Icons\\Boss", "1", "0" }));
    // luaD_poscall copies the six slots from three below the call onto the call's own slots, front to back, so the
    // three locals arrive twice.
    CHECK((Run(L, "local a, b, c = 'x', 'y', 'z' local r1, r2, r3, r4, r5, r6 = GetEncounterData('player', 99) "
        "return r1, r2, r3, r4, r5, r6") == V{ "x", "y", "z", "x", "y", "z" }));
    lua_close(L);
    Stop();
}

// GetEncounterDatasForMapAndDifficulty(token, map, difficulty): ids 0 .. max - 1 only (id 4, the max, is never seen),
// keyed by the row id, the difficulty argument matched against the row + 1. Has...: ids min .. max, true at the first
// one still locked.
COA_TEST(LootLockout_MapAndDifficulty) {
    Start();
    lua_State* L = CoATest::NewState(false);
    Deliver(0x9B4, CoATest::Bytes().U64(kPlayer).U32(2).U32(2).U32(1100).U32(4).U32(5000));
    CHECK((Run(L, "local t = C_LootLockout.GetEncounterDatasForMapAndDifficulty('player', 33, 1) local n = 0 "
        "for k in pairs(t) do n = n + 1 end local e = t[2] return n, t[1].EncounterName, t[1].MapID, t[1].DifficultyID, "
        "t[1].SpellIconID, t[1].OrderIndex, t[1].TimeRemaining, e.EncounterName, e.SpellIconID, e.TimeRemaining, t[4]")
        == V{ "2", "Boss One", "33", "1", "Interface\\Icons\\Boss", "1", "0", "", "Interface\\Icons\\inv_misc_questionmark",
            "100", "nil" }));
    CHECK((Run(L, "local t = GetEncounterDatasForMapAndDifficulty('player', 33, 2) return t[3].EncounterName, t[3].DifficultyID")
        == V{ "Heroic Three", "2" }));
    CHECK((Run(L, "return GetEncounterDatasForMapAndDifficulty('player', 33)") == V{ "nil" }));
    CHECK((CoATest::Call(L, "", "HasEncounterDatasForMapAndDifficulty", { "s:player", "n:33", "n:1" }) == V{ "true" }));
    CHECK((CoATest::Call(L, "", "HasEncounterDatasForMapAndDifficulty", { "s:player", "n:33", "n:2" }) == V{ "false" }));
    CHECK((CoATest::Call(L, "", "HasEncounterDatasForMapAndDifficulty", { "s:target", "n:33", "n:1" }) == V{ "false" }));
    CHECK((CoATest::Call(L, "", "HasEncounterDatasForMapAndDifficulty", { "s:player", "n:33" }) == V{ "nil" }));
    // Only the max id (4) still locked: Has sees it, the table does not.
    s_now = 1200;
    CHECK((CoATest::Call(L, "C_LootLockout", "HasEncounterDatasForMapAndDifficulty", { "s:player", "n:33", "n:1" }) == V{ "true" }));
    CHECK((Run(L, "local t = GetEncounterDatasForMapAndDifficulty('player', 33, 1) return t[2].TimeRemaining, t[4]") == V{ "0", "nil" }));
    // Ids below the container's min id are not visited by Has.
    Add(0, 33, 0, 0, "Zero", 7);
    Deliver(0x9B5, CoATest::Bytes().U64(kOther).U32(0).U32(5000));
    CHECK((CoATest::Call(L, "", "HasEncounterDatasForMapAndDifficulty", { "s:target", "n:33", "n:1" }) == V{ "false" }));
    CHECK((Run(L, "return GetEncounterDatasForMapAndDifficulty('target', 33, 1)[0].TimeRemaining") == V{ "3800" }));
    lua_close(L);
    Stop();
}

// GetLootLockoutTimeRemaining / GetUnitLootLockForEncounter (FUN_102956c0) and GetLootLockouts (FUN_102957d0):
// { [map] = { [raw difficulty] = { [encounter] = seconds } } }; an expired lock still creates its tables; locks on an
// encounter without a row are dropped.
COA_TEST(LootLockout_Lockouts) {
    Start();
    lua_State* L = CoATest::NewState(false);
    Deliver(0x9B4, CoATest::Bytes().U64(kPlayer).U32(4).U32(1).U32(1500).U32(3).U32(900).U32(4).U32(1010).U32(42).U32(9000));
    CHECK((CoATest::Call(L, "", "GetLootLockoutTimeRemaining", { "s:player", "n:1" }) == V{ "500" }));
    CHECK((CoATest::Call(L, "C_LootLockout", "GetUnitLootLockForEncounter", { "s:player", "n:4" }) == V{ "10" }));
    CHECK((CoATest::Call(L, "", "GetUnitLootLockForEncounter", { "s:player" }) == V{ "nil" }));
    CHECK((Run(L, "local t = GetLootLockouts('player') local d0 = t[33][0] local d1 = t[33][1] local n = 0 "
        "for k in pairs(d1) do n = n + 1 end return d0[1], d0[4], n, t[42]") == V{ "500", "10", "0", "nil" }));
    CHECK((Run(L, "local n = 0 for k in pairs(C_LootLockout.GetLootLockouts('target')) do n = n + 1 end return n") == V{ "0" }));
    CHECK((Run(L, "return GetLootLockouts()") == V{ "nil" }));
    lua_close(L);
    Stop();
}

// GetUnitEncounterID(token): the first DungeonEncounterExtra row (by id) whose +4 is the unit's entry; nil without
// a unit or a row.
COA_TEST(LootLockout_UnitEncounterId) {
    Start();
    lua_State* L = CoATest::NewState(false);
    CHECK((CoATest::Call(L, "C_LootLockout", "GetUnitEncounterID", { "s:target" }) == V{ "5" }));
    CHECK((CoATest::Call(L, "", "GetUnitEncounterID", { "s:player" }) == V{ "nil" }));
    CHECK((CoATest::Call(L, "", "GetUnitEncounterID", {}) == V{ "nil" }));
    s_unit = false;
    CHECK((CoATest::Call(L, "", "GetUnitEncounterID", { "s:target" }) == V{ "nil" }));
    lua_close(L);
    Stop();
}

// The UIParent readers outside any lockout (UIParent.lua:1150 / :1748): QueryInstanceBinds answers true and the
// lockout readers answer empty tables / 0. Every glue screen (FUN_10294740) empties the lockouts and the binds.
COA_TEST(LootLockout_OutsideAndGlueReset) {
    Start();
    lua_State* L = CoATest::NewState(false);
    CHECK((Run(L, "return C_LootLockout.QueryInstanceBinds(), #C_LootLockout.ListInstanceBinds(true, 1), "
        "next(GetLootLockouts('player')), GetLootLockoutTimeRemaining('player', 1)") == V{ "true", "0", "nil", "0" }));
    Deliver(0x9B5, CoATest::Bytes().U64(kPlayer).U32(1).U32(1500));
    Deliver(0x6FE, CoATest::Bytes().Str("QUERY_INSTANCE_BINDS_OK").U32(1).U32(9).U32(33).U32(0));
    CoA::Lifecycle::RunGlueScreen();
    CHECK(CoA::LootLockout::SecondsRemaining(kPlayer, 1) == 0);
    CHECK((Run(L, "return #ListInstanceBinds(false, 1)") == V{ "0" }));
    lua_close(L);
    Stop();
}
