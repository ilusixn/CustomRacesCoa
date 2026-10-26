// AscManastorm.cpp (see Manastorm.h): C_Manastorm, the Manastorm state machine, loadouts and reward visibility.
#include "CoA/Source/Modules/Manastorm.h"
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/Challenge.h"
#include "CoA/Source/Modules/Config.h"
#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/DbcPatch.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

struct CVar;

namespace {
    using CoA::Dbc::Table;
    using CoA::Script::CheckNumber;
    using CoA::Script::CheckString;
    using CoA::Script::PushBool;
    using CoA::Script::PushInt;
    using CoA::Script::PushNum;
    using CoA::Script::PushStr;
    using CoA::Script::ToInt;

    const char* const kTypes[] = {   // 0x10B45858
        "SOLO", "DUO", "TRIO", "GROUP", "SOLO_END_GAME", "DUO_END_GAME", "TRIO_END_GAME", "GROUP_END_GAME", "MAX",
    };
    const char* const kEnterResults[] = {   // 0x10B1D580
        "ENTER_MANASTORM_OK", "ENTER_MANASTORM_UNKNOWN", "ENTER_MANASTORM_ALREADY_ACTIVE",
        "ENTER_MANASTORM_TOO_LOW_COMPLETED_LEVEL", "ENTER_MANASTORM_BAD_LEVEL", "ENTER_MANASTORM_BAD_GROUP_LEVELS",
        "ENTER_MANASTORM_COOLDOWN", "ENTER_MANASTORM_GROUP_COOLDOWN", "ENTER_MANASTORM_HIGH_RISK",
        "ENTER_MANASTORM_GROUP_HIGH_RISK", "ENTER_MANASTORM_NO_MANASTORM_CHALLENGE",
        "ENTER_MANASTORM_GROUP_NO_MANASTORM_CHALLENGE", "ENTER_MANASTORM_ON_FLIGHT_PATH",
        "ENTER_MANASTORM_GROUP_ON_FLIGHT_PATH", "ENTER_MANASTORM_IN_PVP_COMBAT", "ENTER_MANASTORM_GROUP_IN_PVP_COMBAT",
        "ENTER_MANASTORM_IN_BATTLEGROUND_OR_ARENA", "ENTER_MANASTORM_GROUP_IN_BATTLEGROUND_OR_ARENA",
        "ENTER_MANASTORM_DUELING", "ENTER_MANASTORM_GROUP_DUELING", "ENTER_MANASTORM_TOO_LOW_PLAYER_LEVEL",
        "ENTER_MANASTORM_GROUP_TOO_LOW_PLAYER_LEVEL", "ENTER_MANASTORM_NOT_GROUP_LEADER",
        "ENTER_MANASTORM_END_GAME_NOT_ENABLED", "ENTER_MANASTORM_BAD_GROUP_SIZE",
    };
    const char* const kLeaveResults[] = {   // 0x10B1C490
        "LEAVE_MANASTORM_OK", "LEAVE_MANASTORM_UNKNOWN", "LEAVE_MANASTORM_NOT_ACTIVE", "LEAVE_MANASTORM_TRANSITION",
    };
    const char* const kLoadoutResults[] = {   // 0x10B205F8
        "SET_MANASTORM_LOADOUT_OK", "SET_MANASTORM_LOADOUT_UNKNOWN", "SET_MANASTORM_LOADOUT_OUT_OF_RANGE",
        "SET_MANASTORM_LOADOUT_BAD_SPELL", "SET_MANASTORM_LOADOUT_ACTIVE_MANASTORM", "SET_MANASTORM_LOADOUT_BAD_RANK",
        "SET_MANASTORM_LOADOUT_ALREADY_SET", "SET_MANASTORM_LOADOUT_NOT_KNOWN",
    };

    template <size_t N>
    int Index(const char* const (&table)[N], const std::string& s) {
        for (size_t i = 0; i < N; ++i) {
            if (s == table[i]) {
                return static_cast<int>(i);
            }
        }

        return -1;
    }

    template <size_t N>
    std::string Name(const char* const (&table)[N], uint32_t v) {
        return v < N ? std::string(table[v]) : "UNEXPECTED_ENUM_VALUE_" + std::to_string(v);
    }

    using Levels = std::vector<std::vector<uint32_t>>;   // per ActiveManastormType, ascending

    struct Reward {   // node +0xC (FUN_102a4cc0's record)
        uint32_t item = 0;
        uint8_t action = 0;   // 0 NONE, 1 HIDE, 2 LOCKED_RATE_LIMIT_EXHAUSTED
        uint32_t lockEnd = 0;
        uint32_t secondsRemaining = 0;
        uint32_t earned = 0;
        uint32_t max = 0;
        uint32_t remaining = 0;
        uint32_t windowStart = 0;
        uint32_t windowEnd = 0;
    };

    struct Mgr {
        std::vector<uint32_t> completed;                                              // +0x00
        uint32_t level = 0;                                                           // +0x0C
        uint32_t id = 0;                                                              // +0x10
        uint32_t type = 0;                                                            // +0x14
        uint32_t cacheA = 0;                                                          // +0x18
        float cacheB = 0.0f;                                                          // +0x1C
        uint32_t cacheC = 0;                                                          // +0x20
        Levels pending;                                                               // +0x24
        std::unordered_map<uint64_t, Levels> levels;                                  // +0x30
        uint32_t chaoticLink = 0;                                                     // +0x50
        uint32_t pendingLoadoutSpell = 0;                                             // +0x54
        std::vector<uint32_t> loadout;                                                // +0x58
        std::unordered_map<uint64_t, std::unordered_map<uint32_t, Reward>> rewards;   // +0x64
        bool pendingApply = false;                                                    // +0x84
    };

    CoA::Manastorm::Deps s_deps;
    Mgr s_mgr;
    int64_t s_objectiveIconTime = 0;   // 0x10BE31A0
    float s_iconScale = 1.0f;
    CVar* s_cullingCVar = nullptr;     // 0x10BE3190 (nothing reads it)
    std::unordered_map<uint32_t, std::set<uint32_t>> s_allowed;   // 0x10BE0658
    bool s_allowedBuilt = false;

    Table& ManastormTable() { return CoA::Dbc::Get("DBFilesClient\\Manastorm.dbc"); }
    Table& Modifiers() { return CoA::Dbc::Get("DBFilesClient\\ManastormModifiers.dbc"); }
    Table& GroupModifiers() { return CoA::Dbc::Get("DBFilesClient\\ManastormPlayerGroupModifiers.dbc"); }
    Table& Messages() { return CoA::Dbc::Get("DBFilesClient\\ManastormMessages.dbc"); }

