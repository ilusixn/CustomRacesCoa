#ifndef COA_SOURCE_MODULES_CURSOR_H
#define COA_SOURCE_MODULES_CURSOR_H

#include <cstdint>

struct CVar;
struct lua_State;

// AscCursor.cpp, the cursor module (installer FUN_10a1e950): cursor sizes and a software cursor.
//
//   cursorSizePreferred (glue, GlueCVars.cpp; callback FUN_10a1e320): the size and the texture variant.
//   The cursor table 0x10D3C7A8: five slots per stock cursor index (1..0x34, names from 0xAD2808); on every glue screen
//     (FUN_10a1e140) empty slots 0 / 2 / 4 are loaded from Interface\Cursor\<name> and <name>2 / <name>4.
//     SetAttackCursor (AscGlobalsJ.cpp, registered with GlobalsJ's bindings) writes indices 4 / 30 of the same table.
//     A second glue-screen callback (FUN_10a1e250) sets the cursor mode (0xC26DE8) to 1.
//   0x7695E0 CVGxCursorCallback (FUN_10a1e260): "1" is the hardware cursor (the software cursor's render layer
//     released); anything else adds the render layer (FUN_10a1eb80) and clears the hardware flag. Then the original.
//   0x616830 CursorSetFromName (FUN_10a1e610): in software mode the texture is loaded here instead of by the original --
//     a name containing "Directions" with a size variant loses its last four characters and gains the variant number --
//     and the cursor mode becomes 0x35; the original is skipped (returns 0).
//   0x683640 CGxDevice::CursorSetVisible (FUN_10a205f0): unless hardware or an item is on the cursor (0xC25DE0), the show
//     flag is kept for the software cursor and the device gets 0.
//   0x6162C0 UpdateCursor (FUN_10a20530): after the original, the device's CursorSetVisible(1) while an item is on it.
//   The render layer (FUN_10a1eb80) draws the current cursor texture as a quad at the mouse position.
namespace CoA::Cursor {
    constexpr uint32_t kCustomMode = 0x35;   // NUM_CURSOR_MODES: the texture CursorSetFromName loaded
    constexpr uint32_t kSlots = 5;

    // FUN_10a1e320: "1" / "2" / "3" / "4" pick 48 / 64 / 96 / 128 px, anything else 32 (variant 0). The variant becomes
    // 2 ("1".."3") or 4 ("4") only when Data\Patch-Q.mpq opens; otherwise a non-zero size keeps the previous one. The
    // drawn size is width px / g_widthPixels (0xC7D2C8) and height PixelToScreenHeight(px) (0x6C0AC0).
    void SizePreferred(const char* value, bool hdAvailable);
    bool OnSizePreferred(CVar* var, const char* oldValue, const char* value, void* arg);
    uint32_t Size();
    uint32_t Variant();
    float Width();
    float Height();

    // FUN_10a1e260, before CVGxCursorCallback.
    void OnGxCursor(const char* value);
    bool Hardware();
    // FUN_10a1e610: true when it took the call over (software mode); CursorSetFromName then returns 0.
    bool SetFromName(const char* name);
    // FUN_10a205f0: the visibility CGxDevice::CursorSetVisible goes on with.
    int32_t CursorVisible(int32_t show);
    bool Show();
    // FUN_10a20530, after UpdateCursor.
    void AfterUpdateCursor();
    // FUN_10a1e140 / FUN_10a1e250 (every glue screen).
    void LoadTable();
    void ResetMode();
    // FUN_10a1eb80's texture: nullptr when hidden; the custom texture for mode 0x35, else the table's slot
    // variant + max(mode, 1) * 5.
    void* DrawTexture();
    void* Slot(uint32_t mode, uint32_t slot);
    // SetAttackCursor(name) (FUN_10a1edf0): "Interface\Cursor\<name>" and "...\Unable<name>" into slot 0, then
    // "<name><n>" / "Unable<name><n>" for n = 2 and 4, into modes 4 (Attack) and 30 (UnableAttack).
    int SetAttackCursor(lua_State* L);

    struct Deps {
        void* (*load)(const char* path) = nullptr;                // 0x4B9760(path, 0x201, 0xAF5718, 1)
        const char* (*cursorName)(uint32_t mode) = nullptr;       // 0xAD2808
        uint32_t (*mode)() = nullptr;                             // 0xC26DE8
        void (*setMode)(uint32_t mode) = nullptr;
        bool (*itemOnCursor)() = nullptr;                         // 0xC25DE0
        void (*addRenderLayer)() = nullptr;                       // 0x4A8BB0(0, 2.0, 3, 0, FUN_10a1eb80, &handle)
        void (*releaseRenderLayer)() = nullptr;                   // 0x47BF30(handle), handle = 0 (when there is one)
        void (*deviceCursorVisible)(int32_t visible) = nullptr;   // 0x68E750(*0xC5DF88, visible)
        float (*screenWidth)() = nullptr;                         // *0xC7D2C8
        float (*pixelToScreenHeight)(int32_t pixels) = nullptr;   // 0x6C0AC0
    };

    Deps& Dependencies();
    void Init();
    // Tests only.
    void Reset();
}

#endif
