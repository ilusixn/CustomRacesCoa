#ifndef COA_SOURCE_MODULES_AURA137_H
#define COA_SOURCE_MODULES_AURA137_H

#include <cstdint>

class CDataStore;

// AscAura137.cpp: the client code patch for aura type 0x137 (FUN_10a73ea0; ClientHooks::aura137TurnWhileFalling), driven
// by (checked against the genuine DLL)
//   - list 0x10BE2B6C before 0x724820 (CGUnit_C::AddAuraEffect) / 0x10BE2B8C before 0x71E930 (RemoveAuraEffect):
//     0x10A4E2E0 / 0x10A4E330 turn it on / off when the unit is the active player (0x4CEE50) and one of the spell's
//     three EffectApplyAuraName columns (+0x17C) is 0x137; both always let the client go on;
//   - SMSG 0x6E9 / 0x6E8 (0x10A4F4B0 / 0x10A4F490, registered by FUN_10a66100): skip the u64 guid, then on / off.
namespace CoA::Aura137 {
    struct Deps {
        bool (*isActivePlayer)(void* unit) = nullptr;       // 0x4CEE50
        bool (*hasAura137)(const void* spell) = nullptr;    // SpellRec +0x17C x 3 == 0x137
    };

    Deps& Dependencies();

    bool BeforeAddAuraEffect(void* unit, uint32_t index, const void* spell);      // 0x10A4E2E0
    bool BeforeRemoveAuraEffect(void* unit, uint32_t index, const void* spell);   // 0x10A4E330
    int32_t OnOff(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);  // 0x6E8
    int32_t OnOn(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);   // 0x6E9

    void Init();
    // Tests only: Deps forgotten, the patch off.
    void Reset();
}

#endif
