#include "CoA/Source/Modules/Graphics.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
    const char kVolumes[] = "DBFilesClient\\OcclusionVolume.dbc";
    const char kPoints[] = "DBFilesClient\\OcclusionVolumePoints.dbc";

    bool s_inWorld = true;
    std::map<void*, std::pair<float, float>> s_attenuation;
    std::vector<bool> s_cullingEnable;
    std::vector<float> s_fogDensity;
    std::vector<float> s_fogEnd;
    uint32_t s_map = 0;
    std::vector<int32_t> s_shadowModes;
    std::vector<float> s_farClips;
    std::vector<size_t> s_used;
    uint32_t s_currentMap = 0;
    std::vector<std::string> s_draws;
    std::vector<std::vector<uint32_t>> s_volumes;
    std::vector<std::vector<uint32_t>> s_points;

    uint32_t F(float f) {
        uint32_t u;
        memcpy(&u, &f, 4);
        return u;
    }

    bool Read(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "OcclusionVolumePoints.dbc")) {
            out = CoATest::Wdbc(s_points, "");
            return true;
        }

        if (strstr(path, "OcclusionVolume.dbc")) {
            out = CoATest::Wdbc(s_volumes, std::string("Wall") + '\0');
            return true;
        }

        return false;
    }

    void Set(const char* name, const char* value) {
        CVar* var = CVar::Lookup(name);
        CHECK(var != nullptr);

        if (var) {
            var->Set(value, true, false, false, false);
        }
    }

    void Start() {
        CoA::CVars::Reset();
        CoA::Packets::Clear();
        CoA::Graphics::Reset();
        s_inWorld = true;
        s_attenuation.clear();
        s_cullingEnable.clear();
        s_fogDensity.clear();
        s_fogEnd.clear();
        s_map = 0;
        s_shadowModes.clear();
        s_farClips.clear();
        s_used.clear();
        s_currentMap = 0;
        s_draws.clear();
        s_volumes.clear();
        s_points.clear();
        CoA::Dbc::SetFileReader(Read);
        CoA::Dbc::Get(kVolumes).Unload();
        CoA::Dbc::Get(kPoints).Unload();
        CoA::Graphics::Deps& d = CoA::Graphics::Dependencies();
        d.inWorld = [] { return s_inWorld; };
        d.setAttenuation = [](void* light, float linear, float quadratic) { s_attenuation[light] = { linear, quadratic }; };
        d.setCullingEnable = [](bool on) { s_cullingEnable.push_back(on); };
        d.setFogDensity = [](float density) { s_fogDensity.push_back(density); };
        d.setFogEnd = [](float end) { s_fogEnd.push_back(end); };
        d.mapId = [] { return s_map; };
        d.shadowCVar = [] { return CVar::Lookup("graphicsTestShadow"); };
        d.setShadowMode = [](int32_t mode) { s_shadowModes.push_back(mode); };
        d.setFarClip = [](float farClip) { s_farClips.push_back(farClip); };
        d.useOccluders = [](const std::vector<CoA::Graphics::Occluder>& o) { s_used.push_back(o.size()); };
        d.currentMap = [] { return s_currentMap; };
        d.drawFill = [](const CoA::Graphics::Occluder& o, const std::vector<uint32_t>& tri, uint32_t color) {
            char b[96];
            snprintf(b, sizeof(b), "fill map %u tris %zu %08X", o.mapId, tri.size() / 3, color);
            s_draws.push_back(b);
        };
        d.drawLine = [](const float a[3], const float b[3], uint32_t color) {
            char s[96];
            snprintf(s, sizeof(s), "line %g,%g -> %g,%g %08X", a[0], a[1], b[0], b[1], color);
            s_draws.push_back(s);
        };
        // The render module's three glue CVars with callbacks (GlueCVars.cpp's entries).
        COA_CVAR("fogOverride", nullptr, 1, "0", CoA::Graphics::OnFogOverride, 1, true, CoA::Graphics::FogOverrideSlot(), false);
        COA_CVAR("fogDistance", nullptr, 1, "498", CoA::Graphics::OnFogDistance, 1, false, CoA::Graphics::FogDistanceSlot(), false);
        COA_CVAR("animateClouds", nullptr, 1, "1", CoA::Graphics::OnAnimateClouds, 1, false, CoA::Graphics::AnimateCloudsSlot(), false);
        CoA::CVars::RegisterGlue();
        CoA::Graphics::Init();
        CoA::CVars::RegisterWorld();
    }

    void Stop() {
        CoA::Dbc::Get(kVolumes).Unload();
        CoA::Dbc::Get(kPoints).Unload();
        CoA::Dbc::SetFileReader(nullptr);
        // Back to the defaults for the CVars other tests may see.
        for (const char* n : { "entityCulling", "portalCulling", "terrainCulling", "lightCulling", "wmoCulling", "antiportal" }) {
            Set(n, "1");
        }

        Set("fogOverride", "0");
        Set("fogDensity", "1");
        Set("fogDistance", "498");
        Set("lightLimitFix", "0");
        Set("lightLinearAttenuation", "1");
        Set("lightQuadraticAttenuation", "1");
        Set("overrideDayProgress", "0");
        Set("mistDensity", "12.0");
        Set("ambientGlow", "1");
        Set("ambientShade", "1");
        Set("animateClouds", "1");
        CoA::Graphics::Reset();
        CoA::CVars::Reset();
        CoA::Packets::Clear();
    }

    struct Spec {
        const char* name;
        const char* def;
        bool callback;
    };

    // FUN_10265d20's FUN_10114540 half, in its order.
    const Spec kWorld[] = {
        { "entityCulling", "1", true },
        { "portalCulling", "1", true },
        { "terrainCulling", "1", true },
        { "lightCulling", "1", true },
        { "wmoCulling", "1", true },
        { "fogDensity", "1", true },
        { "antiportal", "1", true },
        { "lightLimitFix", "0", true },
        { "ambientGlow", "1", false },
        { "ambientShade", "1", false },
        { "lightLinearAttenuation", "1", true },
        { "lightQuadraticAttenuation", "1", true },
        { "overrideDayProgress", "0", true },
        { "mistDensity", "12.0", true },
    };
}

