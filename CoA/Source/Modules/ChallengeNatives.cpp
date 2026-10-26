// AscChallenge.cpp's Lua half: C_Challenge (35 natives) and the trial serializer's merged level fields
// (CoA::Challenge::PushMergedLevels).
#include "CoA/Source/Modules/ChallengeInternal.h"
#include "CoA/Source/Modules/Config.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <initializer_list>
#include <map>
#include <string>

using namespace CoA::Challenge::detail;
using CoA::Dbc::Table;
using CoA::Script::CheckNumber;
using CoA::Script::CheckString;
using CoA::Script::PushBool;
using CoA::Script::PushInt;
using CoA::Script::PushNum;
using CoA::Script::PushStr;
using CoA::Script::ReadInt2;
using CoA::Script::ReadNumber;
using CoA::Script::ReadString;
using CoA::Script::ToInt;
using CoA::Script::ValidateInput;

namespace {
#include "CoA/Source/Modules/CAEnums.generated.inc"

    void Key(lua_State* L, const char* k) {
        lua_pushstring(L, k);
    }

    void Set(lua_State* L) {
        lua_settable(L, -3);
    }

    void SetNum(lua_State* L, const char* k, double v) {
        Key(L, k);
        PushNum(L, v);
        Set(L);
    }

    void SetInt(lua_State* L, const char* k, int32_t v) {
        Key(L, k);
        PushInt(L, v);
        Set(L);
    }

    void SetBool(lua_State* L, const char* k, bool v) {
        Key(L, k);
        PushBool(L, v);
        Set(L);
    }

    void SetStr(lua_State* L, const char* k, const char* v) {
        Key(L, k);
        PushStr(L, v);
        Set(L);
    }

    void NewTable(lua_State* L, int arr = 0) {
        lua_createtable(L, arr, 0);
        lua_checkstack(L, 2);
    }

    double U64d(uint64_t v) {   // FUN_10ae6470
        return static_cast<double>(v);
    }

    // RequiredGameMode as the serializer reads it: a fixed mode name -> its game-mode bit.
    uint32_t RequiredGameModeBit(const char* s) {
        static const struct {
            const char* n;
            uint32_t bit;
        } kModes[] = {
            { "Random", 1 },
            { "Ironman", 2 },
            { "Survivalist", 4 },
            { "Draft", 8 },
            { "Resolute", 0x20 },
            { "WildCard", 0x40 },
            { "Felforged", 0x80 },
            { "Nightmare", 0x100 },
            { "BuildDraft", 0x800 },
            { "Crusader", 0x1000 },
        };
        uint32_t bit = 0;

        for (const auto& m : kModes) {
            if (strcmp(s, m.n) == 0) {
                bit = m.bit;
            }
        }

        return bit;
    }

    // A spell line is listed unless every spell it names is hidden (Spell attributes bit 7); an unknown spell counts
    // as hidden.
    bool SpellHidden(uint32_t spell) {
        uint32_t attributes = 0;

        if (!D().spellAttributes || !D().spellAttributes(spell, attributes)) {
            return true;
        }

        return ((attributes >> 7) & 1) != 0;
    }

    void PushRewards(lua_State* L, const std::vector<Reward>& rewards) {
        Key(L, "Rewards");
        NewTable(L);
        int i = 0;

        for (const Reward& w : rewards) {
            PushNum(L, ++i);
            NewTable(L);
            SetBool(L, "IsFirstCompletion", w.firstCompletion);
            SetNum(L, "ItemID", static_cast<int32_t>(w.item));
            SetNum(L, "ItemAmount", static_cast<int32_t>(w.amount));
            SetNum(L, "Achievement", static_cast<int32_t>(w.achievement));
            SetBool(L, "IsSpecialReward", w.special);
            Set(L);
        }

        Set(L);
    }

    void PushSpell(lua_State* L, Row s) {
        NewTable(L);
        SetNum(L, "PvESpellID", static_cast<int32_t>(U32(s, 0x10)));
        SetNum(L, "PvPSpellID", static_cast<int32_t>(U32(s, 0x14)));
        SetBool(L, "IsDungeonPlayerSpell", U32(s, 0x1C) != 0);
        SetBool(L, "IsDungeonBossSpell", U32(s, 0x20) != 0);
        SetBool(L, "IsDungeonMinionSpell", U32(s, 0x24) != 0);
    }

    void PushConditions(lua_State* L, const std::vector<Row>& conditions) {
        Key(L, "Conditions");
        NewTable(L);
        int i = 0;

        for (Row c : conditions) {
            PushNum(L, ++i);
            NewTable(L);
            SetNum(L, "ElseGroup", static_cast<int32_t>(U32(c, 0x10)));
            SetStr(L, "ConditionType", Str(Conditions(), c, 0x14));
            SetNum(L, "ConditionValue1", F32(c, 0x18));
            SetNum(L, "ConditionValue2", F32(c, 0x1C));
            SetNum(L, "ConditionValue3", F32(c, 0x20));
            SetBool(L, "NegativeCondition", U32(c, 0x24) != 0);
            SetBool(L, "ConditionMet", ConditionMet(c));
            Set(L);
        }

        Set(L);
    }

