#ifndef COA_SOURCE_MODULES_ATTACHFIXESCLIENT_H
#define COA_SOURCE_MODULES_ATTACHFIXESCLIENT_H

#include <cstdint>

class CGObject_C;
class CGPlayer_C;
class CGUnit_C;

// AscAttachFixes.cpp's client half: what the hooks read from client objects. The decomp sites call these.
namespace CoA::AttachFixesClient {
    // FUN_1008de00 / FUN_1008e050: an unfinished quest in the log still needs this creature / game object (entry)
    // killed or used, or one of its quest items collected.
    bool QuestRelevant(CGPlayer_C* player, uint32_t entry, bool gameObject);
    // FUN_1008d8c0: how many of an item the player carries in the backpack and the four equipped bags.
    int32_t CountCarried(CGPlayer_C* player, uint32_t itemId);

    // 0x743530 (CGObject_C::ShouldRenderObjectName): a game object's name shows when UnitNameGO is on and it is
    // quest-relevant.
    bool ForceGameObjectName(const CGObject_C* object);
    // 0x729C70 (CGUnit_C::ShouldRenderObjectName): the creature name rules applied after the client agreed (result
    // non-zero): the UnitName* CVars and aura 2180198.
    int32_t CreatureName(const CGUnit_C* unit, int32_t result);

    // 0x76DDE0 (CalculateAuctionDeposit), replaced: 0 without the auction item (CGAuctionHouse::m_sellItem) or its
    // template, else AttachFixes::AuctionDeposit with RATE_AUCTION_DEPOSIT.
    uint32_t AuctionDeposit(uint32_t depositRate, uint32_t sellPrice, uint32_t runTime);

    // FUN_1008e2a0 / FUN_100b1c60: Map.dbc InstanceType of the current map (0 without a row).
    int32_t CurrentMapInstanceType();

    // A return address outside this image (the original: past 0xDFCBFF, the end of Ascension.exe).
    bool CalledFromOutside(const void* returnAddress);
}

#endif
