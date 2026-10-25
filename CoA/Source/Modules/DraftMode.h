#ifndef COA_SOURCE_MODULES_DRAFTMODE_H
#define COA_SOURCE_MODULES_DRAFTMODE_H

#include <cstdint>

// AscDraftMode.cpp: DraftModeMgr (FUN_102228f0 -> static 0x10BE23B0) -- draft picks, Hand of Fate picks / sacrifices
// and the skill-card flags from SMSG_DRAFT_STATE 0x753 (FUN_10225b40), reset at the glue screen (FUN_10224e40) -- and
// C_DraftRewards over the Hand of Fate rewards (0x6AA list, 0x6B4 claim result, 0x6B7 claimed list).
//
// 0x753: u32 -> +8, u8 -> +0x30, u32 -> +0x34 (selected HoF spell), u32 -> +0x38, u8 -> +0x3C, u32 -> +0x40, u8 x6 ->
// +0x44..+0x49; then +0x58 := +0x3C, +0x5F := +0x44, +0x66 := +0x45, +0x67 := +0x46, +0x68 := +0x47, +0x69 := +0x48;
//   u8 n x {u32 spell, u8 a, u8 b}  draft picks  (i < 3: +0x59+i / +0x4A+i := a, +0x5C+i / +0x4D+i := b)
//   u8 n x {u32 spell, u8 a, u8 b}  HoF picks    (i < 3: +0x60+i / +0x50+i := a, +0x63+i / +0x53+i := b)
//   u8 n x {u32 spell}              HoF sacrifices
//   u32 n, then n x 8 bytes skipped.
// Afterwards +4 := +0x34, and if +0x68 or +0x69 is set and the selected spell's index in the HoF picks is below
// min(count, 3), +0x68 / +0x69 take that pick's a / b bytes. Events (no arguments) fire for each part that changed
// against the pre-packet state, then ASCENSION_DRAFT_STATE_UPDATED unless nothing changed and +0x49 is also unchanged.
namespace CoA::DraftMode {
    // The client questions, shared with ScrollOfFortune.cpp (the originals share FUN_10308570); DraftModeClient.cpp
    // installs them.
    struct Deps {
        bool (*hasPlayer)() = nullptr;                   // 0x4038F0 found the active player
        uint32_t (*playerLevel)() = nullptr;             // descriptor +0xD8
        uint32_t (*itemCount)(uint32_t entry) = nullptr;   // FUN_10308570: the currency-token slots 0x76..0x95
        int64_t (*now)() = nullptr;                      // _time64(nullptr)
    };

    Deps& Dependencies();

    void Init();
    // Tests only: also drops the Hand of Fate rewards, which the glue screen keeps.
    void Reset();
}

#endif
