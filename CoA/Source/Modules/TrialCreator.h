#ifndef COA_SOURCE_MODULES_TRIALCREATOR_H
#define COA_SOURCE_MODULES_TRIALCREATOR_H

#include <cstdint>

// AscTrialCreator.cpp: C_TrialCreator -- player-made "trials" (bundles of challenge levels), the original's
// ChallengeCreatorMgr (accessor FUN_10124310 -> 0x10BDD210, ctor FUN_10120690, module init FUN_10124200: 7 events, 8
// packet handlers, glue reset FUN_10119df0).
//
//   +0x00 entries (0xA4 each): +0 id, +0x18 owned, +0x1C author, +0x34 name, +0x4C description, +0x64 icon, +0x7C
//         challenges {u32 challenge, u32 level, str description} (0x20 each), +0x88 upvoted, +0x89 downvoted, +0x8C
//         upvote times, +0x98 downvote times (u64 each)
//   +0x0C the active trial's id (string)
//   +0x24 trial id -> completions (0x88 each: trial, player, u64 start, u64 complete, race, gender, class), fastest
//         first
//   +0x44 ChallengeCreatorEntryContainer (vtable 0x10B312D0)
// Strings on the wire are u32 length + bytes (FUN_100d3680).
namespace CoA::TrialCreator {
    // The client questions; TrialCreatorClient.cpp installs them.
    struct Deps {
        // SpellIcon.dbc (client container 0xAD488C): some row's texture path (+4) equals the name.
        bool (*iconExists)(const char* icon) = nullptr;
        int64_t (*now)() = nullptr;   // time(nullptr)
    };

    Deps& Dependencies();

    // FUN_10122410 (+0xC): the community trial the player is in, as its id string; C_Challenge refuses start / stop
    // while it is non-empty (codes 0xF / 4).
    bool HasActiveTrial();

    void Init();
    // Tests only.
    void Reset();
}

#endif
