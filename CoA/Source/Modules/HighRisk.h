#ifndef COA_SOURCE_MODULES_HIGHRISK_H
#define COA_SOURCE_MODULES_HIGHRISK_H

#include <cstdint>

// AscHighRisk.cpp: C_HighRisk (FUN_10275520 / handler_GetInsuranceCostPerSlot / handler_IsFelCommutationActive /
// FUN_102756d0); GetInsuranceTotalCost is registered with the same function as GetInsuranceCostPerSlot (0x10275670).
// SMSG_FEL_COMMUTATION_STATUS_UPDATE 0x73D (FUN_102752e0): u8 active (0x10BE2828). ToggleFelCommutation sends CMSG
// 0x73B with the requested u8 state. Every glue screen clears the flag (FUN_10275050).
namespace CoA::HighRisk {
    // The client questions; HighRiskClient.cpp installs them.
    struct Deps {
        uint64_t (*playerGuid)() = nullptr;    // the active player's GUID, 0 without one (0x4038F0)
        uint32_t (*playerLevel)() = nullptr;   // UNIT_FIELD_LEVEL (descriptor +0xD8)
    };

    Deps& Dependencies();

    void Init();
    // Tests only.
    void Reset();
}

#endif
