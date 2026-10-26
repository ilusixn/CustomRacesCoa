// AscItemStats.cpp's client questions: the auction lists, the item cache's required level and the loot-roll index.
#include "CoA/Source/Modules/ItemStats.h"
#include "CoA/Source/Modules/Modules.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Object/ItemStats.h"
#include "WoW/Source/Object/ObjectClient/LootRoll.h"
#include "WoW/Source/Ui/AuctionHouse.h"

namespace {
    TSGrowableArray<AuctionItemInfo>* List(int list) {   // 0xC0F460 / 0xC0F450 / 0xC0F440
        return list == 0 ? &CGAuctionHouse::m_bidderList : list == 1 ? &CGAuctionHouse::m_ownerList : &CGAuctionHouse::m_browseList;
    }

    bool AuctionList(int list, uint32_t& count) {
        TSGrowableArray<AuctionItemInfo>* a = List(list);

        if (!a->m_data) {
            return false;
        }

        count = a->m_count;
        return true;
    }

    // The entry one past the count is still inside the allocation when the array has room (a stale or zeroed entry);
    // past the allocation it reads as zeros here.
    void AuctionEntry(int list, uint32_t index, uint32_t& item, uint32_t& level) {
        TSGrowableArray<AuctionItemInfo>* a = List(list);

        if (index >= a->m_alloc) {
            item = 0;
            level = 0;
            return;
        }

        item = a->m_data[index].m_itemID;
        level = a->m_data[index].m_flags;   // +0x70, Ascension's item level
    }

    bool ItemRequiredLevel(uint32_t item, int32_t& level) {   // 0x67CA30(item, 0, 0, 0, 0) +0x38
        const ItemStats_C* rec = g_itemDBCache.GetRecord(static_cast<int32_t>(item), WOWGUID(), nullptr, nullptr, false);

        if (!rec) {
            return false;
        }

        level = static_cast<int32_t>(rec->m_requiredLevel);
        return true;
    }

    int32_t NextLootRollId() {   // 0xCA0690
        return LootRollNextId();
    }
}

void CoA::ItemStatsClient::Init() {
    CoA::ItemStats::Deps& d = CoA::ItemStats::Dependencies();
    d.auctionList = AuctionList;
    d.auctionEntry = AuctionEntry;
    d.itemRequiredLevel = ItemRequiredLevel;
    d.nextLootRollId = NextLootRollId;
}
