#ifndef COA_SOURCE_MODULES_GROUPFINDER_H
#define COA_SOURCE_MODULES_GROUPFINDER_H

#include <cstdint>

// AscGroupFinder.cpp: C_GroupFinder over GroupFinderMgr (FUN_10272490, static 0x10BE2770). Tables: LFGActivities (+4
// category, +8 min level, +0xC max level, +0x10 name), LFGActivityCategories (+4 group type, +8 name, +0xC order),
// LFGActivityGroupType (+4 name, +8 order) -- patched by SMSG_PATCH_LFG_* 0x579..0x57B.
//
// CMSG 0x57C create / 0x57D update: u32 n x u32 activity, u32 min item level, u8 x3 roles, u32-len title, u32-len
// description, u8, u8, u32 group size, u8 auto accept. 0x57E remove, 0x57F query (no body), 0x584 suggest invite
// {u32-len name}, 0x586 request invite {u32-len name}, 0x59E request invite {u32-len group, u8, u32, u32-len, u32-len
// "CLASS_...", u8}. SMSG 0x580 create / 0x581 update / 0x582 remove: u32 result; 0 -> u32-len GroupID, else the result's
// name. 0x583 query: the whole map (strings u32-len), GROUP_FINDER_LISTED_ACTIVITIES_UPDATED. 0x585
// SUGGESTED_GROUP_INVITE("%s%s"), 0x587 REQUEST_GROUP_INVITE("%s"), 0x59D REQUEST_GROUP_FINDER_INVITE.
namespace CoA::GroupFinder {
    // The client questions; GroupFinderClient.cpp installs them.
    struct Deps {
        uint32_t (*playerLevel)() = nullptr;                      // UNIT_FIELD_LEVEL, 0 without a player
        const char* (*playerName)() = nullptr;                    // the player's name ("Unknown" without one)
        void (*partyCounts)(uint32_t& a, uint32_t& b) = nullptr;  // the client's party / raid member counts (0xBD1998 / 0xBD199C)
        const char* (*classToken)(uint32_t id) = nullptr;         // ChrClasses +0x1C, nullptr without a row
    };

    Deps& Dependencies();

    void Init();
    // Tests only.
    void Reset();
}

#endif
