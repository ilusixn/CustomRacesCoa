#ifndef COA_SOURCE_MODULES_LOOTLOCKOUT_H
#define COA_SOURCE_MODULES_LOOTLOCKOUT_H

#include <cstdint>

class CDataStore;

// AscLootLockout.cpp: C_LootLockout (and the same ten functions again as bare globals, FUN_10296640) over
// LootLockoutMgr (FUN_100d51b0 -> static 0x10BDB6B8; module init 0x10294E50). Checked against the genuine DLL.
//
// +0x04 unordered_map<u64 unit guid, vector<{u32 encounter, u32 expiry}>>; +0x24 the instance binds, 12-byte
// {u32, u32 map, u32 difficulty}. Encounters are the client's DungeonEncounter.dbc (container 0xAD36B0: +0 id, +4 map,
// +8 difficulty, +0xC order, +0x14 name, +0x18 SpellIcon id; container +0xC max id, +0x10 min id).
//   SMSG 0x9B4 (FUN_10295020): u64 guid, u32 n x {encounter, expiry}, each appended (nothing cleared).
//   SMSG 0x9B5 (0x10294EB0): u64 guid, u32 encounter, u32 expiry, appended.
//   SMSG 0x9B6 (0x10294F60): u64 guid, u32 encounter; the first matching entry is erased.
//   SMSG 0x6FE (FUN_10294970): the bind list is emptied; C string result; QUERY_INSTANCE_BINDS_OK (enum 0) then
//     u32 n x 12-byte binds; QUERY_INSTANCE_BINDS_RESULT("%b%s", ok, result).
//   CMSG 0x6FD QueryInstanceBinds; CMSG 0x58C ResetInstanceDifficulty {u32 map, u8 difficulty - 1}.
// Every glue screen (FUN_10294740): the lockouts and the binds emptied.
namespace CoA::LootLockout {
    struct Encounter {
        uint32_t id = 0;              // +0x00
        int32_t map = 0;              // +0x04
        int32_t difficulty = 0;       // +0x08
        int32_t order = 0;            // +0x0C
        const char* name = nullptr;   // +0x14
        uint32_t icon = 0;            // +0x18
    };

    // The client questions; LootLockoutClient.cpp installs them.
    struct Deps {
        uint64_t (*tokenGuid)(const char* token) = nullptr;               // FUN_103086c0 -> 0x60C1C0, 0 for none
        bool (*encounter)(uint32_t id, Encounter& out) = nullptr;        // DungeonEncounter.dbc (0x100B1600)
        uint32_t (*encounterMinId)() = nullptr;                          // container +0x10 (FUN_100b10c0)
        uint32_t (*encounterMaxId)() = nullptr;                          // container +0x0C (FUN_100b0ff0)
        const char* (*spellIcon)(uint32_t id) = nullptr;                 // SpellIcon.dbc +4, nullptr without a row
        bool (*mapInstanceType)(uint32_t map, int32_t& type) = nullptr;  // Map.dbc +8 (FUN_100b1c60)
        bool (*unitEntry)(const char* token, uint32_t& entry) = nullptr; // FUN_103086d0: the unit's descriptor +0xC
        int64_t (*now)() = nullptr;                                      // _time64(nullptr) when unset
    };

    Deps& Dependencies();

    // FUN_102948d0: seconds left on the unit's lock for the encounter; 0 without one, expired, or past 2038.
    uint32_t SecondsRemaining(uint64_t guid, uint32_t encounter);

    int32_t OnAddLockouts(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);   // 0x9B4
    int32_t OnAddLockout(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);    // 0x9B5
    int32_t OnRemoveLockout(void* param, uint32_t opcode, uint32_t time, CDataStore* msg); // 0x9B6
    int32_t OnInstanceBinds(void* param, uint32_t opcode, uint32_t time, CDataStore* msg); // 0x6FE

    void Init();
    // Tests only: the stores empty, Deps forgotten.
    void Reset();
}

#endif
