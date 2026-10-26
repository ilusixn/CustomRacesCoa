#include "CoA/Source/Modules/AlphaAmbience.h"
#include "CoA/Source/Modules/LoadingScreen.h"
#include "CoA/Source/Modules/SceneTransform.h"
#include "CoA/Source/Modules/TerrainScale.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace {
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> s_ambience;
    std::set<uint32_t> s_missingAmbience;

    bool SetAmbience(uint32_t id, uint32_t day, uint32_t night) {
        if (s_missingAmbience.count(id)) {
            return false;
        }

        s_ambience[id] = { day, night };
        return true;
    }

    void StartAmbience() {
        CoA::CVars::Reset();
        CoA::AlphaAmbience::Reset();
        s_ambience.clear();
        s_missingAmbience.clear();
        CoA::AlphaAmbience::Dependencies().setAmbience = SetAmbience;
        CoA::AlphaAmbience::Init();
    }
}

// FUN_1031e8c0: FUN_10114540 with flags 1, "0", category 7 and the last argument 1.
COA_TEST(AlphaAmbience_QueuesItsCVar) {
    StartAmbience();
    size_t count = 0;
    const CoA::CVarEntry* world = CoA::CVars::World(count);
    CHECK(count == 1);

    if (count == 1) {
        CHECK(strcmp(world[0].name, "useAlphaAmbienceSound") == 0);
        CHECK(strcmp(world[0].defaultValue, "0") == 0);
        CHECK(world[0].flags == 1 && world[0].category == 7 && world[0].setCommand);
        CHECK(world[0].callback != nullptr && world[0].slot != nullptr && world[0].help == nullptr);
    }
}

// FUN_1031e840: "1" rewrites the fourteen rows (the first 0x2A entry wins); anything else does nothing.
COA_TEST(AlphaAmbience_OnlyOneRewritesTheRows) {
    StartAmbience();
    CHECK(CoA::AlphaAmbience::OnChanged(nullptr, nullptr, "0", nullptr));
    CHECK(s_ambience.empty());
    CoA::AlphaAmbience::OnChanged(nullptr, nullptr, "2", nullptr);
    CHECK(s_ambience.empty());

    s_missingAmbience.insert(0x1C);
    CHECK(CoA::AlphaAmbience::OnChanged(nullptr, nullptr, "1", nullptr));
    CHECK(s_ambience.size() == 13);
    CHECK((s_ambience[0x19] == std::make_pair(0x33F77u, 0x33F78u)));
    CHECK((s_ambience[0x2A] == std::make_pair(0x33F73u, 0x33F74u)));
    CHECK((s_ambience[0x31] == std::make_pair(0x33F81u, 0x33F82u)));
    size_t n = 0;
    CoA::AlphaAmbience::Table(n);
    CHECK(n == 14);
}

namespace {
    // Four layers' texture offsets / scales (s_vertexShaderConsts +0xD0 / +0x120, 16 bytes each).
    float s_offset[4][4];
    float s_scale[5][4];

    void FillTables() {
        for (uint32_t i = 0; i < 4; ++i) {
            for (uint32_t j = 0; j < 4; ++j) {
                s_offset[i][j] = 1.0f;
                s_scale[i][j] = 8.0f;
            }
        }
    }

    uint32_t Exponent(uint32_t n) {
        return n << 11;
    }
}

// FUN_1033bef0: bits 11..13 of a layer's MCLY flags scale its texture by 1 / 2^e (6 = -2, 7 = -1).
COA_TEST(TerrainScale_ScalesByTheLayerExponent) {
    FillTables();
    const uint32_t flags[4] = { 0, Exponent(1), Exponent(6), Exponent(3) };
    const bool animated[4] = { false, false, false, false };
    CoA::TerrainScale::Apply(4, flags, animated, 3, 5, s_offset, s_scale);
    CHECK(s_scale[0][0] == 8.0f && s_scale[0][1] == 8.0f);
    CHECK(s_scale[1][0] == 4.0f && s_scale[1][1] == 4.0f && s_scale[1][2] == 8.0f);
    CHECK(s_scale[2][0] == 32.0f && s_scale[2][1] == 32.0f);
    CHECK(s_scale[3][0] == 1.0f);
    // Not animated: the offsets stay (+0 for exponents other than 4 / 5).
    CHECK(s_offset[1][0] == 1.0f && s_offset[3][1] == 1.0f);
}

// An animated layer (render layer flag 0x40) scales its offset too, and loses it without speed bits (0x38).
COA_TEST(TerrainScale_AnimatedLayers) {
    FillTables();
    const uint32_t flags[2] = { Exponent(1) | 0x8, Exponent(1) };
    const bool animated[2] = { true, true };
    CoA::TerrainScale::Apply(2, flags, animated, 0, 0, s_offset, s_scale);
    CHECK(s_offset[0][0] == 0.5f && s_offset[0][1] == 0.5f);
    CHECK(s_offset[1][0] == 0.0f && s_offset[1][1] == 0.0f);
}

