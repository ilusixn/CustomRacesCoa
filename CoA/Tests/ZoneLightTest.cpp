#include "CoA/Source/Modules/ZoneLight.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;
    const char kLights[] = "DBFilesClient\\ZoneLight.dbc";
    const char kPoints[] = "DBFilesClient\\ZoneLightPoint.dbc";
    const float kHalf = 17066.666f;

    std::vector<std::vector<uint32_t>> s_lights;
    std::vector<std::vector<uint32_t>> s_points;
    bool s_haveLights = true;
    bool s_havePoints = true;
    V s_calls;
    uint32_t s_map = 1;
    bool s_inside = false;
    float s_distance = 0.0f;
    float s_queryA = 0.0f;
    float s_queryB = 0.0f;
    size_t s_queryPoints = 0;
    CoA::ZoneLight::GlareSettings s_settings;
    bool s_showSky = true;

    uint32_t F(float f) {
        uint32_t u;
        memcpy(&u, &f, 4);
        return u;
    }

    bool Read(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "ZoneLightPoint.dbc")) {
            if (!s_havePoints) {
                return false;
            }

            out = CoATest::Wdbc(s_points, "");
            return true;
        }

        if (strstr(path, "ZoneLight.dbc")) {
            if (!s_haveLights) {
                return false;
            }

            out = CoATest::Wdbc(s_lights, std::string("Elwynn") + '\0');
            return true;
        }

        return false;
    }

    // id, name, map, light, transition, zMin, zMax
    std::vector<uint32_t> Light(uint32_t id, uint32_t map, uint32_t light, float transition, float zMin, float zMax) {
        return { id, 1, map, light, F(transition), F(zMin), F(zMax) };
    }

    // id, zone, x, y, order
    std::vector<uint32_t> Point(uint32_t id, uint32_t zone, float x, float y, uint32_t order) {
        return { id, zone, F(x), F(y), order };
    }

    std::string Num(float f) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%g", f);
        return buf;
    }

    void Start() {
        CoA::Packets::Clear();
        CoA::ZoneLight::Reset();
        s_lights.clear();
        s_points.clear();
        s_haveLights = s_havePoints = true;
        s_calls.clear();
        s_map = 1;
        s_inside = false;
        s_distance = 0.0f;
        s_showSky = true;
        CoA::Dbc::SetFileReader(Read);
        CoA::Dbc::Get(kLights).Unload();
        CoA::Dbc::Get(kPoints).Unload();
        CoA::ZoneLight::Deps& d = CoA::ZoneLight::Dependencies();
        d.mapId = [] { return s_map; };
        d.pointInPolygon = [](const std::vector<std::pair<float, float>>& points, float a, float b, float& distance) {
            s_queryPoints = points.size();
            s_queryA = a;
            s_queryB = b;
            distance = s_distance;
            return s_inside;
        };
        d.addZoneLight = [](uint32_t light, float blend) { s_calls.push_back("light:" + std::to_string(light) + "@" + Num(blend)); };
        d.setMoon2Period = [](float p) { s_calls.push_back("period:" + Num(p)); };
        d.initGlare = [](const char* texture, const CoA::ZoneLight::GlareSettings& s) {
            s_calls.push_back(std::string("glare:") + texture);
            s_settings = s;
        };
        d.moon2Placement = [](float position[3], float& scale) {
            position[0] = 1.0f;
            position[1] = 2.0f;
            position[2] = 3.0f;
            scale = 4.0f;
        };
        d.setGlarePlacement = [](const float p[3], float scaleMin, float scaleMax) {
            s_calls.push_back("place:" + Num(p[0]) + "," + Num(p[1]) + "," + Num(p[2]) + " " + Num(scaleMin) + "-" + Num(scaleMax));
        };
        d.moonColor = [] { return 0xFF112233u; };
        d.setMoon2Color = [](uint32_t c) { s_calls.push_back("moon2Color:" + std::to_string(c)); };
        d.setGlareColor = [](uint32_t c) { s_calls.push_back("glareColor:" + std::to_string(c)); };
        d.showSky = [] { return s_showSky; };
        d.deltaSec = [] { return 0.25f; };
        d.updateGlare = [](float dt) { s_calls.push_back("update:" + Num(dt)); };
        d.renderGlare = [] { s_calls.push_back("render"); };
        CoA::ZoneLight::Init();
    }

    void Stop() {
        CoA::Dbc::Get(kLights).Unload();
        CoA::Dbc::Get(kPoints).Unload();
        CoA::Dbc::SetFileReader(nullptr);
        CoA::ZoneLight::Reset();
        CoA::Packets::Clear();
    }

    const CoA::ZoneLight::Zone* ZoneOf(uint32_t id) {
        auto it = CoA::ZoneLight::Zones().find(id);
        return it == CoA::ZoneLight::Zones().end() ? nullptr : &it->second;
    }
}