COA_TEST(Graphics_QueuesFourteenWorldCVars) {
    CoA::CVars::Reset();
    CoA::Graphics::Reset();
    CoA::Graphics::Init();
    size_t count = 0;
    const CoA::CVarEntry* world = CoA::CVars::World(count);
    CHECK(count == sizeof(kWorld) / sizeof(kWorld[0]));

    for (size_t i = 0; i < count && i < sizeof(kWorld) / sizeof(kWorld[0]); ++i) {
        CHECK(strcmp(world[i].name, kWorld[i].name) == 0);
        CHECK(strcmp(world[i].defaultValue, kWorld[i].def) == 0);
        CHECK(world[i].flags == 1 && world[i].category == 1);
        CHECK((world[i].callback != nullptr) == kWorld[i].callback);
        CHECK(world[i].help == nullptr && !world[i].setCommand && world[i].slot != nullptr);
    }

    size_t glueCount = 0;
    CoA::CVars::Glue(glueCount);
    CHECK(glueCount == 0);
    CoA::CVars::Reset();
    CoA::Graphics::Reset();
}

// The render module's glue CVars carry their callbacks and slots.
COA_TEST(Graphics_GlueCVarsHaveCallbacks) {
    CoA::CVars::Reset();
    CoA::GlueCVars::Init();
    size_t count = 0;
    const CoA::CVarEntry* glue = CoA::CVars::Glue(count);
    std::map<std::string, const CoA::CVarEntry*> byName;

    for (size_t i = 0; i < count; ++i) {
        byName[glue[i].name] = &glue[i];
    }

    CHECK(byName["fogOverride"] && byName["fogOverride"]->callback == CoA::Graphics::OnFogOverride);
    CHECK(byName["fogOverride"] && byName["fogOverride"]->slot == CoA::Graphics::FogOverrideSlot() && byName["fogOverride"]->setCommand);
    CHECK(byName["fogDistance"] && byName["fogDistance"]->callback == CoA::Graphics::OnFogDistance);
    CHECK(byName["fogDistance"] && byName["fogDistance"]->slot == CoA::Graphics::FogDistanceSlot());
    CHECK(byName["animateClouds"] && byName["animateClouds"]->callback == CoA::Graphics::OnAnimateClouds);
    CHECK(byName["animateClouds"] && byName["animateClouds"]->slot == CoA::Graphics::AnimateCloudsSlot());
    CoA::CVars::Reset();
}