    void PushRules(lua_State* L, const std::vector<Row>& rules) {
        Key(L, "Rules");
        NewTable(L, static_cast<int>(rules.size()));
        int i = 0;

        for (Row r : rules) {
            PushNum(L, ++i);
            PushStr(L, Str(Rules(), r, 0x10));   // FUN_1009b2d0
            Set(L);
        }

        Set(L);
    }

    void PushRequirements(lua_State* L, const std::vector<Row>& requirements) {
        Key(L, "Requirements");
        NewTable(L);
        int i = 0;

        for (Row r : requirements) {
            PushNum(L, ++i);
            NewTable(L);
            SetStr(L, "RequirementType", Str(Requirements(), r, 0x10));
            SetNum(L, "MiscValue1", F32(r, 0x14));
            SetNum(L, "MiscValue2", F32(r, 0x18));
            SetNum(L, "MiscValue3", F32(r, 0x1C));
            Set(L);
        }

        Set(L);
    }

    // The per-level fields (the body of FUN_1013c330, inlined in FUN_1013cd30's Levels loop).
    void PushLevelFields(lua_State* L, uint32_t id, uint32_t level, LevelInfo* li) {
        SetBool(L, "IsCompleted", IsCompleted(id, level));
        PushRewards(L, li->rewards);
        Key(L, "Spells");
        NewTable(L);
        int i = 0;

        for (Row s : li->spells) {
            if (SpellHidden(U32(s, 0x10)) && SpellHidden(U32(s, 0x14))) {
                continue;
            }

            PushNum(L, ++i);
            PushSpell(L, s);
            Set(L);
        }

        Set(L);
        PushConditions(L, li->conditions);
        PushRules(L, li->rules);
        PushRequirements(L, li->requirements);
        Key(L, "Modifiers");
        NewTable(L);

        for (const Modifier& m : li->modifiers) {
            const std::string name = m.type < 7 ? kChallengeModifiers[m.type] : "UNEXPECTED_ENUM_VALUE_" + std::to_string(m.type);
            PushStr(L, name.c_str());
            NewTable(L, 3);

            for (int k = 0; k < 3; ++k) {
                PushNum(L, k + 1);
                PushNum(L, m.v[k]);   // FUN_1009b2b0
                Set(L);
            }

            Set(L);
        }

        Set(L);
    }

    // FUN_1013cd30: the challenge table (or nil).
    void PushChallenge(lua_State* L, uint32_t id, bool sort) {
        Info* info = InfoSorted(id, sort);

        if (!info || !info->row) {
            lua_pushnil(L);
            return;
        }

        Row r = info->row;
        Table& t = Challenges();
        NewTable(L);
        SetNum(L, "ID", static_cast<int32_t>(U32(r, 0)));
        SetBool(L, "PvE", U32(r, 4) != 0);
        SetBool(L, "PvP", U32(r, 8) != 0);
        SetBool(L, "RequiresGameMode", U32(r, 0xC) != 0);
        Key(L, "RequiredGameMode");
        PushInt(L, static_cast<int32_t>(RequiredGameModeBit(Str(t, r, 0x10))));
        Set(L);
        SetNum(L, "Flags", static_cast<int32_t>(U32(r, 0x14)));
        SetNum(L, "ExclusiveGroup", static_cast<int32_t>(U32(r, 0x18)));
        SetStr(L, "Name", Str(t, r, 0x1C));
        SetStr(L, "Description", Str(t, r, 0x20));
        SetNum(L, "RequiredGameEvent", static_cast<int32_t>(U32(r, 0x24)));
        SetStr(L, "Icon", Str(t, r, 0x28));
        SetStr(L, "DifficultyRating", Str(t, r, 0x2C));
        Key(L, "GroupIds");
        NewTable(L, static_cast<int>(info->groups.size()));

        for (size_t i = 0; i < info->groups.size(); ++i) {
            PushNum(L, static_cast<double>(i + 1));
            PushInt(L, static_cast<int32_t>(U32(info->groups[i], 8)));   // FUN_1009b2a0
            Set(L);
        }

        Set(L);
        SetBool(L, "IsTrial", U32(r, 0x30) != 0);
        SetBool(L, "IsDungeonTrial", U32(r, 0x34) != 0);
        SetBool(L, "IsPrestige", U32(r, 0x38) != 0);
        SetBool(L, "ShouldCheckMap", U32(r, 0x40) != 0);
        SetInt(L, "MapID", static_cast<int32_t>(U32(r, 0x44)));
        SetInt(L, "MapDifficulty", static_cast<int32_t>(U32(r, 0x48)));
        SetNum(L, "SortOrder", static_cast<int32_t>(U32(r, 0x4C)));
        SetStr(L, "Artwork", Str(t, r, 0x50));
        SetBool(L, "IsFeatured", IsFeatured(id));
        Key(L, "Levels");
        NewTable(L);

        for (uint32_t level = 1; level <= info->maxLevel && info->maxLevel != 0xFFFFFFFF; ++level) {
            PushNum(L, level);

            if (LevelInfo* li = LevelSorted(id, level, sort)) {
                NewTable(L);
                PushLevelFields(L, id, level, li);
            } else {
                lua_pushnil(L);
            }

            Set(L);
        }

        Set(L);
    }

