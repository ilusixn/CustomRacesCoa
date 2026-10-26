#ifndef COA_SOURCE_MODULES_LOADINGSCREEN_H
#define COA_SOURCE_MODULES_LOADINGSCREEN_H

#include <cstdint>

// AscLoadingScreen.cpp (installer FUN_10290630):
//   0x409ED0 LoadSimpleBackgroundTexture, first: the map being loaded (0xAB63BC) gets one of its
//            LoadingScreenVariation.dbc entries (+4 .. +0x40, the non-zero ones) at random as its Map.dbc loading screen
//            (+0x24) (FUN_10290810 / FUN_10210a60: std::mt19937 seeded once from std::random_device, uniform)
//   0x409550 LoadingScreenDisable, after: the gate 0x10BCB8C8 opens; 0x40AB70 LoadingScreenEnable, after: it closes
//   0x86B280 OsSleep runs only while the gate is open (it starts open: 1 in the DLL's .data; the reconstruction has it
//            closed) -- no sleeping during a loading screen
namespace CoA::LoadingScreen {
    struct Deps {
        bool (*hasMap)(uint32_t map) = nullptr;                            // Map.dbc (0xAD4160)
        void (*setMapLoadingScreen)(uint32_t map, uint32_t screen) = nullptr;   // Map.dbc +0x24
    };

    Deps& Dependencies();
    // FUN_10210a60: one of the row's non-zero entries (+4 .. +0x40), 0 when there is none.
    uint32_t PickVariation(const uint8_t* row);
    void ApplyVariation(uint32_t map);       // FUN_10290810
    void AfterLoadingScreenDisable();        // FUN_10290870
    void AfterLoadingScreenEnable();         // FUN_10290890
    bool SleepAllowed();                     // FUN_102908b0

    void Init();
    // Tests only: Deps forgotten, the gate open.
    void Reset();
}

#endif
