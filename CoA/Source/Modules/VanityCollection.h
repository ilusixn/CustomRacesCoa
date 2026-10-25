#ifndef COA_SOURCE_MODULES_VANITYCOLLECTION_H
#define COA_SOURCE_MODULES_VANITYCOLLECTION_H

#include <cstdint>

class CDataStore;

// C_VanityCollection (port of AscVanityCollection.cpp, plus C_VanityCollection.Purchase / IsPurchaseInProgress from
// AscBindings.cpp): the known-items set, purchases, and the item browser over the bridge store's VanityCollection
// records.
namespace CoA::VanityCollection {
    void Init();
    // Known items, the browser and the dependencies emptied (tests).
    void Reset();

    // FUN_1032ea50
    bool Owned(uint32_t item);

    // SMSG 0x6F7 list / 0x6F8 add / 0x6F9 remove / 0x9D0 customization unlocks / 0x573 vanity patch.
    int32_t OnList(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
    int32_t OnAdd(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
    int32_t OnRemove(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
    int32_t OnCustomizationUnlocks(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);
    int32_t OnPatchVanity(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);

    // What IsConsolidatedVanityBuff asks character advancement (AscCA::SpellCustomAttr18); false until that module
    // installs it.
    struct Deps {
        bool (*spellCustomAttr18)(uint32_t spell, uint32_t mask) = nullptr;
    };

    Deps& Dependencies();
}

#endif
