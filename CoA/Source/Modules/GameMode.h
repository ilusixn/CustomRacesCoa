#ifndef COA_SOURCE_MODULES_GAMEMODE_H
#define COA_SOURCE_MODULES_GAMEMODE_H

#include <cstdint>

// AscGameMode.cpp: game mode + per-specialization links -- the original's manager at 0x10be4138 (accessor
// FUN_1031ef10, init 0x1031ee80). 64 functions in the original read it.
//
//   +0x00 custom game mode (what GetCustomGameMode reports)   +0x04 active mode mask (tutorial and skill-card gating)
//   +0x08 + i*0x20, i < 20: one record per specialization: flags (bit 0 = build draft), mystic-enchant preset + has,
//   equipment-set guid (u64) + has.
//
// SMSG_GAME_MODE_CHANGED            0x90B { u32 mode }                 mode = mask = value
// SMSG_UPDATE_SPECIALIZATION_FLAGS  0x5CC { u32 spec, u32 flags }       SPECIALIZATION_FLAGS_UPDATED
// (0x737) mystic-enchant link       { u32 spec, u8 has, [u32 preset] }  MYSTIC_ENCHANT_SPECIALIZATION_LINK_UPDATED
// (0x738) equipment-set link        { u32 spec, u8 has, [u64 guid] }    EQUIPMENT_SET_SPECIALIZATION_LINK_UPDATED
namespace CoA::GameMode {
    uint32_t Mode();                    // +0x00
    uint32_t ActiveMask();              // +0x04
    bool SpecBuildDraft(int32_t spec);  // FUN_1031efd0: spec record flags bit 0; false past 19
    int32_t ActiveSpecIndex();          // FUN_1016f580 (character advancement), -1 when unknown
    void OnModeChanged(void (*cb)());   // run by SMSG_GAME_MODE_CHANGED before the event (FUN_1031bd50 slot)

    // The client / other-module questions; installed by their owners.
    struct Deps {
        int32_t (*activeSpecIndex)() = nullptr;                            // AscCA::ActiveSpecIndex (Plan 5)
        bool (*equipmentSetId)(const char* name, uint32_t& id) = nullptr;  // 0x5AE600: the set's +8 (GUID low)
        const char* (*equipmentSetName)(uint32_t id) = nullptr;            // the client set list: the set's name
    };

    Deps& Dependencies();

    void Init();
    // Tests only.
    void Reset();
}

#endif