// At registration every callback runs with its default: rendering stays stock (terrainCulling sets the culling bit).
COA_TEST(Graphics_DefaultsLeaveRenderingStock) {
    Start();
    Set("entityCulling", "1");
    Set("portalCulling", "1");
    Set("terrainCulling", "1");
    Set("lightCulling", "1");
    Set("wmoCulling", "1");
    Set("fogDensity", "1");
    Set("antiportal", "1");
    Set("lightLimitFix", "0");
    Set("overrideDayProgress", "0");
    Set("mistDensity", "12.0");
    CHECK(CoA::Graphics::EntityCulling());
    CHECK(CoA::Graphics::PortalCulling());
    CHECK(CoA::Graphics::TerrainCulling());
    CHECK(!s_cullingEnable.empty() && s_cullingEnable.back());
    CHECK(CoA::Graphics::LightStampCulling());
    CHECK(CoA::Graphics::WmoCulling());
    CHECK(CoA::Graphics::FogDensityStores());
    CHECK(s_fogDensity.empty());   // fogOverride off: the density callback does nothing
    CHECK(CoA::Graphics::Antiportal());
    CHECK(!CoA::Graphics::LightLimitFix());
    CHECK(CoA::Graphics::DayProgress(0.37f) == 0.37f);
    CHECK(CoA::Graphics::MistDensity() == 12.0f);
    CHECK(CoA::Graphics::FogEndStores());
    // The detours at the defaults.
    uint8_t rec[0x88];

    for (int i = 0; i < 0x88; ++i) {
        rec[i] = static_cast<uint8_t>(i * 7);
    }

    uint8_t copy[0x88];
    memcpy(copy, rec, sizeof(rec));
    CoA::Graphics::ScaleBandRecord(rec);
    CHECK(memcmp(rec, copy, sizeof(rec)) == 0);
    uint32_t params[9] = { 3, 0, 5, 1, F(0.75f), 0, 0, 0, 0 };
    CHECK(CoA::Graphics::LightParams(params) == params);
    CHECK(params[2] == 5 && params[4] == F(0.75f));
    Stop();
}

// Not-"1" values of the switch CVars; "1" restores.
COA_TEST(Graphics_SwitchCallbacks) {
    Start();
    Set("entityCulling", "0");
    CHECK(!CoA::Graphics::EntityCulling());
    Set("entityCulling", "2");
    CHECK(!CoA::Graphics::EntityCulling());
    Set("entityCulling", "1");
    CHECK(CoA::Graphics::EntityCulling());
    Set("portalCulling", "0");
    CHECK(!CoA::Graphics::PortalCulling());
    Set("wmoCulling", "0");
    CHECK(!CoA::Graphics::WmoCulling());
    Set("antiportal", "0");
    CHECK(!CoA::Graphics::Antiportal());
    Set("lightLimitFix", "1");
    CHECK(CoA::Graphics::LightLimitFix());
    Set("lightLimitFix", "2");
    CHECK(!CoA::Graphics::LightLimitFix());
    s_cullingEnable.clear();
    Set("terrainCulling", "0");
    CHECK(!CoA::Graphics::TerrainCulling() && s_cullingEnable.size() == 1 && !s_cullingEnable[0]);
    Set("terrainCulling", "1");
    CHECK(CoA::Graphics::TerrainCulling() && s_cullingEnable.size() == 2 && s_cullingEnable[1]);
    Stop();
}

// 0x81E548 is shared: lightCulling's "1" and an attenuation of exactly 1.0 are the je, the last write wins.
COA_TEST(Graphics_LightStampSharedByCullingAndAttenuation) {
    Start();
    Set("lightCulling", "0");
    CHECK(!CoA::Graphics::LightStampCulling());
    Set("lightLinearAttenuation", "1.0");
    CHECK(CoA::Graphics::LightStampCulling());
    Set("lightQuadraticAttenuation", "2");
    CHECK(!CoA::Graphics::LightStampCulling());
    Set("lightCulling", "1");
    CHECK(CoA::Graphics::LightStampCulling());
    Stop();
}