    void PushCriteria(lua_State* L, const Criteria& c) {   // FUN_1013c0f0 (into the table on top)
        SetNum(L, "Guid", static_cast<int32_t>(c.guid));
        SetNum(L, "Challenge", static_cast<int32_t>(c.challenge));
        SetNum(L, "Level", static_cast<int32_t>(c.level));
        Row req = Requirements().Row(c.requirement);
        SetStr(L, "RequirementType", req ? Str(Requirements(), req, 0x10) : "CHALLENGE_REQUIREMENT_TYPE_NONE");
        SetNum(L, "StartTime", U64d(c.start));
        SetNum(L, "AchieveTime", U64d(c.achieve));
        SetBool(L, "Active", c.active);
        SetNum(L, "MiscValue1", req ? F32(req, 0x14) : 0.0);
        SetNum(L, "MiscValue2", req ? F32(req, 0x18) : 0.0);
        SetNum(L, "MiscValue3", req ? F32(req, 0x1C) : 0.0);
    }

    void PushCompletion(lua_State* L, const Completion& c) {   // FUN_1013bf30
        NewTable(L);
        SetNum(L, "Challenge", static_cast<int32_t>(c.challenge));
        SetNum(L, "Level", static_cast<int32_t>(c.level));
        SetStr(L, "PlayerName", c.player.c_str());
        SetNum(L, "StartTime", static_cast<int32_t>(c.start));
        SetNum(L, "CompleteTime", static_cast<int32_t>(c.complete));
        SetStr(L, "PlayerRace", c.race.c_str());
        SetStr(L, "PlayerGender", c.gender.c_str());
        SetStr(L, "PlayerClass", c.cls.c_str());
    }

    void PushFailure(lua_State* L, const Failure& f) {   // FUN_1013ea00
        NewTable(L);
        SetStr(L, "Name", f.name.c_str());
        SetNum(L, "Challenge", static_cast<int32_t>(f.challenge));
        SetNum(L, "Level", static_cast<int32_t>(f.level));
        SetNum(L, "StartTime", static_cast<int32_t>(f.start));
        SetNum(L, "EndTime", static_cast<int32_t>(f.end));
        SetNum(L, "DeathCount", static_cast<int32_t>(f.deaths));
        SetNum(L, "Ruleset", f.ruleset);
        SetNum(L, "MapId", static_cast<int32_t>(f.map));
        SetNum(L, "Difficulty", f.difficulty);
        SetNum(L, "ZoneId", static_cast<int32_t>(f.zone));
        SetNum(L, "AreaId", static_cast<int32_t>(f.area));
        const float pos[3] = { f.pos[0], f.pos[1], f.pos[2] };
        float x = 0;
        float y = 0;

        if (D().worldPosition) {
            D().worldPosition(pos, f.map, x, y);   // FUN_10256a70 -> 0x544140
        }

        SetNum(L, "X", x);
        SetNum(L, "Y", y);
        SetStr(L, "FailReason", f.failReason.c_str());
        SetStr(L, "PlayerRace", f.race.c_str());
        SetStr(L, "PlayerGender", f.gender.c_str());
        SetStr(L, "PlayerClass", f.cls.c_str());
        SetNum(L, "PlayerLevel", f.playerLevel);
        SetStr(L, "KillerType", f.killerType.c_str());
        SetStr(L, "KillerName", f.killerName.c_str());
        SetNum(L, "KillerSpell", static_cast<int32_t>(f.killerSpell));
        SetNum(L, "KillerDamage", static_cast<int32_t>(f.killerDamage));
        SetNum(L, "KillerEntry", static_cast<int32_t>(f.killerEntry));
        SetNum(L, "KillerLevel", static_cast<int32_t>(f.killerLevel));
    }

    int PushResults(lua_State* L, bool ok, const std::vector<uint32_t>& errors, std::string (*name)(uint32_t)) {
        PushBool(L, ok);

        if (errors.empty()) {
            lua_pushnil(L);
            return 2;
        }

        NewTable(L, static_cast<int>(errors.size()));

        for (size_t i = 0; i < errors.size(); ++i) {
            PushNum(L, static_cast<double>(i + 1));
            PushStr(L, name(errors[i]).c_str());
            Set(L);
        }

        return 2;
    }

