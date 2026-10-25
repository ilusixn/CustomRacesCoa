#ifndef COA_SOURCE_MODULES_REALMINFO_H
#define COA_SOURCE_MODULES_REALMINFO_H

#include <cstdint>

class CDataStore;

// SMSG_REALM_INFO (0x9BC): what kind of realm this is and which character-creation flavours exist (port of
// RealmInfo.cpp). Wire: u32 field0 (realm id), u32 ruleset, f32 x3 rates, u32 maintenance, f32 auction cut,
// f32 vanity deposit rate, u32 vanity deposit max, u8[8] gates, cstring data path, cstring realm name, u8 flag,
// and an optional trailing u32.
namespace CoA::RealmInfo {
    struct Info {
        uint32_t field0 = 0;
        uint32_t ruleset = 0;
        float rateXpQuest = 0.0f;
        float rate1 = 0.0f;
        float rate2 = 0.0f;
        uint32_t flag14 = 0;   // maintenance
        float rate3 = 0.0f;    // auction cut
        float rateAuctionDepositVanity = 0.0f;
        uint32_t maxAuctionDepositVanity = 0;
        uint8_t gates[8] = {};   // realm object +0x40..+0x47
        char dataPath[64] = {};
        char realmName[128] = {};
        uint8_t trailingFlag = 0;   // realm object +0x48
        uint32_t field64 = 0;
        uint8_t visitedArea52 = 0;   // +0x49: sticky once a packet names realm 11
        uint8_t received = 0;

        // C_Realm.IsLive / IsSeasonal / IsLeague / IsPTR / IsDevelopment and the creation gates.
        bool Live() const { return this->gates[0] != 0; }
        bool Seasonal() const { return this->gates[1] != 0; }
        bool League() const { return this->gates[2] != 0; }
        bool PTR() const { return this->gates[3] != 0; }
        bool Dev() const { return this->gates[4] != 0; }
        bool CoA() const { return this->gates[6] != 0 || this->gates[4] != 0; }
        bool Hero() const { return !(this->gates[6] != 0 || this->gates[7] != 0); }
        bool Wildcard() const { return this->gates[7] != 0; }
    };

    const Info& Get();
    // FUN_102fc6c0.
    int32_t OnRealmInfo(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
    // The realm data hot-swap (AscRealmData::Switch), installed by that module: called with a non-empty data path.
    void SetDataPathHandler(void (*handler)(const char* path));
    // Tests only.
    void Reset();
}

#endif
