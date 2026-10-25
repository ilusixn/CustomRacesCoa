#ifndef COA_SOURCE_MODULES_CANTOGGLE_H
#define COA_SOURCE_MODULES_CANTOGGLE_H

#include <cstdint>

// AscCanToggle.cpp: CanToggle<X>Mode(enable) -- can the player switch a challenge mode on / off right now? Each reads a
// boolean (FUN_100b8aa0), collects reason codes and returns (reasons empty, reasons ? { "TOGGLE_<X>_<REASON>", ... } :
// nil) through FUN_100beb90. Shared checks when enabling: CONFIG_<X>_ENABLE(D) missing or false -> DISABLED (2); game-mode
// bits 15..21 -> one reason each, then OUTSIDE_INTERACTION (1); always: enable == the mode's own bit -> ALREADY_ACTIVE
// (3) / ALREADY_INACTIVE (4).
namespace CoA::CanToggle {
    // The client questions; CanToggleClient.cpp installs them.
    struct Deps {
        bool (*hasPlayer)() = nullptr;               // 0x4038F0 found the active player
        uint32_t (*playerLevel)() = nullptr;         // descriptor +0xD8
        uint8_t (*playerClass)() = nullptr;          // UNIT_FIELD_BYTES_0 byte 1 (descriptor +0x5C >> 8)
        bool (*hasAura)(uint32_t spell) = nullptr;   // 0x7282A0 on the player
    };

    Deps& Dependencies();

    void Init();
}

#endif