// sub_1025d020 / sub_1025d0c0 / FUN_10260f90: lights made in world are attenuated and tracked; the callback re-applies
// with the CVar's value from before the change (it runs before the store).
COA_TEST(Graphics_LightsAttenuatedAndTracked) {
    Start();
    int a = 0, b = 0, c = 0;
    CoA::Graphics::LightCreated(&a);
    CHECK(s_attenuation.count(&a) && s_attenuation[&a].first == 1.0f * 0.7f && s_attenuation[&a].second == 1.0f * 0.03f);
    s_inWorld = false;
    CoA::Graphics::LightCreated(&b);
    CHECK(!s_attenuation.count(&b));
    s_inWorld = true;
    CoA::Graphics::LightCreated(&c);
    CHECK(CoA::Graphics::TrackedLights() == 2);
    CoA::Graphics::LightUnlinked(&c);
    CHECK(CoA::Graphics::TrackedLights() == 1);
    CoA::Graphics::LightUnlinked(&b);   // never tracked
    CHECK(CoA::Graphics::TrackedLights() == 1);
    Set("lightLinearAttenuation", "2");
    // The callback ran before the store: still the old 1.0.
    CHECK(s_attenuation[&a].first == 1.0f * 0.7f);
    Set("lightQuadraticAttenuation", "3");
    CHECK(s_attenuation[&a].first == 2.0f * 0.7f && s_attenuation[&a].second == 1.0f * 0.03f);
    CoA::Graphics::LightCreated(&b);
    CHECK(s_attenuation[&b].first == 2.0f * 0.7f && s_attenuation[&b].second == 3.0f * 0.03f);
    Stop();
}

// FUN_1025ebf0: only with fogOverride on and from 0.001 up; exactly 1.0 keeps the client's stores.
COA_TEST(Graphics_FogDensity) {
    Start();
    Set("fogDensity", "0.5");
    CHECK(s_fogDensity.empty() && CoA::Graphics::FogDensityStores());
    s_fogDensity.clear();
    Set("fogOverride", "1");   // re-sets fogDistance and fogDensity -- with fogOverride still 0 (before the store)
    CHECK(s_fogDensity.empty());
    Set("fogDensity", "0.5");
    CHECK(s_fogDensity.size() == 1 && s_fogDensity[0] == 0.5f && !CoA::Graphics::FogDensityStores());
    Set("fogDensity", "0.0005");
    CHECK(s_fogDensity.size() == 1 && !CoA::Graphics::FogDensityStores());
    Set("fogDensity", "1.0");
    CHECK(s_fogDensity.size() == 2 && s_fogDensity[1] == 1.0f && CoA::Graphics::FogDensityStores());
    Set("fogDensity", "0.25");
    // Turning fogOverride off re-runs the callbacks while it is still on: the stores stay NOPed (the original's order).
    Set("fogOverride", "0");
    CHECK(!CoA::Graphics::FogDensityStores() && s_fogDensity.back() == 0.25f);
    Stop();
}

// FUN_10260400: with fogOverride on, clamped to 498..1600; above 498 the fog-end stores are NOPed.
COA_TEST(Graphics_FogDistance) {
    Start();
    Set("fogDistance", "900");
    CHECK(s_fogEnd.empty() && CoA::Graphics::FogEndStores());
    Set("fogOverride", "1");
    Set("fogDistance", "900");
    CHECK(s_fogEnd.size() == 1 && s_fogEnd[0] == 900.0f && !CoA::Graphics::FogEndStores());
    Set("fogDistance", "3000");
    CHECK(s_fogEnd.back() == 1600.0f && !CoA::Graphics::FogEndStores());
    Set("fogDistance", "100");
    CHECK(s_fogEnd.back() == 498.0f && CoA::Graphics::FogEndStores());
    Stop();
}

// FUN_10266c30 from 0.1 up; FUN_102622d0 whatever the value.
COA_TEST(Graphics_DayProgressAndMist) {
    Start();
    Set("overrideDayProgress", "0.5");
    CHECK(CoA::Graphics::DayProgress(0.2f) == 0.5f);
    Set("overrideDayProgress", "0.05");
    CHECK(CoA::Graphics::DayProgress(0.2f) == 0.2f);
    Set("overrideDayProgress", "0.1");
    CHECK(CoA::Graphics::DayProgress(0.2f) == 0.1f);
    Set("mistDensity", "3.5");
    CHECK(CoA::Graphics::MistDensity() == 3.5f);
    Set("mistDensity", "0");
    CHECK(CoA::Graphics::MistDensity() == 0.0f);
    Stop();
}