    // ---- client state ----------------------------------------------------------------------------------------------
    bool HasPlayer() {
        return s_deps.hasPlayer && s_deps.hasPlayer();
    }

    uint64_t PlayerGuid() {
        return s_deps.playerGuid ? s_deps.playerGuid() : 0;
    }

    uint32_t PlayerLevel() {
        return s_deps.playerLevel ? s_deps.playerLevel() : 0;
    }

    bool PlayerHasAura(uint32_t spell) {
        return s_deps.playerHasAura && s_deps.playerHasAura(spell);
    }

    bool Knows(uint32_t spell) {
        return s_deps.knowsSpell && s_deps.knowsSpell(spell);
    }

    uint64_t PartyGuid(uint32_t i) {
        return s_deps.partyGuid ? s_deps.partyGuid(i) : 0;
    }

    uint32_t RaidCount() {
        return s_deps.raidCount ? s_deps.raidCount() : 0;
    }

    uint64_t RaidGuid(uint32_t i) {
        return s_deps.raidGuid ? s_deps.raidGuid(i) : 0;
    }

    int64_t Now() {
        return s_deps.now ? s_deps.now() : _time64(nullptr);
    }

    // FUN_102a4920: the player is at or above the server's max level (never below 60).
    bool EndGame() {
        if (!HasPlayer()) {
            return false;
        }

        return std::max<uint32_t>(CoA::GameEvents::ServerMaxLevel(), 0x3C) <= PlayerLevel();
    }

    // FUN_102a0f00: the raid members with a guid, else the party members, plus one.
    uint32_t GroupSize() {
        uint32_t n = 0;

        if (RaidCount() != 0) {
            for (uint32_t i = 0; i < RaidCount(); ++i) {
                if (RaidGuid(i) != 0) {
                    ++n;
                }
            }

            if (n != 0) {
                return n + 1;
            }
        }

        for (uint32_t i = 0; i < 4; ++i) {
            if (PartyGuid(i) != 0) {
                ++n;
            }
        }

        return n + 1;
    }

    uint32_t GroupTypeIndex() {
        switch (GroupSize()) {
            case 1: return 0;
            case 2: return 1;
            case 3: return 2;
            default: return 3;
        }
    }

    // FUN_10216470: the highest rank of the spell's chain the player knows; the first rank otherwise.
    uint32_t HighestKnownRank(uint32_t spell) {
        const uint32_t first = s_deps.firstRank ? s_deps.firstRank(spell) : spell;

        if (!HasPlayer()) {
            return first;
        }

        const std::vector<uint32_t>* chain = s_deps.rankChain ? s_deps.rankChain(first) : nullptr;

        if (chain) {
            for (auto it = chain->rbegin(); it != chain->rend(); ++it) {
                if (Knows(*it)) {
                    return *it;
                }
            }
        }

        return first;
    }

    // ---- completed levels ------------------------------------------------------------------------------------------
    uint32_t Highest(const Levels& v, uint32_t i) {
        return i < v.size() && !v[i].empty() ? v[i].back() : 0;
    }

    // FUN_102a0f60: over every type (all), of type `index` (specific), or over the four normal / end-game types.
    uint32_t MaxCompleted(const Levels& v, uint32_t index, bool specific, bool endGame, bool all) {
        if (!all && specific) {
            return Highest(v, index);
        }

        uint32_t best = 0;
        const uint32_t from = all ? 0 : endGame ? 4 : 0;
        const uint32_t to = all ? 9 : from + 4;   // DAT_10B45900 = 0..8

        for (uint32_t i = from; i < to; ++i) {
            best = std::max(best, Highest(v, i));
        }

        return best;
    }

    uint32_t MaxOfPair(const Levels& v, uint32_t a, uint32_t b) {   // FUN_102a08f0
        return std::max(Highest(v, a), Highest(v, b));
    }

    // FUN_102a1170: the lowest of the player's and every known group member's highest completed level. A member the
    // map does not know is skipped; the player missing from the map counts as 0.
    uint32_t GroupMin(uint32_t index, bool specific, bool endGame, bool all) {
        auto own = s_mgr.levels.find(PlayerGuid());
        uint32_t v = own == s_mgr.levels.end() ? 0 : MaxCompleted(own->second, index, specific, endGame, all);

        auto member = [&](uint64_t guid) {
            auto it = s_mgr.levels.find(guid);

            if (it != s_mgr.levels.end()) {
                v = std::min(v, MaxCompleted(it->second, index, specific, endGame, all));
            }
        };

        for (uint32_t i = 0; i < 4; ++i) {
            member(PartyGuid(i));
        }

        for (uint32_t i = 0; i < RaidCount(); ++i) {
            member(RaidGuid(i));
        }

        return v;
    }

    // FUN_102a1440: the guids to blame for TOO_LOW_COMPLETED_LEVEL. Each test is the GROUP minimum, not the member's
    // own, so once the group falls short every known member is listed.
    std::vector<uint64_t> BelowRequirement(uint32_t level, bool specific, bool endGame, bool all) {
        std::vector<uint64_t> out;

        if (level == 0) {
            return out;
        }

        const uint32_t threshold = level - 1;

        if (GroupMin(GroupTypeIndex(), specific, endGame, all) < threshold) {
            out.push_back(PlayerGuid());
        }

        auto member = [&](uint64_t guid) {
            if (s_mgr.levels.count(guid) && GroupMin(GroupTypeIndex(), specific, endGame, all) < threshold) {
                out.push_back(guid);
            }
        };

        for (uint32_t i = 0; i < 4; ++i) {
            member(PartyGuid(i));
        }

        for (uint32_t i = 0; i < RaidCount(); ++i) {
            member(RaidGuid(i));
        }

        return out;
    }

