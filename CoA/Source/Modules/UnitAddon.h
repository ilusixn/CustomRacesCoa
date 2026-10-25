#ifndef COA_SOURCE_MODULES_UNITADDON_H
#define COA_SOURCE_MODULES_UNITADDON_H

#include <cstdint>

// AscUnitAddon.cpp: globals that read a unit's addon fields (CoA::ObjectAddon::Unit). Field 0 alternative power type
// (SpellAlternativePowerType.dbc id), 1 alternative power, 2 prestige level, 3 prestige (second value), 5 average item
// level (float), 35 glory points. The unit token goes through Deps (UnitAddonClient.cpp: Script_GetGUIDFromToken,
// 0x60ABF0); the original reads an uninitialised GUID when that fails (GetGloryPoints / UnitAverageItemLevel), GUID 0
// here.
namespace CoA::UnitAddon {
    struct Deps {
        bool (*tokenGuid)(const char* token, uint64_t& guid) = nullptr;   // sets 0 when unknown
        uint64_t (*activePlayerGuid)() = nullptr;
    };

    Deps& Dependencies();
    void Init();
}

#endif
