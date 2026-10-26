// AscChallenge.cpp (see Challenge.h): the data stores, the validators, the entry container, the packets, the table
// patches, the tick and the exported pieces. The Lua natives are ChallengeNatives.cpp.
//
// std::unordered_map / std::sort are used where the original uses them: MSVC's bucket policy (with the 32-bit FNV hash,
// ChallengeInternal.h) and its introsort give the same iteration and tie orders.
#include "CoA/Source/Modules/ChallengeInternal.h"
#include "CoA/Source/Modules/AttachFixes.h"
#include "CoA/Source/Modules/AttachMisc.h"
#include "CoA/Source/Modules/Config.h"
#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/ObjectAddon.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Modules/TrialCreator.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/DbcPatch.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include <algorithm>
#include <cstdio>
#include <ctime>

using namespace CoA::Challenge::detail;
using CoA::Dbc::Table;

namespace {
#include "CoA/Source/Modules/CAEnums.generated.inc"

    CoA::Challenge::Deps s_deps;
    Mgr s_mgr;

    Table& T(const char* name) {
        return CoA::Dbc::Get(name);
    }

    // ---- the client, through Deps ----
    uint32_t PlayerClass() {   // unit bytes0 byte 1 for a unit / player object
        if (!HasPlayer()) {
            return 0;
        }

        return s_deps.playerClass ? s_deps.playerClass() : 0;
    }

    uint64_t ActivePlayerGuid() {
        return s_deps.activePlayerGuid ? s_deps.activePlayerGuid() : 0;
    }

    uint32_t Prestige() {   // FUN_102d9f00: the per-unit record's +0x18
        std::vector<CoA::ObjectAddon::Field>& f = CoA::ObjectAddon::Unit(ActivePlayerGuid());
        return f.size() > 2 ? f[2].value : 0;
    }

    uint32_t PartyMembers() {   // 0x52BD10
        return s_deps.partyMembers ? s_deps.partyMembers() : 0;
    }

    uint64_t PartyLeader() {   // 0xBD1968
        return s_deps.partyLeader ? s_deps.partyLeader() : 0;
    }

    // The reward achievement gate: a flagged (0x302) or unknown achievement, or one already earned.
    bool AchievementBlocks(uint32_t id) {
        if (id == 0) {
            return false;
        }

        uint32_t flags = 0;

        if (!s_deps.achievementFlags || !s_deps.achievementFlags(id, flags)) {   // FUN_100b1150 on 0xAD305C
            return true;
        }

        return (flags & 0x302) != 0 || (s_deps.achievementDone && s_deps.achievementDone(id));
    }

    // ---- lookups ----
    std::vector<LevelInfo*> GroupLevels(uint32_t group, uint32_t level) {   // lambda 5 / FUN_10133700
        std::vector<LevelInfo*> out;
        auto g = s_mgr.groups.find(group);

        if (g == s_mgr.groups.end()) {
            return out;
        }

        for (Info* info : g->second) {
            auto it = info->levels.find(level);

            if (it != info->levels.end()) {
                out.push_back(&it->second);
            }
        }

        return out;
    }

    std::vector<LevelInfo*> Targets(Row r) {   // (+4 challenge, +8 group, +0xC level)
        if (U32(r, 8) == 0) {
            LevelInfo* li = FindLevel(U32(r, 4), U32(r, 0xC));
            return li ? std::vector<LevelInfo*> { li } : std::vector<LevelInfo*>();
        }

        return GroupLevels(U32(r, 8), U32(r, 0xC));
    }

    bool ChallengeOrGroupKnown(uint32_t challenge, uint32_t group) {   // FUN_10133970
        return group != 0 ? s_mgr.groups.count(group) != 0 : s_mgr.challenges.count(challenge) != 0;
    }

    // FUN_1013ba00: at least `count` challenges of the group completed at `level`.
    bool GroupCompletedCount(uint32_t group, uint32_t level, uint32_t count) {
        uint32_t n = 0;
        std::vector<Row> rows;   // FUN_10136100: normal, then trials, then dungeon trials

        for (int pass = 0; pass < 3; ++pass) {
            ForEachRow(Challenges(), [&](Row r) {
                const bool trial = U32(r, 0x30) != 0;
                const bool dungeon = U32(r, 0x34) != 0;

                if ((pass == 0 && !trial && !dungeon) || (pass == 1 && trial) || (pass == 2 && dungeon)) {
                    if (Info* info = FindInfo(U32(r, 0))) {
                        bool in = group == 0;

                        for (Row g : info->groups) {
                            in = in || U32(g, 8) == group;
                        }

                        if (in) {
                            rows.push_back(r);
                        }
                    }
                }
            });
        }

        for (Row r : rows) {
            if (IsCompleted(U32(r, 0), level) && ++n >= count) {
                return true;
            }
        }

        return false;
    }

    // ---- UpdateChallengeDataStores (FUN_10143630) ----
    Reward FromRewardRow(Row r) {   // FUN_101309b0
        Reward w;
        w.item = U32(r, 0x1C);
        w.amount = U32(r, 0x20);

        if (w.amount > 1 && IsFeatured(U32(r, 4))) {
            if (const float* mult = CoA::Config::Float("CONFIG_CHALLENGE_FEATURED_REWARD_MULTIPLIER")) {
                w.amount = static_cast<uint32_t>(static_cast<int64_t>(*mult * static_cast<float>(w.amount)));
            }
        }

        w.achievement = U32(r, 0x24);
        w.firstCompletion = U32(r, 0x18) != 0;
        w.sort = U32(r, 0x28);
        w.special = false;
        return w;
    }

    Reward FromGroupRewardRow(Row r) {
        Reward w;
        w.item = U32(r, 0x18);
        w.amount = U32(r, 0x1C);
        w.achievement = U32(r, 0x20);
        w.firstCompletion = false;
        w.sort = U32(r, 0x24);
        w.special = true;
        return w;
    }

    void LogMissing(const char* what, Row r) {
        CoA::Log::Printf("ChallengeMgr::UpdateChallengeDataStores: Challenge %u, group %u not found for %s %u", U32(r, 4), U32(r, 8), what, U32(r, 0));
    }

    // Attach a row to every level it targets (conditions / requirements / rules / spells / modifiers).
    template <class Member>
    void Attach(Table& t, const char* what, Member member) {
        ForEachRow(t, [&](Row r) {
            std::vector<LevelInfo*> targets = Targets(r);

            if (targets.empty()) {
                if (!ChallengeOrGroupKnown(U32(r, 4), U32(r, 8))) {
                    LogMissing(what, r);
                }

                return;
            }

            for (LevelInfo* li : targets) {
                (li->*member).push_back(r);
            }
        });
    }

