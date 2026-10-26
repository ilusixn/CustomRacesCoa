#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/SpellShadow.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;

    V s_calls;
    char s_handles[16];
    bool s_hasSpell = false;
    uint32_t s_spell = 0;
    bool s_hasPlayer = true;
    float s_player[3] = { 10.0f, 20.0f, 30.0f };
    float s_facing = 1.0f;
    float s_radius = 6.0f;
    float s_facingFrom[3];
    float s_facingTo[3];

    // SpellCustomAttr.dbc: the first row is skipped by FUN_1020b080; +4 spell, +0x10 the shadow flags.
    bool ReadAttrs(const char* path, std::vector<uint8_t>& out) {
        if (!strstr(path, "SpellCustomAttr.dbc")) {
            return false;
        }

        out = CoATest::Wdbc({ { 1, 9999, 0, 0, 0x200000, 0, 0 }, { 2, 701, 0, 0, 0x200000, 0, 0 }, { 3, 702, 0, 0, 0x400000, 0, 0 },
            { 4, 703, 0, 0, 0x600000, 0, 0 }, { 5, 704, 0, 0, 0x100000, 0, 0 } }, std::string(1, '\0'));
        return true;
    }

    void Start() {
        CoA::ClientHooks::Reset();
        CoA::ClientHooks::enterWorld.Clear();
        CoA::CVars::Reset();
        CoA::SpellShadow::Reset();
        CoA::Dbc::SetFileReader(ReadAttrs);
        CoA::Dbc::Get("DBFilesClient\\SpellCustomAttr.dbc").Unload();
        CoA::CA::RebuildSpellCustomAttr();
        s_calls.clear();
        s_hasSpell = false;
        s_spell = 0;
        s_hasPlayer = true;
        s_facing = 1.0f;
        s_radius = 6.0f;
        CoA::SpellShadow::Deps& d = CoA::SpellShadow::Dependencies();
        d.loadTexture = [](const char* path) -> void* {
            s_calls.push_back(std::string("load:") + path);
            return s_handles + s_calls.size();
        };
        d.releaseTexture = [](void* t) { s_calls.push_back("release:" + std::to_string(static_cast<char*>(t) - s_handles)); };
        d.targetingSpell = [](uint32_t& spell) { spell = s_spell; return s_hasSpell; };
        d.playerPosition = [](float out[3]) {
            if (s_hasPlayer) {
                memcpy(out, s_player, sizeof(s_player));
            }

            return s_hasPlayer;
        };
        d.playerFacing = [] { return s_facing; };
        d.facingTo = [](const float from[3], const float to[3]) {
            memcpy(s_facingFrom, from, sizeof(s_facingFrom));
            memcpy(s_facingTo, to, sizeof(s_facingTo));
            return 0.5f;
        };
        d.spellRadius = [] { return s_radius; };
        CoA::SpellShadow::Init();
        CoA::CVars::RegisterWorld();
    }

    void Stop() {
        CoA::Dbc::Get("DBFilesClient\\SpellCustomAttr.dbc").Unload();
        CoA::CA::RebuildSpellCustomAttr();
        CoA::Dbc::SetFileReader(nullptr);
        CoA::SpellShadow::Reset();
        CoA::ClientHooks::enterWorld.Clear();
        CoA::ClientHooks::Reset();
        CoA::CVars::Reset();
    }

    void Target(uint32_t spell) {
        s_hasSpell = true;
        s_spell = spell;
    }

    bool Near(float a, float b) {
        return std::fabs(a - b) < 1e-5f;
    }
}

// FUN_103216a0: SpellShadowShowBad (flags 1, "1", category 4, no callback; checked against the DLL) and the world
// entry load.
COA_TEST(SpellShadow_CVarAndInstall) {
    Start();
    size_t count = 0;
    const CoA::CVarEntry* world = CoA::CVars::World(count);
    CHECK(count == 1);

    if (count == 1) {
        CHECK(strcmp(world[0].name, "SpellShadowShowBad") == 0);
        CHECK(strcmp(world[0].defaultValue, "1") == 0);
        CHECK(world[0].flags == 1 && world[0].category == 4);
        CHECK(world[0].callback == nullptr && world[0].help == nullptr && !world[0].setCommand && world[0].slot != nullptr);
    }

    CHECK(!CoA::ClientHooks::enterWorld.Empty());
    CHECK(CoA::SpellShadow::State() == 0);
    CHECK(CoA::ClientHooks::options.spellShadowTextures == nullptr);
    CHECK(!CoA::ClientHooks::options.spellShadowSizedHeight);
    Stop();
}