    // ---- bindings ----
    int CanActivateChallenge(lua_State* L) {   // FUN_1013f700 (id[, level = 1])
        int32_t id = 0;
        int32_t level = 0;

        if (!ReadInt2(L, id, level)) {
            uint32_t only;

            if (!ReadNumber(L, only)) {
                return 0;
            }

            id = static_cast<int32_t>(only);
            level = 1;
        }

        std::vector<uint32_t> errors;
        const bool ok = CanActivate(static_cast<uint32_t>(id), static_cast<uint32_t>(level), errors, std::set<uint32_t>());
        return PushResults(L, ok, errors, StartResult);
    }

    int CanDeactivateChallenge(lua_State* L) {   // FUN_1013f9a0
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        std::vector<uint32_t> errors;
        const bool ok = CanDeactivate(id, errors, std::set<uint32_t>());
        return PushResults(L, ok, errors, StopResult);
    }

    int CanResurrect(lua_State* L) {   // FUN_10133bb0
        bool ok = true;
        const bool* permadeath = CoA::Config::Bool("CONFIG_CHALLENGE_PERMADEATH_ENABLED");

        if (permadeath && *permadeath) {
            for (auto& a : M().active) {
                if (LevelInfo* li = FindLevel(a.second.id, a.second.level)) {
                    for (Row r : li->requirements) {
                        if (RequirementType(Str(Requirements(), r, 0x10)) == 3 && F32(r, 0x14) <= static_cast<float>(static_cast<int32_t>(a.second.deaths))) {
                            ok = false;
                        }
                    }
                }
            }
        }

        PushBool(L, ok);
        return 1;
    }

    int GetActiveChallenges(lua_State* L) {   // FUN_1013fcc0
        NewTable(L);
        int i = 0;

        for (auto& a : M().active) {
            PushNum(L, ++i);
            NewTable(L);
            SetNum(L, "Id", static_cast<int32_t>(a.second.id));
            SetNum(L, "Level", static_cast<int32_t>(a.second.level));
            SetBool(L, "Enabled", a.second.enabled);
            SetNum(L, "StartTime", U64d(a.second.startTime));
            SetBool(L, "EligibleForCompletion", a.second.eligible);
            SetNum(L, "DeathCount", static_cast<int32_t>(a.second.deaths));
            Set(L);
        }

        return 1;
    }

    int GetCompletedChallenges(lua_State* L) {   // FUN_10141da0
        NewTable(L);
        int i = 0;

        for (auto& c : M().completed) {
            PushNum(L, ++i);
            NewTable(L);
            SetNum(L, "Id", static_cast<int32_t>(c.second.id));
            SetNum(L, "Level", static_cast<int32_t>(c.second.level));
            SetNum(L, "StartTime", U64d(c.second.start));
            SetNum(L, "EndTime", U64d(c.second.end));
            Set(L);
        }

        return 1;
    }

    int GetChallengeCriterias(lua_State* L) {   // FUN_10141140
        NewTable(L);
        int i = 0;

        for (auto& c : M().criteria) {
            for (auto& r : c.second) {
                PushNum(L, ++i);
                NewTable(L);
                PushCriteria(L, r.second);
                Set(L);
            }
        }

        return 1;
    }

    int GetChallengeCriteriaInfo(lua_State* L) {   // FUN_10140dd0 (challenge, requirement type)
        if (!ValidateInput(L, { LUA_TNUMBER, LUA_TSTRING })) {
            return 0;
        }

        const uint32_t id = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
        const std::string type = CheckString(L, 2);
        NewTable(L);
        auto it = M().criteria.find(id);

        if (it == M().criteria.end()) {
            return 1;
        }

        int i = 0;

        for (auto& r : it->second) {
            if (Row req = Requirements().Row(r.second.requirement)) {
                if (type == Str(Requirements(), req, 0x10)) {
                    PushNum(L, ++i);
                    NewTable(L);
                    PushCriteria(L, r.second);
                    Set(L);
                }
            }
        }

        return 1;
    }

    // GetAllChallenges (FUN_1013fea0) / GetAllTrials (FUN_101402f0): normal challenges or trials, listed ones only, in
    // challenge order.
    int PushList(lua_State* L, bool trials) {
        std::vector<Row> rows;

        ForEachRow(Challenges(), [&](Row r) {
            const bool t = U32(r, 0x30) != 0 || U32(r, 0x34) != 0;

            if (t == trials && FindInfo(U32(r, 0))) {
                rows.push_back(r);
            }
        });

        std::sort(rows.begin(), rows.end(), ChallengeBefore);   // FUN_1012e6a0
        NewTable(L);
        int i = 0;

        for (Row r : rows) {
            if (Listed(r)) {
                PushNum(L, ++i);
                PushChallenge(L, U32(r, 0), true);
                Set(L);
            }
        }

        return 1;
    }

    int GetAllChallenges(lua_State* L) {
        return PushList(L, false);
    }

