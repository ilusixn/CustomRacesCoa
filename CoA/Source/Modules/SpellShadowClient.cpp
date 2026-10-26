// AscSpellShadow.cpp's client half: the textures, the targeting spell, the active player and the box / colour / matrix
// arguments of the two ProjectTex::Render2d calls.
#include "CoA/Source/Modules/SpellShadow.h"
#include "CoA/Source/Modules/SpellShadowClient.h"
#include "CoA/Source/Modules/Modules.h"
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Gx/CGxTexFlags.h"
#include "Engine/Source/Services/Texture.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Tempest/CAaBox.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

namespace {
    static_assert(sizeof(CAaBox) == 6 * sizeof(float), "CAaBox is b then t");
    static_assert(sizeof(C44Matrix) == 16 * sizeof(float), "C44Matrix is 16 floats, row-major");

    // 0xAD2D40 (UIUtil/Tooltip.cpp's s_tooltipGreenColor, never written): the colour the cone is drawn with.
    const uint32_t kTooltipGreen = 0xFF00FF00;

    void* LoadTexture(const char* path) {   // 0x4B9760(path, 0x201, status, 1)
        CStatus status;
        return TextureCreate(path, CGxTexFlags(), &status, 1);
    }

    void ReleaseTexture(void* texture) {   // 0x47BF30
        HandleClose(reinterpret_cast<HOBJECT>(texture));
    }

    // *0xD3F4E4 +0x18: the DLL reads the targeting cast's dword at +0x18 -- PendingSpellCastData +0x10, a field the client
    // never writes (CreatePendingSpellCast zeroes it); the spell id is +0x20. Kept as the original reads it.
    bool TargetingSpell(uint32_t& spell) {
        if (!s_targetingSpell) {
            return false;
        }

        spell = s_targetingSpell->m_data.unused_0010;
        return true;
    }

    bool PlayerPosition(float out[3]) {   // vtable +0x2C
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player) {
            return false;
        }

        const C3Vector p = player->GetPosition();
        out[0] = p.x;
        out[1] = p.y;
        out[2] = p.z;
        return true;
    }

    float PlayerFacing() {   // vtable +0x34
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
        return player ? player->GetFacing() : 0.0f;
    }

    float FacingTo(const float from[3], const float to[3]) {   // 0x4F5130
        const C3Vector a(from[0], from[1], from[2]);
        const C3Vector b(to[0], to[1], to[2]);
        return CalculateFacingTo(&a, &b);
    }
}

void CoA::SpellShadowClient::BeforeDraw(int32_t& style, C3Vector& position, float& size) {
    float p[3] = { position.x, position.y, position.z };
    CoA::SpellShadow::BeforeDraw(style, p, size);
    position = C3Vector(p[0], p[1], p[2]);
}

void CoA::SpellShadowClient::BeforeShadowRender(CAaBox& box, const CImVector*& color, const C44Matrix*& matrix) {
    if (CoA::SpellShadow::BeforeShadowRender(reinterpret_cast<float*>(&box))) {
        color = reinterpret_cast<const CImVector*>(&kTooltipGreen);
        matrix = reinterpret_cast<const C44Matrix*>(CoA::SpellShadow::Rotation());
    }
}

void CoA::SpellShadowClient::BeforeSelectionRender(CAaBox& box) {
    CoA::SpellShadow::BeforeSelectionRender(reinterpret_cast<float*>(&box));
}

void CoA::SpellShadowClient::Init() {
    CoA::SpellShadow::Deps& d = CoA::SpellShadow::Dependencies();
    d.loadTexture = LoadTexture;
    d.releaseTexture = ReleaseTexture;
    d.targetingSpell = TargetingSpell;
    d.playerPosition = PlayerPosition;
    d.playerFacing = PlayerFacing;
    d.facingTo = FacingTo;
    d.spellRadius = Spell_C_GetSpellRadius;
}