// FUN_10320b60: both textures load at each world entry; the previous ones are released first.
COA_TEST(SpellShadow_TexturesPerWorldEntry) {
    Start();
    CoA::ClientHooks::enterWorld.Run();
    CHECK((s_calls == V{ "load:Interface\\SpellShadow\\Spell-Shadow-Cone.blp", "load:Interface\\SpellShadow\\Spell-Shadow-Unacceptable.blp" }));
    CHECK(CoA::SpellShadow::Textures()[0] == s_handles + 1 && CoA::SpellShadow::Textures()[1] == s_handles + 2);
    CoA::ClientHooks::enterWorld.Run();
    CHECK((s_calls == V{ "load:Interface\\SpellShadow\\Spell-Shadow-Cone.blp", "load:Interface\\SpellShadow\\Spell-Shadow-Unacceptable.blp",
        "release:1", "release:2", "load:Interface\\SpellShadow\\Spell-Shadow-Cone.blp", "load:Interface\\SpellShadow\\Spell-Shadow-Unacceptable.blp" }));
    Stop();
}

// FUN_10320c20: CAST_ERROR_CURSOR with a flagged targeting spell -> the cone table, state 2 / 3; POINT_CURSOR restores.
COA_TEST(SpellShadow_CursorModes) {
    Start();
    CoA::SpellShadow::OnCursorMode(0x1C);   // no targeting spell
    CHECK(CoA::SpellShadow::State() == 0 && CoA::ClientHooks::options.spellShadowTextures == nullptr);
    Target(704);                            // other flags
    CoA::SpellShadow::OnCursorMode(0x1C);
    CHECK(CoA::SpellShadow::State() == 0 && CoA::ClientHooks::options.spellShadowTextures == nullptr);
    Target(701);
    CoA::SpellShadow::OnCursorMode(2);      // CAST_CURSOR: nothing
    CHECK(CoA::SpellShadow::State() == 0);
    CoA::SpellShadow::OnCursorMode(0x1C);
    CHECK(CoA::SpellShadow::State() == 2 && CoA::ClientHooks::options.spellShadowTextures == CoA::SpellShadow::Textures());
    Target(704);                            // unflagged: the cone stays
    CoA::SpellShadow::OnCursorMode(0x1C);
    CHECK(CoA::SpellShadow::State() == 2);
    Target(702);
    CoA::SpellShadow::OnCursorMode(0x1C);
    CHECK(CoA::SpellShadow::State() == 3);
    Target(703);                            // both flags: 0x200000 is asked first
    CoA::SpellShadow::OnCursorMode(0x1C);
    CHECK(CoA::SpellShadow::State() == 2);
    Target(9999);                           // the skipped first row
    CoA::SpellShadow::OnCursorMode(1);
    CHECK(CoA::SpellShadow::State() == 0 && CoA::ClientHooks::options.spellShadowTextures == nullptr);
    Stop();
}

// FUN_10321240: the half height follows the style first; in state 2 / 3 with a player the target point is saved and moved.
COA_TEST(SpellShadow_BeforeDraw) {
    Start();
    int32_t style = 1;
    float pos[3] = { 1.0f, 2.0f, 3.0f };
    float size = 0.0f;
    CoA::SpellShadow::BeforeDraw(style, pos, size);
    CHECK(!CoA::ClientHooks::options.spellShadowSizedHeight);
    CHECK(style == 1 && pos[0] == 1.0f && size == 0.0f);   // state 0
    style = 0;
    CoA::SpellShadow::BeforeDraw(style, pos, size);
    CHECK(CoA::ClientHooks::options.spellShadowSizedHeight);

    Target(701);
    CoA::SpellShadow::OnCursorMode(0x1C);
    style = 1;
    s_hasPlayer = false;
    CoA::SpellShadow::BeforeDraw(style, pos, size);
    CHECK(style == 1 && pos[0] == 1.0f);   // no player: nothing moves
    s_hasPlayer = true;
    CoA::SpellShadow::BeforeDraw(style, pos, size);
    CHECK(!CoA::ClientHooks::options.spellShadowSizedHeight);   // decided before the style changes
    CHECK(style == 0 && size == 6.0f);
    CHECK(pos[0] == 10.0f && pos[1] == 20.0f && pos[2] == 30.0f);
    CHECK(CoA::SpellShadow::SavedTarget()[0] == 1.0f && CoA::SpellShadow::SavedTarget()[1] == 2.0f && CoA::SpellShadow::SavedTarget()[2] == 3.0f);
    Stop();
}