    void Rebuild() {
        s_mgr.rebuildAt = 0;
        s_mgr.challenges.clear();
        s_mgr.groups.clear();

        ForEachRow(Challenges(), [](Row r) {
            if (!s_mgr.challenges.emplace(U32(r, 0), Info()).second) {
                CoA::Log::Printf("ChallengeMgr::UpdateChallengeDataStores: Challenge %u already exists", U32(r, 0));
            } else {
                s_mgr.challenges[U32(r, 0)].row = r;
            }
        });

        ForEachRow(Groups(), [](Row r) {
            Info* info = FindInfo(U32(r, 4));

            if (!info) {
                CoA::Log::Printf("ChallengeMgr::UpdateChallengeDataStores: Challenge %u not found for group %u", U32(r, 4), U32(r, 8));
                return;
            }

            std::vector<Info*>& members = s_mgr.groups[U32(r, 8)];

            if (std::find(members.begin(), members.end(), info) != members.end()) {
                CoA::Log::Printf("ChallengeMgr::UpdateChallengeDataStores: Challenge %u already exists for group %u", U32(r, 4), U32(r, 8));
            } else {
                members.push_back(info);
            }

            info->groups.push_back(r);
        });

        ForEachRow(Levels(), [](Row r) {
            Info* owner = FindInfo(U32(r, 4));

            if (!owner) {
                CoA::Log::Printf("ChallengeMgr::UpdateChallengeDataStores: Challenge %u not found for level %u", U32(r, 4), U32(r, 0xC));
                return;
            }

            std::vector<Info*> infos;

            if (U32(r, 8) == 0) {
                infos.push_back(owner);
            } else if (s_mgr.groups.count(U32(r, 8))) {
                infos = s_mgr.groups[U32(r, 8)];
            }

            if (infos.empty()) {
                CoA::Log::Printf("ChallengeMgr::UpdateChallengeDataStores: Challenge %u, group %u not found for level %u", U32(r, 4), U32(r, 8), U32(r, 0xC));
            }

            for (Info* info : infos) {
                info->levels[U32(r, 0xC)].row = r;
            }
        });

        Attach(Conditions(), "condition", &LevelInfo::conditions);
        Attach(Requirements(), "requirement", &LevelInfo::requirements);
        Attach(Rules(), "rule", &LevelInfo::rules);
        Attach(Spells(), "spell", &LevelInfo::spells);
        Attach(Modifiers(), "modifier", &LevelInfo::modifierRows);

        // Rewards for this realm's expansion (RealmInfo ruleset) and the player's class; an achievement reward that is
        // flagged or already earned is dropped.
        const uint32_t expansion = CoA::RealmInfo::Get().ruleset;
        const uint32_t cls = PlayerClass();

        ForEachRow(Rewards(), [&](Row r) {
            const int exp = IndexOr(kExpansions, Str(Rewards(), r, 0x10), 3);

            if (exp != 3 && static_cast<uint32_t>(exp) != expansion) {
                return;
            }

            const int c = IndexOr(kChallengeClasses, Str(Rewards(), r, 0x14), 0);

            if (c != 0 && static_cast<uint32_t>(c) != cls) {
                return;
            }

            if (AchievementBlocks(U32(r, 0x24))) {
                return;
            }

            std::vector<LevelInfo*> targets = Targets(r);

            if (targets.empty()) {
                if (!ChallengeOrGroupKnown(U32(r, 4), U32(r, 8))) {
                    LogMissing("reward", r);
                }

                return;
            }

            for (LevelInfo* li : targets) {
                (U32(r, 0x18) == 0 ? li->repeatRewards : li->firstRewards).push_back(r);
            }
        });

        // Group rewards: shown on the challenges of the group once all but one of the required count are completed at
        // that level.
        ForEachRow(GroupRewards(), [&](Row r) {
            const int exp = IndexOr(kExpansions, Str(GroupRewards(), r, 0x10), 3);

            if (exp != 3 && static_cast<uint32_t>(exp) != expansion) {
                return;
            }

            const int c = IndexOr(kChallengeClasses, Str(GroupRewards(), r, 0x14), 0);

            if (c != 0 && static_cast<uint32_t>(c) != cls) {
                return;
            }

            if (AchievementBlocks(U32(r, 0x20))) {
                return;
            }

            std::set<Info*> done;
            Table& ct = Challenges();

            for (uint32_t id = ct.MinId(); ct.Loaded() && id <= ct.MaxId() + 1; ++id) {
                Info* info = FindInfo(id);

                if (!info) {
                    continue;
                }

                bool inGroup = false;

                for (Row g : info->groups) {
                    inGroup = inGroup || U32(g, 8) == U32(r, 4);
                }

                if (inGroup && IsCompleted(U32(info->row, 0), U32(r, 8))) {
                    done.insert(info);
                }
            }

            if (done.size() + 1 != U32(r, 0xC)) {
                return;
            }

            std::vector<LevelInfo*> targets = GroupLevels(U32(r, 4), U32(r, 8));

            if (targets.empty()) {
                if (!s_mgr.groups.count(U32(r, 4))) {
                    CoA::Log::Printf("ChallengeMgr::UpdateChallengeDataStores: Group %u not found for group reward %u", U32(r, 4), U32(r, 0));
                }

                return;
            }

            for (LevelInfo* li : targets) {
                li->groupRewards.push_back(r);
            }
        });

        for (auto& c : s_mgr.challenges) {
            for (auto& l : c.second.levels) {
                for (Row m : l.second.modifierRows) {
                    int type = Index(kChallengeModifiers, Str(Modifiers(), m, 0x10));

                    if (type < 0) {
                        CoA::Log::Printf("Unexpected Value: %s", Str(Modifiers(), m, 0x10));
                        type = 0;
                    }

                    const float v[3] = { F32(m, 0x14), F32(m, 0x18), F32(m, 0x1C) };
                    auto it = std::find_if(l.second.modifiers.begin(), l.second.modifiers.end(),
                        [&](const Modifier& x) { return x.type == static_cast<uint32_t>(type); });

                    if (it == l.second.modifiers.end()) {
                        l.second.modifiers.push_back({ static_cast<uint32_t>(type), { v[0], v[1], v[2] } });
                    } else {
                        for (int i = 0; i < 3; ++i) {
                            it->v[i] += v[i];
                        }
                    }
                }
            }
        }

        for (auto& c : s_mgr.challenges) {
            for (auto& l : c.second.levels) {
                LevelInfo& li = l.second;
                const std::vector<Row>& own = IsCompleted(c.first, l.first) ? li.repeatRewards : li.firstRewards;

                for (Row r : own) {
                    li.rewards.push_back(FromRewardRow(r));
                }

                for (Row r : li.groupRewards) {
                    li.rewards.push_back(FromGroupRewardRow(r));
                }
            }
        }

        for (auto& c : s_mgr.challenges) {
            uint32_t lo = 0xFFFFFFFF;
            uint32_t hi = 0;

            for (auto& l : c.second.levels) {
                lo = std::min(lo, l.first);
                hi = std::max(hi, l.first);
            }

            c.second.minLevel = lo;
            c.second.maxLevel = hi;
        }

        for (auto& c : s_mgr.challenges) {
            for (auto& l : c.second.levels) {
                LevelInfo& li = l.second;
                std::sort(li.requirements.begin(), li.requirements.end(), [](Row a, Row b) { return U32(a, 0x20) < U32(b, 0x20); });   // FUN_1012eb60
                std::sort(li.firstRewards.begin(), li.firstRewards.end(), [](Row a, Row b) { return U32(a, 0x28) < U32(b, 0x28); });   // FUN_1012edb0
                std::sort(li.repeatRewards.begin(), li.repeatRewards.end(), [](Row a, Row b) { return U32(a, 0x28) < U32(b, 0x28); });
                std::sort(li.rewards.begin(), li.rewards.end(), [](const Reward& a, const Reward& b) { return a.sort < b.sort; });   // FUN_1012f2b0
            }
        }
    }

    // FUN_1012e460 comparator: unmet conditions first, then by id.
    void SortConditions(std::vector<Row>& v) {
        std::sort(v.begin(), v.end(), [](Row a, Row b) {
            const bool ma = ConditionMet(a);
            const bool mb = ConditionMet(b);

            if (ma != mb) {
                return !ma && mb;
            }

            return U32(a, 0) < U32(b, 0);
        });
    }

