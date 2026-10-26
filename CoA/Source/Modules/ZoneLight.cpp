// AscZoneLight.cpp (see ZoneLight.h): zone lighting and the second moon's glare.
#include "CoA/Source/Modules/ZoneLight.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include <algorithm>
#include <cstring>
#include <string>

namespace {
    CoA::ZoneLight::Deps s_deps;
    // 0x10D3D9DC (FUN_10086c40). The iteration order -- which decides the zones AddZoneLight's five slots keep -- is the
    // 64-bit std::hash order, not the 32-bit DLL's (ledgered ruling).
    std::unordered_map<uint32_t, CoA::ZoneLight::Zone> s_zones;

    float Bits(uint32_t b) {
        float f;
        memcpy(&f, &b, 4);
        return f;
    }

    const float kHalfWorld = Bits(0x46855555);   // 17066.666, DAT_10B62D04

    struct Point {
        int32_t id;
        float a;
        float b;
        int32_t order;
    };
}

CoA::ZoneLight::Deps& CoA::ZoneLight::Dependencies() {
    return s_deps;
}

CoA::ZoneLight::GlareSettings CoA::ZoneLight::MoonGlareSettings() {
    // FUN_10a4b030's stores into the zeroed block after 0x9AD000 (the vtable 0xA41C2C and +0xA8 = the second moon aside).
    GlareSettings g;
    g.baseScale = Bits(0x40000000);
    g.fadeOutRate = Bits(0x3FC1F07C);
    g.enable = 1;
    g.skipOcclusionOnce = 1;
    g.fadeInRate = Bits(0x4041F07C);
    g.alphaMin = Bits(0x3DCCCCCD);
    g.alphaMax = Bits(0x3F800000);
    g.scaleMin = Bits(0x3F800000);
    g.scaleMax = Bits(0x3F800000);
    g.minDot = Bits(0x3F333333);
    g.curve[0][0] = Bits(0x3DAAAAAB);
    g.curve[0][1] = Bits(0x3F800000);
    g.curve[1][0] = Bits(0x3E0AAAAB);
    g.curve[1][1] = 0.0f;
    g.curve[2][0] = Bits(0x3F72AAAB);
    g.curve[2][1] = 0.0f;
    g.curve[3][0] = Bits(0x3F7FD27E);
    g.curve[3][1] = Bits(0x3F800000);
    g.screenGlow = 0.0f;
    return g;
}

void CoA::ZoneLight::AfterLoadMap() {
    if (s_deps.setMoon2Period) {
        s_deps.setMoon2Period(Bits(0x3F800000));
    }

    if (s_deps.initGlare) {
        s_deps.initGlare("Textures\\moon02Glare.blp", MoonGlareSettings());
    }

    Rebuild();
}

void CoA::ZoneLight::Rebuild() {
    s_zones.clear();   // FUN_10a77e30

    // ZoneLight.dbc {id, name, map, light, transition, zMin, zMax}, in id order.
    CoA::Dbc::Table& zl = CoA::Dbc::Get("DBFilesClient\\ZoneLight.dbc");

    if (zl.Loaded()) {
        for (uint32_t id = zl.MinId(); id < zl.MaxId() + 1; ++id) {
            const uint8_t* row = zl.Row(id);

            if (!row) {
                continue;
            }

            Zone& z = s_zones[CoA::Dbc::Table::U32(row, 0)];
            z.id = CoA::Dbc::Table::U32(row, 0);
            z.map = CoA::Dbc::Table::U32(row, 8);
            z.light = CoA::Dbc::Table::U32(row, 0xC);
            const float t = CoA::Dbc::Table::F32(row, 0x10);
            const float lo = CoA::Dbc::Table::F32(row, 0x14);
            const float hi = CoA::Dbc::Table::F32(row, 0x18);
            z.transition = t <= 1.0f ? 1.0f : t;
            z.zMin = lo <= -64000.0f ? -64000.0f : lo;
            z.zMax = 64000.0f <= hi ? 64000.0f : hi;

            if (z.zMax < z.zMin) {
                std::swap(z.zMin, z.zMax);
            }
        }
    }

    // ZoneLightPoint.dbc {id, zone, x, y, order}, grouped by zone.
    std::unordered_map<uint32_t, std::vector<Point>> groups;
    CoA::Dbc::Table& zp = CoA::Dbc::Get("DBFilesClient\\ZoneLightPoint.dbc");

    if (zp.Loaded()) {
        for (uint32_t id = zp.MinId(); id < zp.MaxId() + 1; ++id) {
            const uint8_t* row = zp.Row(id);

            if (row) {
                groups[CoA::Dbc::Table::U32(row, 4)].push_back({ CoA::Dbc::Table::I32(row, 0), kHalfWorld - CoA::Dbc::Table::F32(row, 0xC),
                    kHalfWorld - CoA::Dbc::Table::F32(row, 8), CoA::Dbc::Table::I32(row, 0x10) });
            }
        }
    }

    // Each group: sorted by (order, id) (FUN_10a3c0f0), one point per order, appended to its zone's polygon.
    for (auto& g : groups) {
        std::vector<Point>& v = g.second;
        std::sort(v.begin(), v.end(), [](const Point& x, const Point& y) {
            return x.order < y.order || (x.order == y.order && x.id < y.id);
        });
        v.erase(std::unique(v.begin(), v.end(), [](const Point& x, const Point& y) { return x.order == y.order; }), v.end());
        Zone& z = s_zones[g.first];

        for (const Point& p : v) {
            z.points.emplace_back(p.a, p.b);

            if (p.a < z.minA) {
                z.minA = p.a;
            }

            if (p.b < z.minB) {
                z.minB = p.b;
            }

            if (z.maxA < p.a) {
                z.maxA = p.a;
            }

            if (z.maxB < p.b) {
                z.maxB = p.b;
            }
        }

        z.minA -= z.transition;
        z.minB -= z.transition;
        z.maxA += z.transition;
        z.maxB += z.transition;
    }
}