// FUN_10264820: each colour's b / g / r bytes x ambientShade (truncated), alpha and the other fields untouched.
COA_TEST(Graphics_AmbientShadeScalesBands) {
    Start();
    Set("ambientShade", "0.5");
    uint8_t rec[0x88];
    memset(rec, 0, sizeof(rec));
    rec[0] = 0xAA;
    rec[0x47] = 0xBB;

    for (int i = 0; i < 16; ++i) {
        rec[0x48 + i * 4 + 0] = 201;
        rec[0x48 + i * 4 + 1] = 100;
        rec[0x48 + i * 4 + 2] = 3;
        rec[0x48 + i * 4 + 3] = 255;
    }

    CoA::Graphics::ScaleBandRecord(rec);
    CHECK(rec[0] == 0xAA && rec[0x47] == 0xBB);

    for (int i = 0; i < 16; ++i) {
        CHECK(rec[0x48 + i * 4 + 0] == 100 && rec[0x48 + i * 4 + 1] == 50 && rec[0x48 + i * 4 + 2] == 1 && rec[0x48 + i * 4 + 3] == 255);
    }

    CoA::Graphics::ScaleBandRecord(nullptr);   // no record: nothing
    Stop();
}

// sub_102648c0 / SMSG 0x650: the override copy gets [1] = 1 and [2] = the skybox; ambientGlow scales [4] of the row
// returned -- the client's own, in place, without an override.
COA_TEST(Graphics_LightParamsOverrideAndGlow) {
    Start();
    Set("ambientGlow", "2");
    uint32_t row[9] = { 7, 0, 11, 2, F(0.5f), 1, 2, 3, 4 };
    CHECK(CoA::Graphics::LightParams(row) == row);
    CHECK(row[4] == F(1.0f));
    CoA::Graphics::LightParams(row);
    CHECK(row[4] == F(2.0f));   // compounds, as the original's in-place write does
    CoATest::Bytes b;
    b.U32(42);
    CHECK(CoATest::Deliver(CoA::Graphics::OnSkyOverride, 0x650, b) == 1);
    CHECK(CoA::Graphics::SkyOverride() == 42);
    uint32_t row2[9] = { 8, 0, 11, 2, F(0.25f), 1, 2, 3, 4 };
    uint32_t* out = CoA::Graphics::LightParams(row2);
    CHECK(out != row2 && out[0] == 8 && out[1] == 1 && out[2] == 42 && out[4] == F(0.5f) && out[8] == 4);
    CHECK(row2[4] == F(0.25f) && row2[2] == 11);
    CHECK(CoA::Graphics::LightParams(nullptr) == nullptr);
    // A zero override clears it; a short packet reads 0 (CoA::Read).
    CoATest::Bytes z;
    z.U8(1);
    CoATest::Deliver(CoA::Graphics::OnSkyOverride, 0x650, z);
    CHECK(CoA::Graphics::SkyOverride() == 0);
    CHECK(CoA::Graphics::LightParams(row2) == row2);
    Stop();
}

// sub_102647c0 / sub_102647f0 / sub_1025d120: the clouds update only when marked, or always with animateClouds 0.
COA_TEST(Graphics_CloudAnimation) {
    Start();
    CHECK(!CoA::Graphics::CloudsTick());
    CoA::Graphics::CloudsSet();
    CHECK(CoA::Graphics::CloudsTick());
    CHECK(!CoA::Graphics::CloudsTick());
    CoA::Graphics::CloudsSet();
    Set("animateClouds", "1");   // the callback clears the mark
    CHECK(!CoA::Graphics::CloudsTick());
    Set("animateClouds", "0");
    CHECK(CoA::Graphics::CloudsTick() && CoA::Graphics::CloudsTick());
    Set("animateClouds", "1");
    CoA::Graphics::MapCap();   // marks too
    CHECK(CoA::Graphics::CloudsTick());
    Stop();
}