    // Map 0x10BE0658 (FUN_10211020's set, built inside CustomDBCMgr FUN_101d3d20 at 0x101D9F17): every
    // ManastormModifiers row with +0x38 set adds +8 under +4. Built once.
    void BuildAllowedLevels() {
        if (s_allowedBuilt) {
            return;
        }

        s_allowedBuilt = true;
        Table& t = Modifiers();

        for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
            if (const uint8_t* r = t.Row(id)) {
                if (Table::U32(r, 0x38) != 0) {
                    s_allowed[Table::U32(r, 4)].insert(Table::U32(r, 8));
                }
            }
        }
    }

    bool LevelAllowed(uint32_t key, uint32_t level) {   // FUN_10211020
        BuildAllowedLevels();
        auto it = s_allowed.find(key);
        return it != s_allowed.end() && it->second.count(level) != 0;
    }

    uint32_t LoadoutsMax() {   // FUN_102a1330
        const int32_t* v = CoA::Config::Int("CONFIG_MANASTORM_LOADOUTS_MAX");
        return v ? static_cast<uint32_t>(*v) : 4;
    }

    // ---- validators ------------------------------------------------------------------------------------------------
    // FUN_102a09a0: why the player cannot enter `level` (ENTER_MANASTORM_* indexes).
    std::vector<uint32_t> EnterErrors(uint32_t level) {
        std::vector<uint32_t> e;

        if (s_mgr.level != 0) {
            e.push_back(2);
        }

        const bool endGame = EndGame();

        if (level != 0 && GroupMin(GroupTypeIndex(), false, endGame, false) < level - 1) {
            e.push_back(3);
        }

        if (!LevelAllowed(HasPlayer() ? (EndGame() ? 2 : 0) : 0, level)) {
            e.push_back(4);
        }

        if (HasPlayer() && PlayerHasAura(0x16C7E)) {
            e.push_back(6);
        }

        if (CoA::Challenge::ActiveHasRule(0x6C)) {
            e.push_back(10);
        }

        if (HasPlayer() && PlayerLevel() < 10) {
            e.push_back(0x14);
        }

        return e;
    }

    // FUN_102a0b80: why `spell` cannot go into loadout slot `slot` (0-based). The range test is `max < slot`, so slot
    // == max passes.
    std::vector<uint32_t> LoadoutErrors(uint32_t slot, uint32_t spell) {
        std::vector<uint32_t> e;

        if (LoadoutsMax() < slot) {
            e.push_back(2);
        }

        const uint8_t* attr = CoA::CA::SpellCustomAttrRow(spell);

        if (!attr || !(attr[0x14] & 0x80)) {
            e.push_back(3);
        }

        if (s_mgr.level != 0) {
            e.push_back(4);
        }

        if (HighestKnownRank(spell) != spell) {
            e.push_back(5);
        }

        if (std::find(s_mgr.loadout.begin(), s_mgr.loadout.end(), spell) != s_mgr.loadout.end()) {
            e.push_back(6);
        }

        if (HasPlayer() && !Knows(spell)) {
            e.push_back(7);
        }

        return e;
    }

    // FUN_102a0200 / FUN_1029fe20 / FUN_102a0010: the codes as an array of names.
    template <size_t N>
    void PushErrors(lua_State* L, const std::vector<uint32_t>& e, const char* const (&table)[N]) {
        lua_createtable(L, 0, static_cast<int>(e.size()));
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < e.size(); ++i) {
            PushNum(L, static_cast<double>(i + 1));
            PushStr(L, Name(table, e[i]).c_str());
            lua_settable(L, -3);
        }
    }

    void PushIntArray(lua_State* L, const std::vector<uint32_t>& v) {   // FUN_1009a460
        lua_createtable(L, 0, static_cast<int>(v.size()));
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < v.size(); ++i) {
            PushNum(L, static_cast<double>(i + 1));
            PushInt(L, static_cast<int32_t>(v[i]));
            lua_settable(L, -3);
        }
    }

    // ---- client patches --------------------------------------------------------------------------------------------
    void SetObjectiveIconScale(float scale) {   // FUN_102a6c10
        s_iconScale = scale;

        if (s_deps.setObjectiveIconScale) {
            s_deps.setObjectiveIconScale(scale);
        }
    }

    void SetSpellRangeFlags(uint32_t flags) {   // FUN_100b2590 rows 0x72 / 0x1BD, +0x14
        if (s_deps.setSpellRangeFlags) {
            s_deps.setSpellRangeFlags(flags);
        }
    }

    // u32 n, then n lists of u32 m + m x u32; each list sorted. The counts are unchecked in the original; here they
    // stop when the packet is used up (a list count is capped by the bytes left so a short packet cannot allocate).
    void ReadLevels(CDataStore* msg, Levels& v) {
        const uint32_t n = CoA::Read::U32(msg);
        v.assign(std::min<uint32_t>(n, CoA::Read::Remaining(msg) / 4), {});

        for (uint32_t i = 0; i < v.size(); ++i) {
            for (uint32_t m = CoA::Read::U32(msg); m && CoA::Read::Remaining(msg) != 0; --m) {
                v[i].push_back(CoA::Read::U32(msg));
            }
        }

        for (auto& l : v) {
            std::sort(l.begin(), l.end());
        }
    }

    void SetSlot(uint32_t slot, uint32_t spell) {
        if (s_mgr.loadout.size() <= slot) {
            s_mgr.loadout.resize(static_cast<size_t>(slot) + 1, 0);
        }

        s_mgr.loadout[slot] = spell;
    }

    const Reward* FindReward(uint64_t guid, uint32_t item) {   // FUN_102a1960
        auto g = s_mgr.rewards.find(guid);

        if (g == s_mgr.rewards.end()) {
            return nullptr;
        }

        auto it = g->second.find(item);
        return it == g->second.end() ? nullptr : &it->second;
    }

    // ---- bindings --------------------------------------------------------------------------------------------------
    int Enter(lua_State* L) {   // FUN_102a68f0: CMSG 0x651 {u32 level}
        uint32_t level;

        if (!CoA::Script::ReadNumber(L, level)) {
            return 0;
        }

        CoA::Packet(0x651).U32(level).Send();
        PushBool(L, true);
        return 1;
    }

    int Leave(lua_State* L) {   // FUN_102a69f0: CMSG 0x665
        CoA::Packet(0x665).Send();
        PushBool(L, true);
        return 1;
    }

    // FUN_102a6380: ok, the errors (nil when none), and for TOO_LOW_COMPLETED_LEVEL the unit token of each member to
    // blame -- "player", else the first raid*, else the first party* token (nil when none).
    int CanEnter(lua_State* L) {
        uint32_t level;

        if (!CoA::Script::ReadNumber(L, level)) {
            return 0;
        }

        const std::vector<uint32_t> e = EnterErrors(level);
        PushBool(L, e.empty());

        if (e.empty()) {
            lua_pushnil(L);
        } else {
            PushErrors(L, e, kEnterResults);
        }

        if (std::find(e.begin(), e.end(), 3u) == e.end()) {
            lua_pushnil(L);
            return 3;
        }

        std::vector<std::string> tokens;

        for (uint64_t guid : BelowRequirement(level, false, EndGame(), false)) {
            std::vector<std::string> list;

            if (s_deps.unitTokens) {
                s_deps.unitTokens(guid, list);
            }

            const std::string* pick = nullptr;

            for (const std::string& t : list) {
                if (!pick && t == "player") {
                    pick = &t;
                }
            }

            for (const std::string& t : list) {
                if (!pick && strncmp(t.c_str(), "raid", 4) == 0) {
                    pick = &t;
                }
            }

            for (const std::string& t : list) {
                if (!pick && strncmp(t.c_str(), "party", 5) == 0) {
                    pick = &t;
                }
            }

            if (pick) {
                tokens.push_back(*pick);
            }
        }

        if (tokens.empty()) {
            lua_pushnil(L);
            return 3;
        }

        lua_createtable(L, 0, static_cast<int>(tokens.size()));   // FUN_100beb90
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < tokens.size(); ++i) {
            PushNum(L, static_cast<double>(i + 1));
            PushStr(L, tokens[i].c_str());
            lua_settable(L, -3);
        }

        return 3;
    }

    int CanLeave(lua_State* L) {   // FUN_102a6790
        std::vector<uint32_t> e;

        if (s_mgr.id == 0) {
            e.push_back(2);
        }

        if (HasPlayer() && PlayerHasAura(0x16C78)) {
            e.push_back(3);
        }

        PushBool(L, e.empty());

        if (e.empty()) {
            lua_pushnil(L);
        } else {
            PushErrors(L, e, kLeaveResults);
        }

        return 2;
    }

    // FUN_102a5690: 1, 6, 11, ... up to the group's highest completed level + 1 (at least 1).
    int GetEnterableLevels(lua_State* L) {
        uint32_t top = GroupMin(GroupTypeIndex(), false, EndGame(), false) + 1;

        if (top == 0) {
            top = 1;
        }

        std::vector<uint32_t> levels;
        uint32_t level = 1;

        do {
            levels.push_back(level);
            level += 5;
        } while (level <= top);

        std::sort(levels.begin(), levels.end());
        PushIntArray(L, levels);
        return 1;
    }

    int IsInManastorm(lua_State* L) {   // FUN_102a6280
        PushBool(L, s_mgr.level != 0);
        return 1;
    }

    int GetActiveLevel(lua_State* L) {   // FUN_102a5080
        PushInt(L, static_cast<int32_t>(s_mgr.level));
        return 1;
    }

    int GetActiveManastormID(lua_State* L) {   // FUN_102a50a0
        PushInt(L, static_cast<int32_t>(s_mgr.id));
        return 1;
    }

    int GetActiveManastormType(lua_State* L) {   // FUN_102a50c0
        PushStr(L, Name(kTypes, s_mgr.type).c_str());
        return 1;
    }

    // FUN_102a55b0: the boss (Manastorm +0xC, 0 without a row), the chaotic link, then spell 93459's
    // (EffectBasePoints[0] + 1) * link and (EffectBasePoints[1] + 1) * link (0, 0 without the spell).
    int GetBoss(lua_State* L) {
        const uint8_t* ms = ManastormTable().Row(s_mgr.id);
        PushInt(L, ms ? Table::I32(ms, 0xC) : 0);
        const int32_t link = static_cast<int32_t>(s_mgr.chaoticLink);
        PushInt(L, link);
        int32_t a = 0;
        int32_t b = 0;

        if (!s_deps.spellBasePoints || !s_deps.spellBasePoints(0x16D13, a, b)) {
            PushInt(L, 0);
            PushInt(L, 0);
            return 4;
        }

        PushInt(L, (a + 1) * link);
        PushInt(L, (b + 1) * link);
        return 4;
    }

    // FUN_102a5a70 GetMaxCompletedLevels(unit): all types, then solo / duo / trio / group each with its end-game twin;
    // nil for a unit the map does not know.
    int GetMaxCompletedLevels(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TSTRING })) {
            return 0;
        }

        uint64_t guid = 0;

        if (s_deps.unitTokenGuid) {
            s_deps.unitTokenGuid(CheckString(L, 1), guid);
        }

        auto it = s_mgr.levels.find(guid);

        if (it == s_mgr.levels.end()) {
            lua_pushnil(L);
            return 1;
        }

        const Levels& v = it->second;
        PushInt(L, static_cast<int32_t>(MaxCompleted(v, 0, false, false, true)));
        PushInt(L, static_cast<int32_t>(MaxOfPair(v, 0, 4)));
        PushInt(L, static_cast<int32_t>(MaxOfPair(v, 1, 5)));
        PushInt(L, static_cast<int32_t>(MaxOfPair(v, 2, 6)));
        PushInt(L, static_cast<int32_t>(MaxOfPair(v, 3, 7)));
        return 5;
    }

    // FUN_102a58d0: the first ManastormModifiers row for (a, b): +0x2C and +0x34; nil, nil without one.
    int GetExperienceModifier(lua_State* L) {
        int32_t a;
        int32_t b;

        if (!CoA::Script::ReadInt2(L, a, b)) {
            return 0;
        }

        Table& t = Modifiers();

        for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
            const uint8_t* r = t.Row(id);

            if (r && Table::I32(r, 4) == a && Table::I32(r, 8) == b) {
                PushNum(L, static_cast<double>(Table::F32(r, 0x2C)));
                PushNum(L, static_cast<double>(Table::F32(r, 0x34)));
                return 2;
            }
        }

        return CoA::Script::PushNils(L, 2);
    }

    // FUN_102a6180: the active Manastorm's modifier row (+4 = Manastorm +8, +8 = the active level): +0x2C * Manastorm
    // +0x18 and +0x34 * Manastorm +0x20.
    int GetStageBonusExperience(lua_State* L) {
        if (const uint8_t* ms = ManastormTable().Row(s_mgr.id)) {
            Table& t = Modifiers();

            for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
                const uint8_t* r = t.Row(id);

                if (r && Table::U32(r, 4) == Table::U32(ms, 8) && Table::U32(r, 8) == s_mgr.level) {
                    PushNum(L, static_cast<double>(Table::F32(r, 0x2C) * Table::F32(ms, 0x18)));
                    PushNum(L, static_cast<double>(Table::F32(r, 0x34) * Table::F32(ms, 0x20)));
                    return 2;
                }
            }
        }

        return CoA::Script::PushNils(L, 2);
    }

    // FUN_102a5dc0 GetRewardModifier(id): Manastorm +0x10 / +0x14 (1.0 without a row), the group modifier -- the
    // ManastormPlayerGroupModifiers row with the largest size (+4) not above the group and, among those, the largest
    // +8 below 2 -- and the two products.
    int GetRewardModifier(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        float a = 1.0f;
        float b = 1.0f;
        float c = 1.0f;
        float d = 1.0f;

        if (const uint8_t* ms = ManastormTable().Row(id)) {
            a = Table::F32(ms, 0x10);
            b = Table::F32(ms, 0x14);
        }

        const uint32_t size = GroupSize();
        uint32_t bestSize = 0;
        uint32_t bestKind = 0;
        Table& t = GroupModifiers();

        for (uint32_t i = t.MinId(); t.Loaded() && i <= t.MaxId(); ++i) {
            const uint8_t* r = t.Row(i);

            if (!r) {
                continue;
            }

            const uint32_t s = Table::U32(r, 4);
            const uint32_t k = Table::U32(r, 8);

            if (s <= size && bestSize <= s && k < 2 && bestKind <= k) {
                c = Table::F32(r, 0xC);
                d = Table::F32(r, 0x10);
                bestSize = s;
                bestKind = k;
            }
        }

        PushNum(L, static_cast<double>(a));
        PushNum(L, static_cast<double>(b));
        PushNum(L, static_cast<double>(c));
        PushNum(L, static_cast<double>(d));
        PushNum(L, static_cast<double>(c * a));
        PushNum(L, static_cast<double>(d * b));
        return 6;
    }

    int ShowObjectiveIcon(lua_State*) {   // FUN_102a6bf0
        s_objectiveIconTime = Now() + 10;
        return 0;
    }

    int GetManastormCacheInfo(lua_State* L) {   // FUN_102a5a20
        PushInt(L, static_cast<int32_t>(s_mgr.cacheA));
        PushNum(L, static_cast<double>(s_mgr.cacheB));
        PushInt(L, static_cast<int32_t>(s_mgr.cacheC));
        return 3;
    }

    int GetNumLoadoutSlots(lua_State* L) {   // FUN_102a5ce0
        PushInt(L, static_cast<int32_t>(LoadoutsMax()));
        return 1;
    }

    int GetLoadoutSpellAtIndex(lua_State* L) {   // FUN_102a59b0
        uint32_t index;

        if (!CoA::Script::ReadNumber(L, index) || index == 0) {
            return 0;
        }

        if (index - 1 < s_mgr.loadout.size() && s_mgr.loadout[index - 1] != 0) {
            PushInt(L, static_cast<int32_t>(s_mgr.loadout[index - 1]));
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int SetLoadoutSpellAtIndex(lua_State* L) {   // FUN_102a6ac0: CMSG 0x689 {u32 slot, u32 spell}
        int32_t index;
        int32_t spell;

        if (!CoA::Script::ReadInt2(L, index, spell) || index == 0) {
            return 0;
        }

        CoA::Packet(0x689).U32(static_cast<uint32_t>(index - 1)).U32(static_cast<uint32_t>(spell)).Send();
        s_mgr.pendingLoadoutSpell = static_cast<uint32_t>(spell);
        PushBool(L, true);
        return 1;
    }

    int CanSetLoadoutSpellAtIndex(lua_State* L) {   // FUN_102a4f60
        int32_t index;
        int32_t spell;

        if (!CoA::Script::ReadInt2(L, index, spell) || index == 0) {
            return 0;
        }

        const std::vector<uint32_t> e = LoadoutErrors(static_cast<uint32_t>(index - 1), static_cast<uint32_t>(spell));
        PushBool(L, e.empty());

        if (e.empty()) {
            lua_pushnil(L);
        } else {
            PushErrors(L, e, kLoadoutResults);
        }

        return 2;
    }

    // FUN_102a51f0: every SpellCustomAttr row flagged 0x80 (+0x14) whose spell is the highest rank the player knows:
    // first the ones some slot accepts, then the rest.
    int GetAvailableLoadoutSpells(lua_State* L) {
        std::vector<uint32_t> settable;
        std::vector<uint32_t> other;
        Table& t = CoA::Dbc::Get("DBFilesClient\\SpellCustomAttr.dbc");

        for (uint32_t i = 0; i < t.Count(); ++i) {
            const uint8_t* r = t.RowAt(i);

            if (!r || !(r[0x14] & 0x80)) {
                continue;
            }

            const uint32_t spell = Table::U32(r, 4);

            if (HighestKnownRank(spell) != spell) {
                continue;
            }

            bool placed = false;

            for (uint32_t slot = 0; slot < LoadoutsMax() && !placed; ++slot) {
                placed = LoadoutErrors(slot, spell).empty();
            }

            (placed ? settable : other).push_back(spell);
        }

        PushIntArray(L, settable);
        PushIntArray(L, other);
        return 2;
    }

    void PushReward(lua_State* L, uint64_t guid, const Reward& r) {   // FUN_102a4cc0
        lua_createtable(L, 0, 0xE);
        lua_checkstack(L, 2);

        auto optional = [&](const char* k, uint32_t v) {
            PushStr(L, k);

            if (v == 0) {
                lua_pushnil(L);
            } else {
                PushInt(L, static_cast<int32_t>(v));
            }

            lua_settable(L, -3);
        };

        auto flag = [&](const char* k, bool v) {
            PushStr(L, k);
            PushBool(L, v);
            lua_settable(L, -3);
        };

        auto integer = [&](const char* k, uint32_t v) {
            PushStr(L, k);
            PushInt(L, static_cast<int32_t>(v));
            lua_settable(L, -3);
        };

        PushStr(L, "context");
        PushStr(L, std::to_string(guid).c_str());
        lua_settable(L, -3);
        integer("itemID", r.item);
        PushStr(L, "action");
        PushStr(L, r.action == 1 ? "HIDE" : r.action == 2 ? "LOCKED_RATE_LIMIT_EXHAUSTED" : "NONE");
        lua_settable(L, -3);
        flag("isHidden", r.action == 1);
        flag("isLocked", r.action == 2);
        optional("lockEndTimestamp", r.lockEnd);
        optional("secondsRemaining", r.secondsRemaining);
        integer("earnedQuantity", r.earned);
        integer("maxQuantity", r.max);
        integer("remainingQuantity", r.remaining);
        optional("windowStart", r.windowStart);
        optional("windowEnd", r.windowEnd);
    }

    // FUN_102a5fa0 GetRewardVisibility(guid[, item]): every item's table keyed by item id, or the one item (nil).
    int GetRewardVisibility(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER })) {
            return 0;
        }

        const uint64_t guid = static_cast<uint64_t>(CheckNumber(L, 1));

        if (lua_type(L, 2) < 1) {
            auto g = s_mgr.rewards.find(guid);

            if (g == s_mgr.rewards.end()) {
                lua_createtable(L, 0, 0);
                return 1;
            }

            lua_createtable(L, 0, static_cast<int>(g->second.size()));
            lua_checkstack(L, 2);

            for (const auto& e : g->second) {
                PushInt(L, static_cast<int32_t>(e.first));
                PushReward(L, guid, e.second);
                lua_settable(L, -3);
            }

            return 1;
        }

        const uint32_t item = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));

        if (const Reward* r = FindReward(guid, item)) {
            PushReward(L, guid, *r);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    bool ReadGuidItem(lua_State* L, uint64_t& guid, uint32_t& item) {   // FUN_1029f060
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TNUMBER })) {
            return false;
        }

        guid = static_cast<uint64_t>(CheckNumber(L, 1));
        item = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        return true;
    }

    // FUN_102a62b0: true, lock end, seconds remaining (nil for 0) when locked; else false, nil, nil.
    int IsRewardLocked(lua_State* L) {
        uint64_t guid;
        uint32_t item;

        if (!ReadGuidItem(L, guid, item)) {
            return 0;
        }

        const Reward* r = FindReward(guid, item);

        if (!r || r->action != 2) {
            PushBool(L, false);
            return 1 + CoA::Script::PushNils(L, 2);
        }

        PushBool(L, true);

        if (r->lockEnd == 0) {
            lua_pushnil(L);
        } else {
            PushInt(L, static_cast<int32_t>(r->lockEnd));
        }

        if (r->secondsRemaining == 0) {
            lua_pushnil(L);
        } else {
            PushInt(L, static_cast<int32_t>(r->secondsRemaining));
        }

        return 3;
    }

    // FUN_102a5d00: earned, max, remaining, seconds remaining (nil if 0), locked -- or nil without a limit (max 0).
    int GetRewardLimitProgress(lua_State* L) {
        uint64_t guid;
        uint32_t item;

        if (!ReadGuidItem(L, guid, item)) {
            return 0;
        }

        const Reward* r = FindReward(guid, item);

        if (!r || r->max == 0) {
            lua_pushnil(L);
            return 1;
        }

        PushInt(L, static_cast<int32_t>(r->earned));
        PushInt(L, static_cast<int32_t>(r->max));
        PushInt(L, static_cast<int32_t>(r->remaining));

        if (r->secondsRemaining == 0) {
            lua_pushnil(L);
        } else {
            PushInt(L, static_cast<int32_t>(r->secondsRemaining));
        }

        PushBool(L, r->action == 2);
        return 5;
    }

    bool AcceptAnyValue(CVar*, const char*, const char*, void*) {   // 0x102A0990
        return true;
    }

    const CoA::Binding s_bindings[] = {   // registrar 0x102A7FE0, in its order
        COA_NATIVE("C_Manastorm", "Enter", Enter),
        COA_NATIVE("C_Manastorm", "Leave", Leave),
        COA_NATIVE("C_Manastorm", "CanEnter", CanEnter),
        COA_NATIVE("C_Manastorm", "CanLeave", CanLeave),
        COA_NATIVE("C_Manastorm", "GetEnterableLevels", GetEnterableLevels),
        COA_NATIVE("C_Manastorm", "IsInManastorm", IsInManastorm),
        COA_NATIVE("C_Manastorm", "GetActiveLevel", GetActiveLevel),
        COA_NATIVE("C_Manastorm", "GetActiveManastormID", GetActiveManastormID),
        COA_NATIVE("C_Manastorm", "GetBoss", GetBoss),
        COA_NATIVE("C_Manastorm", "GetMaxCompletedLevels", GetMaxCompletedLevels),
        COA_NATIVE("C_Manastorm", "GetActiveManastormType", GetActiveManastormType),
        COA_NATIVE("C_Manastorm", "GetExperienceModifier", GetExperienceModifier),
        COA_NATIVE("C_Manastorm", "GetStageBonusExperience", GetStageBonusExperience),
        COA_NATIVE("C_Manastorm", "GetRewardModifier", GetRewardModifier),
        COA_NATIVE("C_Manastorm", "ShowObjectiveIcon", ShowObjectiveIcon),
        COA_NATIVE("C_Manastorm", "GetManastormCacheInfo", GetManastormCacheInfo),
        COA_NATIVE("C_Manastorm", "GetNumLoadoutSlots", GetNumLoadoutSlots),
        COA_NATIVE("C_Manastorm", "GetLoadoutSpellAtIndex", GetLoadoutSpellAtIndex),
        COA_NATIVE("C_Manastorm", "SetLoadoutSpellAtIndex", SetLoadoutSpellAtIndex),
        COA_NATIVE("C_Manastorm", "CanSetLoadoutSpellAtIndex", CanSetLoadoutSpellAtIndex),
        COA_NATIVE("C_Manastorm", "GetAvailableLoadoutSpells", GetAvailableLoadoutSpells),
        COA_NATIVE("C_Manastorm", "GetRewardVisibility", GetRewardVisibility),
        COA_NATIVE("C_Manastorm", "IsRewardLocked", IsRewardLocked),
        COA_NATIVE("C_Manastorm", "GetRewardLimitProgress", GetRewardLimitProgress),
    };
}

