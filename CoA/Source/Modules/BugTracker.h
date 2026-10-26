#ifndef COA_SOURCE_MODULES_BUGTRACKER_H
#define COA_SOURCE_MODULES_BUGTRACKER_H

#include <cstdint>

// AscBugTracker.cpp: C_BugTracker (12) and the global CreateBugReport over the bug report composer (static 0x10BC9650,
// registrar FUN_100e8260; module init 0x100e76a0).
//
//   +0x00 u32 RealmInfo +0x64 (taken on every clear)   +0x0C str title (1..0x80)
//   +0x04 u32 category (<= 9, clear -> 9)              +0x24 str description (1..0x1000)
//   +0x08 u32 priority (<= 3, clear -> 0)              +0x3C u8 public
//   +0x40 time64 last submit (0 = never)
//
// Every setter and the clear (FUN_100e76f0) fire BUG_REPORT_MODIFIED while in the world (0xBD0792). The report is
// cleared on every glue screen and at every world entry (FUN_100e71b0), by ClearReport and by SMSG 0x566.
//   CMSG 0x562 (FUN_100e71c0): u32 realm field, u32 category, u32 priority, u8 public, u32 len + title bytes, u32 len
//        + description bytes (no terminators); stamps the time.
//   SMSG 0x565 CREATE_BUG_REPORT_ERROR (FUN_100e7340): u32 len + bytes -> event("%s").
//   SMSG 0x566 CREATE_BUG_REPORT_SUCCESS (FUN_100e7540): u32 id -> clear, event("%u").
// SubmitReport (FUN_100e7f30) refuses within 2.0 s (0x10B2CC08) of the last send; CreateBugReport does not check.
namespace CoA::BugTracker {
    struct Deps {
        int64_t (*now)() = nullptr;   // _time64(nullptr); unset reads 0
    };

    Deps& Dependencies();

    void Init();
    // Tests only: the report as loaded (FUN_100060b0's clear, before RealmInfo is known), Deps forgotten.
    void Reset();
}

#endif
