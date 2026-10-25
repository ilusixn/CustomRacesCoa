#ifndef COA_SOURCE_MODULES_PATCHDATA_H
#define COA_SOURCE_MODULES_PATCHDATA_H

#include <cstdint>

// AscPatchData.cpp: the SMSG_PATCH_* handlers outside the generated table (PatchData.cpp; the game-object textures in
// PatchDataClient.cpp).
namespace CoA::PatchData {
    // What the patches tell the modules that consume them. Each module installs its entry when it is ported; until
    // then the row is stored and nothing else happens.
    struct Deps {
        void (*itemAppearancesPatched)() = nullptr;                  // 0x693: AscAppearance_ItemAppearancesPatched
        void (*itemSetAppearancesPatched)() = nullptr;               // 0x6EC: AscAppearance_ItemSetAppearancesPatched
        void (*rebuildOutfits)() = nullptr;                          // 0x692, a new row: AscAppearance_RebuildOutfits
        void (*tagTypesPatched)() = nullptr;                         // 0x6C0: AscCAFilter_TagTypesPatched (FUN_10217220)
        void (*geosetsRebuild)(const uint8_t* row) = nullptr;        // 0x6FC: AscGeosets_Rebuild (FUN_101ceee0)
        void (*gameObjectTextures)() = nullptr;                      // 0x6FB: FUN_1020c500 (PatchDataClient)
        void (*rebuildSpellCustomAttr)() = nullptr;                  // 0x5F4, an overwrite: FUN_1020b080
        void (*spellTagRemove)(uint32_t spell, uint32_t tag) = nullptr;   // 0x6C1: FUN_10217510
        void (*spellTagAdd)(uint32_t spell, uint32_t tag) = nullptr;      // 0x6C1: FUN_102175b0
        void (*rebuildKnownTags)() = nullptr;                        // 0x6C1: FUN_10182ff0
    };

    Deps& Dependencies();

    // The map 0x10BE0708 (spell (+4) -> SpellAddon.dbc row; e.g. FUN_103284e0): the row indexed under `spell`, or
    // nullptr.
    const uint8_t* SpellAddonForSpell(uint32_t spell);
}

#endif
