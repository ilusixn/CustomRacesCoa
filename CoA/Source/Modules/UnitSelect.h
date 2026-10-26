#ifndef COA_SOURCE_MODULES_UNITSELECT_H
#define COA_SOURCE_MODULES_UNITSELECT_H

#include <cstdint>
#include <vector>

struct CVar;

// AscUnitSelect.cpp: AoE radius indicators, quest-unit circles, the player's own selection circle and auto-interact
// (installer FUN_102daaf0). Its seven FUN_10114540 CVars are checked against the genuine DLL (0x102DAB13 .. 0x102DACF6:
// help nullptr, flags 1; aoeRadiusIndicator* / AutoInteract* category 3, showQuestUnitCircles / ObjectSelectionCircleSelf
// category 4).
//
//   World entry (FUN_102da1c0)  the two cone splats (0x10BE3858 / 0x10BE385C), released first when loaded
//   after 0x7460C0 (FUN_102da2b0)  UnitSelectTexture / TrackingTexture / UnitSelectTexture-Quest (0x10BE384C / 850 / 854)
//   0x72B7E0 replaced (FUN_102da6c0)  CGUnit_C::Render skipped once AutoInteractCursorTime (ms, when set) has passed
//                                     since the last click-to-move; else drawn with the tracking texture
//   0x727400 around (FUN_102da340)  a call from past the exe image first sends CMSG 0x51F "Ascension" / "CTM" /
//                                   "Click-to-Move called by bot."; after it the tracking turn rate is AutoInteractTurnSpeed
//                                   and the time is kept
//   after 0x4F6F90 (FUN_102dbf60)  the indicators, the quest circles, the player's own circle
//   every other 0x6DF050 (FUN_102db0d0)  the projected-batch z pad (0x82080B): 10.0 outside a dungeon / raid, back to
//                                        the stock 2.0 inside one while any aoeRadiusIndicator* is 1; the installer
//                                        writes 10.0 once at start-up
namespace CoA::UnitSelect {
    struct Colours {
        uint32_t self = 0;       // 0xAD2D3C
        uint32_t enemy = 0;      // 0xAD2D48
        uint32_t friendly = 0;   // 0xAD2D40
    };

    struct Deps {
        // 0x4B9760(path, 0x201, status, 1) / 0x47BF30.
        void* (*loadTexture)(const char* path) = nullptr;
        void (*releaseTexture)(void* texture) = nullptr;
        uint32_t (*now)() = nullptr;                                  // 0x86AE20
        void (*setTrackingTurnRate)(float rate) = nullptr;            // 0xCA11DC
        // Map.dbc +8 of the current map (0xADFBC4); false without a row.
        bool (*mapInstanceType)(uint32_t& type) = nullptr;
        void (*setProjectionZPad)(float pad) = nullptr;               // the operand at 0x82080B
        Colours (*colours)() = nullptr;
        // The client half's drawers (FUN_102db9c0 / FUN_102db140 over every visible object; FUN_102dbfe0; the player's
        // vtable +0x6C).
        void (*drawIndicators)() = nullptr;
        void (*drawQuestCircles)() = nullptr;
        void (*drawOwnCircle)() = nullptr;
    };

    Deps& Dependencies();

    // The DLL's texture slots (the drawers point the 0x744EB1 operand at them).
    void* const* UnitSelectTexture();   // 0x10BE384C
    void* const* TrackingTexture();     // 0x10BE3850
    void* const* QuestTexture();        // 0x10BE3854
    // The projected-batch z pad (0x82080B's float); the client half copies it into the Engine at install.
    float ProjectionZPad();
    void* Cone30();                     // 0x10BE3858
    void* Cone90();                     // 0x10BE385C

    void LoadCones();                   // FUN_102da1c0
    void AfterObjectInitialize();       // FUN_102da2b0

    // FUN_102da6c0: true when the auto-track cursor is not drawn.
    bool SkipAutoTrackCursor();
    // FUN_102da340: before the original (returnPastImage: its return address is past the exe image) and after it.
    void BeforeClickToMove(bool returnPastImage);
    void AfterClickToMove();

    // The indicator colour for an effect of a unit as the player sees it, or false when its CVar is off (or the unit is
    // neutral): self / enemy (reaction < 3) / friendly (reaction > 3).
    bool IndicatorColour(bool isSelf, int32_t reaction, uint32_t& colour);
    // {min x, min y, min z, max x, max y, max z}: r either side in x / y, 2r in z.
    void Box(const float p[3], float r, float box[6]);
    // FUN_102db140's cone: an implicit target A of 24 / 54 / 104 / 59 among the three.
    bool IsCone(const uint32_t targetA[3]);
    // SpellRadius row {+4 base, +8 per level, +0xC max} at `level`: level * perLevel + base, capped at max.
    float Radius(float base, float perLevel, float max, uint32_t level);
    // FUN_102db140's placement: the box at 0.725 x r turned to the facing and pushed another 0.725 x that ahead.
    void ConeBox(const float p[3], float r, float facing, float box[6]);
    // FUN_102dbfe0: an entry quest-relevant this frame (cached; the cache is pushed again even when it hit).
    bool QuestCircle(uint32_t entry, bool (*relevant)(uint32_t entry, void* ctx), void* ctx);
    const std::vector<uint32_t>& QuestEntries();

    void Render();   // FUN_102dbf60
    void Tick();     // FUN_102db0d0

    void Init();
    // Tests only.
    void Reset();
}

#endif
