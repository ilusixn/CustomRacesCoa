#ifndef COA_SOURCE_MODULES_CHALLENGE_H
#define COA_SOURCE_MODULES_CHALLENGE_H

#include <cstdint>
#include <set>
#include <utility>
#include <vector>

struct lua_State;

// AscChallenge.cpp: C_Challenge -- the original's ChallengeMgr (accessor FUN_1013b8b0, ctor FUN_10132340, module init
// FUN_1013b5b0: 18 events, 30 packet handlers, the tick 0x1013BE30 after client 0x403340, enter-world 0x1013BF00, glue
// reset FUN_10135010).
//
// Static data (Challenge*.dbc, hot-patched by SMSG_PATCH_CHALLENGE_*) is indexed by ChallengeMgr::UpdateChallengeDataStores
// (FUN_10143630) into
//   challenge id -> CachedChallengeInfo {+0 Challenge row, +4 ChallengeGroups rows, +0x10 level map, +0x30 min level,
//                   +0x34 max level}                                                                (manager +0x158)
//   group id     -> challenges in it                                                                (manager +0x178)
//   level        -> CachedChallengeLevelInfo {+0 ChallengeLevels row, +4 conditions, +0x10 requirements, +0x1C
//                   first-completion rewards, +0x28 repeat rewards, +0x34 group rewards, +0x40 rules, +0x4C spells,
//                   +0x58 computed rewards (0x12 each), +0x64 modifier rows, +0x70 summed modifiers (0x10 each)}
// A row keyed (challenge, group 0, level) belongs to that challenge; (x, group, level) to every challenge of the group
// (lambdas 2..5 of UpdateChallengeDataStores).
//
// Server state: active challenges (+0x28, SMSG 0x596 / 0x5A6 / 0x593 / 0x595), completed (+0x48 multimap, 0x597 /
// 0x598), criteria (+0x68, 0x599 / 0x59A), pending sync (+0x88, 0x59B), failures (+0x94, 0xEF each, 0x5A2 / 0x5A3),
// completions (+0xA0, 0x5C7 / 0x5C8), broken rules (+0xC0, 0x5BA / 0x5BB), the ChallengeEntryContainer (+0xE0,
// SetChallengeFilter).
//
// Challenge row: +0 id, +4 PvE, +8 PvP, +0xC requires game mode, +0x10 required game mode (name), +0x14 flags, +0x18
// exclusive group, +0x1C name, +0x20 description, +0x24 required game event, +0x28 icon, +0x2C difficulty (name), +0x30
// trial, +0x34 dungeon trial, +0x38 prestige, +0x3C, +0x40 check map, +0x44 map, +0x48 map difficulty, +0x4C sort order,
// +0x50 artwork.
namespace CoA::Challenge {
    // The client questions; ChallengeClient.cpp installs them.
    struct Deps {
        bool (*hasPlayer)() = nullptr;                                     // 0x4038F0 found the active player
        uint8_t (*playerClass)() = nullptr;                                // descriptor +0x5D
        uint32_t (*playerLevel)() = nullptr;                               // descriptor +0xD8
        uint64_t (*activePlayerGuid)() = nullptr;                          // 0x4D3790
        uint32_t (*partyMembers)() = nullptr;                              // 0x52BD10 CGPartyInfo::GetNumPartyMembers
        uint64_t (*partyLeader)() = nullptr;                               // 0xBD1968 CGPartyInfo::m_leader
        bool (*achievementDone)(uint32_t id) = nullptr;                    // FUN_103087e0 over 0xACFDB0
        bool (*achievementFlags)(uint32_t id, uint32_t& flags) = nullptr;  // Achievement.dbc (0xAD305C) row +0x24
        int32_t (*currentMap)() = nullptr;                                 // 0xBD088C CGGameUI::m_iCurrentMapID
        int32_t (*instanceDifficulty)() = nullptr;                         // 0xBD0894 CGGameUI::m_instanceDifficulty
        bool (*spellAttributes)(uint32_t spell, uint32_t& attributes) = nullptr;   // FetchSpell, Spell +0x10
        // 0x544140 CGWorldMap::GetWorldPosition(pos, map, &x, &y, nullptr, false, 0).
        void (*worldPosition)(const float pos[3], uint32_t map, float& x, float& y) = nullptr;
        bool (*unitTokenGuid)(const char* token, uint64_t& guid) = nullptr;   // Script_GetGUIDFromToken
        int64_t (*now)() = nullptr;                                         // time(nullptr)
    };

    Deps& Dependencies();

    // FUN_10133dc0 / FUN_10134a80 with an ignore set; true when no (unignored) reason is found.
    bool CanActivate(uint32_t id, uint32_t level, const std::set<uint32_t>& ignore);
    bool CanDeactivate(uint32_t id, const std::set<uint32_t>& ignore);
    std::vector<uint32_t> ActiveIds();   // manager +0x2C list order

    const uint8_t* ChallengeRowOf(uint32_t id);   // Challenge.dbc row
    int Difficulty(const uint8_t* row);           // FUN_1011ef10 on +0x2C (CHALLENGE_DIFFICULTY_*, default 0)
    bool HasSoloRule(uint32_t id, uint32_t level);   // a rule of type CHALLENGE_RULES_TYPE_NO_GROUP
    // FUN_1013b350: some active challenge's current level carries a rule of this CHALLENGE_RULES_TYPE index (an unknown
    // name logs and counts as 0).
    bool ActiveHasRule(uint32_t type);

    // The trial serializer's merged level fields (FUN_101243b0 after its Challenges list): IsCompleted (always false),
    // Rewards (every level's computed rewards, concatenated), then Spells, Conditions, Rules and Requirements each
    // merged, sorted by row address and deduplicated. Into the table on top.
    void PushMergedLevels(lua_State* L, const std::vector<std::pair<uint32_t, uint32_t>>& levels, bool sortConditions);

    // FUN_10143630(false): re-index a second from now (the achievement packet hooks, AttachMisc::achievementsChanged).
    void ScheduleRebuild();

    void Init();
    // Tests only.
    void Reset();
}

#endif
