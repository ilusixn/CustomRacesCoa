#ifndef COA_SOURCE_MODULES_GAMEEVENTS_H
#define COA_SOURCE_MODULES_GAMEEVENTS_H

#include <cstdint>

class CDataStore;

// GameEventMgr (FUN_100d5110 -> static, FUN_10256d50 ctor): std::map<u16 eventId, 0x34-byte record> filled by
// SMSG_GAME_EVENT_INFO 0x9BD; GetGameEventInfo / GetGameEventInfos / IsGameEventActive / GetServerMaxLevel.
// Port of AscGameEvents.cpp.
//
// Record (copied from the packet verbatim): +0 u16 eventId, +2 u8 state (GAMEEVENT_*), +3 u64 start time, +0xB u64
// end time, +0x13 u8 active, +0x14 i32 length. An update that turns a record active fires GAME_EVENT_STARTED("%u",
// id) unless it was already active; one that turns it inactive fires GAME_EVENT_ENDED if it was active before.
namespace CoA::GameEvents {
    bool IsActive(uint16_t eventId);   // FUN_10256e20
    uint32_t ServerMaxLevel();         // FUN_102fc4d0

    int32_t OnGameEventInfo(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
    // Tests only.
    void Reset();
}

#endif
