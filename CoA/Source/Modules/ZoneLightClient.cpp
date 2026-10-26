// AscZoneLight.cpp's client half: the second glare (DLL block 0x10D3D714: a DNMoonGlare, vtable 0xA41C2C), the planets
// (0xD38E28), DNInfo (0xD38B00), the current map (0xADFBC4) and CWorldMath::PointInPolygon (0x7F9C90).
#include "CoA/Source/Modules/ZoneLight.h"
#include "CoA/Source/Modules/ZoneLightClient.h"
#include "CoA/Source/Modules/Modules.h"
#include "Engine/Source/Tempest/C2Vector.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/WorldClient/CWorldMath.h"
#include "WoW/Source/WorldClient/DayNight.h"
#include "WoW/Source/WorldClient/DayNight__DNInfo.h"
#include "WoW/Source/WorldClient/Map.h"
#include <cstring>

namespace {
    static_assert(sizeof(std::pair<float, float>) == sizeof(C2Vector), "the polygon is an array of C2Vector");

    // 0x10D3D714 (FUN_10086b40: 0xB0 zeroed bytes); a static DNMoonGlare is the same zeroed object with its vtable.
    DayNight::DNMoonGlare s_moon2Glare;

    uint32_t MapId() {   // *0xADFBC4
        return static_cast<uint32_t>(s_mapId);
    }

    bool PointInPolygon(const std::vector<std::pair<float, float>>& points, float a, float b, float& distance) {
        const C2Vector point(a, b);
        return CWorldMath::PointInPolygon(static_cast<int32_t>(points.size()), reinterpret_cast<const C2Vector*>(points.data()), point, &distance) != 0;
    }

    void AddZoneLight(uint32_t light, float blend) {   // 0x7ECEF0 then 0x7ED150
        DayNight::GetInfo()->AddZoneLight(static_cast<int32_t>(light), blend);
    }

    void SetMoon2Period(float period) {   // 0xD38E68 +0x1C
        DayNight::g_planets.m_moon2.m_period = period;
    }

    void InitGlare(const char* texture, const CoA::ZoneLight::GlareSettings& s) {
        DayNight::DNMoonGlare& g = s_moon2Glare;
        g.Initialize(texture);   // 0x9AD000
        g.m_baseScale = s.baseScale;
        g.m_planet = &DayNight::g_planets.m_moon2;
        g.m_fadeOutRate = s.fadeOutRate;
        g.m_enable = s.enable;
        g.m_skipOcclusionOnce = s.skipOcclusionOnce;
        g.m_fadeInRate = s.fadeInRate;
        g.m_alphaMin = s.alphaMin;
        g.m_alphaMax = s.alphaMax;
        g.m_scaleMin = s.scaleMin;
        g.m_scaleMax = s.scaleMax;
        g.m_minDot = s.minDot;

        for (int i = 0; i < 4; ++i) {
            g.m_alphaCurve[i].m_time = s.curve[i][0];
            g.m_alphaCurve[i].m_value = s.curve[i][1];
        }

        g.m_screenGlow = s.screenGlow;
    }

    void Moon2Placement(float position[3], float& scale) {
        const DayNight::DNPlanet& m = DayNight::g_planets.m_moon2;
        position[0] = m.m_position.x;
        position[1] = m.m_position.y;
        position[2] = m.m_position.z;
        scale = m.m_scale;
    }

    void SetGlarePlacement(const float position[3], float scaleMin, float scaleMax) {
        s_moon2Glare.m_scaleMin = scaleMin;
        s_moon2Glare.m_position = C3Vector(position[0], position[1], position[2]);
        s_moon2Glare.m_scaleMax = scaleMax;
    }

    uint32_t MoonColor() {   // 0xD38E48 +0xC
        uint32_t c;
        memcpy(&c, &DayNight::g_planets.m_moon.m_color, 4);
        return c;
    }

    void SetMoon2Color(uint32_t c) {
        memcpy(&DayNight::g_planets.m_moon2.m_color, &c, 4);
    }

    void SetGlareColor(uint32_t c) {
        memcpy(&s_moon2Glare.m_color, &c, 4);
    }

    bool ShowSky() {   // 0xD38CCC
        return DayNight::g_dnInfo.m_showSky != 0;
    }

    float DeltaSec() {   // 0xD38B48
        return DayNight::g_dnInfo.m_deltaSec;
    }

    void UpdateGlare(float deltaSec) {   // 0x7EF6E0, called directly
        s_moon2Glare.DayNight::DNGlare::Update(deltaSec);
    }

    void RenderGlare() {   // 0x9AC400, called directly
        s_moon2Glare.DayNight::DNGlare::Render();
    }
}

void CoA::ZoneLightClient::AfterLoadMap() {
    CoA::ZoneLight::AfterLoadMap();
}

void CoA::ZoneLightClient::AfterFindZoneLight(const C3Vector* position) {
    const float p[3] = { position->x, position->y, position->z };
    CoA::ZoneLight::AfterFindZoneLight(p);
}

void CoA::ZoneLightClient::AfterSetPlanets() {
    CoA::ZoneLight::AfterSetPlanets();
}

void CoA::ZoneLightClient::AfterSetColors() {
    CoA::ZoneLight::AfterSetColors();
}

void CoA::ZoneLightClient::AfterRenderGlares() {
    CoA::ZoneLight::AfterRenderGlares();
}

void CoA::ZoneLightClient::Init() {
    CoA::ZoneLight::Deps& d = CoA::ZoneLight::Dependencies();
    d.mapId = MapId;
    d.pointInPolygon = PointInPolygon;
    d.addZoneLight = AddZoneLight;
    d.setMoon2Period = SetMoon2Period;
    d.initGlare = InitGlare;
    d.moon2Placement = Moon2Placement;
    d.setGlarePlacement = SetGlarePlacement;
    d.moonColor = MoonColor;
    d.setMoon2Color = SetMoon2Color;
    d.setGlareColor = SetGlareColor;
    d.showSky = ShowSky;
    d.deltaSec = DeltaSec;
    d.updateGlare = UpdateGlare;
    d.renderGlare = RenderGlare;
}
