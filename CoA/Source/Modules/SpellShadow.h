#ifndef COA_SOURCE_MODULES_SPELLSHADOW_H
#define COA_SOURCE_MODULES_SPELLSHADOW_H

#include <cstdint>

struct CVar;

// AscSpellShadow.cpp (installer FUN_103216a0): the cone indicator for directional ground-target spells.
//
//   CVar SpellShadowShowBad (FUN_10114540 world queue; flags 1, "1", category 4; slot 0x10BE442C) -- registered, never
//     read.
//   World entry (FUN_10320b60): Interface\SpellShadow\Spell-Shadow-Cone.blp and -Unacceptable.blp are (re)loaded into
//     0x10BE4418 / 0x10BE441C (the old ones released through 0x47BF30). Only the cone is ever drawn.
//   0x616800 CursorSetMode (before; FUN_10320c20): mode 1 (POINT_CURSOR) puts the stock texture table back into the
//     ground-target draw (the operand at 0x4F8A4B = 0xB74350), state 0; mode 0x1C (CAST_ERROR_CURSOR) with a targeting
//     spell (*0xD3F4E4 +0x18) whose SpellCustomAttr +0x10 has 0x200000 / 0x400000 points it at the cone table
//     (&0x10BE4418), state 2 / 3. Anything else leaves both alone.
//   0x4F8A40 DrawSpellCursorShadow (before; FUN_10321240): the operand at 0x4F8AD4 -- the box's half height -- becomes
//     0xA4040C (2.0) while the style (*0xAC79A4) is set, else 0xB74370 (the shadow size); then, with an active player in
//     state 2 / 3, the target point (0xB74380) is saved and replaced by the player's position, the style becomes 0 and the
//     size Spell_C_GetSpellRadius().
//   0x7E4370 ProjectTex::Render2d (before; FUN_10321830): called from 0x4F8CF0 (the shadow draw) in state 2 (angle from
//     the player to the saved point, 0x4F5130) or 3 (the player's facing) with an active player: the matrix 0x10BCC428
//     becomes a rotation by -angle, both corners of the box move 7.5 yd along the angle, and the colour / matrix
//     arguments become 0xAD2D40 (the tooltip green) / that matrix; called from 0x725A8F (the target selection circle),
//     the box's z extent grows by 2.5 at each end. The box is the caller's, changed in place.
namespace CoA::SpellShadow {
    struct Deps {
        void* (*loadTexture)(const char* path) = nullptr;     // 0x4B9760(path, 0x201, status, 1)
        void (*releaseTexture)(void* texture) = nullptr;      // 0x47BF30
        bool (*targetingSpell)(uint32_t& spell) = nullptr;   // *0xD3F4E4 +0x18; false without one
        bool (*playerPosition)(float out[3]) = nullptr;      // the active player's vtable +0x2C; false without a player
        float (*playerFacing)() = nullptr;                    // vtable +0x34
        float (*facingTo)(const float from[3], const float to[3]) = nullptr;   // 0x4F5130 CalculateFacingTo
        float (*spellRadius)() = nullptr;                    // 0x8019C0 Spell_C_GetSpellRadius
    };

    Deps& Dependencies();

    void LoadTextures();                     // FUN_10320b60 (world entry)
    void OnCursorMode(uint32_t mode);        // FUN_10320c20 (before 0x616800)
    // FUN_10321240 (before 0x4F8A40): the client's style (0xAC79A4), target point (0xB74380) and size (0xB74370).
    void BeforeDraw(int32_t& style, float position[3], float& size);
    // FUN_10321830 from 0x4F8CF0: true when the colour argument becomes the tooltip green and the matrix Rotation().
    bool BeforeShadowRender(float box[6]);
    // FUN_10321830 from 0x725A8F.
    void BeforeSelectionRender(float box[6]);

    // FUN_102bd3f0: m = RotZ(angle) x m (row-major 4x4).
    void RotateBy(float m[16], float angle);

    int32_t State();                         // 0x10BE4414
    void* const* Textures();                 // 0x10BE4418 / 0x10BE441C
    const float* Rotation();                 // 0x10BCC428
    const float* SavedTarget();              // 0x10BE4420

    void Init();
    // Tests only.
    void Reset();
}

#endif