void CoA::ZoneLight::AfterFindZoneLight(const float position[3]) {
    const float qa = -(position[1] - kHalfWorld);
    const float qb = -(position[0] - kHalfWorld);
    const uint32_t map = s_deps.mapId ? s_deps.mapId() : 0;

    for (auto& e : s_zones) {
        const Zone& z = e.second;

        if (z.map != map) {
            continue;
        }

        const float t = z.transition;
        const float zPos = position[2];

        if (z.zMin - t > zPos || zPos > z.zMax + t) {
            continue;
        }

        float margin = z.zMax - zPos;
        const float below = zPos - z.zMin;
        margin = (margin < below ? margin : below) + t;

        if (qa < z.minA || qb < z.minB || z.maxA < qa || z.maxB < qb) {
            continue;
        }

        float dist = 0.0f;

        if (s_deps.pointInPolygon && s_deps.pointInPolygon(z.points, qa, qb, dist)) {
            dist = -dist;
        }

        if (!(t > dist)) {
            continue;
        }

        const float inside = -(dist - t);
        const float blend = (margin < inside ? margin : inside) / (t + t) * 100.0f;

        if (s_deps.addZoneLight) {
            s_deps.addZoneLight(z.light, blend);
        }
    }
}

void CoA::ZoneLight::AfterSetPlanets() {
    if (!s_deps.moon2Placement || !s_deps.setGlarePlacement) {
        return;
    }

    float position[3];
    float scale;
    s_deps.moon2Placement(position, scale);
    s_deps.setGlarePlacement(position, scale, scale);
}

void CoA::ZoneLight::AfterSetColors() {
    if (!s_deps.moonColor) {
        return;
    }

    const uint32_t color = s_deps.moonColor();

    if (s_deps.setMoon2Color) {
        s_deps.setMoon2Color(color);
    }

    if (s_deps.setGlareColor) {
        s_deps.setGlareColor(color);
    }
}

void CoA::ZoneLight::AfterRenderGlares() {
    if (!s_deps.showSky || !s_deps.showSky()) {
        return;
    }

    if (s_deps.updateGlare) {
        s_deps.updateGlare(s_deps.deltaSec ? s_deps.deltaSec() : 0.0f);
    }

    if (s_deps.renderGlare) {
        s_deps.renderGlare();
    }
}

int32_t CoA::ZoneLight::OnPatchZoneLight(void*, uint32_t, uint32_t, CDataStore* msg) {
    const uint32_t id = CoA::Read::U32(msg);
    const std::string name = CoA::Read::Str(msg);
    CoA::Dbc::Table& t = CoA::Dbc::Get("DBFilesClient/ZoneLight.dbc");
    uint32_t f[7] = { id, t.AddString(name.c_str()), 0, 0, 0, 0, 0 };

    for (uint32_t i = 2; i < 7; ++i) {
        f[i] = CoA::Read::U32(msg);
    }

    // Overwritten in place when live, inserted otherwise (Upsert does both).
    t.Upsert(id, std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(f), reinterpret_cast<const uint8_t*>(f) + sizeof(f)));
    return 1;
}

const std::unordered_map<uint32_t, CoA::ZoneLight::Zone>& CoA::ZoneLight::Zones() {
    return s_zones;
}

void CoA::ZoneLight::Init() {
    // FUN_10a66d40's five detours (ZoneLightClient / the marked DayNight sites) and SMSG 0x680.
    COA_PACKET(0x680, CoA::ZoneLight::OnPatchZoneLight);
}

void CoA::ZoneLight::Reset() {
    s_deps = Deps();
    s_zones.clear();
}
