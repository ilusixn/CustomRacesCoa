#ifndef COA_SOURCE_MODULES_RECOVERYSERVICE_H
#define COA_SOURCE_MODULES_RECOVERYSERVICE_H

#include <cstdint>

// AscRecoveryService.cpp: C_RecoveryService (5) over RecoveryServiceMgr (FUN_103056d0, static 0x10BE3B38, init
// FUN_102fea30). Six FilterableContainerBase instances, 0x7C bytes apart, one per category:
//   1 DISENCHANTED_ITEM   +0xF8    query 0x5D2 -> 0x5D3, recover 0x5D4 -> 0x5D5
//   2 DELETED_CHARACTER   +0x00    query 0x5DA -> 0x5DB, recover 0x5DC -> 0x5DD
//   3 VENDORED_ITEM       +0x1F0   query 0x5DE -> 0x5DF, recover 0x5E0 -> 0x5E1
//   4 DELETED_ITEM        +0x7C    query 0x5E2 -> 0x5E3, recover 0x5E4 -> 0x5E5
//   5 MYTHIC_RECYCLING    +0x174   query 0x5E6 -> 0x5E7, recover 0x5E8 -> 0x5E9
//   6 WORLDFORGED_ITEM    +0x26C   query 0x682 -> 0x683, recover 0x684 -> 0x685
// A query response resets the category's container and loads u32 n records, then RECOVERY_QUERY_RESULT("%s"). The view
// (container +0x10) is filled only by UpdateFilter (FUN_10306f80 -> the generic engine FUN_102499c0, no filters or
// sorts); recovered records (+0x20) never pass it.
//
// Recover (0x10306A10): CMSG {u32 position of view[index - 1] in the container's ALL list (+4), str text}; the
// (category, position) pair is held at +0x2E8 / +0x2EC until a result. The result (FUN_103060b0), when it equals
// <TYPE>_RECOVERY_OK, marks the VIEW entry at that position recovered (the original indexes +0x10 with the +4
// position -- kept) and refilters with the container's last text (+0x1C); every result fires
// RECOVERY_RESULT("%s%s", category, result).
//
// The item search (FUN_103004e0) reads the client's Item.dbc row (through Deps) and the DLL's ItemAddon record
// (SMSG 0x569); the character search (FUN_102ffbf0) reads ChrRaces / ChrClasses names (through Deps).
namespace CoA::RecoveryService {
    struct ItemRow {
        uint32_t classId = 0;         // +0x04
        uint32_t subclassId = 0;      // +0x08
        uint32_t inventoryType = 0;   // +0x18
    };

    struct Deps {
        bool (*itemRow)(uint32_t entry, ItemRow& out) = nullptr;   // FUN_100b1870 on Item.dbc; unset: no row
        const char* (*raceName)(uint32_t id) = nullptr;            // FUN_100b13f0 ChrRaces +0x38; unset: no row
        const char* (*className)(uint32_t id) = nullptr;           // FUN_100b13c0 ChrClasses +0x10; unset: no row
    };

    Deps& Dependencies();

    void Init();
    // Tests only: every category empty, nothing pending, Deps forgotten.
    void Reset();
}

#endif
