#ifndef COA_SOURCE_MODULES_ASSETQUERY_H
#define COA_SOURCE_MODULES_ASSETQUERY_H

#include <cstdint>

// AscAssetQuery.cpp: C_AssetQueryService (FUN_100e3770) and the manager behind it (FUN_100e15d0, static 0x10BDBB30).
// Checked against the genuine DLL.
//
// Per kind (item / quest / creature) a request queue (+0x00 / +0x2C / +0x58), the set of every id ever queued
// (+0x0C / +0x38 / +0x64), a per-second batch counter (+0x84 / +0x88 / +0x8C) and an in-flight list of {id, time64,
// succeeded, failed} (+0x98 / +0xA4 / +0xB0); +0x90 the second the counters were last reset.
//   TryCache*(id) (FUN_100e2d60 / ... with the prefetch flag clear): false when the client cache has the record; true
//   and queued when the id was never seen; else false, moving the id to the queue's front if it still waits there.
//   The tick (FUN_100e1700, after every idle tick): the counters reset once a second; then for each kind with a
//   non-empty queue, at most 3 batches a second unless C_Realm.IsDevelopment, up to 50 ids move to the in-flight list
//   and go out as CMSG 0x61B (items) / 0x61C (quests) / 0x61A (creatures) {u32 count, u32 ids}; then in-flight ids
//   younger than 31 s that reached the cache fire <KIND>_CACHE_REQUEST_SUCCESS("%u"), older ones leave the seen-set
//   and fire _FAILURE. Back at the glue screen (FUN_100e1480) everything is cleared.
namespace CoA::AssetQuery {
    enum Kind { ITEM = 0, QUEST = 1, CREATURE = 2 };

    struct Deps {
        // 0x67CA30 / 0x67DE90 / 0x67B6A0 (the client caches' GetRecord, no callback): the record is there.
        bool (*cached)(int kind, uint32_t id) = nullptr;
        int64_t (*now)() = nullptr;   // _time64(nullptr) when unset
    };

    Deps& Dependencies();

    bool TryCache(int kind, uint32_t id);
    bool TryCacheItem(uint32_t id);   // FUN_100e2d60(id, false)
    void Tick();                      // FUN_100e1700
    void OnGlueScreen();              // FUN_100e1480

    // Also fills MysticEnchant's cacheItem Dep.
    void Init();
    // Tests only: the manager cleared, Deps forgotten.
    void Reset();
}

#endif