    // FUN_1008e380: the player is at the level cap (or, on a dev realm, at 60 / 70 / 80).
    bool AtMaxLevel() {
        const uint32_t level = PlayerLevel();

        if (level >= CoA::GameEvents::ServerMaxLevel()) {
            return true;
        }

        return CoA::RealmInfo::Get().gates[4] != 0 && (level == 60 || level == 70 || level == 80);
    }

    // ---- ChallengeEntryContainer (vtable 0x10B32484) ----
    class Container : public CoA::Filter::Container<Row> {
    protected:
        std::vector<Row> DoPopulate() override {   // FUN_101b8ce0
            std::vector<Row> out;

            ForEachRow(Challenges(), [&](Row r) {
                if (Listed(r)) {
                    out.push_back(r);
                }
            });

            std::sort(out.begin(), out.end(), ChallengeBefore);
            return out;
        }

        bool ArgMatch(uint32_t filter, const Row& r) override {   // FUN_101b9270
            if (s_mgr.active.count(U32(r, 0)) || U32(r, 0x34) != 0) {
                return true;
            }

            auto hasSoloRule = [&]() {
                if (LevelInfo* li = FindLevel(U32(r, 0), 1)) {
                    for (Row rule : li->rules) {
                        if (IndexOr(kChallengeRules, Str(Rules(), rule, 0x10), 2) == 2) {   // FUN_1011f040(.., 2)
                            return true;
                        }
                    }
                }

                return false;
            };

            switch (filter) {
                case 1:
                    return U32(r, 0x30) != 0;
                case 2:
                    return U32(r, 0x30) == 0;
                case 4:
                    return U32(r, 0x38) != 0;
                case 5:
                    return U32(r, 0x38) == 0;
                case 6: {
                    std::vector<uint32_t> e;
                    return CanActivate(U32(r, 0), 1, e, std::set<uint32_t>());
                }
                case 7:
                    return U32(r, 4) != 0;
                case 8:
                    return U32(r, 8) != 0;
                case 9:
                    return hasSoloRule();
                case 10:
                    return !hasSoloRule();
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                    return IndexOr(kChallengeDifficulties, Str(Challenges(), r, 0x2C), 0) == static_cast<int>(filter - 11);
                default:
                    return false;
            }
        }

        uint32_t ArgGroup(uint32_t filter) override {   // FUN_101b95f0
            switch (filter) {
                case 1:
                case 2:
                case 3:
                    return 1;
                case 4:
                case 5:
                    return 2;
                case 7:
                case 8:
                    return 3;
                case 9:
                case 10:
                    return 4;
                case 11:
                case 12:
                case 13:
                case 14:
                case 15:
                    return 5;
                default:
                    return 0;
            }
        }

        bool TextMatch(const std::string& text, const Row& r) override {   // FUN_101b9100
            if (text.empty()) {
                return true;
            }

            if (CoA::Filter::Lower(Str(Challenges(), r, 0x1C)).find(text) != std::string::npos) {
                return true;
            }

            return CoA::Filter::Lower(Str(Challenges(), r, 0x20)).find(text) != std::string::npos;
        }
    };

    Container s_entries;

    // ---- packets ----
    // The 7a convention: a list's count is trusted only while a record's (minimum) size is left.
    constexpr uint32_t kActiveSize = 0x1A;
    constexpr uint32_t kCompletedSize = 0x1C;
    constexpr uint32_t kCriteriaSize = 0x21;
    constexpr uint32_t kPendingSize = 8;
    constexpr uint32_t kFailureMinSize = 99;      // seven empty strings
    constexpr uint32_t kCompletionMinSize = 40;   // four empty strings
    constexpr uint32_t kRuleMinSize = 4;          // an empty string

    Active ReadActive(CDataStore* p) {   // 0x1A bytes
        Active a;
        a.guid = CoA::Read::U32(p);
        a.id = CoA::Read::U32(p);
        a.level = CoA::Read::U32(p);
        a.enabled = CoA::Read::U8(p) != 0;
        a.startTime = CoA::Read::U64(p);
        a.eligible = CoA::Read::U8(p) != 0;
        a.deaths = CoA::Read::U32(p);
        return a;
    }

    int32_t OnActiveList(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x596
        s_mgr.active.clear();

        for (uint32_t n = CoA::Read::U32(p); n && CoA::Read::Remaining(p) >= kActiveSize; --n) {
            const Active a = ReadActive(p);
            s_mgr.active[a.id] = a;
        }

        CoA::Events::Signal("CHALLENGE_ACTIVE_LIST_CHANGED", nullptr);
        return 1;
    }

    int32_t OnActiveUpdate(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x5A6
        const Active a = ReadActive(p);
        auto old = s_mgr.active.find(a.id);
        const bool existed = old != s_mgr.active.end();
        const uint32_t oldDeaths = existed ? old->second.deaths : 0;
        s_mgr.active[a.id] = a;

        if (!existed || oldDeaths == a.deaths) {
            return 1;
        }

        // A death under a no-death requirement: remaining deaths.
        if (LevelInfo* li = FindLevel(a.id, a.level)) {
            for (Row r : li->requirements) {
                if (RequirementType(Str(Requirements(), r, 0x10)) == 3) {
                    const uint32_t allowed = static_cast<uint32_t>(static_cast<int64_t>(F32(r, 0x14)));
                    const uint32_t left = allowed > a.deaths ? allowed - a.deaths : 0;
                    CoA::Events::Signal("CHALLENGE_DEATH_UPDATE", "%u%u%u", a.id, a.level, left);
                    break;
                }
            }
        }

        return 1;
    }

    Completed ReadCompleted(CDataStore* p) {   // 0x1C bytes
        Completed c;
        c.guid = CoA::Read::U32(p);
        c.id = CoA::Read::U32(p);
        c.level = CoA::Read::U32(p);
        c.start = CoA::Read::U64(p);
        c.end = CoA::Read::U64(p);
        return c;
    }

    int32_t OnCompletedList(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x597
        s_mgr.completed.clear();

        for (uint32_t n = CoA::Read::U32(p); n && CoA::Read::Remaining(p) >= kCompletedSize; --n) {
            const Completed c = ReadCompleted(p);
            s_mgr.completed.emplace(c.id, c);
        }

        CoA::Challenge::ScheduleRebuild();
        CoA::Events::Signal("CHALLENGE_COMPLETED_LIST_CHANGED", nullptr);
        return 1;
    }

    int32_t OnCompleted(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x598
        const Completed c = ReadCompleted(p);
        s_mgr.completed.emplace(c.id, c);   // FUN_10130b60
        CoA::Challenge::ScheduleRebuild();
        CoA::Events::Signal("CHALLENGE_COMPLETED", "%u%u", c.id, c.level);
        return 1;
    }

    Criteria ReadCriteria(CDataStore* p) {   // 0x21 bytes
        Criteria c;
        c.guid = CoA::Read::U32(p);
        c.challenge = CoA::Read::U32(p);
        c.level = CoA::Read::U32(p);
        c.requirement = CoA::Read::U32(p);
        c.start = CoA::Read::U64(p);
        c.achieve = CoA::Read::U64(p);
        c.active = CoA::Read::U8(p) != 0;
        return c;
    }

