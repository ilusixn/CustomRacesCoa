#ifndef COA_SOURCE_MODULES_CALLBOARD_H
#define COA_SOURCE_MODULES_CALLBOARD_H

#include <cstdint>
#include <string>

// AscCallboard.cpp: C_Callboard.GetView over CallboardViewMgr (static 0x10BDD1E8 via FUN_10119470: +4 categories,
// +0x10 loaded; module init FUN_10119420). C_Callboard.RequestView (CMSG 0x55F) is in Bindings.cpp.
//   SMSG 0x589 (FUN_101194e0): u32 n x Category -- replaces the view, marks it loaded, CALLBOARD_VIEW_UPDATED.
//   SMSG 0x58A (FUN_101197c0): str -- CALLBOARD_VIEW_REQUEST_RESULT(str).
// Category (100 bytes, FUN_10118240): str ContentType, str Name, u32 RemainingCompletions, u32 MoneyCost,
//   u32 TokenCost, Reward, u32 n x SubCategory.
// SubCategory (0x5C, FUN_101187e0): str Type, str Name, u32 RemainingCompletions, u32 MoneyCost, u32 TokenCost,
//   u32 MinimumLevel, Reward.
// Reward (FUN_10116af0): u32 RewardMoney, RewardHonor, RewardArenaPoints, CallboardCachePoints,
//   u32 n x {u32 ItemID, u32 Amount} (FUN_10117120).
// The view is never cleared: it survives glue screens and world entries until the next 0x589.
namespace CoA::Callboard {
    void Init();
    // Tests only: the view not loaded.
    void Reset();

    // FUN_10337270 (C_TemporalContracts.ActivateAll*): nothing until the view is loaded; otherwise every category (or,
    // given a content type, those whose ContentType or Name equals it) hands each sub-category's Type to fn once per
    // remaining completion. True when fn ran at all. Installed as TemporalContracts' forEachCompletion.
    bool ForEachCompletion(const std::string* contentType, void (*fn)(const std::string& type, void* ctx), void* ctx);
}

// AscCallboardCache.cpp: C_CallboardCache (4; FUN_10116270 / FUN_101162f0 / FUN_10116370 over FUN_10115df0, and
// FUN_10116400).
//
// SMSG 0x730 (FUN_10115400) replaces the level table (static 0x10BDD1C4): u32 n, then n raw 0x18-byte entries -- +8
// gating game event (0 = always), +0xC level threshold, +0x10 info value, +0x14 points.
// The current cache level is the client's progress counter for criteria 0x13901 (through Deps). FUN_10115df0 walks
// the eligible entries (event 0 or active): the first whose threshold <= level is "current", the entry after it
// "next"; while none is current, each entry above the level is "upcoming". Upcoming / next are dropped unless they
// share the current entry's event. Verified against the genuine DLL.
namespace CoA::CallboardCache {
    struct Deps {
        // FUN_100b11b0(0x13901) then 0x5B2350: the progress node's value (+0x10); unset reads 0 (no node).
        uint32_t (*cacheLevel)() = nullptr;
        // AscQuestAddon::Get (not ported yet): the quest's points; unset answers as the original does while the record
        // is still being fetched (nil).
        bool (*questAddonPoints)(uint32_t quest, uint32_t& points) = nullptr;
    };

    Deps& Dependencies();

    void Init();
    // Tests only: the level table empty, Deps forgotten.
    void Reset();
}

#endif
