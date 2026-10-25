#ifndef COA_SOURCE_MODULES_MISC_H
#define COA_SOURCE_MODULES_MISC_H

#include <cstdint>

// AscMisc.cpp's pieces the client calls (the rest are natives and packets in Misc.cpp / MiscClient.cpp).
namespace CoA::Misc {
    // FUN_10a4f500, which replaces the client's SMSG_SERVERTIME handler (0x7E2A50): the second u32 is the server's
    // epoch time; its distance from local time, corrected for the local time zone, is GetServerTimeOffset.
    void ServerTimeReceived(int32_t serverTime);
}

// MiscClient.cpp: LoadADT's one-shot tile override (FUN_10112920 over CMap::LoadArea 0x7D9A20) and CreateWorldText's
// position (LAB_10a77da0 over WorldTextCreate 0x7E6DC0).
namespace CoA::MiscClient {
    // The file to load instead for the tile (x, y) when LoadADT armed it; disarms. nullptr otherwise.
    const char* TakeAdtOverride(int32_t x, int32_t y);
    // The position CreateWorldText stored, when all three coordinates are non-zero; cleared either way.
    bool TakeWorldTextPosition(float position[3]);
}

#endif