    int32_t OnCriteriaList(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x599
        s_mgr.criteria.clear();

        for (uint32_t n = CoA::Read::U32(p); n && CoA::Read::Remaining(p) >= kCriteriaSize; --n) {
            const Criteria c = ReadCriteria(p);
            s_mgr.criteria[c.challenge][c.requirement] = c;
        }

        CoA::Events::Signal("CHALLENGE_CRITERIA_LIST_CHANGED", nullptr);
        return 1;
    }

    int32_t OnCriteriaUpdate(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x59A
        const Criteria c = ReadCriteria(p);
        bool completed = false;
        bool failed = false;
        auto outer = s_mgr.criteria.find(c.challenge);

        if (outer != s_mgr.criteria.end()) {
            auto old = outer->second.find(c.requirement);

            if (old != outer->second.end()) {
                if (old->second.achieve == 0) {
                    completed = c.achieve != 0;
                } else if (old->second.active) {
                    failed = !(c.active || c.achieve != 0);
                }
            }
        }

        s_mgr.criteria[c.challenge][c.requirement] = c;
        const char* type = "CHALLENGE_REQUIREMENT_TYPE_NONE";
        double v[3] = { 0, 0, 0 };

        if (Row req = Requirements().Row(c.requirement)) {
            type = Str(Requirements(), req, 0x10);
            v[0] = F32(req, 0x14);
            v[1] = F32(req, 0x18);
            v[2] = F32(req, 0x1C);
        }

        CoA::Events::Signal("CHALLENGE_CRITERIA_UPDATED", "%u%u%s%f%f%f", c.challenge, c.level, type, v[0], v[1], v[2]);

        if (completed) {
            CoA::Events::Signal("CHALLENGE_CRITERIA_COMPLETED", "%u%u%s%f%f%f", c.challenge, c.level, type, v[0], v[1], v[2]);
        } else if (failed) {
            CoA::Events::Signal("CHALLENGE_CRITERIA_FAILED", "%u%u%s%f%f%f", c.challenge, c.level, type, v[0], v[1], v[2]);
        }

        return 1;
    }

    int32_t OnSyncRequest(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x59B
        s_mgr.pending.clear();
        const uint32_t arg = CoA::Read::U32(p);

        for (uint32_t n = CoA::Read::U32(p); n && CoA::Read::Remaining(p) >= kPendingSize; --n) {
            const uint32_t id = CoA::Read::U32(p);
            const uint32_t level = CoA::Read::U32(p);
            s_mgr.pending.emplace_back(id, level);
        }

        CoA::Events::Signal("CHALLENGE_SYNC_REQUEST", "%u", arg);
        return 1;
    }

    Failure ReadFailure(CDataStore* p) {   // FUN_1013f230
        Failure f;
        f.name = CoA::Read::Name(p);
        f.challenge = CoA::Read::U32(p);
        f.level = CoA::Read::U32(p);
        f.start = CoA::Read::U64(p);
        f.end = CoA::Read::U64(p);
        f.deaths = CoA::Read::U32(p);
        f.playerLevel = CoA::Read::U8(p);
        f.ruleset = CoA::Read::U8(p);
        f.map = CoA::Read::U32(p);
        f.difficulty = CoA::Read::U8(p);
        f.zone = CoA::Read::U32(p);
        f.area = CoA::Read::U32(p);

        for (float& v : f.pos) {
            v = CoA::Read::F32(p);
        }

        f.failReason = CoA::Read::Name(p);
        f.race = CoA::Read::Name(p);
        f.gender = CoA::Read::Name(p);
        f.cls = CoA::Read::Name(p);
        f.killerType = CoA::Read::Name(p);
        f.killerName = CoA::Read::Name(p);
        f.killerSpell = CoA::Read::U32(p);
        f.killerDamage = CoA::Read::U32(p);
        f.killerEntry = CoA::Read::U32(p);
        f.killerLevel = CoA::Read::U32(p);
        return f;
    }

    int32_t OnFailureList(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x5A2
        s_mgr.failures.clear();

        for (uint32_t n = CoA::Read::U32(p); n && CoA::Read::Remaining(p) >= kFailureMinSize; --n) {
            s_mgr.failures.push_back(ReadFailure(p));
        }

        CoA::Events::Signal("CHALLENGE_FAILURE_LIST_CHANGED", nullptr);
        return 1;
    }

    int32_t OnFailure(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x5A3
        s_mgr.failures.push_back(ReadFailure(p));
        const Failure& f = s_mgr.failures.back();
        CoA::Events::Signal("CHALLENGE_FAILURE_ADDED", "%u%u%u", f.challenge, f.level, static_cast<uint32_t>(s_mgr.failures.size() - 1));
        return 1;
    }

    Completion ReadCompletion(CDataStore* p) {   // FUN_1013efd0
        Completion c;
        c.challenge = CoA::Read::U32(p);
        c.level = CoA::Read::U32(p);
        c.player = CoA::Read::Name(p);
        c.start = CoA::Read::U64(p);
        c.complete = CoA::Read::U64(p);
        c.race = CoA::Read::Name(p);
        c.gender = CoA::Read::Name(p);
        c.cls = CoA::Read::Name(p);
        return c;
    }

    void SortCompletions(uint32_t id, uint32_t level) {   // FUN_10143520: fastest first
        auto c = s_mgr.completions.find(id);

        if (c == s_mgr.completions.end()) {
            return;
        }

        auto l = c->second.find(level);

        if (l == c->second.end()) {
            return;
        }

        std::sort(l->second.begin(), l->second.end(), [](const Completion& a, const Completion& b) { return a.complete - a.start < b.complete - b.start; });
    }

    int32_t OnCompletionList(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x5C7
        const uint32_t id = CoA::Read::U32(p);
        const uint32_t level = CoA::Read::U32(p);
        auto c = s_mgr.completions.find(id);

        if (c != s_mgr.completions.end()) {
            c->second.erase(level);
            CoA::Challenge::ScheduleRebuild();
        }

        for (uint32_t n = CoA::Read::U32(p); n && CoA::Read::Remaining(p) >= kCompletionMinSize; --n) {
            Completion r = ReadCompletion(p);
            s_mgr.completions[r.challenge][r.level].push_back(std::move(r));
        }

        SortCompletions(id, level);
        CoA::Events::Signal("CHALLENGE_COMPLETION_LIST_CHANGED", "%u%u", id, level);
        return 1;
    }

    int32_t OnCompletion(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x5C8
        Completion r = ReadCompletion(p);
        const uint32_t id = r.challenge;
        const uint32_t level = r.level;
        s_mgr.completions[id][level].push_back(std::move(r));
        SortCompletions(id, level);
        CoA::Events::Signal("CHALLENGE_COMPLETION_ADDED", "%u%u%u", id, level, static_cast<uint32_t>(s_mgr.completions[id][level].size() - 1));
        return 1;
    }

    int32_t OnBrokenRuleList(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x5BA
        s_mgr.brokenRules.clear();

        for (uint32_t n = CoA::Read::U32(p); n && CoA::Read::Remaining(p) >= kRuleMinSize; --n) {
            s_mgr.brokenRules.insert(CoA::Read::Name(p));
        }

        CoA::Challenge::ScheduleRebuild();
        return 1;
    }

    int32_t OnBrokenRuleUpdate(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x5BB
        const std::string rule = CoA::Read::Name(p);
        const uint8_t broken = CoA::Read::U8(p);

        if (broken) {
            s_mgr.brokenRules.insert(rule);
        } else {
            s_mgr.brokenRules.erase(rule);
        }

        CoA::Challenge::ScheduleRebuild();
        CoA::Events::Signal("CHALLENGE_RULE_BROKEN_UPDATE", "%s%b", rule.c_str(), static_cast<int>(broken));
        return 1;
    }

