#ifndef COA_SOURCE_MODULES_GMTICKET_H
#define COA_SOURCE_MODULES_GMTICKET_H

#include <cstdint>

// AscGMTicket.cpp: C_GMTicket (27), C_PlayerTicket (14) and C_GM (2) over the original's GMTicketMgr (FUN_1024d0c0,
// static 0x10BCB3C0; module init FUN_1024cf40) and PlayerTicketMgr (FUN_102552d0, static 0x10BE2560, +0x110 has-ticket;
// module init FUN_102551c0). Every binding is gated on client CGGameUI::CanPerformAction(0x11) (FUN_10111ce0) and
// returns nothing when it fails.
//
// Ticket record (0x110 bytes, reader FUN_10248730, Lua writer FUN_10248ea0), wire order (all reads unchecked in the
// original): str TicketID +0, str Title +0x18, u32 CreationDate +0x30, opt u32 LastMessageDate +0x34, str Creator +0x3C,
// u8 Status +0x54, opt str AISuggestion +0x58, u32 n x {str Command, str Reason} +0x74, u32 n x {str SQL, str Reason}
// +0x80, u32 n x {u32 BugTrackerReportID, str Reason} +0x8C, opt str AISuggestionSummary +0x98, u32 n x Message +0xB4,
// u32 n x str AssignedTo +0xC0, u8 IsLastMessageUnseen +0xCC, u8 IsAnyMessageUnseen, u8 IsLastMessageFromGM, u8 Priority,
// u8 Category, u8 IsClosedByCreator +0xD1, opt u32 CloseDate +0xD4, str CreatorLocale +0xDC, opt str OnlineCharacterName
// +0xF4 ("opt" = u8 present + value).
// Message (0x68, FUN_102434c0): str TicketID, u32 MessageID +0x18, u8 IsFromGM +0x1C, str Sender, opt str Message, u8
// GMOnly +0x54, u32 Timestamp +0x58, u32 n x ReadReceipt {str TicketID, u32 MessageID, str ReadBy, u32 Timestamp}.
//
// GMTicketContainer (vtable 0x10B3E774) is the generic FilterableContainer engine FUN_102499c0 (the same as Build
// Creator's: populate key first, then the selected sort keys, then the filter key, std::sort) over snapshot copies of
// the ticket list: [1] token 0x100cc230 (false), [2] filter FUN_10251c00, [3] search FUN_10250940, [4] filter group
// FUN_10251e40, [7] populate key FUN_10251e90, [8] base (no key), [9] sort key FUN_10252570.
namespace CoA::GMTicket {
    struct Deps {
        // ClntObjMgrGetActivePlayerObj (0x4038F0) and its name (vtable +0xD8); nullptr without an active player.
        const char* (*activePlayerName)() = nullptr;
    };

    Deps& Dependencies();

    void Init();
    // Tests only: both managers back to their constructed state, Deps forgotten.
    void Reset();
}

#endif
