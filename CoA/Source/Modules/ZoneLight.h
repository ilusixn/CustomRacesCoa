#ifndef COA_SOURCE_MODULES_ZONELIGHT_H
#define COA_SOURCE_MODULES_ZONELIGHT_H

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

class CDataStore;

// AscZoneLight.cpp: zone lighting and the second moon's glare -- five detours from the attach init (installer
// FUN_10a66d40) and SMSG 0x680.
//
//   0x7F2790 DayNight::LoadMap -> FUN_10a4b030: after the original the second moon (DNPlanets +0x40, 0xD38E68) gets period
//            1.0, the second glare (a zeroed DNMoonGlare, 0x10D3D714) is initialised with Textures\moon02Glare.blp (0x9AD000,
//            every map load) and set up like the stock moon glare, and the zone table is rebuilt from ZoneLight.dbc +
//            ZoneLightPoint.dbc.
//   0x77EED0 DayNight::FindAndAddZoneLight -> FUN_10a4b880: after the original every zone of the current map (0xADFBC4)
//            the position is near blends its light in (DNInfo::AddZoneLight, 0x7ED150).
//   0x7EECC0 DayNight::SetPlanets -> sub_10a4baa0: after, the second glare takes the second moon's position and scale.
//   0x7F3230 DayNight::SetColors -> sub_10a4ba70: after, the second moon and its glare take the first moon's colour.
//   0x7F0870 DayNight::RenderGlares -> sub_10a4ba30: after, while DNInfo::m_showSky (0xD38CCC), the second glare is updated
//            (0x7EF6E0, DNInfo::m_deltaSec 0xD38B48) and drawn (0x9AC400).
//   SMSG 0x680 (FUN_1021a4d0): one ZoneLight row -- u32 id, C-string name, u32 map, light, transition, zMin, zMax --
//            overwritten in place or inserted; used from the next LoadMap on.
//
// Zones (0x10D3D9DC, an unordered_map keyed by ZoneLight id): { id, map, light, transition, zMin, zMax, polygon, box }.
// Polygon points and query positions live in the flipped plane (17066.666 - y, 17066.666 - x); the box is grown by the
// transition width.
namespace CoA::ZoneLight {
    struct Zone {
        uint32_t id = 0;
        uint32_t map = 0;
        uint32_t light = 0;
        float transition = 50.0f;     // FUN_10a3c6f0
        float zMin = -64000.0f;
        float zMax = 64000.0f;
        std::vector<std::pair<float, float>> points;
        float minA = 17066.666f;
        float minB = 17066.666f;
        float maxA = 0.0f;
        float maxB = 0.0f;
    };

    // The second glare's set-up (DNGlare fields after 0x9AD000), the stock moon glare's values.
    struct GlareSettings {
        uint32_t enable = 0;            // +0x08
        float baseScale = 0.0f;         // +0x20
        float fadeInRate = 0.0f;        // +0x28
        float fadeOutRate = 0.0f;       // +0x2C
        uint32_t skipOcclusionOnce = 0; // +0x38
        float curve[4][2] = {};         // +0x70 (time, value)
        float scaleMin = 0.0f;          // +0x90
        float scaleMax = 0.0f;          // +0x94
        float minDot = 0.0f;            // +0x98
        float alphaMin = 0.0f;          // +0x9C
        float alphaMax = 0.0f;          // +0xA0
        float screenGlow = 0.0f;        // +0xA4
    };

    struct Deps {
        uint32_t (*mapId)() = nullptr;                                      // *0xADFBC4
        // 0x7F9C90 CWorldMath::PointInPolygon: inside or not, the distance to the nearest edge.
        bool (*pointInPolygon)(const std::vector<std::pair<float, float>>& points, float a, float b, float& distance) = nullptr;
        void (*addZoneLight)(uint32_t light, float blend) = nullptr;        // DayNight::GetInfo()->AddZoneLight
        // The sky objects: the second moon (DNPlanets::m_moon2), the first moon (m_moon), the second glare.
        void (*setMoon2Period)(float period) = nullptr;
        void (*initGlare)(const char* texture, const GlareSettings& settings) = nullptr;   // 0x9AD000, vtable, fields
        void (*moon2Placement)(float position[3], float& scale) = nullptr;
        void (*setGlarePlacement)(const float position[3], float scaleMin, float scaleMax) = nullptr;
        uint32_t (*moonColor)() = nullptr;
        void (*setMoon2Color)(uint32_t color) = nullptr;
        void (*setGlareColor)(uint32_t color) = nullptr;
        bool (*showSky)() = nullptr;                                        // DNInfo::m_showSky
        float (*deltaSec)() = nullptr;                                      // DNInfo::m_deltaSec
        void (*updateGlare)(float deltaSec) = nullptr;                      // 0x7EF6E0
        void (*renderGlare)() = nullptr;                                    // 0x9AC400
    };

    Deps& Dependencies();

    void AfterLoadMap();                        // FUN_10a4b030
    void Rebuild();                             // its zone table part
    void AfterFindZoneLight(const float position[3]);   // FUN_10a4b880
    void AfterSetPlanets();                     // sub_10a4baa0
    void AfterSetColors();                      // sub_10a4ba70
    void AfterRenderGlares();                   // sub_10a4ba30
    int32_t OnPatchZoneLight(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);   // SMSG 0x680

    GlareSettings MoonGlareSettings();
    const std::unordered_map<uint32_t, Zone>& Zones();

    void Init();
    // Tests only.
    void Reset();
}

#endif