CoA::Manastorm::Deps& CoA::Manastorm::Dependencies() {
    return s_deps;
}

bool CoA::Manastorm::InManastorm() {
    return s_mgr.level != 0;
}

float CoA::Manastorm::ObjectiveIconScale() {
    return s_iconScale;
}

const std::vector<uint32_t>& CoA::Manastorm::CompletedThisRun() {
    return s_mgr.completed;
}

// ---- packets (the reads are unchecked in the original; a short packet reads zeros -- CoA::Read) --------------------
int32_t CoA::Manastorm::OnEnterResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_102a1a10
    const std::string result = CoA::Read::Str(msg);
    const int i = Index(kEnterResults, result);

    if (i < 0) {
        CoA::Log::Printf("Unexpected Value: %s", result.c_str());
    } else if (i == 0) {
        s_mgr.completed.clear();
    }

    CoA::Events::Signal("ENTER_MANASTORM_RESULT", "%s", result.c_str());
    return 1;
}

int32_t CoA::Manastorm::OnLeaveResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_102a1d40
    const std::string result = CoA::Read::Str(msg);
    CoA::Events::Signal("LEAVE_MANASTORM_RESULT", "%s", result.c_str());
    return 1;
}

int32_t CoA::Manastorm::OnActiveData(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_102a1e90
    const uint32_t oldLevel = s_mgr.level;
    const uint32_t oldA = s_mgr.cacheA;
    const float oldB = s_mgr.cacheB;
    const uint32_t oldC = s_mgr.cacheC;
    s_mgr.level = CoA::Read::U32(msg);
    s_mgr.id = CoA::Read::U32(msg);
    const std::string type = CoA::Read::Str(msg);
    int t = Index(kTypes, type);

    if (t < 0) {
        CoA::Log::Printf("Unexpected Value: %s", type.c_str());
        t = 8;
    }

    s_mgr.type = static_cast<uint32_t>(t);

    if (s_mgr.type == 8) {
        CoA::Log::Printf("ManastormMgr::HandleManastormActiveDataOpcode: Unknown ActiveManastormType: %s", type.c_str());
    }

    s_mgr.cacheA = CoA::Read::U32(msg);
    s_mgr.cacheB = CoA::Read::F32(msg);
    s_mgr.cacheC = CoA::Read::U32(msg);

    if (s_mgr.level != oldLevel) {
        CoA::Events::Signal("ACTIVE_MANASTORM_UPDATED", "%u%u", oldLevel, s_mgr.level);
    }

    if (s_mgr.cacheA != oldA || std::fabs(s_mgr.cacheB - oldB) > 0.001f || s_mgr.cacheC != oldC) {
        CoA::Events::Signal("MANASTORM_CACHE_INFO_UPDATED", "%u%f%u", s_mgr.cacheA, static_cast<double>(s_mgr.cacheB), s_mgr.cacheC);
    }

    if (oldLevel == 0) {
        if (s_mgr.level != 0) {
            CoA::ClientHooks::manastormSkipsModalSpell = true;   // 0x80DB37 = 0xEB
            SetSpellRangeFlags(0);
        }
    } else if (s_mgr.level == 0) {
        SetObjectiveIconScale(1.0f);
        CoA::ClientHooks::manastormSkipsModalSpell = false;      // 0x80DB37 = 0x75
        SetSpellRangeFlags(2);
    }

    return 1;
}

