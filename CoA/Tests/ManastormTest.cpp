#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/Manastorm.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/CVars.h"
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
#include <set>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;
    using Packets = std::vector<std::vector<uint8_t>>;

    const uint64_t kPlayer = 0x11;
    const uint64_t kParty1 = 0x21;
    const uint64_t kParty2 = 0x22;

    uint32_t F(float f) {
        uint32_t v;
        memcpy(&v, &f, 4);
        return v;
    }

    struct Strings {
        std::string block;

        uint32_t operator()(const char* s) {
            const uint32_t at = static_cast<uint32_t>(this->block.size()) + 1;   // Wdbc puts "" first
            this->block.append(s);
            this->block.push_back('\0');
            return at;
        }
    };

    std::vector<uint32_t> MessageRow(uint32_t id, uint32_t difficulty, uint32_t level, uint32_t event, uint32_t a, uint32_t b, uint32_t c) {
        std::vector<uint32_t> r = { id, difficulty, level, event, a };
        r.insert(r.end(), 16, b);
        r.push_back(0);
        r.insert(r.end(), 16, c);
        r.push_back(0);
        return r;
    }

    bool ReadTables(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "ManastormModifiers.dbc")) {
            // id, +4 key, +8 level, ..., +0x2C f, +0x30, +0x34 f, +0x38 allowed (15 dwords)
            std::vector<std::vector<uint32_t>> rows;
            auto row = [&](uint32_t id, uint32_t key, uint32_t level, float a, float b, uint32_t allowed) {
                std::vector<uint32_t> r(15, 0);
                r[0] = id;
                r[1] = key;
                r[2] = level;
                r[0x2C / 4] = F(a);
                r[0x34 / 4] = F(b);
                r[0x38 / 4] = allowed;
                rows.push_back(r);
            };
            row(1, 0, 1, 1.5f, 2.5f, 1);
            row(2, 0, 6, 3.0f, 4.0f, 1);
            row(3, 0, 11, 5.0f, 6.0f, 0);      // not allowed
            row(4, 2, 1, 7.0f, 8.0f, 1);       // end game key
            row(5, 7, 3, 0.5f, 0.25f, 0);      // the stage bonus row (key 7 = Manastorm +8)
            out = CoATest::Wdbc(rows, std::string(1, '\0'));
            return true;
        }

        if (strstr(path, "ManastormPlayerGroupModifiers.dbc")) {
            // id, +4 size, +8 kind, +0xC f, +0x10 f
            out = CoATest::Wdbc({ { 1, 1, 0, F(1.1f), F(1.2f) }, { 2, 2, 0, F(2.1f), F(2.2f) }, { 3, 2, 1, F(3.1f), F(3.2f) },
                { 4, 2, 2, F(9.0f), F(9.0f) }, { 5, 5, 0, F(5.1f), F(5.2f) } }, std::string(1, '\0'));
            return true;
        }

        if (strstr(path, "ManastormMessages.dbc")) {
            Strings s;
            const uint32_t a = s("A");
            const uint32_t b = s("B");
            const uint32_t c = s("C");
            const uint32_t x = s("X");
            out = CoATest::Wdbc({ MessageRow(1, 0, 5, 0, a, b, c), MessageRow(2, 0, 5, 77, x, x, x), MessageRow(3, 1, 5, 0, x, x, x),
                MessageRow(4, 0, 6, 0, x, x, x), MessageRow(5, 0, 5, 78, c, b, a) }, s.block, 7);
            return true;
        }

        if (strstr(path, "SpellCustomAttr.dbc")) {
            // id, +4 spell, ..., +0x14 flags (0x80 = a loadout spell)
            out = CoATest::Wdbc({ { 1, 9999, 0, 0, 0, 0, 0 }, { 2, 501, 0, 0, 0, 0x80, 0 }, { 3, 502, 0, 0, 0, 0x80, 0 },
                { 4, 503, 0, 0, 0, 0x80, 0 }, { 5, 600, 0, 0, 0, 0x80, 0 }, { 6, 0, 0, 0, 0, 0, 0 } }, std::string(1, '\0'));
            return true;
        }

        return false;
    }

    const char* const kTables[] = { "DBFilesClient\\Manastorm.dbc", "DBFilesClient\\ManastormModifiers.dbc",
        "DBFilesClient\\ManastormPlayerGroupModifiers.dbc", "DBFilesClient\\ManastormMessages.dbc", "DBFilesClient\\SpellCustomAttr.dbc" };

    bool s_hasPlayer = true;
    uint32_t s_level = 30;
    std::set<uint32_t> s_auras;
    std::set<uint32_t> s_known;
    uint64_t s_party[4] = {};
    std::vector<uint64_t> s_raid;
    std::vector<std::string> s_calls;
    uint32_t s_difficulty = 0;
    int64_t s_now = 1000;
    std::map<uint32_t, std::vector<uint32_t>> s_chains;

    struct FakeObject {
        int32_t raidTarget;
        uint32_t type;
        float pos[3];
    };
    float s_playerPos[3] = { 0, 0, 0 };

    void Start() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::CVars::Reset();
        CoA::ClientHooks::Reset();
        CoA::ClientHooks::afterPreAnimate.Clear();
        CoA::GameEvents::Reset();
        CoA::Manastorm::Reset();
        CoA::Dbc::SetFileReader(ReadTables);

        for (const char* t : kTables) {
            CoA::Dbc::Get(t).Unload();
        }

        CoA::CA::RebuildSpellCustomAttr();
        CoA::Manastorm::Init();
        s_hasPlayer = true;
        s_level = 30;
        s_auras.clear();
        s_known = { 501, 502, 503, 600, 601 };
        memset(s_party, 0, sizeof(s_party));
        s_raid.clear();
        s_calls.clear();
        s_difficulty = 0;
        s_now = 1000;
        s_chains = { { 600, { 600, 601 } } };
        s_playerPos[0] = s_playerPos[1] = s_playerPos[2] = 0;
        CoA::Manastorm::Deps& d = CoA::Manastorm::Dependencies();
        d.hasPlayer = [] { return s_hasPlayer; };
        d.playerGuid = []() -> uint64_t { return s_hasPlayer ? kPlayer : 0; };
        d.playerLevel = [] { return s_level; };
        d.playerHasAura = [](uint32_t spell) { return s_auras.count(spell) != 0; };
        d.knowsSpell = [](uint32_t spell) { return s_known.count(spell) != 0; };
        d.firstRank = [](uint32_t spell) { return spell == 601 ? 600u : spell; };
        d.rankChain = [](uint32_t first) -> const std::vector<uint32_t>* {
            auto it = s_chains.find(first);
            return it == s_chains.end() ? nullptr : &it->second;
        };
        d.partyGuid = [](uint32_t i) { return s_party[i]; };
        d.raidCount = [] { return static_cast<uint32_t>(s_raid.size()); };
        d.raidGuid = [](uint32_t i) { return s_raid[i]; };
        d.spellBasePoints = [](uint32_t spell, int32_t& a, int32_t& b) {
            if (spell != 93459) {
                return false;
            }

            a = 9;
            b = 4;
            return true;
        };
        d.instanceDifficulty = [] { return s_difficulty; };
        d.unitTokens = [](uint64_t guid, std::vector<std::string>& out) {
            if (guid == kPlayer) {
                out = { "player" };
            } else if (guid == kParty1) {
                out = { "target", "party1", "raid3" };
            } else if (guid == kParty2) {
                out = { "focus", "party2" };   // not in the levels map: never blamed
            }
        };
        d.unitTokenGuid = [](const char* token, uint64_t& guid) {
            guid = !strcmp(token, "player") ? kPlayer : !strcmp(token, "party1") ? kParty1 : 0;
            return guid != 0;
        };
        d.setSpellRangeFlags = [](uint32_t flags) { s_calls.push_back("range:" + std::to_string(flags)); };
        d.raidTargetIndex = [](void* o) { return static_cast<FakeObject*>(o)->raidTarget; };
        d.objectType = [](void* o) { return static_cast<FakeObject*>(o)->type; };
        d.positions = [](void* o, float player[3], float other[3]) {
            if (!s_hasPlayer) {
                return false;
            }

            memcpy(player, s_playerPos, sizeof(s_playerPos));
            memcpy(other, static_cast<FakeObject*>(o)->pos, sizeof(float) * 3);
            return true;
        };
        d.now = [] { return s_now; };
    }

    void Stop() {
        CoATest::StopRecordingPackets();
        CoATest::StopRecordingSignals();

        for (const char* t : kTables) {
            CoA::Dbc::Get(t).Unload();
        }

        CoA::CA::RebuildSpellCustomAttr();
        CoA::Dbc::SetFileReader(nullptr);
        CoA::Manastorm::Reset();
        CoA::GameEvents::Reset();
        CoA::ClientHooks::Reset();
        CoA::ClientHooks::afterPreAnimate.Clear();
        CoA::CVars::Reset();
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

    CoATest::Bytes Active(uint32_t level, uint32_t id, const char* type, uint32_t a, float b, uint32_t c) {
        return CoATest::Bytes().U32(level).U32(id).Str(type).U32(a).F32(b).U32(c);
    }

    // n lists: {m, values...}
    CoATest::Bytes Lists(CoATest::Bytes b, const std::vector<std::vector<uint32_t>>& lists) {
        b.U32(static_cast<uint32_t>(lists.size()));

        for (const auto& l : lists) {
            b.U32(static_cast<uint32_t>(l.size()));

            for (uint32_t v : l) {
                b.U32(v);
            }
        }

        return b;
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

    // "a,b,c" of an array table returned by the chunk.
    const char* kJoin = "local function join(t) if not t then return 'nil' end local s = '' for i = 1, #t do s = s .. (i > 1 and ',' or '') .. tostring(t[i]) end return s end ";
}

// Outside a Manastorm (Minimap.lua:235 / :346): not in one, level / id 0, type SOLO, cache info 0 / 0 / 0.
COA_TEST(Manastorm_OutsideReaders) {
    Start();
    lua_State* L = CoATest::NewState(false);
    CHECK((CoATest::Call(L, "C_Manastorm", "IsInManastorm") == V{ "false" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetActiveLevel") == V{ "0" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetActiveManastormID") == V{ "0" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetActiveManastormType") == V{ "SOLO" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetManastormCacheInfo") == V{ "0", "0", "0" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetBoss") == V{ "0", "0", "0", "0" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetStageBonusExperience") == V{ "nil", "nil" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetNumLoadoutSlots") == V{ "4" }));
    CHECK(!CoA::Manastorm::InManastorm());
    lua_close(L);
    Stop();
}

// SMSG 0x660: ACTIVE_MANASTORM_UPDATED(old, new) on a level change, MANASTORM_CACHE_INFO_UPDATED when a cache field
// moves (the float by more than 0.001). Entering (0 -> n) turns the 0x80DB37 jmp on and sets the SpellRange flags to 0;
// leaving restores the icon scale, the jne and the flags (2). An unknown type becomes MAX.
COA_TEST(Manastorm_ActiveData) {
    Start();
    lua_State* L = CoATest::NewState(false);
    CoATest::RecordSignals();
    Deliver(0x660, Active(3, 12, "DUO", 5, 1.5f, 7));
    CHECK((CoATest::Signals() == V{ "ACTIVE_MANASTORM_UPDATED:0,3", "MANASTORM_CACHE_INFO_UPDATED:5,1.5,7" }));
    CHECK(CoA::ClientHooks::manastormSkipsModalSpell);
    CHECK((s_calls == V{ "range:0" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "IsInManastorm") == V{ "true" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetActiveLevel") == V{ "3" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetActiveManastormID") == V{ "12" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetActiveManastormType") == V{ "DUO" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetManastormCacheInfo") == V{ "5", "1.5", "7" }));
    CoATest::Signals().clear();
    Deliver(0x660, Active(3, 12, "BOGUS", 5, 1.5005f, 7));   // same level, float within 0.001: nothing fires
    CHECK(CoATest::Signals().empty());
    CHECK((CoATest::Call(L, "C_Manastorm", "GetActiveManastormType") == V{ "MAX" }));
    Deliver(0x660, Active(4, 12, "GROUP_END_GAME", 5, 1.5005f, 8));
    CHECK((CoATest::Signals() == V{ "ACTIVE_MANASTORM_UPDATED:3,4", "MANASTORM_CACHE_INFO_UPDATED:5,1.5005,8" }));
    CHECK((s_calls == V{ "range:0" }));   // n -> m patches nothing
    CHECK((CoATest::Call(L, "C_Manastorm", "GetActiveManastormType") == V{ "GROUP_END_GAME" }));
    CoATest::Signals().clear();
    Deliver(0x660, Active(4, 12, "MAX", 5, 1.6f, 8));
    CHECK((CoATest::Signals() == V{ "MANASTORM_CACHE_INFO_UPDATED:5,1.6,8" }));
    // The icon scale is set by the pre-animate hook while shown; leaving puts it back to 1.
    CoA::Manastorm::OnGlueScreen();
    CHECK(CoA::Manastorm::ObjectiveIconScale() == 1.0f);
    Deliver(0x660, Active(2, 1, "SOLO", 0, 0.0f, 0));
    CHECK(CoA::ClientHooks::manastormSkipsModalSpell);
    CHECK(CoATest::Call(L, "C_Manastorm", "ShowObjectiveIcon").empty());
    FakeObject boss = { 2, 3, { 100.0f, 0.0f, 0.0f } };
    CoA::ClientHooks::afterPreAnimate.Run(&boss);
    CHECK(CoA::Manastorm::ObjectiveIconScale() == 5.0f);
    CoATest::Signals().clear();
    s_calls.clear();
    Deliver(0x660, Active(0, 0, "SOLO", 0, 0.0f, 0));
    CHECK((CoATest::Signals() == V{ "ACTIVE_MANASTORM_UPDATED:2,0" }));
    CHECK(!CoA::ClientHooks::manastormSkipsModalSpell);
    CHECK((s_calls == V{ "range:2" }));
    CHECK(CoA::Manastorm::ObjectiveIconScale() == 1.0f);
    // A short packet reads zeros past the end.
    Deliver(0x660, CoATest::Bytes().U32(9));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetActiveLevel") == V{ "9" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetActiveManastormType") == V{ "MAX" }));
    lua_close(L);
    Stop();
}

// SMSG 0x652 / 0x666 / 0x67B: the results as events; ENTER_MANASTORM_OK empties the levels completed this run (0x65E
// appends). 0x65E: MANASTORM_LEVEL_COMPLETED, the chaotic link back to 0, then MANASTORM_LEVEL_UNLOCKED for every message
// row of the instance difficulty and level whose game event is 0 or active.
COA_TEST(Manastorm_ResultsAndCompletedLevel) {
    Start();
    lua_State* L = CoATest::NewState(false);
    CoATest::RecordSignals();
    Deliver(0x65E, CoATest::Bytes().U32(4));
    Deliver(0x67F, CoATest::Bytes().U32(3));
    CoATest::Signals().clear();
    // Event 77 inactive, 78 active.
    CoATest::Bytes ev = CoATest::Bytes().U16(78).U8(1).U64(0).U64(0).U8(1).I32(0);
    ev.data.resize(0x34, 0);
    CDataStore evMsg(ev.data.data(), static_cast<uint32_t>(ev.data.size()));
    CoA::GameEvents::OnGameEventInfo(nullptr, 0x9BD, 0, &evMsg);
    CoATest::Signals().clear();
    Deliver(0x65E, CoATest::Bytes().U32(5));
    CHECK((CoATest::Signals() == V{ "MANASTORM_LEVEL_COMPLETED:5", "MANASTORM_LEVEL_UNLOCKED:A,B,C", "MANASTORM_LEVEL_UNLOCKED:C,B,A" }));
    CHECK((CoA::Manastorm::CompletedThisRun() == std::vector<uint32_t>{ 4, 5 }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetBoss") == V{ "0", "0", "0", "0" }));   // the link reset
    s_difficulty = 1;
    CoATest::Signals().clear();
    Deliver(0x65E, CoATest::Bytes().U32(5));
    CHECK((CoATest::Signals() == V{ "MANASTORM_LEVEL_COMPLETED:5", "MANASTORM_LEVEL_UNLOCKED:X,X,X" }));
    CoATest::Signals().clear();
    Deliver(0x652, CoATest::Bytes().Str("ENTER_MANASTORM_COOLDOWN"));
    CHECK((CoA::Manastorm::CompletedThisRun().size() == 3));
    Deliver(0x652, CoATest::Bytes().Str("NOT_A_RESULT"));
    CHECK((CoA::Manastorm::CompletedThisRun().size() == 3));
    Deliver(0x652, CoATest::Bytes().Str("ENTER_MANASTORM_OK"));
    CHECK(CoA::Manastorm::CompletedThisRun().empty());
    Deliver(0x666, CoATest::Bytes().Str("LEAVE_MANASTORM_TRANSITION"));
    Deliver(0x67B, CoATest::Bytes());
    CHECK((CoATest::Signals() == V{ "ENTER_MANASTORM_RESULT:ENTER_MANASTORM_COOLDOWN", "ENTER_MANASTORM_RESULT:NOT_A_RESULT",
        "ENTER_MANASTORM_RESULT:ENTER_MANASTORM_OK", "LEAVE_MANASTORM_RESULT:LEAVE_MANASTORM_TRANSITION", "MANASTORM_FAILED" }));
    lua_close(L);
    Stop();
}

// SMSG 0x65F: the player's completed levels (each list sorted); without a player they wait and move to the player after
// the idle tick. 0x65D {guid, lists}: MANASTORM_COMPLETED_LEVELS_UPDATED only for the player and only on a change.
// GetMaxCompletedLevels(unit): all types, then solo / duo / trio / group each with its end-game twin; nil for an
// unknown unit.
COA_TEST(Manastorm_CompletedLevels) {
    Start();
    lua_State* L = CoATest::NewState(false);
    CoATest::RecordSignals();
    s_hasPlayer = false;
    Deliver(0x65F, Lists(CoATest::Bytes(), { { 6, 1, 11 }, { 3 }, {}, {}, { 2, 21 }, {}, { 31 } }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetMaxCompletedLevels", { "s:player" }) == V{ "nil" }));
    CoA::Lifecycle::RunAfterIdle();   // still no player: waits
    s_hasPlayer = true;
    CHECK((CoATest::Call(L, "C_Manastorm", "GetMaxCompletedLevels", { "s:player" }) == V{ "nil" }));
    CoA::Lifecycle::RunAfterIdle();
    CHECK((CoATest::Call(L, "C_Manastorm", "GetMaxCompletedLevels", { "s:player" }) == V{ "31", "21", "3", "31", "0" }));
    CHECK(CoATest::Signals().empty());
    // 0x65D for the player: the same lists fire nothing, a change fires once.
    Deliver(0x65D, Lists(CoATest::Bytes().U64(kPlayer), { { 1, 6, 11 }, { 3 }, {}, {}, { 21, 2 }, {}, { 31 } }));
    CHECK(CoATest::Signals().empty());
    Deliver(0x65D, Lists(CoATest::Bytes().U64(kPlayer), { { 16 } }));
    CHECK((CoATest::Signals() == V{ "MANASTORM_COMPLETED_LEVELS_UPDATED" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetMaxCompletedLevels", { "s:player" }) == V{ "16", "16", "0", "0", "0" }));
    Deliver(0x65D, Lists(CoATest::Bytes().U64(kParty1), { {}, {}, {}, { 9, 4 } }));
    CHECK(CoATest::Signals().size() == 1);
    CHECK((CoATest::Call(L, "C_Manastorm", "GetMaxCompletedLevels", { "s:party1" }) == V{ "9", "0", "0", "0", "9" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetMaxCompletedLevels", { "s:party2" }) == V{ "nil" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetMaxCompletedLevels", { "n:1" }) == V{ "nil" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetMaxCompletedLevels") == V{}));
    // With a player 0x65F goes straight to the player's lists; a lying list count stops at the packet's end.
    Deliver(0x65F, Lists(CoATest::Bytes(), { { 2 }, { 40 } }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetMaxCompletedLevels", { "s:player" }) == V{ "40", "2", "40", "0", "0" }));
    Deliver(0x65F, CoATest::Bytes().U32(1000000).U32(1).U32(7));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetMaxCompletedLevels", { "s:player" }) == V{ "7", "7", "0", "0", "0" }));
    lua_close(L);
    Stop();
}

// CanEnter(level): ok, the ENTER_MANASTORM_* names, and for TOO_LOW_COMPLETED_LEVEL the tokens to blame ("player",
// else the first raid*, else the first party*; a member without such a token is left out). The group minimum is
// tested for every member, so once it falls short every known member is listed. GetEnterableLevels: 1, 6, 11, ... up
// to the group's highest + 1. Enter / Leave: CMSG 0x651 {u32} / 0x665.
COA_TEST(Manastorm_Enter) {
    Start();
    lua_State* L = CoATest::NewState(false);
    const std::string j = kJoin;
    Deliver(0x65D, Lists(CoATest::Bytes().U64(kPlayer), { { 10 } }));
    CHECK((Run(L, j + "local ok, e, t = C_Manastorm.CanEnter(6) return ok, e, t") == V{ "true", "nil", "nil" }));
    CHECK((Run(L, j + "local ok, e, t = C_Manastorm.CanEnter(11) return ok, join(e), t") == V{ "false", "ENTER_MANASTORM_BAD_LEVEL", "nil" }));
    CHECK((Run(L, j + "local ok, e, t = C_Manastorm.CanEnter(12) return ok, join(e), join(t)")
        == V{ "false", "ENTER_MANASTORM_TOO_LOW_COMPLETED_LEVEL,ENTER_MANASTORM_BAD_LEVEL", "player" }));
    CHECK((Run(L, j + "return join(C_Manastorm.GetEnterableLevels())") == V{ "1,6,11" }));
    // A party member with fewer levels pulls the group down; party2 is not in the map and is skipped.
    s_party[0] = kParty1;
    s_party[1] = kParty2;
    Deliver(0x65D, Lists(CoATest::Bytes().U64(kParty1), { { 3 } }));
    CHECK((Run(L, j + "return join(C_Manastorm.GetEnterableLevels())") == V{ "1" }));
    CHECK((Run(L, j + "local ok, e, t = C_Manastorm.CanEnter(6) return ok, join(e), join(t)")
        == V{ "false", "ENTER_MANASTORM_TOO_LOW_COMPLETED_LEVEL", "player,raid3" }));
    // Already active, the 93310 aura, under level 10.
    Deliver(0x660, Active(1, 5, "SOLO", 0, 0.0f, 0));
    s_auras = { 93310 };
    s_level = 9;
    CHECK((Run(L, j + "local ok, e = C_Manastorm.CanEnter(1) return ok, join(e)")
        == V{ "false", "ENTER_MANASTORM_ALREADY_ACTIVE,ENTER_MANASTORM_COOLDOWN,ENTER_MANASTORM_TOO_LOW_PLAYER_LEVEL" }));
    // At the server's max level (60) the end-game key (2) and the end-game types (4..7) are used.
    s_level = 60;
    s_auras.clear();
    Deliver(0x660, Active(0, 0, "SOLO", 0, 0.0f, 0));
    CHECK((Run(L, j + "local ok, e = C_Manastorm.CanEnter(6) return ok, join(e)")
        == V{ "false", "ENTER_MANASTORM_TOO_LOW_COMPLETED_LEVEL,ENTER_MANASTORM_BAD_LEVEL" }));
    CHECK((Run(L, j + "return C_Manastorm.CanEnter(1)") == V{ "true", "nil", "nil" }));
    // Without a player its own entry counts as 0, which blames it (no token) and every known member.
    s_hasPlayer = false;
    CHECK((Run(L, j + "local ok, e, t = C_Manastorm.CanEnter(6) return ok, join(e), join(t)")
        == V{ "false", "ENTER_MANASTORM_TOO_LOW_COMPLETED_LEVEL", "raid3" }));
    CHECK((Run(L, "return C_Manastorm.CanEnter('x')") == V{}));
    CoATest::RecordPackets();
    CHECK((CoATest::Call(L, "C_Manastorm", "Enter", { "n:11.9" }) == V{ "true" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "Enter") == V{}));
    CHECK((CoATest::Call(L, "C_Manastorm", "Leave") == V{ "true" }));
    CHECK((CoATest::SentPackets() == Packets{ CoATest::Expected(0x651, CoATest::Bytes().U32(11)), CoATest::Expected(0x665, CoATest::Bytes()) }));
    lua_close(L);
    Stop();
}

// CanLeave: NOT_ACTIVE without an active Manastorm id, TRANSITION with aura 93304 on the player.
COA_TEST(Manastorm_CanLeave) {
    Start();
    lua_State* L = CoATest::NewState(false);
    const std::string j = kJoin;
    CHECK((Run(L, j + "local ok, e = C_Manastorm.CanLeave() return ok, join(e)") == V{ "false", "LEAVE_MANASTORM_NOT_ACTIVE" }));
    Deliver(0x660, Active(1, 5, "SOLO", 0, 0.0f, 0));
    CHECK((Run(L, j + "local ok, e = C_Manastorm.CanLeave() return ok, e") == V{ "true", "nil" }));
    s_auras = { 93304 };
    CHECK((Run(L, j + "local ok, e = C_Manastorm.CanLeave() return ok, join(e)") == V{ "false", "LEAVE_MANASTORM_TRANSITION" }));
    s_hasPlayer = false;
    CHECK((Run(L, j + "return C_Manastorm.CanLeave()") == V{ "true", "nil" }));
    lua_close(L);
    Stop();
}

// GetBoss: Manastorm +0xC (the row from SMSG 0x67D), the chaotic link (SMSG 0x67F, MANASTORM_CHAOTIC_LINK_UPDATED on a
// change), then (spell 93459's base points + 1) * link. GetStageBonusExperience: the modifier row for (Manastorm +8,
// active level), times Manastorm +0x18 / +0x20. GetRewardModifier(id): Manastorm +0x10 / +0x14, the group modifier and
// the products. GetExperienceModifier(a, b): the first modifier row's +0x2C / +0x34.
COA_TEST(Manastorm_Modifiers) {
    Start();
    lua_State* L = CoATest::NewState(false);
    CoATest::RecordSignals();
    // id 12: +8 key 7, +0xC boss 4242, +0x10 2.0, +0x14 3.0, +0x18 4.0, +0x20 8.0
    Deliver(0x67D, CoATest::Bytes().U32(12).U32(0).U32(7).U32(4242).F32(2.0f).F32(3.0f).F32(4.0f).U32(0).F32(8.0f));
    Deliver(0x660, Active(3, 12, "SOLO", 0, 0.0f, 0));
    CoATest::Signals().clear();
    Deliver(0x67F, CoATest::Bytes().U32(2));
    Deliver(0x67F, CoATest::Bytes().U32(2));
    CHECK((CoATest::Signals() == V{ "MANASTORM_CHAOTIC_LINK_UPDATED:0,2" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetBoss") == V{ "4242", "2", "20", "10" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetStageBonusExperience") == V{ "2", "2" }));
    Deliver(0x660, Active(4, 12, "SOLO", 0, 0.0f, 0));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetStageBonusExperience") == V{ "nil", "nil" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetExperienceModifier", { "n:0", "n:6" }) == V{ "3", "4" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetExperienceModifier", { "n:2", "n:6" }) == V{ "nil", "nil" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetExperienceModifier", { "n:0" }) == V{}));
    // Solo: the size-1 row. Two: the size-2 rows, the largest kind below 2 (kind 1).
    CHECK((CoATest::Call(L, "C_Manastorm", "GetRewardModifier", { "n:12" })
        == V{ "2", "3", "1.100000024", "1.200000048", "2.200000048", "3.600000143" }));
    s_party[2] = 0x99;
    CHECK((CoATest::Call(L, "C_Manastorm", "GetRewardModifier", { "n:99" })
        == V{ "1", "1", "3.099999905", "3.200000048", "3.099999905", "3.200000048" }));
    // A raid with members counts them instead (+1): size 5, but the size-5 row's kind 0 is below the best kind so far (1).
    s_raid = { 0, 7, 8, 9, 10 };
    CHECK((CoATest::Call(L, "C_Manastorm", "GetRewardModifier", { "n:99" })[2] == "3.099999905"));
    s_raid = { 0, 0 };   // a raid without members: the party counts
    CHECK((CoATest::Call(L, "C_Manastorm", "GetRewardModifier", { "n:99" })[2] == "3.099999905"));
    s_party[2] = 0;
    CHECK((CoATest::Call(L, "C_Manastorm", "GetRewardModifier", { "n:99" })[2] == "1.100000024"));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetRewardModifier") == V{}));
    lua_close(L);
    Stop();
}

// Loadouts: 0x688 the whole list, 0x68B one slot (MANASTORM_LOADOUT_UPDATE, 1-based), 0x68A the result of CMSG 0x689 --
// OK puts the spell in flight into the slot. CanSetLoadoutSpellAtIndex lists the SET_MANASTORM_LOADOUT_* reasons (the
// range test lets slot == max through); GetAvailableLoadoutSpells: the 0x80 SpellCustomAttr spells whose highest known
// rank they are, settable first.
COA_TEST(Manastorm_Loadout) {
    Start();
    lua_State* L = CoATest::NewState(false);
    const std::string j = kJoin;
    CoATest::RecordSignals();
    Deliver(0x688, CoATest::Bytes().U32(3).U32(501).U32(0).U32(502));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetLoadoutSpellAtIndex", { "n:1" }) == V{ "501" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetLoadoutSpellAtIndex", { "n:2" }) == V{ "nil" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetLoadoutSpellAtIndex", { "n:4" }) == V{ "nil" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetLoadoutSpellAtIndex", { "n:0" }) == V{}));
    Deliver(0x68B, CoATest::Bytes().U32(5).U32(503));
    CHECK((CoATest::Signals() == V{ "MANASTORM_LOADOUT_UPDATE:6,503" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetLoadoutSpellAtIndex", { "n:6" }) == V{ "503" }));
    CoATest::RecordPackets();
    CHECK((CoATest::Call(L, "C_Manastorm", "SetLoadoutSpellAtIndex", { "n:2", "n:600" }) == V{ "true" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "SetLoadoutSpellAtIndex", { "n:0", "n:600" }) == V{}));
    CHECK((CoATest::SentPackets() == Packets{ CoATest::Expected(0x689, CoATest::Bytes().U32(1).U32(600)) }));
    CoATest::Signals().clear();
    Deliver(0x68A, CoATest::Bytes().U32(1).Str("SET_MANASTORM_LOADOUT_BAD_RANK"));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetLoadoutSpellAtIndex", { "n:2" }) == V{ "nil" }));
    Deliver(0x68A, CoATest::Bytes().U32(1).Str("SET_MANASTORM_LOADOUT_OK"));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetLoadoutSpellAtIndex", { "n:2" }) == V{ "600" }));
    Deliver(0x68A, CoATest::Bytes().U32(2).Str("SET_MANASTORM_LOADOUT_OK"));   // the spell in flight is now 0
    CHECK((CoATest::Call(L, "C_Manastorm", "GetLoadoutSpellAtIndex", { "n:3" }) == V{ "nil" }));
    CHECK((CoATest::Signals() == V{ "MANASTORM_LOADOUT_RESULT:2,SET_MANASTORM_LOADOUT_BAD_RANK",
        "MANASTORM_LOADOUT_RESULT:2,SET_MANASTORM_LOADOUT_OK", "MANASTORM_LOADOUT_RESULT:3,SET_MANASTORM_LOADOUT_OK" }));
    // Loadout now {501, 600, 0, 0, 0, 503}.
    CHECK((Run(L, j + "return C_Manastorm.CanSetLoadoutSpellAtIndex(5, 9999)") == V{ "false", "table" }));
    CHECK((Run(L, j + "local ok, e = C_Manastorm.CanSetLoadoutSpellAtIndex(6, 9999) return join(e)")
        == V{ "SET_MANASTORM_LOADOUT_OUT_OF_RANGE,SET_MANASTORM_LOADOUT_BAD_SPELL,SET_MANASTORM_LOADOUT_NOT_KNOWN" }));
    CHECK((Run(L, j + "local ok, e = C_Manastorm.CanSetLoadoutSpellAtIndex(5, 503) return join(e)") == V{ "SET_MANASTORM_LOADOUT_ALREADY_SET" }));
    CHECK((Run(L, j + "return C_Manastorm.CanSetLoadoutSpellAtIndex(5, 502)") == V{ "true", "nil" }));
    s_known.erase(502);
    CHECK((Run(L, j + "local ok, e = C_Manastorm.CanSetLoadoutSpellAtIndex(1, 502) return join(e)") == V{ "SET_MANASTORM_LOADOUT_NOT_KNOWN" }));
    // 600's chain {600, 601}: 601 is the highest known rank, so 600 is the wrong rank.
    CHECK((Run(L, j + "local ok, e = C_Manastorm.CanSetLoadoutSpellAtIndex(3, 600) return join(e)")
        == V{ "SET_MANASTORM_LOADOUT_BAD_RANK,SET_MANASTORM_LOADOUT_ALREADY_SET" }));
    Deliver(0x660, Active(1, 5, "SOLO", 0, 0.0f, 0));
    CHECK((Run(L, "return C_Manastorm.CanSetLoadoutSpellAtIndex(0, 502)") == V{}));
    CHECK((Run(L, j + "local ok, e = C_Manastorm.CanSetLoadoutSpellAtIndex(1, 502) return join(e)")
        == V{ "SET_MANASTORM_LOADOUT_ACTIVE_MANASTORM,SET_MANASTORM_LOADOUT_NOT_KNOWN" }));
    Deliver(0x660, Active(0, 0, "SOLO", 0, 0.0f, 0));
    // 501 / 503 already set, 502 unknown (not settable), 600 the wrong rank (left out).
    CHECK((Run(L, j + "local a, b = C_Manastorm.GetAvailableLoadoutSpells() return join(a), join(b)") == V{ "", "501,502,503" }));
    s_known.insert(502);
    CHECK((Run(L, j + "local a, b = C_Manastorm.GetAvailableLoadoutSpells() return join(a), join(b)") == V{ "502", "501,503" }));
    lua_close(L);
    Stop();
}

// SMSG 0x6AD: every read bounds-checked (a short packet drops everything), actions above 2 skipped, an empty result
// erases the guid, MANASTORM_REWARD_VISIBILITY_UPDATED("%s%u", decimal guid, n). GetRewardVisibility / IsRewardLocked /
// GetRewardLimitProgress read it.
COA_TEST(Manastorm_RewardVisibility) {
    Start();
    lua_State* L = CoATest::NewState(false);
    CoATest::RecordSignals();
    auto reward = [](CoATest::Bytes& b, uint32_t item, uint8_t action, uint32_t lockEnd, uint32_t secs, uint32_t earned,
                      uint32_t max, uint32_t remaining, uint32_t start, uint32_t end) {
        b.U32(item).U8(action).U32(lockEnd).U32(secs).U32(earned).U32(max).U32(remaining).U32(start).U32(end);
    };
    CoATest::Bytes b = CoATest::Bytes().U64(42).U32(3);
    reward(b, 100, 2, 5000, 60, 1, 3, 2, 10, 20);
    reward(b, 101, 1, 0, 0, 0, 0, 0, 0, 0);
    reward(b, 102, 3, 0, 0, 0, 5, 0, 0, 0);   // unknown action: skipped
    Deliver(0x6AD, b);
    CHECK((CoATest::Signals() == V{ "MANASTORM_REWARD_VISIBILITY_UPDATED:42,3" }));
    CHECK((Run(L, "local t = C_Manastorm.GetRewardVisibility(42) local r = t[100] return r.context, r.itemID, r.action, "
        "r.isHidden, r.isLocked, r.lockEndTimestamp, r.secondsRemaining, r.earnedQuantity, r.maxQuantity, r.remainingQuantity, "
        "r.windowStart, r.windowEnd, t[101].action, t[101].isHidden, t[101].lockEndTimestamp, t[102]")
        == V{ "42", "100", "LOCKED_RATE_LIMIT_EXHAUSTED", "false", "true", "5000", "60", "1", "3", "2", "10", "20", "HIDE", "true",
            "nil", "nil" }));
    CHECK((Run(L, "return C_Manastorm.GetRewardVisibility(42, 101).action, C_Manastorm.GetRewardVisibility(42, 7), "
        "next(C_Manastorm.GetRewardVisibility(41))") == V{ "HIDE", "nil", "nil" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "IsRewardLocked", { "n:42", "n:100" }) == V{ "true", "5000", "60" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "IsRewardLocked", { "n:42", "n:101" }) == V{ "false", "nil", "nil" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "IsRewardLocked", { "n:42" }) == V{}));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetRewardLimitProgress", { "n:42", "n:100" }) == V{ "1", "3", "2", "60", "true" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetRewardLimitProgress", { "n:42", "n:101" }) == V{ "nil" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetRewardVisibility") == V{}));
    // A count the packet cannot hold drops the packet; a later one replaces the guid's map; an empty one erases it.
    CoATest::Signals().clear();
    Deliver(0x6AD, CoATest::Bytes().U64(42).U32(2).U32(100));
    Deliver(0x6AD, CoATest::Bytes().U64(42));
    Deliver(0x6AD, CoATest::Bytes().U32(1));
    CHECK(CoATest::Signals().empty());
    CHECK((CoATest::Call(L, "C_Manastorm", "IsRewardLocked", { "n:42", "n:100" })[0] == "true"));
    CoATest::Bytes c = CoATest::Bytes().U64(42).U32(1);
    reward(c, 101, 0, 0, 7, 0, 0, 0, 0, 0);
    Deliver(0x6AD, c);
    CHECK((CoATest::Call(L, "C_Manastorm", "IsRewardLocked", { "n:42", "n:100" }) == V{ "false", "nil", "nil" }));
    CHECK((Run(L, "local r = C_Manastorm.GetRewardVisibility(42, 101) return r.action, r.secondsRemaining") == V{ "NONE", "7" }));
    CoATest::Bytes d = CoATest::Bytes().U64(42).U32(1);
    reward(d, 101, 9, 0, 0, 0, 0, 0, 0, 0);
    Deliver(0x6AD, d);
    CHECK((CoATest::Signals() == V{ "MANASTORM_REWARD_VISIBILITY_UPDATED:42,1", "MANASTORM_REWARD_VISIBILITY_UPDATED:42,1" }));
    CHECK((Run(L, "return next(C_Manastorm.GetRewardVisibility(42))") == V{ "nil" }));
    lua_close(L);
    Stop();
}

// FUN_102a4a10 (after 0x743EC0): only in a Manastorm with ShowObjectiveIcon called, for a unit (type 3) with a raid
// target icon (< 8): the icon scale becomes distance / 20 clamped to [1, 10]. No player: nothing.
COA_TEST(Manastorm_ObjectiveIcon) {
    Start();
    FakeObject unit = { 1, 3, { 60.0f, 0.0f, 80.0f } };   // distance 100 -> 5
    CoA::ClientHooks::afterPreAnimate.Run(&unit);
    CHECK(CoA::Manastorm::ObjectiveIconScale() == 1.0f);   // not in a Manastorm
    Deliver(0x660, Active(1, 5, "SOLO", 0, 0.0f, 0));
    CoA::ClientHooks::afterPreAnimate.Run(&unit);
    CHECK(CoA::Manastorm::ObjectiveIconScale() == 1.0f);   // the icon not shown
    lua_State* L = CoATest::NewState(false);
    CoATest::Call(L, "C_Manastorm", "ShowObjectiveIcon");
    CoA::ClientHooks::afterPreAnimate.Run(&unit);
    CHECK(CoA::Manastorm::ObjectiveIconScale() == 5.0f);
    FakeObject far = { 7, 3, { 0.0f, 400.0f, 0.0f } };
    CoA::ClientHooks::afterPreAnimate.Run(&far);
    CHECK(CoA::Manastorm::ObjectiveIconScale() == 10.0f);
    FakeObject near = { 0, 3, { 1.0f, 1.0f, 1.0f } };
    CoA::ClientHooks::afterPreAnimate.Run(&near);
    CHECK(CoA::Manastorm::ObjectiveIconScale() == 1.0f);
    FakeObject unmarked = { 8, 3, { 60.0f, 0.0f, 80.0f } };
    CoA::ClientHooks::afterPreAnimate.Run(&unmarked);
    CHECK(CoA::Manastorm::ObjectiveIconScale() == 1.0f);
    FakeObject player = { 1, 4, { 60.0f, 0.0f, 80.0f } };
    CoA::ClientHooks::afterPreAnimate.Run(&player);
    CHECK(CoA::Manastorm::ObjectiveIconScale() == 1.0f);
    s_hasPlayer = false;
    CoA::ClientHooks::afterPreAnimate.Run(&unit);
    CHECK(CoA::Manastorm::ObjectiveIconScale() == 1.0f);
    // The glue screen resets the manager but not the icon time.
    s_hasPlayer = true;
    CoA::Lifecycle::RunGlueScreen();
    Deliver(0x660, Active(1, 5, "SOLO", 0, 0.0f, 0));
    CoA::ClientHooks::afterPreAnimate.Run(&unit);
    CHECK(CoA::Manastorm::ObjectiveIconScale() == 5.0f);
    lua_close(L);
    Stop();
}

// FUN_102a0d70 on every glue screen: the levels, the active Manastorm, the chaotic link, the loadout, the rewards and the
// pending lists are cleared and the icon scale is 1. The world CVar manastormObjectiveIconCulling is queued.
COA_TEST(Manastorm_GlueReset) {
    Start();
    lua_State* L = CoATest::NewState(false);
    Deliver(0x660, Active(3, 12, "TRIO", 1, 2.0f, 3));
    Deliver(0x65D, Lists(CoATest::Bytes().U64(kPlayer), { { 10 } }));
    Deliver(0x67F, CoATest::Bytes().U32(4));
    Deliver(0x688, CoATest::Bytes().U32(1).U32(501));
    CoATest::Bytes b = CoATest::Bytes().U64(42).U32(1).U32(100).U8(2).U32(0).U32(0).U32(0).U32(0).U32(0).U32(0).U32(0);
    Deliver(0x6AD, b);
    s_hasPlayer = false;
    Deliver(0x65F, Lists(CoATest::Bytes(), { { 7 } }));
    s_hasPlayer = true;
    CoA::Lifecycle::RunGlueScreen();
    CoA::Lifecycle::RunAfterIdle();   // the pending list was dropped too
    CHECK((CoATest::Call(L, "C_Manastorm", "IsInManastorm") == V{ "false" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetActiveManastormType") == V{ "SOLO" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetManastormCacheInfo") == V{ "0", "0", "0" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetMaxCompletedLevels", { "s:player" }) == V{ "nil" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetLoadoutSpellAtIndex", { "n:1" }) == V{ "nil" }));
    CHECK((CoATest::Call(L, "C_Manastorm", "IsRewardLocked", { "n:42", "n:100" })[0] == "false"));
    CHECK((CoATest::Call(L, "C_Manastorm", "GetBoss")[1] == "0"));
    size_t n = 0;
    const CoA::CVarEntry* world = CoA::CVars::World(n);
    bool found = false;

    for (size_t i = 0; i < n; ++i) {
        if (!strcmp(world[i].name, "manastormObjectiveIconCulling")) {
            found = world[i].flags == 1 && !strcmp(world[i].defaultValue, "0") && world[i].category == 4 && !world[i].help &&
                world[i].callback && world[i].callback(nullptr, "0", "1", nullptr);
        }
    }

    CHECK(found);
    lua_close(L);
    Stop();
}