    int32_t OnStartResponse(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x593
        const uint32_t id = CoA::Read::U32(p);
        const uint32_t level = CoA::Read::U32(p);
        const uint32_t result = CoA::Read::U32(p);
        const uint64_t start = CoA::Read::U64(p);
        const uint8_t eligible = CoA::Read::U8(p);

        if (result == 0) {
            uint64_t guid = 0;

            if (!s_deps.unitTokenGuid || !s_deps.unitTokenGuid("player", guid)) {
                return 1;
            }

            // The player's GUID low part, as the packed-GUID helpers trim it.
            const uint32_t high = static_cast<uint32_t>(guid >> 32);
            uint32_t low = static_cast<uint32_t>(guid);

            if (high >= 0x10000000) {
                const uint32_t kind = high >> 16;

                if (!(kind == 0xF101 || kind == 0x1FC0 || kind == 0 || kind == 8000 || kind == 0x1F50 || kind == 0x4000 || kind == 0xF100)) {
                    low &= 0xFFFFFF;
                }
            }

            Active a;
            a.guid = low;
            a.id = id;
            a.level = level;
            a.enabled = true;
            a.startTime = start;
            a.eligible = eligible != 0;
            a.deaths = 0;
            s_mgr.active[id] = a;
        }

        CoA::Events::Signal("CHALLENGE_START_RESPONSE", "%u%u%s", id, level, StartResult(result).c_str());
        return 1;
    }

    int32_t OnStopResponse(void*, uint32_t, uint32_t, CDataStore* p) {   // 0x595
        const uint32_t id = CoA::Read::U32(p);
        const uint32_t level = CoA::Read::U32(p);
        const uint32_t result = CoA::Read::U32(p);

        if (result == 0) {
            s_mgr.active.erase(id);
        }

        CoA::Events::Signal("CHALLENGE_STOP_RESPONSE", "%u%u%s", id, level, StopResult(result).c_str());
        return 1;
    }

    // ---- SMSG_PATCH_CHALLENGE_*: mark the affected challenges (CHALLENGE_PATCHED two seconds later) and re-index a
    // second later.
    void MarkPatched(uint32_t id) {   // FUN_10135130
        s_mgr.patched.insert(id);
        s_mgr.patchedAt = Now() + 2;
    }

    void MarkGroup(uint32_t group) {   // FUN_10135180
        auto g = s_mgr.groups.find(group);

        if (g == s_mgr.groups.end()) {
            return;
        }

        std::vector<Info*> members = g->second;

        for (Info* info : members) {
            if (info->row) {
                MarkPatched(U32(info->row, 0));
            }
        }
    }

    void AfterChallenge(const uint8_t* r) {
        s_entries.Reset();
        CoA::Challenge::ScheduleRebuild();
        MarkPatched(U32(r, 0));
    }

    void AfterKeyed(const uint8_t* r) {   // (+4 challenge, +8 group)
        CoA::Challenge::ScheduleRebuild();

        if (U32(r, 4)) {
            MarkPatched(U32(r, 4));
        }

        if (U32(r, 8)) {
            MarkGroup(U32(r, 8));
        }
    }

    void AfterGroup(const uint8_t* r) {
        CoA::Challenge::ScheduleRebuild();
        MarkPatched(U32(r, 4));
    }

    void AfterFeatured(const uint8_t* r) {
        for (uint32_t off : { 8u, 0xCu, 0x10u }) {
            if (U32(r, off)) {
                MarkPatched(U32(r, off));
            }
        }
    }

    void AfterGroupReward(const uint8_t* r) {
        CoA::Challenge::ScheduleRebuild();

        if (U32(r, 4)) {
            MarkGroup(U32(r, 4));
        }
    }

    // The tick after client 0x403340 (0x1013BE30).
    void Tick() {
        const int64_t now = Now();

        if (!s_mgr.patched.empty() && now > s_mgr.patchedAt) {
            CoA::Events::Signal("CHALLENGE_PATCHED", nullptr);
            s_mgr.patched.clear();
        }

        if (s_mgr.rebuildAt != 0 && now > s_mgr.rebuildAt) {
            Rebuild();
        }
    }

    void OnEnteredWorld() {   // 0x1013BF00
        if (!s_mgr.enteredWorld) {
            CoA::Challenge::ScheduleRebuild();
            s_mgr.enteredWorld = true;
        }
    }

    void OnGlueScreen() {   // FUN_10135010
        s_mgr.patched.clear();
        s_mgr.active.clear();
        s_mgr.completed.clear();
        s_mgr.criteria.clear();
        s_mgr.pending.clear();
        s_mgr.failures.clear();
        s_mgr.completions.clear();
        s_mgr.brokenRules.clear();
        s_entries.Reset();
        s_mgr.rebuildAt = 0;
        s_mgr.enteredWorld = false;
        s_mgr.filterMap = -1;
        s_mgr.filterDifficulty = -1;
    }
}

// ---- detail ----
Table& CoA::Challenge::detail::Challenges() {
    return T("DBFilesClient\\Challenge.dbc");
}

Table& CoA::Challenge::detail::Groups() {
    return T("DBFilesClient\\ChallengeGroups.dbc");
}

Table& CoA::Challenge::detail::Levels() {
    return T("DBFilesClient\\ChallengeLevels.dbc");
}

Table& CoA::Challenge::detail::Conditions() {
    return T("DBFilesClient\\ChallengeConditions.dbc");
}

Table& CoA::Challenge::detail::Requirements() {
    return T("DBFilesClient\\ChallengeRequirements.dbc");
}

Table& CoA::Challenge::detail::Rules() {
    return T("DBFilesClient\\ChallengeRules.dbc");
}

Table& CoA::Challenge::detail::Spells() {
    return T("DBFilesClient\\ChallengeSpells.dbc");
}

Table& CoA::Challenge::detail::Modifiers() {
    return T("DBFilesClient\\ChallengeModifiers.dbc");
}

Table& CoA::Challenge::detail::Rewards() {
    return T("DBFilesClient\\ChallengeRewards.dbc");
}

Table& CoA::Challenge::detail::GroupRewards() {
    return T("DBFilesClient\\ChallengeGroupRewards.dbc");
}

Table& CoA::Challenge::detail::Featured() {
    return T("DBFilesClient\\ChallengeFeatured.dbc");
}

Table& CoA::Challenge::detail::ConditionTypes() {
    return T("DBFilesClient\\ChallengeConditionTypes.dbc");
}

Table& CoA::Challenge::detail::RequirementTypes() {
    return T("DBFilesClient\\ChallengeRequirementTypes.dbc");
}

Table& CoA::Challenge::detail::RuleTypes() {
    return T("DBFilesClient\\ChallengeRuleTypes.dbc");
}

Table& CoA::Challenge::detail::ModifierTypes() {
    return T("DBFilesClient\\ChallengeModifierTypes.dbc");
}

Mgr& CoA::Challenge::detail::M() {
    return s_mgr;
}

CoA::Challenge::Deps& CoA::Challenge::detail::D() {
    return s_deps;
}

bool CoA::Challenge::detail::HasPlayer() {
    return s_deps.hasPlayer && s_deps.hasPlayer();
}

uint32_t CoA::Challenge::detail::PlayerLevel() {
    if (!HasPlayer()) {
        return 0;
    }

    return s_deps.playerLevel ? s_deps.playerLevel() : 0;
}

