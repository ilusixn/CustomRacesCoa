#ifndef COA_SOURCE_MODULES_MYSTICENCHANTPRESET_H
#define COA_SOURCE_MODULES_MYSTICENCHANTPRESET_H

// AscMysticEnchantPreset.cpp: C_MysticEnchantPreset, the original's subsystem at 0x102F1780..0x102F37F0.
//
// Manager DAT_10BCC088 (accessor FUN_102F3030): the active preset (0-based), the presets (slot enchants each) and the
// preset an Activate is waiting on. SMSG 0x5FF replaces the presets, 0x600 sets the active one; 0x602 / 0x60D / 0x616
// are the save / activate / unlock results (an RE_PRESET_* name). The glue screen (FUN_102F2230) resets it.
namespace CoA::MysticEnchantPreset {
    void Init();
    // FUN_102F2230: active = -1, no presets, pending = 0.
    void Reset();
}

#endif
