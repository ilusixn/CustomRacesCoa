#ifndef COA_SOURCE_MODULES_ATTACHPATCHES_H
#define COA_SOURCE_MODULES_ATTACHPATCHES_H

#include <cstdint>

// AscAttachPatches.cpp: the attach-time client code patches of FUN_10a3d840. Each row is a marked source edit at its
// decomp site; what the edits read that is data rather than code lives here.
namespace CoA::AttachPatches {
    // 0x5238AE: the uiScale CVar's default ("1.0") becomes the DLL's "0.85".
    extern const char* const kUiScaleDefault;

    // FUN_10a5b790: the minimap's 6-entry zoom tables (0xA41E04 outdoor tiles, 0xA41E1C indoor radius) become 8
    // entries; the zoom clamp 5 -> 7 (0x7F3AFD / 0x7F3B03).
    extern const int32_t kLevels[8];
    extern const float kDistances[8];
    constexpr uint32_t kMaxMinimapZoom = 7;

    // FUN_10a5f0e0: the nameplate class colours (0xB2DA20, read at 0x98EE88), 0xAARRGGBB -- the client's 12 with
    // class 10 recoloured, then the 21 CoA classes 12..32.
    extern const uint32_t kClassColours[33];

    // FUN_10a5e020: the race file-name table (0xB24180, read at 0x4CDA43) -- the client's 22 entries, then "" to 32.
    extern const char* const kRaceFileNames[32];

    // 0x52B27A: CGGameUI::Idle's idle-logout jne became jmp (off); SetAFKLogoutTimer turns it back on.
    bool AfkLogoutEnabled();
    void SetAfkLogoutEnabled(bool enabled);

    // 0x728AA3: the power type DoPowerRegen refreshes the runes for (6 -> 0); AscCAMgr changes it.
    uint8_t RuneUiClass();
    void SetRuneUiClass(uint8_t powerType);
}

#endif