// FUN_1026ad40.
COA_TEST(Graphics_FarClipClamp) {
    CHECK(CoA::Graphics::FarClipClamp(100.0f, 0) == 183.33f);
    CHECK(CoA::Graphics::FarClipClamp(500.0f, 1) == 500.0f);
    CHECK(CoA::Graphics::FarClipClamp(1000.0f, 530) == 791.66f);
    CHECK(CoA::Graphics::FarClipClamp(1000.0f, 571) == 791.66f);
    CHECK(CoA::Graphics::FarClipClamp(1000.0f, 909) == 700.0f);
    CHECK(CoA::Graphics::FarClipClamp(1000.0f, 2) == 1000.0f);
    CHECK(CoA::Graphics::FarClipClamp(5000.0f, 2) == 1583.33f);
}

// sub_1025b020: maps 907 / 909 cap the shadow mode at 3 once, restored on leaving; sub_1025b0c0; sub_1025b0f0.
COA_TEST(Graphics_Lifecycle) {
    Start();
    CVar::Register("graphicsTestShadow", nullptr, 1, "4", nullptr, 1, false, nullptr, false);
    s_map = 1;
    CoA::Graphics::MapCap();
    CHECK(s_shadowModes.empty());
    s_map = 907;
    CoA::Graphics::MapCap();
    CoA::Graphics::MapCap();
    CHECK(s_shadowModes.size() == 1 && s_shadowModes[0] == 3);
    s_map = 1;
    CoA::Graphics::MapCap();
    CHECK(s_shadowModes.size() == 2 && s_shadowModes[1] == 4);
    Set("graphicsTestShadow", "2");
    s_map = 909;
    CoA::Graphics::MapCap();
    CHECK(s_shadowModes.size() == 2);
    s_map = 1;
    CoA::Graphics::MapCap();
    CHECK(s_shadowModes.size() == 2);

    CVar::Register("farClip", nullptr, 1, "777", nullptr, 1, false, nullptr, false);
    Set("farClip", "777");
    CoA::Graphics::ReapplyFarClip();
    CHECK(s_farClips.size() == 1 && s_farClips[0] == 777.0f);

    Set("entityCulling", "0");
    Set("wmoCulling", "0");
    Set("lightLimitFix", "1");
    CoA::Graphics::CullingOn();
    CHECK(CoA::Graphics::EntityCulling() && CoA::Graphics::WmoCulling());
    CHECK(CVar::Lookup("entityCulling")->m_intValue == 1 && CVar::Lookup("wmoCulling")->m_intValue == 1);
    CHECK(CoA::Graphics::LightLimitFix());
    Stop();
}

// FUN_10264d10 / SMSG 0x9D2 / 0x9D3: volumes with three or more points become records (points in id order, bounds);
// the client table is replaced only when there is at least one.
COA_TEST(Graphics_OccluderBuildAndPatches) {
    Start();
    // OcclusionVolume {id, name, map, flags}; OcclusionVolumePoints {id, volume, x, y, z}.
    s_volumes = { { 1, 1, 0, 1 }, { 2, 1, 530, 2 } };
    s_points = { { 10, 1, F(1), F(0), F(0) }, { 11, 1, F(0), F(1), F(0) }, { 5, 1, F(0), F(0), F(2) },
                 { 20, 2, F(5), F(5), F(5) }, { 21, 2, F(6), F(6), F(6) } };
    CoA::Graphics::BuildOccluders();
    const std::vector<CoA::Graphics::Occluder>& o = CoA::Graphics::Occluders();
    CHECK(o.size() == 1 && s_used.size() == 1 && s_used[0] == 1);

    if (o.size() == 1) {
        CHECK(o[0].mapId == 0 && o[0].flags == 1 && o[0].points.size() == 9);
        CHECK(o[0].points[2] == 2.0f && o[0].points[3] == 1.0f);   // point 5 first
        CHECK(o[0].min[0] == 0.0f && o[0].max[0] == 1.0f && o[0].max[2] == 2.0f && o[0].min[2] == 0.0f);
    }

    // A third point for volume 2 (an id inside the table's range: FUN_10133ac0 drops the old last row when it grows).
    CoATest::Bytes p;
    p.U32(19).U32(2).F32(7.0f).F32(5.0f).F32(9.0f);
    CHECK(CoATest::Deliver(CoA::Graphics::OnPatchVolumePoint, 0x9D3, p) == 1);
    CHECK(CoA::Graphics::Occluders().size() == 2 && s_used.back() == 2);
    // Volume 2 moved to map 1 with flags 3.
    CoATest::Bytes v;
    v.U32(2).U32(99).U32(1).U32(3).Str("Moved");
    CHECK(CoATest::Deliver(CoA::Graphics::OnPatchVolume, 0x9D2, v) == 1);
    CHECK(CoA::Graphics::Occluders().size() == 2 && CoA::Graphics::Occluders()[1].mapId == 1 && CoA::Graphics::Occluders()[1].flags == 3);
    const uint8_t* row = CoA::Dbc::Get(kVolumes).Row(2);
    CHECK(row && strcmp(CoA::Dbc::Get(kVolumes).Str(row, 4), "Moved") == 0);

    // Short packets are ignored (7a guard): nothing patched, nothing rebuilt.
    const size_t builds = s_used.size();
    CoATest::Bytes sp;
    sp.U32(30).U32(2).F32(1.0f);
    CHECK(CoATest::Deliver(CoA::Graphics::OnPatchVolumePoint, 0x9D3, sp) == 1);
    CoATest::Bytes sv;
    sv.U32(3).U32(0).U32(1).U32(0);   // no name terminator
    CHECK(CoATest::Deliver(CoA::Graphics::OnPatchVolume, 0x9D2, sv) == 1);
    CHECK(s_used.size() == builds && CoA::Dbc::Get(kPoints).Row(30) == nullptr && CoA::Dbc::Get(kVolumes).Row(3) == nullptr);
    Stop();
}