int64_t CoA::Challenge::detail::Now() {
    return s_deps.now ? s_deps.now() : 0;
}

int32_t CoA::Challenge::detail::CurrentMap() {   // 0xBD088C
    return s_deps.currentMap ? s_deps.currentMap() : 0;
}

int32_t CoA::Challenge::detail::InstanceDifficulty() {   // 0xBD0894
    return s_deps.instanceDifficulty ? s_deps.instanceDifficulty() : 0;
}

Info* CoA::Challenge::detail::FindInfo(uint32_t id) {
    auto it = s_mgr.challenges.find(id);
    return it == s_mgr.challenges.end() ? nullptr : &it->second;
}

LevelInfo* CoA::Challenge::detail::FindLevel(uint32_t id, uint32_t level) {
    Info* info = FindInfo(id);

    if (!info) {
        return nullptr;
    }

    auto it = info->levels.find(level);
    return it == info->levels.end() ? nullptr : &it->second;
}

// FUN_1013b9c0: a completed record for (challenge, level).
bool CoA::Challenge::detail::IsCompleted(uint32_t id, uint32_t level) {
    auto range = s_mgr.completed.equal_range(id);

    for (auto it = range.first; it != range.second; ++it) {
        if (it->second.level == level) {
            return true;
        }
    }

    return false;
}

// FUN_1013bab0: the row whose week is trunc(tm_yday / 7.0f), UTC.
bool CoA::Challenge::detail::IsFeatured(uint32_t id) {
    if (id == 0) {
        return false;
    }

    const time_t now = static_cast<time_t>(Now());
    tm t = {};
    gmtime_s(&t, &now);
    const uint32_t week = static_cast<uint32_t>(static_cast<int64_t>(static_cast<float>(t.tm_yday) / 7.0f));
    Table& f = Featured();

    if (!f.Loaded()) {
        return false;
    }

    for (uint32_t i = f.MinId(); i < f.MaxId() + 1; ++i) {
        if (Row r = f.Row(i)) {
            if (U32(r, 4) == week) {
                return U32(r, 8) == id || U32(r, 0xC) == id || U32(r, 0x10) == id;
            }
        }
    }

    return false;
}

// FUN_101b7900 (condition row: +0x14 type, +0x18..+0x20 values, +0x24 negative).
bool CoA::Challenge::detail::ConditionMet(Row c) {
    int type = Index(kChallengeConditions, Str(Conditions(), c, 0x14));

    if (type < 0) {
        CoA::Log::Printf("Unexpected Value: %s", Str(Conditions(), c, 0x14));
        type = 0;
    }

    const bool neg = U32(c, 0x24) != 0;
    const float v1 = F32(c, 0x18);
    const float v2 = F32(c, 0x1C);
    const float v3 = F32(c, 0x20);
    const uint32_t mode = CoA::GameMode::Mode();
    auto clear = [&](int bit) { return (((mode >> bit) & 1) == 0) != neg; };

    switch (type) {
        case 1:
            return clear(0xF);
        case 2:
            if ((mode & 0x10000) || (HasPlayer() && PlayerLevel() != 1)) {
                return neg;
            }

            break;
        case 3:
            return clear(0x11);
        case 4:
            return clear(0x12);
        case 5:
            return clear(0x13);
        case 6:
            return clear(0x14);
        case 7:
            return clear(0x15);
        case 8:
            return IsCompleted(static_cast<uint32_t>(static_cast<int64_t>(v1)), static_cast<uint32_t>(static_cast<int64_t>(v2))) != neg;
        case 9:
            return GroupCompletedCount(static_cast<uint32_t>(static_cast<int64_t>(v1)), static_cast<uint32_t>(static_cast<int64_t>(v2)),
                       static_cast<uint32_t>(static_cast<int64_t>(v3))) != neg;
        case 10:
            return (static_cast<uint32_t>(static_cast<int64_t>(v1)) <= Prestige()) != neg;
        case 0xB: {
            const uint8_t size = static_cast<uint8_t>(PartyMembers() + 1);
            const uint8_t want = static_cast<uint8_t>(static_cast<int32_t>(v1));

            if (size < 2) {
                return (want < 2) != neg;
            }

            return (size == want) != neg;
        }
        case 0xC:
            return ((mode & 0x3F8000) == 0) != neg;
        case 0xD:
            return clear(0x16);
        case 0x10:
            return clear(0x17);
        default:
            break;
    }

    return !neg;
}

Info* CoA::Challenge::detail::InfoSorted(uint32_t id, bool sort) {
    Info* info = FindInfo(id);

    if (info && sort) {
        for (auto& l : info->levels) {
            SortConditions(l.second.conditions);
        }
    }

    return info;
}

LevelInfo* CoA::Challenge::detail::LevelSorted(uint32_t id, uint32_t level, bool sort) {
    LevelInfo* li = FindLevel(id, level);

    if (li && sort) {
        SortConditions(li->conditions);
    }

    return li;
}

int CoA::Challenge::detail::RequirementType(const char* s) {
    return Index(kChallengeRequirements, s);
}

Row CoA::Challenge::detail::ChallengeRow(uint32_t id) {
    return Challenges().Row(id);
}

