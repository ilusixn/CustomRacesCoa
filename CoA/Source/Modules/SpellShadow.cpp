// AscSpellShadow.cpp (see SpellShadow.h): the directional ground-target cone.
#include "CoA/Source/Modules/SpellShadow.h"
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include <cmath>
#include <cstring>

namespace {
    CoA::SpellShadow::Deps s_deps;
    void* s_textures[2] = {};   // 0x10BE4418 cone / 0x10BE441C unacceptable
    int32_t s_state = 0;        // 0x10BE4414
    float s_saved[3] = {};      // 0x10BE4420 (x, y) / 0x10BE4428 (z)
    float s_rotation[16] = {};  // 0x10BCC428
    CVar* s_cvar = nullptr;     // 0x10BE442C

    // FUN_103249d0: the spell's SpellCustomAttr +0x10 has a bit of mask.
    bool CustomAttr10(uint32_t spell, uint32_t mask) {
        const uint8_t* row = CoA::CA::SpellCustomAttrRow(spell);
        uint32_t value = 0;

        if (row) {
            memcpy(&value, row + 0x10, 4);
        }

        return row && (value & mask) != 0;
    }

    bool Aiming() {
        return s_state == 2 || s_state == 3;
    }
}

CoA::SpellShadow::Deps& CoA::SpellShadow::Dependencies() {
    return s_deps;
}

void CoA::SpellShadow::LoadTextures() {
    if (s_deps.releaseTexture) {
        if (s_textures[0]) {
            s_deps.releaseTexture(s_textures[0]);
        }

        if (s_textures[1]) {
            s_deps.releaseTexture(s_textures[1]);
        }
    }

    if (s_deps.loadTexture) {
        s_textures[0] = s_deps.loadTexture("Interface\\SpellShadow\\Spell-Shadow-Cone.blp");
        s_textures[1] = s_deps.loadTexture("Interface\\SpellShadow\\Spell-Shadow-Unacceptable.blp");
    }
}

void CoA::SpellShadow::OnCursorMode(uint32_t mode) {
    if (mode == 1) {
        CoA::ClientHooks::options.spellShadowTextures = nullptr;
        s_state = 0;
        return;
    }

    if (mode != 0x1C) {
        return;
    }

    uint32_t spell = 0;

    if (!s_deps.targetingSpell || !s_deps.targetingSpell(spell)) {
        return;
    }

    if (CustomAttr10(spell, 0x200000)) {
        CoA::ClientHooks::options.spellShadowTextures = s_textures;
        s_state = 2;
    } else if (CustomAttr10(spell, 0x400000)) {
        CoA::ClientHooks::options.spellShadowTextures = s_textures;
        s_state = 3;
    }
}

void CoA::SpellShadow::BeforeDraw(int32_t& style, float position[3], float& size) {
    CoA::ClientHooks::options.spellShadowSizedHeight = style == 0;
    float player[3];

    if (!Aiming() || !s_deps.playerPosition || !s_deps.playerPosition(player)) {
        return;
    }

    memcpy(s_saved, position, sizeof(s_saved));
    memcpy(position, player, sizeof(player));
    style = 0;
    size = s_deps.spellRadius ? s_deps.spellRadius() : 0.0f;
}

bool CoA::SpellShadow::BeforeShadowRender(float box[6]) {
    float player[3];

    if (!Aiming() || !s_deps.playerPosition || !s_deps.playerPosition(player)) {
        return false;
    }

    float angle;

    if (s_state == 2) {
        angle = s_deps.facingTo ? s_deps.facingTo(player, s_saved) : 0.0f;
    } else {
        angle = s_deps.playerFacing ? s_deps.playerFacing() : 0.0f;
    }

    // 0x407F40: identity, then rotated by -angle.
    memset(s_rotation, 0, sizeof(s_rotation));
    s_rotation[0] = s_rotation[5] = s_rotation[10] = s_rotation[15] = 1.0f;
    RotateBy(s_rotation, -angle);

    const float dx = static_cast<float>(cos(static_cast<double>(angle))) * 7.5f;
    box[3] = dx + box[3];
    const float dy = static_cast<float>(sin(static_cast<double>(angle))) * 7.5f;
    box[4] = dy + box[4];
    box[5] = box[5] + 0.0f;
    box[0] = dx + box[0];
    box[1] = dy + box[1];
    box[2] = box[2] + 0.0f;
    return true;
}

void CoA::SpellShadow::BeforeSelectionRender(float box[6]) {
    box[5] = box[5] + 2.5f;
    box[2] = box[2] - 2.5f;
}

void CoA::SpellShadow::RotateBy(float m[16], float angle) {
    const float c = static_cast<float>(cos(static_cast<double>(angle)));
    const float s = static_cast<float>(sin(static_cast<double>(angle)));
    const float r[16] = { c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    float out[16];

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            out[i * 4 + j] = r[i * 4 + 1] * m[4 + j] + r[i * 4] * m[j] + r[i * 4 + 2] * m[8 + j] + r[i * 4 + 3] * m[12 + j];
        }
    }

    memcpy(m, out, sizeof(out));
}

int32_t CoA::SpellShadow::State() {
    return s_state;
}

void* const* CoA::SpellShadow::Textures() {
    return s_textures;
}

const float* CoA::SpellShadow::Rotation() {
    return s_rotation;
}

const float* CoA::SpellShadow::SavedTarget() {
    return s_saved;
}

void CoA::SpellShadow::Init() {
    // FUN_103216a0: the world-entry load, the three detours (SpellShadowClient / the marked client sites), the CVar.
    CoA::ClientHooks::enterWorld.Add(CoA::SpellShadow::LoadTextures);
    // name, help, flags, default, callback, category, setCommand, slot, world
    COA_CVAR("SpellShadowShowBad", nullptr, 1, "1", nullptr, 4, false, &s_cvar, true);
}

void CoA::SpellShadow::Reset() {
    s_deps = Deps();
    s_textures[0] = s_textures[1] = nullptr;
    s_state = 0;
    memset(s_saved, 0, sizeof(s_saved));
    memset(s_rotation, 0, sizeof(s_rotation));
    s_cvar = nullptr;
}