    int GetAllTrials(lua_State* L) {
        return PushList(L, true);
    }

    int GetBrokenChallengeRules(lua_State* L) {   // FUN_10140740: one type string per broken rule row
        std::vector<std::string> out;

        ForEachRow(Rules(), [&](Row r) {
            if (M().brokenRules.count(Str(Rules(), r, 0x10))) {
                out.push_back(Str(Rules(), r, 0x10));
            }
        });

        NewTable(L, static_cast<int>(out.size()));

        for (size_t i = 0; i < out.size(); ++i) {
            PushNum(L, static_cast<double>(i + 1));
            PushStr(L, out[i].c_str());
            Set(L);
        }

        return 1;
    }

    int HasBrokenChallengeRule(lua_State* L) {   // FUN_101429b0
        std::string s;

        if (!ReadString(L, s)) {
            return 0;
        }

        PushBool(L, M().brokenRules.count(s) != 0);
        return 1;
    }

    int HasChallengeRule(lua_State* L) {   // FUN_10142ae0: any active challenge's level has the rule
        std::string s;

        if (!ReadString(L, s)) {
            return 0;
        }

        bool found = false;

        for (auto& a : M().active) {
            if (LevelInfo* li = FindLevel(a.second.id, a.second.level)) {
                for (Row r : li->rules) {
                    found = found || s == Str(Rules(), r, 0x10);
                }
            }
        }

        PushBool(L, found);
        return 1;
    }