// FUN_10a4b030: period 1.0 for the second moon, the glare (every map load) set up like the stock moon glare, the zones.
COA_TEST(ZoneLight_AfterLoadMap) {
    Start();
    CHECK(CoA::Packets::Has(0x680));
    s_lights = { Light(5, 1, 77, 10.0f, 0.0f, 100.0f) };
    CoA::ZoneLight::AfterLoadMap();
    CHECK((s_calls == V{ "period:1", "glare:Textures\\moon02Glare.blp" }));
    CHECK(s_settings.enable == 1 && s_settings.skipOcclusionOnce == 1);
    CHECK(s_settings.baseScale == 2.0f && s_settings.scaleMin == 1.0f && s_settings.scaleMax == 1.0f);
    CHECK(F(s_settings.fadeOutRate) == 0x3FC1F07C && F(s_settings.fadeInRate) == 0x4041F07C);
    CHECK(F(s_settings.minDot) == 0x3F333333 && F(s_settings.alphaMin) == 0x3DCCCCCD && s_settings.alphaMax == 1.0f);
    CHECK(F(s_settings.curve[0][0]) == 0x3DAAAAAB && s_settings.curve[0][1] == 1.0f);
    CHECK(F(s_settings.curve[1][0]) == 0x3E0AAAAB && s_settings.curve[1][1] == 0.0f);
    CHECK(F(s_settings.curve[2][0]) == 0x3F72AAAB && s_settings.curve[2][1] == 0.0f);
    CHECK(F(s_settings.curve[3][0]) == 0x3F7FD27E && s_settings.curve[3][1] == 1.0f);
    CHECK(s_settings.screenGlow == 0.0f);
    CHECK(ZoneOf(5) != nullptr);
    Stop();
}

// The zone rows: transition at least 1, z clamped to +-64000 and ordered.
COA_TEST(ZoneLight_ZoneRows) {
    Start();
    s_lights = {
        Light(5, 1, 77, 0.5f, -70000.0f, 70000.0f),
        Light(6, 2, 78, 20.0f, 300.0f, 100.0f),
    };
    CoA::ZoneLight::Rebuild();
    const CoA::ZoneLight::Zone* a = ZoneOf(5);
    const CoA::ZoneLight::Zone* b = ZoneOf(6);
    CHECK(a && a->id == 5 && a->map == 1 && a->light == 77 && a->transition == 1.0f && a->zMin == -64000.0f && a->zMax == 64000.0f);
    CHECK(b && b->transition == 20.0f && b->zMin == 100.0f && b->zMax == 300.0f);
    CHECK(a && a->points.empty() && a->minA == kHalf && a->maxA == 0.0f);   // no points: the box is not grown
    Stop();
}

// The polygon: sorted by (order, id), one point per order, flipped plane, box grown by the transition; a point group
// without a zone row makes a default zone.
COA_TEST(ZoneLight_Polygons) {
    Start();
    s_lights = { Light(5, 1, 77, 10.0f, 0.0f, 100.0f) };
    s_points = {
        Point(13, 5, 100.0f, 200.0f, 2),
        Point(11, 5, 300.0f, 400.0f, 1),
        Point(12, 5, 999.0f, 999.0f, 1),   // same order, higher id: dropped
        Point(14, 5, 500.0f, 50.0f, 3),
        Point(20, 9, 1.0f, 2.0f, 0),       // zone 9 has no row
    };
    CoA::ZoneLight::Rebuild();
    const CoA::ZoneLight::Zone* z = ZoneOf(5);
    CHECK(z && z->points.size() == 3);

    if (z && z->points.size() == 3) {
        CHECK(z->points[0].first == kHalf - 400.0f && z->points[0].second == kHalf - 300.0f);
        CHECK(z->points[1].first == kHalf - 200.0f && z->points[1].second == kHalf - 100.0f);
        CHECK(z->points[2].first == kHalf - 50.0f && z->points[2].second == kHalf - 500.0f);
        CHECK(z->minA == (kHalf - 400.0f) - 10.0f && z->maxA == (kHalf - 50.0f) + 10.0f);
        CHECK(z->minB == (kHalf - 500.0f) - 10.0f && z->maxB == (kHalf - 100.0f) + 10.0f);
    }

    const CoA::ZoneLight::Zone* d = ZoneOf(9);
    CHECK(d && d->map == 0 && d->light == 0 && d->transition == 50.0f && d->points.size() == 1);
    CHECK(d && d->minA == (kHalf - 2.0f) - 50.0f);

    // A rebuild starts over.
    s_points.clear();
    CoA::Dbc::Get(kPoints).Unload();
    CoA::ZoneLight::Rebuild();
    CHECK(ZoneOf(9) == nullptr && ZoneOf(5) && ZoneOf(5)->points.empty());
    Stop();
}

// Missing tables: no zones, no lights.
COA_TEST(ZoneLight_MissingTables) {
    Start();
    s_haveLights = s_havePoints = false;
    CoA::ZoneLight::AfterLoadMap();
    CHECK(CoA::ZoneLight::Zones().empty());
    const float pos[3] = { 0.0f, 0.0f, 0.0f };
    CoA::ZoneLight::AfterFindZoneLight(pos);
    CHECK((s_calls == V{ "period:1", "glare:Textures\\moon02Glare.blp" }));
    Stop();
}

