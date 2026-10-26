#ifndef COA_SOURCE_MODULES_ITEMSTATS_H
#define COA_SOURCE_MODULES_ITEMSTATS_H

#include <cstddef>
#include <cstdint>
#include <vector>

class CDataStore;

// AscItemStats.cpp: the scaled item-stat store (static 0x10BDF360, built by FUN_1006dcf0 as an itemstatcache.wdb store
// with CMSG 0x6FF / SMSG 0x700) and the three globals over it and the tooltip context. Checked against the genuine DLL.
//
// Store: an unordered_map keyed by the u64 {item, level}; each entry holds the 0xA8-byte record, the callbacks waiting
// for it and a loaded flag.
//   FUN_10280330 Get(item, level): 0/0 is nothing. Unknown: an entry with the load callback queued and CMSG 0x6FF
//     {u64 key}; nothing. Loaded: the record. Pending: the callback queued AGAIN (the events fire once per miss).
//   SMSG 0x700 (0x101AF5B0, unchecked reads): u64 key, u32, u32, 0x50 bytes, 0x18 bytes, 12 x u32 -- the record
//     verbatim; only for a requested key: the entry is loaded and its callbacks run once, then the queue is emptied.
//   The load callback (FUN_10280500): GET_ITEM_INFO_RECEIVED ("%u%b", item, true; the client has no such event, so
//     it is dropped as in the original), AUCTION_ITEM_LIST_UPDATE, AUCTION_BIDDER_LIST_UPDATE, AUCTION_OWNER_LIST_UPDATE.
// Record: +0x00 item, +0x04 level, +0x08, +0x0C, +0x10 10 x {stat type, value}, +0x60 2 x {min f, max f, +8},
// +0x78 armor, +0x7C armor (reborn), +0x80..+0x94 holy / fire / nature / frost / shadow / arcane, +0x98 block,
// +0x9C random property, +0xA0 required level, +0xA4 sell price.
// SMSG_LOOT_ROLL_ADDON 0x73F: i32 level for the client's next loot-roll index (*0xCA0690), kept in 0x10BE2F74.
namespace CoA::ItemStats {
    // The client questions; ItemStatsClient.cpp installs them.
    struct Deps {
        // CGAuctionHouse::m_bidderList (0xC0F460), m_ownerList (0xC0F450), m_browseList (0xC0F440) for list 0 / 1 / 2:
        // false when its data pointer is null; else its count.
        bool (*auctionList)(int list, uint32_t& count) = nullptr;
        // Entry index (0-based, may be the one past the count): +8 item, +0x70 level.
        void (*auctionEntry)(int list, uint32_t index, uint32_t& item, uint32_t& level) = nullptr;
        bool (*itemRequiredLevel)(uint32_t item, int32_t& level) = nullptr;   // 0x67CA30 record +0x38
        int32_t (*nextLootRollId)() = nullptr;                                // s_nextLootRollId (0xCA0690)
    };

    Deps& Dependencies();

    // FUN_10280330: the 0xA8-byte record for {item, level}, or nullptr (and CMSG 0x6FF asks for it).
    const uint8_t* Get(uint32_t item, uint32_t level);

    // FUN_101b0070 (the attach hook on 0x635710, AttachLua's clientCacheVersion): the store is emptied when the version
    // differs from its own (0x10BDF384), which then takes it.
    void CacheVersion(uint32_t version);

    // itemstatcache.wdb (AscCacheFiles.cpp, not ported yet).
    uint32_t& Version();
    size_t Count();
    void ForEachRecord(void (*cb)(const uint8_t* record, void* ctx), void* ctx);
    void InsertLoaded(const uint8_t* record);   // FUN_101aee00 + FUN_101af410
    void PruneUnloaded();
    void ClearStore();

    // The tooltip state AscTooltipScaling.cpp (not ported yet) shares.
    uint64_t& TooltipGuid();                       // 0x10BE2EB8
    uint32_t& InspectedSlot();                     // 0x10BE2F08
    std::vector<uint32_t>& LootRollLevels();       // 0x10BE2F74

    int32_t OnItemStats(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);      // 0x700
    int32_t OnLootRollAddon(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);  // 0x73F

    void Init();
    // Tests only: the store and the tooltip state empty, version 0, Deps forgotten.
    void Reset();
}

#endif