// FUN_10321830 from the shadow draw: state 2 aims at the saved point, state 3 along the player's facing.
COA_TEST(SpellShadow_ShadowRender) {
    Start();
    float box[6] = { 0.0f, 0.0f, -1.0f, 2.0f, 2.0f, 1.0f };
    CHECK(!CoA::SpellShadow::BeforeShadowRender(box));
    CHECK(box[0] == 0.0f && box[3] == 2.0f);

    Target(701);
    CoA::SpellShadow::OnCursorMode(0x1C);
    int32_t style = 1;
    float pos[3] = { 100.0f, 200.0f, 300.0f };
    float size = 0.0f;
    CoA::SpellShadow::BeforeDraw(style, pos, size);
    s_hasPlayer = false;
    CHECK(!CoA::SpellShadow::BeforeShadowRender(box));
    s_hasPlayer = true;
    CHECK(CoA::SpellShadow::BeforeShadowRender(box));
    CHECK(s_facingFrom[0] == 10.0f && s_facingTo[0] == 100.0f && s_facingTo[2] == 300.0f);
    const float dx = static_cast<float>(cos(0.5)) * 7.5f;
    const float dy = static_cast<float>(sin(0.5)) * 7.5f;
    CHECK(Near(box[0], dx) && Near(box[1], dy) && box[2] == -1.0f && Near(box[3], 2.0f + dx) && Near(box[4], 2.0f + dy) && box[5] == 1.0f);
    const float* m = CoA::SpellShadow::Rotation();
    CHECK(Near(m[0], cosf(-0.5f)) && Near(m[1], sinf(-0.5f)) && Near(m[4], -sinf(-0.5f)) && Near(m[5], cosf(-0.5f)));
    CHECK(m[2] == 0.0f && m[3] == 0.0f && m[10] == 1.0f && m[15] == 1.0f && m[12] == 0.0f);

    Target(702);
    CoA::SpellShadow::OnCursorMode(0x1C);
    s_facing = 2.0f;
    float box2[6] = {};
    CHECK(CoA::SpellShadow::BeforeShadowRender(box2));
    CHECK(Near(box2[0], static_cast<float>(cos(2.0)) * 7.5f) && Near(box2[4], static_cast<float>(sin(2.0)) * 7.5f));
    CHECK(Near(CoA::SpellShadow::Rotation()[0], cosf(-2.0f)));
    Stop();
}

// FUN_10321830 from the selection circle: the z extent grows by 2.5 each way in every state.
COA_TEST(SpellShadow_SelectionRender) {
    Start();
    float box[6] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };
    CoA::SpellShadow::BeforeSelectionRender(box);
    CHECK(box[0] == 1.0f && box[1] == 2.0f && box[2] == 0.5f && box[3] == 4.0f && box[4] == 5.0f && box[5] == 8.5f);
    Stop();
}

// FUN_102bd3f0: RotZ(angle) x m.
COA_TEST(SpellShadow_RotateBy) {
    float m[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    CoA::SpellShadow::RotateBy(m, 0.0f);
    CHECK(m[0] == 1 && m[5] == 6 && m[15] == 16);
    const float a = 0.3f;
    const float c = static_cast<float>(cos(static_cast<double>(a)));
    const float s = static_cast<float>(sin(static_cast<double>(a)));
    float n[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    CoA::SpellShadow::RotateBy(n, a);
    CHECK(Near(n[0], c * 1 + s * 5) && Near(n[1], c * 2 + s * 6) && Near(n[4], -s * 1 + c * 5) && Near(n[7], -s * 4 + c * 8));
    CHECK(n[8] == 9 && n[12] == 13 && n[15] == 16);
}
