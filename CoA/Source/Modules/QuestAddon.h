#ifndef COA_SOURCE_MODULES_QUESTADDON_H
#define COA_SOURCE_MODULES_QUESTADDON_H

#include <cstddef>
#include <cstdint>

class CDataStore;

// AscQuestAddon.cpp: the quest addon cache (static 0x10BDF3A8, built by FUN_1006de50 as FUN_101af030("questcacheaddon.wdb",
// 0x731, 0x732, 1)) -- an unordered_map keyed by quest id, each entry holding its record, the callbacks waiting for it and
// a loaded flag.
//
// FUN_10115fc0 Get(quest, cb, arg): quest 0 is nothing; an unknown quest gets an entry (cb queued when given) and CMSG
// 0x731 {u32 quest}; a known, loaded one returns its record; a known, pending one queues cb.
// SMSG 0x732 (FUN_101af9f0): u32 quest, u32 points, u32 unk -- only for a requested quest; the record becomes
// {quest, points, unk, quest}, the entry is loaded, every queued callback runs with (quest, arg), the queue is emptied.
// Users: C_CallboardCache.GetPointsForQuest, C_TemporalContracts, IsQuestSequenced (GlobalsJ). Verified against the DLL.
namespace CoA::QuestAddon {
    struct Record {
        uint32_t quest = 0;    // +0xC
        uint32_t points = 0;   // +0x10
        uint32_t unk = 0;      // +0x14 (bit 0: IsQuestSequenced)
        uint32_t quest2 = 0;   // +0x18
    };

    using Callback = void (*)(uint32_t quest, void* arg);
    const Record* Get(uint32_t quest, Callback cb = nullptr, void* arg = nullptr);

    // FUN_101b0070 (the attach hook on 0x635710, AttachLua's clientCacheVersion): the store is emptied when the
    // version differs from its own (0x10BDF3CC), which then takes it.
    void CacheVersion(uint32_t version);

    // questcacheaddon.wdb (AscCacheFiles.cpp, not ported yet; written at shutdown, never read back).
    uint32_t& Version();
    size_t Count();
    void ForEachRecord(void (*cb)(const Record& record, void* ctx), void* ctx);
    void PruneUnloaded();
    void ClearStore();

    int32_t OnResponse(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);   // SMSG 0x732

    // Also installs the questAddonPoints Deps of CallboardCache and TemporalContracts and GlobalsJ's IsQuestSequenced.
    void Init();
    // Tests only: the store empty, version 0.
    void Reset();
}

#endif