// e = 4 / 5: the chunk position modulo 2 / 4 times -0.5 / 0.25 is added (u from +0x28, v from +0x24).
COA_TEST(TerrainScale_TilesLargeTextures) {
    FillTables();
    const uint32_t flags[2] = { Exponent(4), Exponent(5) };
    const bool animated[2] = { false, false };
    CoA::TerrainScale::Apply(2, flags, animated, 3, 7, s_offset, s_scale);
    CHECK(s_scale[0][0] == 0.5f && s_scale[1][0] == 0.25f);
    CHECK(s_offset[0][0] == 1.0f + 1.0f * -0.5f && s_offset[0][1] == 1.0f + 1.0f * -0.5f);
    CHECK(s_offset[1][0] == 1.0f + 3.0f * 0.25f && s_offset[1][1] == 1.0f + 3.0f * 0.25f);
    FillTables();
    CoA::TerrainScale::Apply(2, flags, animated, 2, 5, s_offset, s_scale);
    CHECK(s_offset[0][0] == 1.0f + 1.0f * -0.5f && s_offset[0][1] == 1.0f);
    CHECK(s_offset[1][0] == 1.0f + 1.0f * 0.25f && s_offset[1][1] == 1.0f + 2.0f * 0.25f);
}

namespace {
    float Height(float v) { return v * 2.0f; }
    float Width(float v) { return v * 3.0f; }
}

// FUN_101ae900 keeps {H(q0), W(q1), H(q2), W(q3)}; FUN_101aed40 passes them as (738, 740, 734, 73C).
COA_TEST(SceneTransform_ViewportFromTheFrameRect) {
    CoA::SceneTransform::Deps& d = CoA::SceneTransform::Dependencies();
    d.toNdcHeight = Height;
    d.toNdcWidth = Width;
    const float rect[4] = { 1.0f, 2.0f, 3.0f, 4.0f };   // CRect: minY, minX, maxY, maxX
    CoA::SceneTransform::Keep(rect);
    float minX = 0.0f, maxX = 1.0f, minY = 0.0f, maxY = 1.0f;
    CoA::SceneTransform::ViewportArgs(minX, maxX, minY, maxY);
    CHECK(minX == 6.0f && maxX == 12.0f && minY == 2.0f && maxY == 6.0f);
    CoA::SceneTransform::Reset();
}

namespace {
    std::map<uint32_t, uint32_t> s_screens;
    std::set<uint32_t> s_maps;

    bool HasMap(uint32_t map) { return s_maps.count(map) != 0; }
    void SetScreen(uint32_t map, uint32_t screen) { s_screens[map] = screen; }

    bool ReadVariations(const char* path, std::vector<uint8_t>& out) {
        if (strcmp(path, "DBFilesClient\\LoadingScreenVariation.dbc") != 0) {
            return false;
        }

        std::vector<uint32_t> one(17, 0), many(17, 0), none(17, 0);
        one[0] = 1;
        one[5] = 77;
        many[0] = 530;
        many[1] = 10;
        many[3] = 20;
        many[16] = 30;
        none[0] = 571;
        out = CoATest::Wdbc({ one, many, none }, std::string(1, '\0'));
        return true;
    }

    void StartLoading() {
        CoA::LoadingScreen::Reset();
        CoA::Dbc::Get("DBFilesClient\\LoadingScreenVariation.dbc").Unload();
        CoA::Dbc::SetFileReader(ReadVariations);
        s_screens.clear();
        s_maps = { 1, 530, 571 };
        CoA::LoadingScreen::Deps& d = CoA::LoadingScreen::Dependencies();
        d.hasMap = HasMap;
        d.setMapLoadingScreen = SetScreen;
    }

    void StopLoading() {
        CoA::Dbc::SetFileReader(nullptr);
        CoA::Dbc::Get("DBFilesClient\\LoadingScreenVariation.dbc").Unload();
    }
}

COA_TEST(LoadingScreen_PicksANonZeroVariation) {
    StartLoading();
    CoA::LoadingScreen::ApplyVariation(1);
    CHECK(s_screens[1] == 77);

    std::set<uint32_t> seen;

    for (int i = 0; i < 200; ++i) {
        CoA::LoadingScreen::ApplyVariation(530);
        seen.insert(s_screens[530]);
    }

    CHECK((seen == std::set<uint32_t>{ 10, 20, 30 }));

    // Every entry zero: the map keeps its screen; no variation row or no Map.dbc row: nothing.
    CoA::LoadingScreen::ApplyVariation(571);
    CHECK(s_screens.count(571) == 0);
    s_maps.erase(1);
    s_screens.clear();
    CoA::LoadingScreen::ApplyVariation(1);
    CHECK(s_screens.empty());
    CoA::LoadingScreen::ApplyVariation(0xFFFFFFFF);
    CHECK(s_screens.empty());
    StopLoading();
}

// The gate (0x10BCB8C8, 1 in the DLL's .data): OsSleep runs except between a loading screen's start (0x40AB70) and
// its end (0x409550).
COA_TEST(LoadingScreen_SleepGate) {
    CoA::LoadingScreen::Reset();
    CHECK(CoA::LoadingScreen::SleepAllowed());
    CoA::LoadingScreen::AfterLoadingScreenEnable();
    CHECK(!CoA::LoadingScreen::SleepAllowed());
    CoA::LoadingScreen::AfterLoadingScreenDisable();
    CHECK(CoA::LoadingScreen::SleepAllowed());
}
