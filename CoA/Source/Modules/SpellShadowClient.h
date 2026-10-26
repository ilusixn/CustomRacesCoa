#ifndef COA_SOURCE_MODULES_SPELLSHADOWCLIENT_H
#define COA_SOURCE_MODULES_SPELLSHADOWCLIENT_H

#include <cstdint>

class C3Vector;
class C44Matrix;
class CAaBox;
class CImVector;

// AscSpellShadow.cpp's client half: the detour bodies at the marked client sites (see SpellShadow.h).
namespace CoA::SpellShadowClient {
    // FUN_10321240, before 0x4F8A40 DrawSpellCursorShadow: the client's style / target point / size.
    void BeforeDraw(int32_t& style, C3Vector& position, float& size);
    // FUN_10321830 from 0x4F8CF0 (the shadow's ProjectTex::Render2d): the box, colour and matrix arguments.
    void BeforeShadowRender(CAaBox& box, const CImVector*& color, const C44Matrix*& matrix);
    // FUN_10321830 from 0x725A8F (CGUnit_C::RenderTargetSelection's first Render2d).
    void BeforeSelectionRender(CAaBox& box);
}

#endif