// FUN_10133dc0: every reason `id` at `level` cannot be started (CHALLENGE_START_*), minus `ignore`.
bool CoA::Challenge::detail::CanActivate(uint32_t id, uint32_t level, std::vector<uint32_t>& errors, const std::set<uint32_t>& ignore) {
    auto add = [&](uint32_t code) {
        if (!ignore.count(code)) {
            errors.push_back(code);
        }
    };

    Row row = ChallengeRow(id);

    if (!row) {
        add(1);
    } else {
        if (!ignore.count(3) && s_mgr.active.count(id)) {
            errors.push_back(3);
        }

        if (!ignore.count(4) && U32(row, 0x18) != 0) {
            for (auto& a : s_mgr.active) {
                if (Row other = ChallengeRow(a.second.id)) {
                    if (U32(other, 0x18) == U32(row, 0x18)) {
                        errors.push_back(4);
                        break;
                    }
                }
            }
        }

        if (!ignore.count(5) && U32(row, 0x24) != 0 && !CoA::GameEvents::IsActive(static_cast<uint16_t>(U32(row, 0x24)))) {
            errors.push_back(5);
        }

        if (!ignore.count(6) && (U32(row, 0x14) & 9) != 0) {
            errors.push_back(6);
        }

        if (!ignore.count(8) && level > 1 && (U32(row, 0x14) & 0x12) != 0 && !IsCompleted(id, level - 1)) {
            errors.push_back(8);
        }

        if (!ignore.count(9) && PartyLeader() != 0 && ActivePlayerGuid() != PartyLeader()) {
            errors.push_back(9);
        }

        if (!ignore.count(0xA) && U32(row, 0x38) == 0 && Prestige() != 0) {
            errors.push_back(0xA);
        }

        if (!ignore.count(0xB) && U32(row, 0x38) == 0 && (CoA::GameMode::ActiveMask() & 0x3F8000) != 0) {
            errors.push_back(0xB);
        }

        if (!ignore.count(0xC)) {
            if (LevelInfo* li = FindLevel(id, level)) {
                for (Row r : li->rules) {
                    if (s_mgr.brokenRules.count(Str(Rules(), r, 0x10))) {
                        add(0xC);
                        break;
                    }
                }
            }
        }

        if (!ignore.count(2) && U32(row, 0xC) != 0) {
            // RequiredGameMode (+0x10) names one of the realm game modes; its bit must be active. Unknown names and
            // "None" require that no restricted mode (0x19EF) is active.
            static const uint32_t kMasks[7] = { 0x0, 0x1, 0x2, 0x4, 0x8, 0x20, 0x40 };   // 0x10B31900
            const int m = IndexOr(kChallengeGameModes, Str(Challenges(), row, 0x10), -1);
            const uint32_t mask = m >= 0 ? kMasks[m] : 0;
            const uint32_t mode = CoA::GameMode::Mode();

            if (mask != 0 ? (mode & mask) == 0 : (mode & 0x19EF) != 0) {
                errors.push_back(2);
            }
        }

        if (!ignore.count(7)) {
            if (LevelInfo* li = FindLevel(id, level)) {
                for (Row c : li->conditions) {
                    if (!ConditionMet(c)) {
                        add(7);
                        break;
                    }
                }
            }
        }

        const bool trial = U32(row, 0x30) != 0;
        const bool dungeon = U32(row, 0x34) != 0;

        if (!ignore.count(0xD) && !trial && !dungeon) {
            for (auto& p : s_mgr.pending) {
                if (Row pr = ChallengeRow(p.first)) {
                    if (U32(pr, 0x30) != 0) {
                        add(0xD);
                        break;
                    }
                }
            }
        }

        if (!ignore.count(0xE) && trial) {
            for (auto& p : s_mgr.pending) {
                if (Row pr = ChallengeRow(p.first)) {
                    if (U32(pr, 0x30) == 0 && U32(pr, 0x34) == 0) {
                        add(0xE);
                        break;
                    }
                }
            }
        }

        if (!ignore.count(0x11) && !trial && !dungeon) {
            add(0x11);
        }

        if (U32(row, 0x40) != 0) {
            if (!ignore.count(0x13) && U32(row, 0x44) != static_cast<uint32_t>(CurrentMap())) {
                add(0x13);
            }

            if (!ignore.count(0x14) && U32(row, 0x48) != static_cast<uint32_t>(InstanceDifficulty())) {
                add(0x14);
            }
        }

        if (!ignore.count(0x17)) {
            LevelInfo* li = FindLevel(id, level);

            if (!li || !li->row) {
                add(0x17);
            } else if (U32(li->row, 0x10) != 0 && !CoA::GameEvents::IsActive(static_cast<uint16_t>(U32(li->row, 0x10)))) {
                add(0x17);
            }
        }
    }

    if (!ignore.count(0xF) && CoA::TrialCreator::HasActiveTrial()) {
        add(0xF);
    }

    if (!ignore.count(0x10)) {
        const int32_t* forced = CoA::Config::Int("CONFIG_CHALLENGE_FORCED");
        const int32_t* forcedLevel = CoA::Config::Int("CONFIG_CHALLENGE_FORCED_LEVEL");

        if (forced && forcedLevel && *forced != 0 && !(static_cast<uint32_t>(*forced) == id && static_cast<uint32_t>(*forcedLevel) == level)) {
            add(0x10);
        }
    }

    return errors.empty();
}

// FUN_10134a80: every reason `id` cannot be stopped (CHALLENGE_STOP_*).
bool CoA::Challenge::detail::CanDeactivate(uint32_t id, std::vector<uint32_t>& errors, const std::set<uint32_t>& ignore) {
    auto add = [&](uint32_t code) {
        if (!ignore.count(code)) {
            errors.push_back(code);
        }
    };

    auto active = s_mgr.active.find(id);

    if (active == s_mgr.active.end()) {
        add(1);
    }

    bool permadeath = false;

    if (HasPlayer() && !AtMaxLevel()) {
        if (const bool* on = CoA::Config::Bool("CONFIG_CHALLENGE_PERMADEATH_ENABLED")) {
            permadeath = *on;
        }
    }

    if (permadeath && active != s_mgr.active.end() && active->second.deaths != 0) {
        if (LevelInfo* li = FindLevel(id, active->second.level)) {
            for (Row r : li->requirements) {
                if (RequirementType(Str(Requirements(), r, 0x10)) == 3) {
                    add(2);
                    break;
                }
            }
        }
    }

    if (PartyLeader() != 0 && ActivePlayerGuid() != PartyLeader()) {
        add(3);
    }

    if (CoA::TrialCreator::HasActiveTrial()) {
        add(4);
    }

    const int32_t* forced = CoA::Config::Int("CONFIG_CHALLENGE_FORCED");
    const int32_t* forcedLevel = CoA::Config::Int("CONFIG_CHALLENGE_FORCED_LEVEL");

    if (forced && forcedLevel && *forced != 0 && static_cast<uint32_t>(*forced) == id) {
        const uint32_t level = active != s_mgr.active.end() ? active->second.level : 0;

        if (static_cast<uint32_t>(*forcedLevel) == level) {
            add(5);
        }
    }

    return errors.empty();
}

std::string CoA::Challenge::detail::StartResult(uint32_t code) {
    if (code < 0x18) {
        return kChallengeStartResults[code];
    }

    return "UNEXPECTED_ENUM_VALUE_" + std::to_string(code);
}

std::string CoA::Challenge::detail::StopResult(uint32_t code) {
    if (code < 6) {
        return kChallengeStopResults[code];
    }

    return "UNEXPECTED_ENUM_VALUE_" + std::to_string(code);
}

// FUN_1012ae20 / FUN_101b86c0: active first, then dungeon trials, then featured, then SortOrder.
bool CoA::Challenge::detail::ChallengeBefore(Row a, Row b) {
    const bool aa = s_mgr.active.count(U32(a, 0)) != 0;
    const bool ab = s_mgr.active.count(U32(b, 0)) != 0;

    if (aa != ab) {
        return aa;
    }

    if (U32(a, 0x34) != 0) {
        return true;
    }

    if (U32(b, 0x34) != 0) {
        return false;
    }

    if (IsFeatured(U32(a, 0))) {
        return true;
    }

    if (IsFeatured(U32(b, 0))) {
        return false;
    }

    return U32(a, 0x4C) < U32(b, 0x4C);
}

// The list filter GetAllChallenges / GetAllTrials / the container share: hidden (flags bit 0) never; "check activation"
// (bit 2) only when startable at level 1; map-bound ones only on their map or while active.
bool CoA::Challenge::detail::Listed(Row r) {
    if (U32(r, 0x14) & 1) {
        return false;
    }

    std::set<uint32_t> none;

    if (U32(r, 0x14) & 4) {
        std::vector<uint32_t> e;

        if (!CanActivate(U32(r, 0), 1, e, none)) {
            return false;
        }
    }

    if (U32(r, 0x40) != 0 && !s_mgr.active.count(U32(r, 0))) {
        std::vector<uint32_t> e;
        CanActivate(U32(r, 0), 1, e, none);

        if (std::find(e.begin(), e.end(), 0x13u) != e.end() || std::find(e.begin(), e.end(), 0x14u) != e.end()) {
            return false;
        }
    }

    return true;
}

CoA::Filter::Container<Row>& CoA::Challenge::detail::Entries() {
    return s_entries;
}

// ---- exported ----
CoA::Challenge::Deps& CoA::Challenge::Dependencies() {
    return s_deps;
}

bool CoA::Challenge::CanActivate(uint32_t id, uint32_t level, const std::set<uint32_t>& ignore) {
    std::vector<uint32_t> errors;
    return detail::CanActivate(id, level, errors, ignore);
}