int32_t CoA::Manastorm::OnData(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_102a2cf0
    Levels* target;

    if (!HasPlayer()) {
        target = &s_mgr.pending;
        s_mgr.pendingApply = true;
    } else {
        target = &s_mgr.levels[PlayerGuid()];
    }

    ReadLevels(msg, *target);
    return 1;
}

int32_t CoA::Manastorm::OnUpdateMaxCompletedLevel(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_102a36f0
    const uint64_t guid = CoA::Read::U64(msg);
    Levels& entry = s_mgr.levels[guid];
    const Levels old = entry;
    ReadLevels(msg, entry);

    if (HasPlayer() && PlayerGuid() == guid && old != entry) {
        CoA::Events::Signal("MANASTORM_COMPLETED_LEVELS_UPDATED", nullptr);
    }

    return 1;
}

int32_t CoA::Manastorm::OnCompletedLevel(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_102a2990
    const uint32_t level = CoA::Read::U32(msg);
    s_mgr.completed.push_back(level);
    s_mgr.chaoticLink = 0;
    CoA::Events::Signal("MANASTORM_LEVEL_COMPLETED", "%u", level);
    // Every message row for the instance difficulty and level whose game event (+0xC, u16) is unset or active.
    Table& t = Messages();
    const uint32_t difficulty = s_deps.instanceDifficulty ? s_deps.instanceDifficulty() : 0;

    for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
        const uint8_t* r = t.Row(id);

        if (!r || Table::U32(r, 4) != difficulty || Table::U32(r, 8) != level) {
            continue;
        }

        if (Table::U32(r, 0xC) != 0 && !CoA::GameEvents::IsActive(static_cast<uint16_t>(Table::U32(r, 0xC)))) {
            continue;
        }

        CoA::Events::Signal("MANASTORM_LEVEL_UNLOCKED", "%s%s%s", t.Str(r, 0x10), t.Str(r, 0x14), t.Str(r, 0x18));
    }

    return 1;
}

