#ifndef COA_SOURCE_MODULES_SPELLCHARGES_H
#define COA_SOURCE_MODULES_SPELLCHARGES_H

#include <cstdint>

class CDataStore;

// AscSpellCharges.cpp: spell charges (map 0x10BE43E4: SpellChargesCategory id -> {+0xC used, +0x10 next recharge (ms),
// +0x14 recharge start (ms), +0x18 recharge duration}); times are the client's millisecond clock 0x86AE20. Module init
// FUN_10320060's caller; checked against the genuine DLL.
//   SMSG 0x9C2 clear all; 0x9C3 (FUN_10320160) {u32 category} removed; 0x9C4 (FUN_10320230) u32 n x {u32 category,
//   i32 remaining ms, u8 used}, kept for a known category with a max-charges column (+4) and a non-zero remaining
//   time; 0x9C5 (FUN_10320350) {u32 category, u32 ms, u32 used}: used 0 removes, a new category starts now.
//   Every handler fires SPELL_UPDATE_CHARGES while in world; 0x9C5 only when the category passed.
//   On world entry (0x1031FD70) the map is emptied; after every idle tick (FUN_10320680) each entry whose recharge
//   has ended gives a charge back (the last one removes it) and fires SPELL_UPDATE_CHARGES while in world.
//   GetSpellCharges(spell) (FUN_103204c0): SpellCharges.dbc (+4 category) -> SpellChargesCategory.dbc (+4 max, +8 ms).
namespace CoA::SpellCharges {
    struct Deps {
        uint32_t (*now)() = nullptr;   // 0x86AE20 (OsGetAsyncTimeMs); 0 when unset
    };

    Deps& Dependencies();

    int32_t OnClear(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);    // 0x9C2
    int32_t OnRemove(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);   // 0x9C3
    int32_t OnList(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);     // 0x9C4
    int32_t OnUpdate(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);   // 0x9C5

    void Tick();         // FUN_10320680
    void EnterWorld();   // 0x1031FD70

    void Init();
    // Tests only: the map empty, Deps forgotten.
    void Reset();
}

#endif