// FUN_10a4b880: map, z range, box, polygon distance; the blend is min(z margin, inside depth) / (2 x transition) x 100.
COA_TEST(ZoneLight_Blend) {
    Start();
    s_lights = { Light(5, 1, 77, 10.0f, 0.0f, 100.0f) };
    // A box around the flipped point of world (x, y) = (1000, 2000): a = kHalf - 2000, b = kHalf - 1000.
    s_points = { Point(1, 5, 900.0f, 1900.0f, 0), Point(2, 5, 1100.0f, 1900.0f, 1), Point(3, 5, 1100.0f, 2100.0f, 2), Point(4, 5, 900.0f, 2100.0f, 3) };
    CoA::ZoneLight::Rebuild();
    const float pos[3] = { 1000.0f, 2000.0f, 50.0f };

    s_inside = true;
    s_distance = 4.0f;
    CoA::ZoneLight::AfterFindZoneLight(pos);
    CHECK((s_calls == V{ "light:77@70" }));
    CHECK(s_queryPoints == 4 && s_queryA == -(2000.0f - kHalf) && s_queryB == -(1000.0f - kHalf));

    s_calls.clear();
    s_inside = false;
    CoA::ZoneLight::AfterFindZoneLight(pos);   // outside by 4 of 10
    CHECK((s_calls == V{ "light:77@30" }));
    s_calls.clear();
    s_distance = 10.0f;
    CoA::ZoneLight::AfterFindZoneLight(pos);   // at the transition width: nothing
    CHECK(s_calls.empty());

    s_inside = true;
    s_distance = 20.0f;
    const float high[3] = { 1000.0f, 2000.0f, 95.0f };
    CoA::ZoneLight::AfterFindZoneLight(high);  // z margin 5 + 10
    CHECK((s_calls == V{ "light:77@75" }));
    s_calls.clear();
    const float low[3] = { 1000.0f, 2000.0f, 5.0f };
    CoA::ZoneLight::AfterFindZoneLight(low);   // the margin below (5) is the smaller one
    CHECK((s_calls == V{ "light:77@75" }));
    s_calls.clear();
    const float over[3] = { 1000.0f, 2000.0f, 110.5f };
    CoA::ZoneLight::AfterFindZoneLight(over);  // above zMax + transition
    const float far[3] = { 5000.0f, 2000.0f, 50.0f };
    CoA::ZoneLight::AfterFindZoneLight(far);   // outside the box
    s_map = 2;
    CoA::ZoneLight::AfterFindZoneLight(pos);   // another map
    CHECK(s_calls.empty());
    Stop();
}

// The planets / colours / glare detours.
COA_TEST(ZoneLight_SkyHooks) {
    Start();
    CoA::ZoneLight::AfterSetPlanets();
    CoA::ZoneLight::AfterSetColors();
    CoA::ZoneLight::AfterRenderGlares();
    s_showSky = false;
    CoA::ZoneLight::AfterRenderGlares();
    CHECK((s_calls == V{ "place:1,2,3 4-4", "moon2Color:" + std::to_string(0xFF112233u), "glareColor:" + std::to_string(0xFF112233u),
        "update:0.25", "render" }));
    Stop();
}

// SMSG 0x680: one ZoneLight row, inserted or overwritten, used from the next rebuild on.
COA_TEST(ZoneLight_PatchPacket) {
    Start();
    s_lights = { Light(5, 1, 77, 10.0f, 0.0f, 100.0f) };
    CoA::ZoneLight::Rebuild();
    CoATest::Bytes b;
    b.U32(5).Str("Westfall").U32(3).U32(88).F32(30.0f).F32(-5.0f).F32(5.0f);
    CoATest::Deliver(CoA::ZoneLight::OnPatchZoneLight, 0x680, b);
    CHECK(ZoneOf(5) && ZoneOf(5)->light == 77);   // not before the next LoadMap
    const uint8_t* row = CoA::Dbc::Get(kLights).Row(5);
    CHECK(row && CoA::Dbc::Table::U32(row, 8) == 3 && strcmp(CoA::Dbc::Get(kLights).Str(row, 4), "Westfall") == 0);
    CoA::ZoneLight::Rebuild();
    CHECK(ZoneOf(5) && ZoneOf(5)->map == 3 && ZoneOf(5)->light == 88 && ZoneOf(5)->transition == 30.0f);
    CHECK(ZoneOf(5) && ZoneOf(5)->zMin == -5.0f && ZoneOf(5)->zMax == 5.0f);
    // A new id past the table's last one: inserted -- and, as the DLL's FUN_10133ac0 does (kept, Plan 13 ledger), the old
    // last row drops out of the grown index.
    CoATest::Bytes c;
    c.U32(9).Str("New").U32(1).U32(99).F32(5.0f).F32(0.0f).F32(10.0f);
    CoATest::Deliver(CoA::ZoneLight::OnPatchZoneLight, 0x680, c);
    CoA::ZoneLight::Rebuild();
    CHECK(ZoneOf(9) && ZoneOf(9)->light == 99 && ZoneOf(9)->zMax == 10.0f);
    CHECK(ZoneOf(5) == nullptr);
    Stop();
}