int32_t CoA::Manastorm::OnFail(void*, uint32_t, uint32_t, CDataStore*) {   // FUN_102a2f40
    CoA::Events::Signal("MANASTORM_FAILED", nullptr);
    return 1;
}

int32_t CoA::Manastorm::OnChaoticLink(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_102a28d0
    const uint32_t old = s_mgr.chaoticLink;
    s_mgr.chaoticLink = CoA::Read::U32(msg);

    if (s_mgr.chaoticLink != old) {
        CoA::Events::Signal("MANASTORM_CHAOTIC_LINK_UPDATED", "%u%u", old, s_mgr.chaoticLink);
    }

    return 1;
}

// FUN_102a3040: u32 n, n x u32 (the count capped by the bytes left).
int32_t CoA::Manastorm::OnLoadout(void*, uint32_t, uint32_t, CDataStore* msg) {
    const uint32_t n = CoA::Read::U32(msg);
    s_mgr.loadout.assign(std::min<uint32_t>(n, CoA::Read::Remaining(msg) / 4), 0);

    for (uint32_t& s : s_mgr.loadout) {
        s = CoA::Read::U32(msg);
    }

    return 1;
}

int32_t CoA::Manastorm::OnLoadoutResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_102a4080
    const uint32_t slot = CoA::Read::U32(msg);
    const std::string result = CoA::Read::Str(msg);
    const int i = Index(kLoadoutResults, result);

    if (i < 0) {
        CoA::Log::Printf("Unexpected Value: %s", result.c_str());
    } else if (i == 0) {
        SetSlot(slot, s_mgr.pendingLoadoutSpell);
        s_mgr.pendingLoadoutSpell = 0;
    }

    CoA::Events::Signal("MANASTORM_LOADOUT_RESULT", "%u%s", slot + 1, result.c_str());
    return 1;
}