    int GetChallengeAtIndex(lua_State* L) {   // FUN_101408b0 (index, sortConditions)
        if (!ValidateInput(L, { LUA_TNUMBER, LUA_TBOOLEAN })) {
            return 0;
        }

        const int32_t index = ToInt(CheckNumber(L, 1));
        const bool sort = lua_toboolean(L, 2) != 0;

        if (index == 0) {
            return 0;
        }

        const std::vector<Row>& f = Entries().Filtered();

        if (static_cast<uint32_t>(index - 1) < f.size() && f[index - 1]) {
            PushChallenge(L, U32(f[index - 1], 0), sort);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetChallengeIDByIndex(lua_State* L) {   // handler_GetChallengeIDByIndex
        uint32_t index;

        if (!ReadNumber(L, index) || index == 0) {
            return 0;
        }

        const std::vector<Row>& f = Entries().Filtered();

        if (index - 1 < f.size() && f[index - 1]) {
            PushNum(L, static_cast<int32_t>(U32(f[index - 1], 0)));
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    int GetNumChallenges(lua_State* L) {
        PushInt(L, static_cast<int32_t>(Entries().Size()));
        return 1;
    }

    int GetChallengeInfo(lua_State* L) {   // handler_GetChallengeInfo
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        PushChallenge(L, id, true);
        return 1;
    }

    int GetChallengeInfoByLevel(lua_State* L) {   // FUN_10141760 -> FUN_1013c330
        int32_t id;
        int32_t level;

        if (!ReadInt2(L, id, level)) {
            return 0;
        }

        if (!InfoSorted(static_cast<uint32_t>(id), true)) {
            return 1;
        }

        LevelInfo* li = LevelSorted(static_cast<uint32_t>(id), static_cast<uint32_t>(level), true);

        if (!li) {
            return 1;
        }

        PushChallenge(L, static_cast<uint32_t>(id), true);
        PushLevelFields(L, static_cast<uint32_t>(id), static_cast<uint32_t>(level), li);
        return 1;
    }

    // FUN_101417b0 (id): (available level, highest completed level, max level).
    int GetChallengeLevels(lua_State* L) {
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        uint32_t lo = 0;
        uint32_t hi = 0;

        if (Info* info = FindInfo(id)) {
            hi = info->maxLevel;
            lo = info->minLevel;
        }

        const uint32_t maxLevel = hi;
        uint32_t available = 0;
        uint32_t completed = 0;

        if (hi >= lo) {
            for (uint32_t l = hi;; --l) {
                if (IsCompleted(id, l)) {
                    completed = l;
                    break;
                }

                if (l == lo || l == 0) {
                    break;
                }
            }

            const std::set<uint32_t> ignore = { 3 };

            for (uint32_t l = hi;; --l) {
                std::vector<uint32_t> e;

                if (CanActivate(id, l, e, ignore)) {
                    available = l;
                    break;
                }

                if (l == lo || l == 0) {
                    break;
                }
            }
        }

        PushNum(L, std::max<uint32_t>(1, available));
        PushNum(L, completed);
        PushNum(L, maxLevel);
        return 3;
    }

    int GetPendingChallenges(lua_State* L) {   // FUN_10142400
        NewTable(L);
        int i = 0;

        for (auto& p : M().pending) {
            PushNum(L, ++i);
            lua_createtable(L, 0, 2);
            SetNum(L, "ChallengeId", static_cast<int32_t>(p.first));
            SetNum(L, "Level", static_cast<int32_t>(p.second));
            Set(L);
        }

        return 1;
    }

    int GetChallengesWithGroupID(lua_State* L) {   // FUN_10141af0
        uint32_t group;

        if (!ReadNumber(L, group)) {
            return 0;
        }

        std::vector<uint32_t> ids;

        for (int pass = 0; pass < 3; ++pass) {
            ForEachRow(Challenges(), [&](Row r) {
                const bool trial = U32(r, 0x30) != 0;
                const bool dungeon = U32(r, 0x34) != 0;

                if (!((pass == 0 && !trial && !dungeon) || (pass == 1 && trial) || (pass == 2 && dungeon))) {
                    return;
                }

                Info* info = FindInfo(U32(r, 0));

                if (!info) {
                    return;
                }

                bool in = group == 0;

                for (Row g : info->groups) {
                    in = in || U32(g, 8) == group;
                }

                if (in) {
                    ids.push_back(U32(r, 0));
                }
            });
        }

        NewTable(L, static_cast<int>(ids.size()));

        for (size_t i = 0; i < ids.size(); ++i) {
            PushNum(L, static_cast<double>(i + 1));
            PushInt(L, static_cast<int32_t>(ids[i]));
            Set(L);
        }

        return 1;
    }

    int IsChallengeActive(lua_State* L) {   // handler_IsChallengeActive: (active, level)
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        auto it = M().active.find(id);
        PushBool(L, it != M().active.end());
        PushNum(L, it != M().active.end() ? static_cast<int32_t>(it->second.level) : 0);
        return 2;
    }

    int GetChallengeFailure(lua_State* L) {   // handler_GetChallengeFailure (0-based index)
        uint32_t index;

        if (!ReadNumber(L, index)) {
            return 0;
        }

        if (index >= M().failures.size()) {
            lua_pushnil(L);
        } else {
            PushFailure(L, M().failures[index]);
        }

        return 1;
    }

    // FUN_101412b0 (zone, count): this zone's failures of the last 7 days, latest first.
    int GetChallengeFailures(lua_State* L) {
        int32_t zone;
        int32_t count;

        if (!ReadInt2(L, zone, count)) {
            return 0;
        }

        NewTable(L);
        const uint64_t since = static_cast<uint64_t>(Now() - 0x93A80);
        std::vector<const Failure*> list;

        for (const Failure& f : M().failures) {
            if (f.zone == static_cast<uint32_t>(zone) && f.end >= since) {
                list.push_back(&f);
            }
        }

        std::sort(list.begin(), list.end(), [](const Failure* a, const Failure* b) { return a->end > b->end; });   // FUN_1012e870

        if (list.size() > static_cast<uint32_t>(count)) {
            list.resize(static_cast<uint32_t>(count));
        }

        int i = 0;

        for (const Failure* f : list) {
            PushNum(L, ++i);
            PushFailure(L, *f);
            Set(L);
        }

        return 1;
    }

    int QueryChallengeFailures(lua_State* L) {   // FUN_10142ec0
        CoA::Packet(0x5A1).Send();
        PushBool(L, true);
        return 1;
    }

    int QueryChallengeCompletions(lua_State* L) {   // FUN_10142da0 (challenge, level)
        int32_t id;
        int32_t level;

        if (!ReadInt2(L, id, level)) {
            return 0;
        }

        CoA::Packet(0x5C6).U32(static_cast<uint32_t>(id)).U32(static_cast<uint32_t>(level)).Send();
        PushBool(L, true);
        return 1;
    }

    const std::vector<Completion>* CompletionsOf(uint32_t id, uint32_t level) {
        auto c = M().completions.find(id);

        if (c == M().completions.end()) {
            return nullptr;
        }

        auto l = c->second.find(level);
        return l == c->second.end() ? nullptr : &l->second;
    }

    bool Read3(lua_State* L, uint32_t& a, uint32_t& b, uint32_t& c) {   // FUN_100d0410
        if (!ValidateInput(L, { LUA_TNUMBER, LUA_TNUMBER, LUA_TNUMBER })) {
            return false;
        }

        a = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
        b = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        c = static_cast<uint32_t>(ToInt(CheckNumber(L, 3)));
        return true;
    }

    int GetChallengeCompletion(lua_State* L) {   // handler_GetChallengeCompletion (challenge, level, index)
        uint32_t id;
        uint32_t level;
        uint32_t index;

        if (!Read3(L, id, level, index)) {
            return 0;
        }

        const std::vector<Completion>* v = CompletionsOf(id, level);

        if (!v || index >= v->size()) {
            lua_pushnil(L);
        } else {
            PushCompletion(L, (*v)[index]);
        }

        return 1;
    }

    // FUN_10140ab0 (challenge, level, ranks): completions grouped by equal completion time, fastest first.
    int GetChallengeCompletions(lua_State* L) {
        uint32_t id;
        uint32_t level;
        uint32_t ranks;

        if (!Read3(L, id, level, ranks) || ranks == 0) {
            return 0;
        }

        NewTable(L);
        std::vector<std::vector<const Completion*>> groups;

        if (const std::vector<Completion>* v = CompletionsOf(id, level)) {
            for (const Completion& c : *v) {
                const uint64_t d = c.complete - c.start;

                if (groups.empty() || groups.back().back()->complete - groups.back().back()->start != d) {
                    groups.emplace_back();
                }

                groups.back().push_back(&c);
            }
        }

        for (uint32_t g = 0; g < ranks && g < groups.size(); ++g) {
            PushNum(L, g + 1);
            NewTable(L);

            for (size_t j = 0; j < groups[g].size(); ++j) {
                PushNum(L, static_cast<double>(j + 1));
                PushCompletion(L, *groups[g][j]);
                Set(L);
            }

            Set(L);
        }

        return 1;
    }

    // Localization lookups: the type row whose name (+4) equals the argument, else nil.
    int Localize(lua_State* L, Table& t, std::initializer_list<uint32_t> offs) {
        std::string s;

        if (!ReadString(L, s)) {
            return 0;
        }

        if (t.Loaded()) {
            for (uint32_t id = t.MinId(); id < t.MaxId() + 1; ++id) {
                if (Row r = t.Row(id)) {
                    if (s == Str(t, r, 4)) {
                        for (uint32_t off : offs) {
                            PushStr(L, Str(t, r, off));
                        }

                        return static_cast<int>(offs.size());
                    }
                }
            }
        }

        lua_pushnil(L);
        return 1;
    }

    int GetConditionLocalization(lua_State* L) {   // FUN_10141f20
        return Localize(L, ConditionTypes(), { 8, 0xC, 0x10, 0x14, 0x18, 0x1C, 0x20 });
    }

    int GetRequirementLocalization(lua_State* L) {   // FUN_10142520
        return Localize(L, RequirementTypes(), { 8, 0xC, 0x10, 0x14, 0x18, 0x1C, 0x20 });
    }

    int GetModifierLocalization(lua_State* L) {   // FUN_101421a0
        return Localize(L, ModifierTypes(), { 8, 0xC, 0x10 });
    }

    int GetRuleLocalization(lua_State* L) {   // FUN_101427a0
        return Localize(L, RuleTypes(), { 8, 0xC });
    }

    int SendChallengeSyncResponse(lua_State* L) {   // FUN_10142f90 (accept)
        if (!ValidateInput(L, { LUA_TBOOLEAN })) {
            return 0;
        }

        const uint8_t accept = lua_toboolean(L, 1) != 0;
        CoA::Packet(0x59C).U8(accept).Send();
        PushBool(L, true);
        return 1;
    }

    int StartChallenge(lua_State* L) {   // FUN_10143300 (id, level)
        int32_t id;
        int32_t level;

        if (!ReadInt2(L, id, level)) {
            return 0;
        }

        CoA::Packet(0x592).U32(static_cast<uint32_t>(id)).U32(static_cast<uint32_t>(level)).Send();
        PushBool(L, true);
        return 1;
    }

    int StopChallenge(lua_State* L) {   // FUN_10143420 (id)
        uint32_t id;

        if (!ReadNumber(L, id)) {
            return 0;
        }

        CoA::Packet(0x594).U32(id).Send();
        PushBool(L, true);
        return 1;
    }

    // FUN_101430f0 (text, {FILTER_* = bool}): a map / difficulty change repopulates first.
    int SetChallengeFilter(lua_State* L) {
        if (!ValidateInput(L, { LUA_TSTRING, LUA_TTABLE })) {
            return 0;
        }

        const std::string text = CheckString(L, 1);
        std::vector<uint32_t> filters;
        std::map<std::string, bool> names;   // FUN_10131480
        CoA::Script::ToBoolMap(L, 2, names);   // FUN_10131480 (FrameScript::lua_totable)

        for (auto& n : names) {
            if (n.second) {
                const int i = Index(kChallengeFilters, n.first.c_str());

                if (i >= 0) {
                    filters.push_back(static_cast<uint32_t>(i));
                }
            }
        }

        const int32_t map = CurrentMap();
        const int32_t difficulty = InstanceDifficulty();
        Mgr& m = M();

        if (m.filterMap != map || m.filterDifficulty != difficulty) {
            Entries().Reset();
            m.filterMap = map;
            m.filterDifficulty = difficulty;
        }

        Entries().ApplyFilter(text, filters, {});
        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_Challenge", "StartChallenge", StartChallenge),
        COA_NATIVE("C_Challenge", "GetActiveChallenges", GetActiveChallenges),
        COA_NATIVE("C_Challenge", "GetCompletedChallenges", GetCompletedChallenges),
        COA_NATIVE("C_Challenge", "GetChallengeCriterias", GetChallengeCriterias),
        COA_NATIVE("C_Challenge", "GetChallengeCriteriaInfo", GetChallengeCriteriaInfo),
        COA_NATIVE("C_Challenge", "StopChallenge", StopChallenge),
        COA_NATIVE("C_Challenge", "GetAllChallenges", GetAllChallenges),
        COA_NATIVE("C_Challenge", "GetAllTrials", GetAllTrials),
        COA_NATIVE("C_Challenge", "GetChallengeInfo", GetChallengeInfo),
        COA_NATIVE("C_Challenge", "GetChallengeInfoByLevel", GetChallengeInfoByLevel),
        COA_NATIVE("C_Challenge", "IsChallengeActive", IsChallengeActive),
        COA_NATIVE("C_Challenge", "CanActivateChallenge", CanActivateChallenge),
        COA_NATIVE("C_Challenge", "GetChallengeLevels", GetChallengeLevels),
        COA_NATIVE("C_Challenge", "SendChallengeSyncResponse", SendChallengeSyncResponse),
        COA_NATIVE("C_Challenge", "GetPendingChallenges", GetPendingChallenges),
        COA_NATIVE("C_Challenge", "CanResurrect", CanResurrect),
        COA_NATIVE("C_Challenge", "CanDeactivateChallenge", CanDeactivateChallenge),
        COA_NATIVE("C_Challenge", "GetChallengesWithGroupID", GetChallengesWithGroupID),
        COA_NATIVE("C_Challenge", "QueryChallengeFailures", QueryChallengeFailures),
        COA_NATIVE("C_Challenge", "GetChallengeFailures", GetChallengeFailures),
        COA_NATIVE("C_Challenge", "GetChallengeFailure", GetChallengeFailure),
        COA_NATIVE("C_Challenge", "HasBrokenChallengeRule", HasBrokenChallengeRule),
        COA_NATIVE("C_Challenge", "GetBrokenChallengeRules", GetBrokenChallengeRules),
        COA_NATIVE("C_Challenge", "GetConditionLocalization", GetConditionLocalization),
        COA_NATIVE("C_Challenge", "GetRuleLocalization", GetRuleLocalization),
        COA_NATIVE("C_Challenge", "GetRequirementLocalization", GetRequirementLocalization),
        COA_NATIVE("C_Challenge", "GetModifierLocalization", GetModifierLocalization),
        COA_NATIVE("C_Challenge", "HasChallengeRule", HasChallengeRule),
        COA_NATIVE("C_Challenge", "QueryChallengeCompletions", QueryChallengeCompletions),
        COA_NATIVE("C_Challenge", "GetChallengeCompletions", GetChallengeCompletions),
        COA_NATIVE("C_Challenge", "GetChallengeCompletion", GetChallengeCompletion),
        COA_NATIVE("C_Challenge", "GetNumChallenges", GetNumChallenges),
        COA_NATIVE("C_Challenge", "GetChallengeAtIndex", GetChallengeAtIndex),
        COA_NATIVE("C_Challenge", "SetChallengeFilter", SetChallengeFilter),
        COA_NATIVE("C_Challenge", "GetChallengeIDByIndex", GetChallengeIDByIndex),
    };
}

void CoA::Challenge::detail::InitNatives() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}

// The trial serializer's merged level fields. Rows are ordered by address as the original's FUN_1011e1f0 orders them:
// rows from the table file sit in file order in one block, as in the original.
void CoA::Challenge::PushMergedLevels(lua_State* L, const std::vector<std::pair<uint32_t, uint32_t>>& levels, bool sortConditions) {
    std::vector<Row> spells;
    std::vector<Row> conditions;
    std::vector<Row> requirements;
    std::vector<Row> rules;
    std::vector<Reward> rewards;

    for (const auto& lv : levels) {
        if (LevelInfo* li = LevelSorted(lv.first, lv.second, sortConditions)) {
            spells.insert(spells.end(), li->spells.begin(), li->spells.end());
            conditions.insert(conditions.end(), li->conditions.begin(), li->conditions.end());
            requirements.insert(requirements.end(), li->requirements.begin(), li->requirements.end());
            rules.insert(rules.end(), li->rules.begin(), li->rules.end());
            rewards.insert(rewards.end(), li->rewards.begin(), li->rewards.end());
        }
    }

    for (std::vector<Row>* v : { &spells, &conditions, &requirements, &rules }) {   // FUN_1011e1f0 + std::unique
        std::sort(v->begin(), v->end());
        v->erase(std::unique(v->begin(), v->end()), v->end());
    }

    SetBool(L, "IsCompleted", false);
    PushRewards(L, rewards);
    Key(L, "Spells");
    NewTable(L);
    int i = 0;

    for (Row s : spells) {
        PushNum(L, ++i);
        PushSpell(L, s);
        Set(L);
    }

    Set(L);
    PushConditions(L, conditions);
    PushRules(L, rules);
    PushRequirements(L, requirements);
}
