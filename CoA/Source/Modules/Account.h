#ifndef COA_SOURCE_MODULES_ACCOUNT_H
#define COA_SOURCE_MODULES_ACCOUNT_H

#include <cstdint>

class CDataStore;

// C_AccountInfo (AccountInfo object 0x10bdb114, accessor FUN_1008c870) and C_Realm (RealmInfo 0x10bdb138); port of
// AscAccount.cpp.
//
// SMSG_ACCOUNT_INFO 0x9BB: u32, u32 GM level, u8 newcomer, u8 guide, then u32 n characters of {str name, str sex,
// u32 level, str class, str race, str faction}. "Is GM" comes from SMSG 0x5C0.
namespace CoA::Account {
    uint32_t GmLevel();

    int32_t OnAccountInfo(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
    int32_t OnGMStatus(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);

    // What SMSG 0x5C0 with a set byte does to the client (AccountClient.cpp; tests install their own).
    void SetGmEffect(void (*effect)());
    // AccountClient.cpp: installs the client effect.
    void InstallClient();
    // Tests only.
    void Reset();
}

#endif