int32_t CoA::Manastorm::OnLoadoutUpdate(void*, uint32_t, uint32_t, CDataStore* msg) {   // FUN_102a4460
    const uint32_t slot = CoA::Read::U32(msg);
    const uint32_t spell = CoA::Read::U32(msg);
    SetSlot(slot, spell);
    CoA::Events::Signal("MANASTORM_LOADOUT_UPDATE", "%u%u", slot + 1, spell);
    return 1;
}

// FUN_102a30c0: u64 guid, u32 n, n x 0x21 {u32 item, u8 action, u32 lockEnd, u32 secondsRemaining, u32 earned, u32 max,
// u32 remaining, u32 windowStart, u32 windowEnd}. Every read is bounds-checked and a short packet drops everything.
// Actions above 2 are skipped; an empty result erases the guid's entry; later records of an item replace earlier ones.
int32_t CoA::Manastorm::OnRewardVisibility(void*, uint32_t, uint32_t, CDataStore* msg) {
    if (CoA::Read::Remaining(msg) < 8) {
        return 1;
    }

    const uint64_t guid = CoA::Read::U64(msg);

    if (CoA::Read::Remaining(msg) < 4) {
        return 1;
    }

    const uint32_t n = CoA::Read::U32(msg);

    if (n != 0 && CoA::Read::Remaining(msg) / 0x21 < n) {
        return 1;
    }

    std::unordered_map<uint32_t, Reward> fresh;

    for (uint32_t i = 0; i < n; ++i) {
        if (CoA::Read::Remaining(msg) < 0x21) {
            return 1;
        }

        Reward r;
        r.item = CoA::Read::U32(msg);
        r.action = CoA::Read::U8(msg);
        r.lockEnd = CoA::Read::U32(msg);
        r.secondsRemaining = CoA::Read::U32(msg);
        r.earned = CoA::Read::U32(msg);
        r.max = CoA::Read::U32(msg);
        r.remaining = CoA::Read::U32(msg);
        r.windowStart = CoA::Read::U32(msg);
        r.windowEnd = CoA::Read::U32(msg);

        if (r.action <= 2) {
            fresh[r.item] = r;
        }
    }

    if (fresh.empty()) {
        s_mgr.rewards.erase(guid);
    } else {
        s_mgr.rewards[guid].swap(fresh);
    }

    CoA::Events::Signal("MANASTORM_REWARD_VISIBILITY_UPDATED", "%s%u", std::to_string(guid).c_str(), n);
    return 1;
}

