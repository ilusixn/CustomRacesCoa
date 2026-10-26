#ifndef COA_SOURCE_MODULES_CHALLENGEINTERNAL_H
#define COA_SOURCE_MODULES_CHALLENGEINTERNAL_H

// What Challenge.cpp and ChallengeNatives.cpp share (the reconstruction's AscChallenge.cpp anonymous namespace). Not
// for other modules: they use Challenge.h.
#include "CoA/Source/Modules/Challenge.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/FilterContainer.h"
#include "CoA/Source/Runtime/Log.h"
#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace CoA::Challenge::detail {
    using Row = const uint8_t*;

    // The 32-bit build's std::hash<uint32_t> (FNV-1a over the four bytes, 32-bit basis and prime). The x64 std::hash
    // uses the 64-bit constants, which would place the keys in other buckets; with this one MSVC's bucket policy gives
    // the original's iteration order (GetActiveChallenges, GetCompletedChallenges, GetChallengeCriterias, ActiveIds).
    struct Fnv32 {
        size_t operator()(uint32_t v) const {
            uint32_t h = 2166136261u;

            for (int i = 0; i < 4; ++i) {
                h ^= (v >> (i * 8)) & 0xFF;
                h *= 16777619u;
            }

            return h;
        }
    };

    template <class V>
    using U32Map = std::unordered_map<uint32_t, V, Fnv32>;

    inline uint32_t U32(Row r, uint32_t off) {
        return CoA::Dbc::Table::U32(r, off);
    }

    inline float F32(Row r, uint32_t off) {
        return CoA::Dbc::Table::F32(r, off);
    }

    inline const char* Str(CoA::Dbc::Table& t, Row r, uint32_t off) {
        return t.Str(r, off);
    }

    CoA::Dbc::Table& Challenges();         // DBFilesClient\Challenge.dbc
    CoA::Dbc::Table& Groups();             // ChallengeGroups.dbc
    CoA::Dbc::Table& Levels();             // ChallengeLevels.dbc
    CoA::Dbc::Table& Conditions();         // ChallengeConditions.dbc
    CoA::Dbc::Table& Requirements();       // ChallengeRequirements.dbc
    CoA::Dbc::Table& Rules();              // ChallengeRules.dbc
    CoA::Dbc::Table& Spells();             // ChallengeSpells.dbc
    CoA::Dbc::Table& Modifiers();          // ChallengeModifiers.dbc
    CoA::Dbc::Table& Rewards();            // ChallengeRewards.dbc
    CoA::Dbc::Table& GroupRewards();       // ChallengeGroupRewards.dbc
    CoA::Dbc::Table& Featured();           // ChallengeFeatured.dbc
    CoA::Dbc::Table& ConditionTypes();     // ChallengeConditionTypes.dbc
    CoA::Dbc::Table& RequirementTypes();   // ChallengeRequirementTypes.dbc
    CoA::Dbc::Table& RuleTypes();          // ChallengeRuleTypes.dbc
    CoA::Dbc::Table& ModifierTypes();      // ChallengeModifierTypes.dbc

    // Ids min..max, the original's loops.
    template <class Fn>
    void ForEachRow(CoA::Dbc::Table& t, Fn fn) {
        if (!t.Loaded()) {
            return;
        }

        for (uint32_t id = t.MinId(); id <= t.MaxId(); ++id) {
            if (Row r = t.Row(id)) {
                fn(r);
            }
        }
    }

    // FUN_10087260 over a {name, length} table; -1 when absent.
    template <size_t N>
    int Index(const char* const (&table)[N], const char* s) {
        for (size_t i = 0; i < N; ++i) {
            if (strcmp(table[i], s ? s : "") == 0) {
                return static_cast<int>(i);
            }
        }

        return -1;
    }

    // The enum parsers log "Unexpected Value: {}" and fall back to the caller's default.
    template <size_t N>
    int IndexOr(const char* const (&table)[N], const char* s, int fallback) {
        const int i = Index(table, s);

        if (i < 0) {
            CoA::Log::Printf("Unexpected Value: %s", s ? s : "");
            return fallback;
        }

        return i;
    }

    // ---- cached data stores ----
    struct Reward {   // 0x12 bytes packed in the original
        uint32_t item = 0;
        uint32_t amount = 0;
        uint32_t achievement = 0;
        bool firstCompletion = false;
        uint32_t sort = 0;      // +0x0D
        bool special = false;   // +0x11
    };

    struct Modifier {
        uint32_t type = 0;
        float v[3] = { 0, 0, 0 };
    };

    struct LevelInfo {
        Row row = nullptr;
        std::vector<Row> conditions;
        std::vector<Row> requirements;
        std::vector<Row> firstRewards;
        std::vector<Row> repeatRewards;
        std::vector<Row> groupRewards;
        std::vector<Row> rules;
        std::vector<Row> spells;
        std::vector<Row> modifierRows;
        std::vector<Reward> rewards;
        std::vector<Modifier> modifiers;
    };

    struct Info {
        Row row = nullptr;
        std::vector<Row> groups;
        U32Map<LevelInfo> levels;
        uint32_t minLevel = 0;
        uint32_t maxLevel = 0;
    };

    // ---- server state ----
    struct Active {   // 0x1A bytes packed
        uint32_t guid = 0;
        uint32_t id = 0;
        uint32_t level = 0;
        bool enabled = false;
        uint64_t startTime = 0;
        bool eligible = false;
        uint32_t deaths = 0;
    };

    struct Completed {
        uint32_t guid = 0;
        uint32_t id = 0;
        uint32_t level = 0;
        uint64_t start = 0;
        uint64_t end = 0;
    };

    struct Criteria {
        uint32_t guid = 0;
        uint32_t challenge = 0;
        uint32_t level = 0;
        uint32_t requirement = 0;
        uint64_t start = 0;
        uint64_t achieve = 0;
        bool active = false;
    };

    struct Failure {   // 0xEF bytes packed
        std::string name;
        uint32_t challenge = 0;
        uint32_t level = 0;
        uint64_t start = 0;
        uint64_t end = 0;
        uint32_t deaths = 0;
        uint8_t playerLevel = 0;
        uint8_t ruleset = 0;
        uint32_t map = 0;
        uint8_t difficulty = 0;
        uint32_t zone = 0;
        uint32_t area = 0;
        float pos[3] = { 0, 0, 0 };
        std::string failReason;
        std::string race;
        std::string gender;
        std::string cls;
        std::string killerType;
        std::string killerName;
        uint32_t killerSpell = 0;
        uint32_t killerDamage = 0;
        uint32_t killerEntry = 0;
        uint32_t killerLevel = 0;
    };

    struct Completion {   // 0x78 bytes
        uint32_t challenge = 0;
        uint32_t level = 0;
        std::string player;
        uint64_t start = 0;
        uint64_t complete = 0;
        std::string race;
        std::string gender;
        std::string cls;
    };

    struct Mgr {
        int64_t patchedAt = 0;                                     // +0x00
        std::unordered_set<uint32_t, Fnv32> patched;               // +0x04
        U32Map<Active> active;                                     // +0x28
        std::unordered_multimap<uint32_t, Completed, Fnv32> completed;   // +0x48
        U32Map<U32Map<Criteria>> criteria;                         // +0x68
        std::vector<std::pair<uint32_t, uint32_t>> pending;        // +0x88
        std::vector<Failure> failures;                             // +0x94
        U32Map<U32Map<std::vector<Completion>>> completions;       // +0xA0
        std::unordered_set<std::string> brokenRules;               // +0xC0
        U32Map<Info> challenges;                                   // +0x158
        U32Map<std::vector<Info*>> groups;                         // +0x178
        int64_t rebuildAt = 0;                                     // +0x1A0
        bool enteredWorld = false;                                 // +0x1A8
        int32_t filterMap = -1;                                    // +0x1AC
        int32_t filterDifficulty = -1;                             // +0x1B0
    };

    Mgr& M();
    CoA::Challenge::Deps& D();

    // ---- the client, through Deps ----
    bool HasPlayer();
    uint32_t PlayerLevel();
    int64_t Now();
    int32_t CurrentMap();
    int32_t InstanceDifficulty();

    // ---- lookups ----
    Info* FindInfo(uint32_t id);                       // lambda 2 / FUN_10145ff0
    LevelInfo* FindLevel(uint32_t id, uint32_t level);   // lambda 4 / FUN_101460a0
    bool IsCompleted(uint32_t id, uint32_t level);     // FUN_1013b9c0
    bool IsFeatured(uint32_t id);                      // FUN_1013bab0
    bool ConditionMet(Row c);                          // FUN_101b7900
    Info* InfoSorted(uint32_t id, bool sort);          // FUN_10136810
    LevelInfo* LevelSorted(uint32_t id, uint32_t level, bool sort);   // FUN_101368f0
    int RequirementType(const char* s);                // FUN_10130f40
    Row ChallengeRow(uint32_t id);

    // FUN_10133dc0 / FUN_10134a80: every reason, minus `ignore`.
    bool CanActivate(uint32_t id, uint32_t level, std::vector<uint32_t>& errors, const std::set<uint32_t>& ignore);
    bool CanDeactivate(uint32_t id, std::vector<uint32_t>& errors, const std::set<uint32_t>& ignore);
    std::string StartResult(uint32_t code);   // FUN_10099eb0
    std::string StopResult(uint32_t code);    // FUN_1009a050

    bool ChallengeBefore(Row a, Row b);   // FUN_1012ae20 / FUN_101b86c0
    bool Listed(Row r);

    CoA::Filter::Container<Row>& Entries();   // ChallengeEntryContainer (vtable 0x10B32484)

    void InitNatives();
}

#endif
