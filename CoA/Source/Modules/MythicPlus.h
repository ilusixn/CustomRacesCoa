#ifndef COA_SOURCE_MODULES_MYTHICPLUS_H
#define COA_SOURCE_MODULES_MYTHICPLUS_H

#include <cstdint>

class CDataStore;

// C_MythicPlus over MythicPlusMgr (port of AscMythicPlus.cpp; FUN_100d52f0, static 0x10BDB68C).
namespace CoA::MythicPlus {
    void Init();
    // The run state emptied (every glue screen, FUN_102bb0b0).
    void Reset();

    bool EncounterKilled(uint32_t encounter);

    // SMSG 0x56D keystone patch / 0x9C1 instance info / 0x908 state / 0x909 progress / 0x90A timer.
    int32_t OnPatchKeystone(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
    int32_t OnInstanceInfo(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
    int32_t OnStateChanged(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
    int32_t OnProgressUpdate(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
    int32_t OnTimerUpdate(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
}

#endif
