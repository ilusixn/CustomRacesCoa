#ifndef COA_TESTS_CHALLENGEFIXTURE_H
#define COA_TESTS_CHALLENGEFIXTURE_H

// Shared by ChallengeTest.cpp and TrialCreatorTest.cpp: the Challenge*.dbc tables built row by row, the client fakes
// behind CoA::Challenge::Deps / CoA::TrialCreator::Deps, and helpers to feed packets and read Lua results.
#include "CoA/Source/Modules/AttachMisc.h"
#include "CoA/Source/Modules/Challenge.h"
#include "CoA/Source/Modules/Config.h"
#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/ObjectAddon.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Modules/TrialCreator.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ChallengeFixture {
    using V = std::vector<std::string>;
    using Dwords = std::vector<uint32_t>;

    struct Strings {
        std::string block;

        uint32_t operator()(const char* s) {
            const uint32_t at = static_cast<uint32_t>(this->block.size()) + 1;
            this->block.append(s);
            this->block.push_back('\0');
            return at;
        }
    };

    inline uint32_t F(float f) {
        uint32_t v;
        memcpy(&v, &f, 4);
        return v;
    }

    // One table: its layout spec ('L' = 16 locale slots + flags in the file), its struct rows and its string block.
    struct TableData {
        std::string spec;
        std::vector<Dwords> rows;
        Strings str;
    };

    inline std::map<std::string, TableData>& Tables() {
        static std::map<std::string, TableData> t;
        return t;
    }

    inline const char* const kPaths[] = { "DBFilesClient\\Challenge.dbc", "DBFilesClient\\ChallengeGroups.dbc", "DBFilesClient\\ChallengeLevels.dbc",
        "DBFilesClient\\ChallengeConditions.dbc", "DBFilesClient\\ChallengeRequirements.dbc", "DBFilesClient\\ChallengeRules.dbc",
        "DBFilesClient\\ChallengeSpells.dbc", "DBFilesClient\\ChallengeModifiers.dbc", "DBFilesClient\\ChallengeRewards.dbc",
        "DBFilesClient\\ChallengeGroupRewards.dbc", "DBFilesClient\\ChallengeFeatured.dbc", "DBFilesClient\\ChallengeConditionTypes.dbc",
        "DBFilesClient\\ChallengeRequirementTypes.dbc", "DBFilesClient\\ChallengeRuleTypes.dbc", "DBFilesClient\\ChallengeModifierTypes.dbc" };

    inline std::string Base(const char* path) {
        const char* a = strrchr(path, '\\');
        const char* b = strrchr(path, '/');
        const char* s = a > b ? a : b;
        return s ? s + 1 : path;
    }

    inline TableData& Tbl(const char* name) {   // "Challenge.dbc"
        return Tables()[name];
    }

    inline Dwords Expand(const std::string& spec, const Dwords& dwords) {
        Dwords out;

        for (size_t i = 0; i < spec.size(); ++i) {
            const uint32_t v = i < dwords.size() ? dwords[i] : 0;

            if (spec[i] == 'L') {
                out.insert(out.end(), 16, v);
                out.push_back(0);
            } else {
                out.push_back(v);
            }
        }

        return out;
    }

    inline bool ReadTables(const char* path, std::vector<uint8_t>& out) {
        auto it = Tables().find(Base(path));

        if (it == Tables().end() || it->second.rows.empty()) {
            return false;
        }

        TableData& t = it->second;
        std::vector<Dwords> rows;

        for (const Dwords& r : t.rows) {
            rows.push_back(t.spec.empty() ? r : Expand(t.spec, r));
        }

        out = CoATest::Wdbc(rows, t.str.block, t.spec.empty() ? 0 : static_cast<uint32_t>(t.spec.size()));
        return true;
    }

    inline void Unload() {
        for (const char* p : kPaths) {
            CoA::Dbc::Get(p).Unload();
        }
    }

    // ---- row builders (struct dwords) ----
    struct ChallengeRow {
        uint32_t id = 0;
        uint32_t pve = 0;
        uint32_t pvp = 0;
        uint32_t requiresMode = 0;
        const char* mode = "";
        uint32_t flags = 0;
        uint32_t exclusive = 0;
        const char* name = "";
        const char* desc = "";
        uint32_t event = 0;
        const char* icon = "";
        const char* difficulty = "CHALLENGE_DIFFICULTY_EASY";
        uint32_t trial = 0;
        uint32_t dungeon = 0;
        uint32_t prestige = 0;
        uint32_t checkMap = 0;
        uint32_t map = 0;
        uint32_t mapDifficulty = 0;
        uint32_t sort = 0;
        const char* artwork = "";
    };

    inline void AddChallenge(const ChallengeRow& c) {
        TableData& t = Tbl("Challenge.dbc");
        t.spec = "dddddddLLdddddddddddd";
        Dwords r(21, 0);
        r[0] = c.id;
        r[1] = c.pve;
        r[2] = c.pvp;
        r[3] = c.requiresMode;
        r[4] = t.str(c.mode);
        r[5] = c.flags;
        r[6] = c.exclusive;
        r[7] = t.str(c.name);
        r[8] = t.str(c.desc);
        r[9] = c.event;
        r[10] = t.str(c.icon);
        r[11] = t.str(c.difficulty);
        r[12] = c.trial;
        r[13] = c.dungeon;
        r[14] = c.prestige;
        r[16] = c.checkMap;
        r[17] = c.map;
        r[18] = c.mapDifficulty;
        r[19] = c.sort;
        r[20] = t.str(c.artwork);
        t.rows.push_back(r);
    }

    inline void AddGroup(uint32_t id, uint32_t challenge, uint32_t group) {
        Tbl("ChallengeGroups.dbc").rows.push_back({ id, challenge, group });
    }

    inline void AddLevel(uint32_t id, uint32_t challenge, uint32_t group, uint32_t level, uint32_t event = 0) {
        Tbl("ChallengeLevels.dbc").rows.push_back({ id, challenge, group, level, event });
    }

    inline void AddCondition(uint32_t id, uint32_t challenge, uint32_t group, uint32_t level, const char* type, float v1 = 0, float v2 = 0, float v3 = 0,
        uint32_t neg = 0, uint32_t elseGroup = 0) {
        TableData& t = Tbl("ChallengeConditions.dbc");
        t.rows.push_back({ id, challenge, group, level, elseGroup, t.str(type), F(v1), F(v2), F(v3), neg });
    }

    inline void AddRequirement(uint32_t id, uint32_t challenge, uint32_t group, uint32_t level, const char* type, float v1, uint32_t sort) {
        TableData& t = Tbl("ChallengeRequirements.dbc");
        t.rows.push_back({ id, challenge, group, level, t.str(type), F(v1), F(v1 + 1), F(v1 + 2), sort });
    }

    inline void AddRule(uint32_t id, uint32_t challenge, uint32_t group, uint32_t level, const char* type) {
        TableData& t = Tbl("ChallengeRules.dbc");
        t.rows.push_back({ id, challenge, group, level, t.str(type) });
    }

    inline void AddSpell(uint32_t id, uint32_t challenge, uint32_t group, uint32_t level, uint32_t pve, uint32_t pvp, uint32_t player = 0, uint32_t boss = 0,
        uint32_t minion = 0) {
        Tbl("ChallengeSpells.dbc").rows.push_back({ id, challenge, group, level, pve, pvp, 0, player, boss, minion });
    }

    inline void AddModifier(uint32_t id, uint32_t challenge, uint32_t group, uint32_t level, const char* type, float a, float b, float c) {
        TableData& t = Tbl("ChallengeModifiers.dbc");
        t.rows.push_back({ id, challenge, group, level, t.str(type), F(a), F(b), F(c) });
    }

    inline void AddReward(uint32_t id, uint32_t challenge, uint32_t group, uint32_t level, const char* expansion, const char* cls, uint32_t first,
        uint32_t item, uint32_t amount, uint32_t achievement, uint32_t sort) {
        TableData& t = Tbl("ChallengeRewards.dbc");
        t.rows.push_back({ id, challenge, group, level, t.str(expansion), t.str(cls), first, item, amount, achievement, sort });
    }

    inline void AddGroupReward(uint32_t id, uint32_t group, uint32_t level, uint32_t count, const char* expansion, const char* cls, uint32_t item,
        uint32_t amount, uint32_t achievement, uint32_t sort) {
        TableData& t = Tbl("ChallengeGroupRewards.dbc");
        t.rows.push_back({ id, group, level, count, t.str(expansion), t.str(cls), item, amount, achievement, sort });
    }

    inline void AddFeatured(uint32_t id, uint32_t week, uint32_t a, uint32_t b, uint32_t c) {
        Tbl("ChallengeFeatured.dbc").rows.push_back({ id, week, a, b, c });
    }

    // ---- client fakes ----
    struct Client {
        bool player = true;
        uint8_t cls = 1;
        uint32_t level = 10;
        uint64_t guid = 7;
        uint32_t party = 0;
        uint64_t leader = 0;
        std::set<uint32_t> achievementsDone;
        std::map<uint32_t, uint32_t> achievementFlags;   // id -> flags; absent = no Achievement.dbc row
        int32_t map = 0;
        int32_t difficulty = 0;
        std::map<uint32_t, uint32_t> spellAttributes;    // absent = no Spell.dbc row
        bool tokenFound = true;
        uint64_t tokenGuid = 7;
        int64_t now = 1000000;
        std::set<std::string> icons;
    };

    inline Client& C() {
        static Client c;
        return c;
    }

    inline void InstallDeps() {
        CoA::Challenge::Deps& d = CoA::Challenge::Dependencies();
        d.hasPlayer = [] { return C().player; };
        d.playerClass = [] { return C().cls; };
        d.playerLevel = [] { return C().level; };
        d.activePlayerGuid = [] { return C().guid; };
        d.partyMembers = [] { return C().party; };
        d.partyLeader = [] { return C().leader; };
        d.achievementDone = [](uint32_t id) { return C().achievementsDone.count(id) != 0; };
        d.achievementFlags = [](uint32_t id, uint32_t& flags) {
            auto it = C().achievementFlags.find(id);

            if (it == C().achievementFlags.end()) {
                return false;
            }

            flags = it->second;
            return true;
        };
        d.currentMap = [] { return C().map; };
        d.instanceDifficulty = [] { return C().difficulty; };
        d.spellAttributes = [](uint32_t spell, uint32_t& a) {
            auto it = C().spellAttributes.find(spell);

            if (it == C().spellAttributes.end()) {
                return false;
            }

            a = it->second;
            return true;
        };
        d.worldPosition = [](const float pos[3], uint32_t map, float& x, float& y) {
            x = pos[0] / 100.0f;
            y = pos[1] / 100.0f + static_cast<float>(map);
        };
        d.unitTokenGuid = [](const char* token, uint64_t& guid) {
            guid = strcmp(token, "player") == 0 && C().tokenFound ? C().tokenGuid : 0;
            return strcmp(token, "player") == 0 && C().tokenFound;
        };
        d.now = [] { return C().now; };
        CoA::TrialCreator::Deps& t = CoA::TrialCreator::Dependencies();
        t.iconExists = [](const char* icon) { return C().icons.count(icon) != 0; };
        t.now = [] { return C().now; };
    }

    inline V Feed(uint32_t opcode, CoATest::Bytes bytes) {
        CoATest::RecordSignals();
        CDataStore msg(bytes.data.data(), static_cast<uint32_t>(bytes.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }

    // Runs Lua code that sets the global r; returns r as a string ("nil" / "true" / "false" for those).
    inline std::string Run(lua_State* L, const std::string& code) {
        if (luaL_dostring(L, code.c_str()) != 0) {
            std::string err = lua_isstring(L, -1) ? lua_tostring(L, -1) : "?";
            lua_pop(L, 1);
            return "error: " + err;
        }

        lua_getglobal(L, "r");
        std::string out = lua_isboolean(L, -1) ? (lua_toboolean(L, -1) ? "true" : "false") : lua_isstring(L, -1) ? lua_tostring(L, -1) : "nil";
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_setglobal(L, "r");
        return out;
    }

    // A Lua helper the tests share: j(v) flattens a value (tables with sorted keys) into one string.
    // (The test state has only the base library: sort is a plain insertion sort.)
    inline const char* kJoin =
        "function rep(s, n) local r = '' while n > 0 do if n % 2 == 1 then r = r .. s end s = s .. s n = (n - n % 2) / 2 end return r end "
        "function sort(t, lt) for i = 2, #t do local v = t[i] local k = i - 1 while k >= 1 and lt(v, t[k]) do t[k + 1] = t[k] k = k - 1 end t[k + 1] = v end end "
        "function j(v) if type(v) ~= 'table' then return tostring(v) end "
        "local ks = {} for k in pairs(v) do ks[#ks + 1] = k end "
        "sort(ks, function(a, b) if type(a) == type(b) then return a < b end return type(a) < type(b) end) "
        "local s = '{' for i, k in ipairs(ks) do s = s .. (i > 1 and ',' or '') .. tostring(k) .. '=' .. j(v[k]) end return s .. '}' end "
        "function seq(t, f) local s = '' for i = 1, #t do local x = t[i] if f then x = x[f] end s = s .. (i > 1 and ',' or '') .. tostring(x) end return s end";

    inline void Realm(uint32_t ruleset, uint8_t devGate = 0) {
        CoATest::Bytes b;
        b.U32(1).U32(ruleset).F32(0).F32(0).F32(0).U32(0).F32(0).F32(0).U32(0);
        b.U8(1).U8(0).U8(0).U8(0).U8(devGate).U8(0).U8(0).U8(0);
        b.Str("").Str("Realm").U8(0);
        CoATest::Deliver(CoA::RealmInfo::OnRealmInfo, 0x9BC, b);
    }

    inline void Mode(uint32_t mode) {
        Feed(0x90B, CoATest::Bytes().U32(mode));
    }

    inline void Configs(const std::vector<std::pair<const char*, int32_t>>& ints, const std::vector<std::pair<const char*, bool>>& bools,
        const std::vector<std::pair<const char*, float>>& floats) {
        CoATest::Bytes b;
        b.U32(static_cast<uint32_t>(ints.size()));

        for (const auto& kv : ints) {
            b.Name(kv.first).I32(kv.second);
        }

        b.U32(static_cast<uint32_t>(bools.size()));

        for (const auto& kv : bools) {
            b.Name(kv.first).U8(kv.second ? 1 : 0);
        }

        b.U32(static_cast<uint32_t>(floats.size()));

        for (const auto& kv : floats) {
            b.Name(kv.first).F32(kv.second);
        }

        b.U32(0);
        CoATest::Deliver(CoA::Config::OnUpdateConfigs, 0x58D, b);
    }

    inline void Event(uint16_t id, bool active) {
        CoATest::Bytes b;
        uint8_t raw[0x34] = {};
        memcpy(raw, &id, 2);
        raw[0x13] = active ? 1 : 0;
        b.Raw(raw, sizeof(raw));
        CoATest::Deliver(CoA::GameEvents::OnGameEventInfo, 0x9BD, b);
    }

    // FUN_10143630(false), then an idle tick two seconds later.
    inline void Rebuild() {
        CoA::Challenge::ScheduleRebuild();
        C().now += 2;
        CoA::Lifecycle::RunAfterIdle();
    }

    inline CoATest::Bytes ActiveRecord(uint32_t id, uint32_t level, uint32_t deaths = 0, uint64_t start = 100) {
        CoATest::Bytes b;
        b.U32(7).U32(id).U32(level).U8(1).U64(start).U8(1).U32(deaths);
        return b;
    }

    inline void SetActive(const std::vector<std::pair<uint32_t, uint32_t>>& list) {
        CoATest::Bytes b;
        b.U32(static_cast<uint32_t>(list.size()));

        for (const auto& a : list) {
            b.Raw(ActiveRecord(a.first, a.second).data.data(), 0x1A);
        }

        Feed(0x596, b);
    }

    inline void SetCompleted(const std::vector<std::pair<uint32_t, uint32_t>>& list) {
        CoATest::Bytes b;
        b.U32(static_cast<uint32_t>(list.size()));

        for (const auto& c : list) {
            b.U32(7).U32(c.first).U32(c.second).U64(10).U64(20);
        }

        Feed(0x597, b);
    }

    // Every module the two managers read, reset; both inited; an empty table set; the fakes at their defaults.
    inline lua_State* Setup() {
        Tables().clear();
        C() = Client();
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::ClientHooks::Reset();
        CoA::ClientHooks::afterEnterWorld.Clear();
        CoA::AttachMisc::achievementsChanged.Clear();
        CoA::RealmInfo::Reset();
        CoA::Config::Reset();
        CoA::GameEvents::Reset();
        CoA::ObjectAddon::Reset();
        CoA::GameMode::Reset();
        CoA::GameMode::Init();
        CoA::Dbc::SetFileReader(ReadTables);
        Unload();
        Realm(2);
        CoA::Challenge::Reset();
        CoA::TrialCreator::Reset();
        CoA::Challenge::Init();
        CoA::TrialCreator::Init();
        InstallDeps();
        lua_State* L = CoATest::NewState(false);
        luaL_dostring(L, kJoin);
        return L;
    }

    // After the tables are filled: reload them and rebuild the data stores.
    inline void Load() {
        Unload();
        Rebuild();
    }

    inline void Done() {
        Mode(0);
        CoA::Challenge::Reset();
        CoA::TrialCreator::Reset();
        CoA::Config::Reset();
        CoA::GameEvents::Reset();
        CoA::RealmInfo::Reset();
        CoA::ObjectAddon::Reset();
        Tables().clear();
        Unload();
        CoA::Dbc::SetFileReader(nullptr);
    }
}

#endif
