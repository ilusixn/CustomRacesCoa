#ifndef COA_SOURCE_MODULES_GLOBALSG_H
#define COA_SOURCE_MODULES_GLOBALSG_H

#include <cstdint>

// AscGlobalsG.cpp's natives that rewrite client code or data (the AFK timers, the name-plate range and size); the
// client reads these values where the original's bytes sit. GlobalsGClient.cpp has the natives over client objects.
namespace CoA::GlobalsG {
    // SetAFKTimer (FUN_10a50910): CGGameUI::Idle's auto-sit compare (0x52B259, stock 300000) and its jl (0x52B25F,
    // jmp while 0: never AFK).
    int32_t AfkTime();
    bool AfkEnabled();
    // SetAFKLogoutTimer (FUN_10a503c0): the idle-logout compare (0x52B266, stock 1800000); 0 also turns the logout off
    // (AttachPatches::AfkLogoutEnabled, the jne at 0x52B27A), anything else on.
    int32_t AfkLogoutTime();
    // SetNamePlateRange: 5..60 yards, squared (0xADAA7C, stock 1681 = 41^2).
    float NamePlateRangeSq();
    // AscNamePlates.cpp FUN_102bebc0 (nameplateDistance) writes the same float.
    void SetNamePlateRangeSq(float rangeSq);
    // SetNamePlateScaleX / Y: 0..1 (0xB2DA74 width, stock 0.1; 0xB2DA70 height, stock 0.025).
    float NamePlateWidth();
    float NamePlateHeight();

    // Tests only.
    void Reset();
}

#endif