bool CoA::Challenge::CanDeactivate(uint32_t id, const std::set<uint32_t>& ignore) {
    std::vector<uint32_t> errors;
    return detail::CanDeactivate(id, errors, ignore);
}

std::vector<uint32_t> CoA::Challenge::ActiveIds() {
    std::vector<uint32_t> out;

    for (auto& a : s_mgr.active) {
        out.push_back(a.second.id);
    }

    return out;
}

const uint8_t* CoA::Challenge::ChallengeRowOf(uint32_t id) {
    return ChallengeRow(id);
}

int CoA::Challenge::Difficulty(const uint8_t* row) {
    return IndexOr(kChallengeDifficulties, Str(Challenges(), row, 0x2C), 0);
}

bool CoA::Challenge::HasSoloRule(uint32_t id, uint32_t level) {
    if (LevelInfo* li = FindLevel(id, level)) {
        for (Row rule : li->rules) {
            if (IndexOr(kChallengeRules, Str(Rules(), rule, 0x10), 2) == 2) {
                return true;
            }
        }
    }

    return false;
}

bool CoA::Challenge::ActiveHasRule(uint32_t type) {
    for (auto& a : s_mgr.active) {
        if (LevelInfo* li = FindLevel(a.second.id, a.second.level)) {
            for (Row rule : li->rules) {
                if (static_cast<uint32_t>(IndexOr(kChallengeRules, Str(Rules(), rule, 0x10), 0)) == type) {
                    return true;
                }
            }
        }
    }

    return false;
}

void CoA::Challenge::ScheduleRebuild() {   // FUN_10143630(false)
    s_mgr.rebuildAt = Now() + 1;
}

void CoA::Challenge::Init() {
    detail::InitNatives();
    COA_PACKET(0x596, OnActiveList);
    COA_PACKET(0x5A6, OnActiveUpdate);
    COA_PACKET(0x597, OnCompletedList);
    COA_PACKET(0x598, OnCompleted);
    COA_PACKET(0x599, OnCriteriaList);
    COA_PACKET(0x59A, OnCriteriaUpdate);
    COA_PACKET(0x59B, OnSyncRequest);
    COA_PACKET(0x5A2, OnFailureList);
    COA_PACKET(0x5A3, OnFailure);
    COA_PACKET(0x5C7, OnCompletionList);
    COA_PACKET(0x5C8, OnCompletion);
    COA_PACKET(0x5BA, OnBrokenRuleList);
    COA_PACKET(0x5BB, OnBrokenRuleUpdate);
    COA_PACKET(0x593, OnStartResponse);
    COA_PACKET(0x595, OnStopResponse);

    // Struct images with u32-length strings (FUN_100d3680), except the modifier tables' C strings.
    CoA::DbcPatch::RegisterSized(0x591, "DBFilesClient\\Challenge.dbc", 0x54, (1ull << 4) | (1ull << 7) | (1ull << 8) | (1ull << 10) | (1ull << 11) | (1ull << 20), AfterChallenge);
    CoA::DbcPatch::RegisterSized(0x5B2, "DBFilesClient\\ChallengeConditions.dbc", 0x28, 1ull << 5, AfterKeyed);
    CoA::DbcPatch::RegisterSized(0x5B3, "DBFilesClient\\ChallengeFeatured.dbc", 0x14, 0, AfterFeatured);
    CoA::DbcPatch::RegisterSized(0x5B4, "DBFilesClient\\ChallengeGroupRewards.dbc", 0x28, (1ull << 4) | (1ull << 5), AfterGroupReward);
    CoA::DbcPatch::RegisterSized(0x5B5, "DBFilesClient\\ChallengeGroups.dbc", 0xC, 0, AfterGroup);
    CoA::DbcPatch::RegisterSized(0x5B6, "DBFilesClient\\ChallengeRequirements.dbc", 0x24, 1ull << 4, AfterKeyed);
    CoA::DbcPatch::RegisterSized(0x5B7, "DBFilesClient\\ChallengeRewards.dbc", 0x2C, (1ull << 4) | (1ull << 5), AfterKeyed);
    CoA::DbcPatch::RegisterSized(0x5B8, "DBFilesClient\\ChallengeRules.dbc", 0x14, 1ull << 4, AfterKeyed);
    CoA::DbcPatch::RegisterSized(0x5B9, "DBFilesClient\\ChallengeSpells.dbc", 0x28, 0, AfterKeyed);
    CoA::DbcPatch::RegisterSized(0x5BC, "DBFilesClient\\ChallengeRequirementTypes.dbc", 0x24, 0x1FEull, nullptr);
    CoA::DbcPatch::RegisterSized(0x5BE, "DBFilesClient\\ChallengeConditionTypes.dbc", 0x24, 0x1FEull, nullptr);
    CoA::DbcPatch::RegisterSized(0x5BD, "DBFilesClient\\ChallengeRuleTypes.dbc", 0x10, 0xEull, nullptr);
    CoA::DbcPatch::RegisterSized(0x677, "DBFilesClient\\ChallengeLevels.dbc", 0x14, 0, AfterKeyed);
    CoA::DbcPatch::Register(0x663, "DBFilesClient\\ChallengeModifiers.dbc", 0x20, 1ull << 4, AfterKeyed);
    CoA::DbcPatch::Register(0x664, "DBFilesClient\\ChallengeModifierTypes.dbc", 0x14, 0x1Eull, nullptr);

    CoA::Lifecycle::OnGlueScreen(OnGlueScreen);
    CoA::ClientHooks::afterEnterWorld.Add(OnEnteredWorld);
    CoA::Lifecycle::OnAfterIdle(Tick);
    // The attach hooks after 0x5B3020 / 0x5B32F0 / 0x5B3610 (AscAttachMisc.cpp) and AscAttachFixes' rule test.
    CoA::AttachMisc::achievementsChanged.Add(CoA::Challenge::ScheduleRebuild);
    CoA::AttachFixes::Dependencies().challengeHasRule = CoA::Challenge::ActiveHasRule;

    COA_EVENT("CHALLENGE_ACTIVE_LIST_CHANGED");
    COA_EVENT("CHALLENGE_DEATH_UPDATE");
    COA_EVENT("CHALLENGE_COMPLETED_LIST_CHANGED");
    COA_EVENT("CHALLENGE_COMPLETED");
    COA_EVENT("CHALLENGE_CRITERIA_LIST_CHANGED");
    COA_EVENT("CHALLENGE_CRITERIA_UPDATED");
    COA_EVENT("CHALLENGE_CRITERIA_COMPLETED");
    COA_EVENT("CHALLENGE_CRITERIA_FAILED");
    COA_EVENT("CHALLENGE_SYNC_REQUEST");
    COA_EVENT("CHALLENGE_FAILURE_LIST_CHANGED");
    COA_EVENT("CHALLENGE_FAILURE_ADDED");
    COA_EVENT("CHALLENGE_COMPLETION_LIST_CHANGED");
    COA_EVENT("CHALLENGE_COMPLETION_ADDED");
    COA_EVENT("CHALLENGE_RULE_BROKEN_UPDATE");
    COA_EVENT("CHALLENGE_START_RESPONSE");
    COA_EVENT("CHALLENGE_STOP_RESPONSE");
    COA_EVENT("CHALLENGE_PATCHED");
}

void CoA::Challenge::Reset() {
    OnGlueScreen();
    s_mgr.patchedAt = 0;
    s_mgr.challenges.clear();
    s_mgr.groups.clear();
}