// No volume qualifies: the client keeps its own table.
COA_TEST(Graphics_NoOccludersKeepsClientTable) {
    Start();
    s_volumes = { { 1, 1, 0, 1 } };
    s_points = { { 10, 1, F(1), F(0), F(0) }, { 11, 1, F(0), F(1), F(0) } };
    CoA::Graphics::BuildOccluders();
    CHECK(CoA::Graphics::Occluders().empty() && s_used.empty());
    Stop();
}

// FUN_10264930 / FUN_10266560 / SetRenderOcclusion.
COA_TEST(Graphics_OcclusionOverlay) {
    Start();
    // A unit square in the x/y plane: two triangles.
    const std::vector<float> square = { 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0 };
    CHECK(CoA::Graphics::Triangulate(square).size() == 6);
    // A vertical square (y constant) projects onto x/z.
    const std::vector<float> wall = { 0, 5, 0, 1, 5, 0, 1, 5, 1, 0, 5, 1 };
    CHECK(CoA::Graphics::Triangulate(wall).size() == 6);

    s_volumes = { { 1, 1, 0, 1 }, { 2, 1, 1, 1 } };
    s_points = { { 10, 1, F(0), F(0), F(0) }, { 11, 1, F(1), F(0), F(0) }, { 12, 1, F(1), F(1), F(0) }, { 13, 1, F(0), F(1), F(0) },
                 { 20, 2, F(0), F(0), F(0) }, { 21, 2, F(1), F(0), F(0) }, { 22, 2, F(1), F(1), F(0) } };
    CoA::Graphics::BuildOccluders();
    CoA::Graphics::DrawOcclusion();
    CHECK(s_draws.empty());   // the overlay is off

    CoA::LuaReg::Reset();
    CoA::Graphics::Init();
    lua_State* L = CoATest::NewState(false);
    CoATest::Call(L, "", "SetRenderOcclusion", { "b:1", "b:1" });   // two arguments: ignored
    CHECK(!CoA::Graphics::OcclusionOverlay());
    CHECK(CoATest::Call(L, "", "SetRenderOcclusion", { "b:1" }).empty());
    CHECK(CoA::Graphics::OcclusionOverlay());
    s_currentMap = 0;
    CoA::Graphics::DrawOcclusion();
    CHECK(s_draws.size() == 5);
    CHECK(s_draws.size() == 5 && s_draws[0] == "fill map 0 tris 2 4040FF20");
    CHECK(s_draws.size() == 5 && s_draws[4] == "line 0,1 -> 0,0 B040FF20");
    CoATest::Call(L, "", "SetRenderOcclusion", { "nil" });
    CHECK(!CoA::Graphics::OcclusionOverlay());
    lua_close(L);
    CoA::LuaReg::Reset();
    Stop();
}