// ---- lifecycle -------------------------------------------------------------------------------------------------------
void CoA::Manastorm::OnGlueScreen() {   // FUN_102a0d70 (the icon time 0x10BE31A0 is not touched)
    BuildAllowedLevels();
    s_mgr = Mgr();
    SetObjectiveIconScale(1.0f);
}

void CoA::Manastorm::ApplyPending() {   // FUN_102a4ba0
    if (!s_mgr.pendingApply || !HasPlayer()) {
        return;
    }

    s_mgr.levels[PlayerGuid()] = s_mgr.pending;
    s_mgr.pending.clear();
    s_mgr.pendingApply = false;
}

// FUN_102a4a10: while in a Manastorm with the objective icon shown, a unit (type 3) carrying a raid target icon
// (0x5728C0 < 8) rescales the icon to its distance from the player / 20, clamped to [1, 10]. Each square through
// pow(double, 2.0) narrowed to float, the sum in float, the root in double.
void CoA::Manastorm::OnPreAnimate(void* object) {
    if (s_mgr.level == 0 || s_objectiveIconTime <= 0) {
        return;
    }

    if (!s_deps.raidTargetIndex || s_deps.raidTargetIndex(object) >= 8) {
        return;
    }

    if (!s_deps.objectType || s_deps.objectType(object) != 3) {
        return;
    }

    float a[3] = {};
    float b[3] = {};

    if (!s_deps.positions || !s_deps.positions(object, a, b)) {   // the original calls through the player unchecked
        return;
    }

    const float dz = static_cast<float>(std::pow(static_cast<double>(a[2] - b[2]), 2.0));
    const float dx = static_cast<float>(std::pow(static_cast<double>(a[0] - b[0]), 2.0));
    const float dy = static_cast<float>(std::pow(static_cast<double>(a[1] - b[1]), 2.0));
    const float d = static_cast<float>(std::sqrt(static_cast<double>(dy + dx + dz)));
    SetObjectiveIconScale(std::min(10.0f, std::max(1.0f, d / 20.0f)));
}

void CoA::Manastorm::Init() {   // 0x102A4540
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_EVENT("ENTER_MANASTORM_RESULT");
    COA_EVENT("LEAVE_MANASTORM_RESULT");
    COA_EVENT("MAX_COMPLETED_MANASTORM_LEVEL_UPDATED");          // registered, never fired
    COA_EVENT("MAX_COMPLETED_MANASTORM_SOLO_LEVEL_UPDATED");
    COA_EVENT("MAX_COMPLETED_MANASTORM_DUO_LEVEL_UPDATED");
    COA_EVENT("MAX_COMPLETED_MANASTORM_TRIO_LEVEL_UPDATED");
    COA_EVENT("MAX_COMPLETED_MANASTORM_GROUP_LEVEL_UPDATED");
    COA_EVENT("MANASTORM_LEVEL_COMPLETED");
    COA_EVENT("MANASTORM_COMPLETED_LEVELS_UPDATED");
    COA_EVENT("ACTIVE_MANASTORM_UPDATED");
    COA_EVENT("MANASTORM_CACHE_INFO_UPDATED");
    COA_EVENT("MANASTORM_FAILED");
    COA_EVENT("MANASTORM_LEVEL_UNLOCKED");
    COA_EVENT("MANASTORM_CHAOTIC_LINK_UPDATED");
    COA_EVENT("MANASTORM_LOADOUT_RESULT");
    COA_EVENT("MANASTORM_LOADOUT_UPDATE");
    COA_EVENT("MANASTORM_REWARD_VISIBILITY_UPDATED");
    COA_PACKET(0x652, CoA::Manastorm::OnEnterResult);
    COA_PACKET(0x666, CoA::Manastorm::OnLeaveResult);
    COA_PACKET(0x65D, CoA::Manastorm::OnUpdateMaxCompletedLevel);
    COA_PACKET(0x65E, CoA::Manastorm::OnCompletedLevel);
    COA_PACKET(0x65F, CoA::Manastorm::OnData);
    COA_PACKET(0x660, CoA::Manastorm::OnActiveData);
    COA_PACKET(0x67B, CoA::Manastorm::OnFail);
    // FUN_102a3f40: a raw 0x24 Manastorm.dbc row. FUN_102a3b50: a 0x1C ManastormMessages.dbc row, then its three
    // strings. (0x68C / 0x68D, registered here too in the original, are PatchData.cpp's pure patches.)
    CoA::DbcPatch::Register(0x67D, "DBFilesClient\\Manastorm.dbc", 0x24, 0, nullptr);
    CoA::DbcPatch::Register(0x67E, "DBFilesClient\\ManastormMessages.dbc", 0x1C, (1ull << 4) | (1ull << 5) | (1ull << 6), nullptr);
    COA_PACKET(0x67F, CoA::Manastorm::OnChaoticLink);
    COA_PACKET(0x688, CoA::Manastorm::OnLoadout);
    COA_PACKET(0x68A, CoA::Manastorm::OnLoadoutResult);
    COA_PACKET(0x68B, CoA::Manastorm::OnLoadoutUpdate);
    COA_PACKET(0x6AD, CoA::Manastorm::OnRewardVisibility);
    CoA::Lifecycle::OnGlueScreen(CoA::Manastorm::OnGlueScreen);
    CoA::ClientHooks::afterPreAnimate.Add(CoA::Manastorm::OnPreAnimate);   // list 0x10BE2CF8
    CoA::Lifecycle::OnAfterIdle(CoA::Manastorm::ApplyPending);             // list 0x10BE29CC
    // FUN_10114540 (slot 0x10BE3190, which nothing reads): no help, flags 1, "0", always accepted, category 4.
    COA_CVAR("manastormObjectiveIconCulling", nullptr, 1, "0", AcceptAnyValue, 4, false, &s_cullingCVar, true);
}

void CoA::Manastorm::Reset() {
    s_deps = Deps();
    s_mgr = Mgr();
    s_objectiveIconTime = 0;
    s_iconScale = 1.0f;
    s_allowed.clear();
    s_allowedBuilt = false;
    CoA::ClientHooks::manastormSkipsModalSpell = false;
}
