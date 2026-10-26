// AscGraphics.cpp (see Graphics.h): the render module's CVars, detour bodies, lifecycle, occlusion volumes and packets.
#include "CoA/Source/Modules/CrashContext.h"
#include "CoA/Source/Modules/Graphics.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "ThirdParty/mapbox/earcut.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

namespace {
    CoA::Graphics::Deps s_deps;

    // ---- the code the callbacks rewrite, as switches (each at the stock bytes) ----------------------------------
    bool s_entityShadows = true;     // 0x793A45 / 0x7BBA22
    bool s_lodObjectCull = true;     // 0x791D6A
    bool s_entityCulling = true;     // 0x793115 / 0x793126 / 0x79313F
    bool s_portalCulling = true;     // 0x7AD2EE
    bool s_terrainCulling = true;    // 0x79AA42
    bool s_lightStamp = true;        // 0x81E548 (je)
    bool s_wmoCulling = true;        // 0x792AF2 / 0x792AFF / 0x7B3AAF / 0x7B3A8E
    bool s_fogDensityStores = true;  // 0x7F1A13 / 0x7F1A49 / 0x7F1A1B / 0x7F1777 / 0x7F19F4
    bool s_fogEndStores = true;      // 0x7ED982 / 0x7EC15A
    bool s_antiportal = true;        // 0x7D8444
    bool s_lightLimitFix = false;    // 0x7D00BF
    bool s_dayOverride = false;      // the 24 reads of 0xD38B04
    float s_dayProgress = 0.0f;      // DAT_10be271c
    float s_mistDensity = 12.0f;     // DAT_10bcb4d4; the three reads start at the client's 12.0 (0xA1047C)

    // ---- DLL state ----------------------------------------------------------------------------------------------
    bool s_occlusionOverlay = false; // DAT_10be270d
    bool s_cloudsDirty = false;      // DAT_10bcb4b8
    bool s_capApplied = false;       // DAT_10be270c
    int32_t s_capSaved = -1;         // DAT_10bcb478
    uint32_t s_skyOverride = 0;      // DAT_10be2720
    uint32_t s_skyParams[9] = {};    // DAT_10be2724
    std::vector<void*> s_lights;     // DAT_10be2710
    std::vector<CoA::Graphics::Occluder> s_occluders;   // DAT_10be2748 / 2754

    // ---- the CVar slots (0x10BE26C0..0x10BE2708) -------------------------------------------------------------
    CVar* s_portalCullingVar = nullptr;     // 0x10BE26C0
    CVar* s_terrainCullingVar = nullptr;    // 0x10BE26C4
    CVar* s_wmoCullingVar = nullptr;        // 0x10BE26C8
    CVar* s_lightCullingVar = nullptr;      // 0x10BE26CC
    CVar* s_fogOverrideVar = nullptr;       // 0x10BE26D0
    CVar* s_fogDensityVar = nullptr;        // 0x10BE26D4
    CVar* s_fogDistanceVar = nullptr;       // 0x10BE26D8
    CVar* s_ambientShadeVar = nullptr;      // 0x10BE26DC
    CVar* s_linearVar = nullptr;            // 0x10BE26E0
    CVar* s_quadraticVar = nullptr;         // 0x10BE26E4
    CVar* s_dayProgressVar = nullptr;       // 0x10BE26E8
    CVar* s_entityCullingVar = nullptr;     // 0x10BE26F0
    CVar* s_antiportalVar = nullptr;        // 0x10BE26F4
    CVar* s_lightLimitFixVar = nullptr;     // 0x10BE26F8
    CVar* s_ambientGlowVar = nullptr;       // 0x10BE26FC
    CVar* s_animateCloudsVar = nullptr;     // 0x10BE2700
    CVar* s_mistDensityVar = nullptr;       // 0x10BE2708

    bool IsOne(const char* value) {
        return value && strcmp(value, "1") == 0;
    }

    float ToFloat(const char* value) {
        return static_cast<float>(atof(value ? value : ""));
    }

    // FUN_10114c40: the length assert, then CVar::Set(value, 1, 0, 0, 0) (CrashContext::SetCVar).
    void SetVar(CVar* var, const char* value) {
        CoA::CrashContext::SetCVar(var, value, false);
    }

    // Re-set with the current string (the original passes the CVar's own string pointer; a copy here, the same text).
    void SetAgain(CVar* var) {
        if (var && var->m_stringValue.m_str) {
            const std::string current = var->m_stringValue.m_str;
            SetVar(var, current.c_str());
        }
    }

    // sub_1025d020 / FUN_10260f90: linear x 0.7, quadratic x 0.03, each CVar's float or 1.0 when missing.
    void Attenuate(void* light) {
        const float linear = s_linearVar ? s_linearVar->m_floatValue : 1.0f;
        const float quadratic = s_quadraticVar ? s_quadraticVar->m_floatValue : 1.0f;

        if (s_deps.setAttenuation) {
            s_deps.setAttenuation(light, linear * 0.7f, quadratic * 0.03f);
        }
    }

    void ZoneFeedback(int32_t) {
        CoA::Graphics::ReapplyFarClip();
    }

    CoA::Dbc::Table& Volumes() {   // {id, name, map, flags}
        return CoA::Dbc::Get("DBFilesClient\\OcclusionVolume.dbc");
    }

    CoA::Dbc::Table& VolumePoints() {   // {id, volume, x, y, z}
        return CoA::Dbc::Get("DBFilesClient\\OcclusionVolumePoints.dbc");
    }

    // The rows of a table in id order (MinId..MaxId).
    std::vector<const uint8_t*> RowsById(CoA::Dbc::Table& t) {
        std::vector<const uint8_t*> rows;

        if (!t.Loaded()) {
            return rows;
        }

        for (uint32_t id = t.MinId(); id <= t.MaxId() && id >= t.MinId(); ++id) {
            if (const uint8_t* row = t.Row(id)) {
                rows.push_back(row);
            }

            if (id == 0xFFFFFFFF) {
                break;
            }
        }

        return rows;
    }

    bool ById(const uint8_t* a, const uint8_t* b) {
        return CoA::Dbc::Table::I32(a, 0) < CoA::Dbc::Table::I32(b, 0);
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "SetRenderOcclusion", CoA::Graphics::SetRenderOcclusion),
    };
}

// ---- glue CVars ---------------------------------------------------------------------------------------------------
bool CoA::Graphics::OnEntityShadows(CVar*, const char*, const char* value, void*) {
    s_entityShadows = IsOne(value);
    return true;
}

bool CoA::Graphics::EntityShadows() {
    return s_entityShadows;
}

bool CoA::Graphics::OnLodObjectCull(CVar*, const char*, const char* value, void*) {
    s_lodObjectCull = IsOne(value);
    return true;
}

bool CoA::Graphics::LodObjectCull() {
    return s_lodObjectCull;
}

bool CoA::Graphics::OnFogOverride(CVar*, const char*, const char*, void*) {
    SetAgain(s_fogDistanceVar);
    SetAgain(s_fogDensityVar);
    return true;
}

bool CoA::Graphics::OnFogDistance(CVar*, const char*, const char* value, void*) {
    if (!s_fogOverrideVar || s_fogOverrideVar->m_intValue == 0) {
        return true;
    }

    const float x = ToFloat(value);
    const float lo = 498.0f <= x ? x : 498.0f;
    const float d = x <= 1600.0f ? lo : 1600.0f;

    if (s_deps.setFogEnd) {
        s_deps.setFogEnd(d);
    }

    s_fogEndStores = !(498.0f < d);
    return true;
}

bool CoA::Graphics::FogEndStores() {
    return s_fogEndStores;
}

bool CoA::Graphics::OnAnimateClouds(CVar*, const char*, const char*, void*) {
    s_cloudsDirty = false;
    return true;
}

CVar** CoA::Graphics::FogOverrideSlot() {
    return &s_fogOverrideVar;
}

CVar** CoA::Graphics::FogDistanceSlot() {
    return &s_fogDistanceVar;
}

CVar** CoA::Graphics::AnimateCloudsSlot() {
    return &s_animateCloudsVar;
}

// ---- world CVars ----------------------------------------------------------------------------------------------------
bool CoA::Graphics::OnEntityCulling(CVar*, const char*, const char* value, void*) {
    s_entityCulling = IsOne(value);
    return true;
}

bool CoA::Graphics::EntityCulling() {
    return s_entityCulling;
}

bool CoA::Graphics::OnPortalCulling(CVar*, const char*, const char* value, void*) {
    s_portalCulling = IsOne(value);
    return true;
}

bool CoA::Graphics::PortalCulling() {
    return s_portalCulling;
}

bool CoA::Graphics::OnTerrainCulling(CVar*, const char*, const char* value, void*) {
    s_terrainCulling = IsOne(value);

    if (s_deps.setCullingEnable) {
        s_deps.setCullingEnable(s_terrainCulling);
    }

    return true;
}

bool CoA::Graphics::TerrainCulling() {
    return s_terrainCulling;
}

bool CoA::Graphics::OnLightCulling(CVar*, const char*, const char* value, void*) {
    s_lightStamp = IsOne(value);
    return true;
}

bool CoA::Graphics::OnWmoCulling(CVar*, const char*, const char* value, void*) {
    s_wmoCulling = IsOne(value);
    return true;
}

bool CoA::Graphics::WmoCulling() {
    return s_wmoCulling;
}

bool CoA::Graphics::OnFogDensity(CVar*, const char*, const char* value, void*) {
    if (!s_fogOverrideVar || s_fogOverrideVar->m_intValue == 0) {
        return true;
    }

    const double d = atof(value ? value : "");

    if (d < 0.001) {
        return true;
    }

    if (s_deps.setFogDensity) {
        s_deps.setFogDensity(static_cast<float>(d));
    }

    s_fogDensityStores = d == 1.0;
    return true;
}

bool CoA::Graphics::FogDensityStores() {
    return s_fogDensityStores;
}

bool CoA::Graphics::OnAntiportal(CVar*, const char*, const char* value, void*) {
    s_antiportal = IsOne(value);
    return true;
}

bool CoA::Graphics::Antiportal() {
    return s_antiportal;
}

bool CoA::Graphics::OnLightLimitFix(CVar*, const char*, const char* value, void*) {
    s_lightLimitFix = IsOne(value);
    return true;
}

bool CoA::Graphics::LightLimitFix() {
    return s_lightLimitFix;
}

bool CoA::Graphics::OnLightAttenuation(CVar*, const char*, const char* value, void*) {
    s_lightStamp = ToFloat(value) == 1.0f;

    for (void* light : s_lights) {
        Attenuate(light);
    }

    return true;
}

bool CoA::Graphics::LightStampCulling() {
    return s_lightStamp;
}

bool CoA::Graphics::OnOverrideDayProgress(CVar*, const char*, const char* value, void*) {
    const float v = ToFloat(value);
    s_dayOverride = !(v < 0.1f);

    if (s_dayOverride) {
        s_dayProgress = v;
    }

    return true;
}

float CoA::Graphics::DayProgress(float stock) {
    return s_dayOverride ? s_dayProgress : stock;
}

bool CoA::Graphics::OnMistDensity(CVar*, const char*, const char* value, void*) {
    s_mistDensity = ToFloat(value);
    return true;
}

float CoA::Graphics::MistDensity() {
    return s_mistDensity;
}

// ---- detours --------------------------------------------------------------------------------------------------------
void CoA::Graphics::LightCreated(void* light) {
    if (!light || !s_deps.inWorld || !s_deps.inWorld()) {
        return;
    }

    Attenuate(light);
    s_lights.push_back(light);
}

void CoA::Graphics::LightUnlinked(void* light) {
    s_lights.erase(std::remove(s_lights.begin(), s_lights.end(), light), s_lights.end());
}

size_t CoA::Graphics::TrackedLights() {
    return s_lights.size();
}

void CoA::Graphics::ScaleBandRecord(uint8_t* record) {
    if (!s_ambientShadeVar || !record) {
        return;
    }

    for (uint32_t i = 0; i < 0x10; ++i) {
        for (uint32_t c = 0; c < 3; ++c) {
            uint8_t& b = record[0x48 + i * 4 + c];
            b = static_cast<uint8_t>(static_cast<int32_t>(static_cast<float>(b) * s_ambientShadeVar->m_floatValue));
        }
    }
}

uint32_t* CoA::Graphics::LightParams(uint32_t* row) {
    if (!row) {
        return nullptr;
    }

    memcpy(s_skyParams, row, sizeof(s_skyParams));
    uint32_t* p = row;

    if (s_skyOverride != 0) {
        s_skyParams[1] = 1;
        p = s_skyParams;
        s_skyParams[2] = s_skyOverride;
    }

    if (s_ambientGlowVar) {
        float glow;
        memcpy(&glow, &p[4], 4);
        glow = s_ambientGlowVar->m_floatValue * glow;
        memcpy(&p[4], &glow, 4);
    }

    return p;
}

void CoA::Graphics::CloudsSet() {
    s_cloudsDirty = true;
}

bool CoA::Graphics::CloudsTick() {
    if (!s_cloudsDirty && (!s_animateCloudsVar || s_animateCloudsVar->m_intValue != 0)) {
        return false;
    }

    s_cloudsDirty = false;
    return true;
}

float CoA::Graphics::FarClipClamp(float farClip, uint32_t mapId) {
    float cap = 1583.33f;

    if (mapId == 0 || mapId == 1 || mapId == 530 || mapId == 571) {
        cap = 791.66f;
    } else if (mapId == 909) {
        cap = 700.0f;
    }

    const float lo = 183.33f <= farClip ? farClip : 183.33f;
    return farClip <= cap ? lo : cap;
}

// ---- lifecycle ------------------------------------------------------------------------------------------------------
void CoA::Graphics::MapCap() {
    s_cloudsDirty = true;
    const uint32_t map = s_deps.mapId ? s_deps.mapId() : 0;
    CVar* cvar = s_deps.shadowCVar ? s_deps.shadowCVar() : nullptr;

    if (map == 0x38B || map == 0x38D) {
        if (!s_capApplied && cvar && (s_capSaved = cvar->m_intValue, 3 < s_capSaved)) {
            if (s_deps.setShadowMode) {
                s_deps.setShadowMode(3);
            }

            s_capApplied = true;
        }
    } else if (s_capApplied && cvar) {
        if (s_deps.setShadowMode) {
            s_deps.setShadowMode(s_capSaved);
        }

        s_capApplied = false;
        s_capSaved = -1;
    }
}

void CoA::Graphics::ReapplyFarClip() {
    if (CVar* v = CVar::Lookup("farClip")) {
        if (s_deps.setFarClip) {
            s_deps.setFarClip(v->m_floatValue);
        }
    }
}

void CoA::Graphics::CullingOn() {
    for (CVar* v : { s_entityCullingVar, s_portalCullingVar, s_terrainCullingVar, s_lightCullingVar, s_wmoCullingVar }) {
        if (v && v->m_intValue == 0) {
            SetVar(v, "1");
        }
    }
}

// ---- occlusion volumes ------------------------------------------------------------------------------------------------
void CoA::Graphics::BuildOccluders() {
    s_occluders.clear();
    std::vector<const uint8_t*> volumes = RowsById(Volumes());
    std::sort(volumes.begin(), volumes.end(), ById);

    std::unordered_map<uint32_t, std::vector<const uint8_t*>> byVolume;

    for (const uint8_t* row : RowsById(VolumePoints())) {
        byVolume[CoA::Dbc::Table::U32(row, 4)].push_back(row);
    }

    for (const uint8_t* volume : volumes) {
        auto it = byVolume.find(CoA::Dbc::Table::U32(volume, 0));

        if (it == byVolume.end() || it->second.size() <= 2) {
            continue;
        }

        std::vector<const uint8_t*>& rows = it->second;
        std::sort(rows.begin(), rows.end(), ById);
        Occluder o;

        for (int k = 0; k < 3; ++k) {
            o.min[k] = 3.4028235e38f;
            o.max[k] = -3.4028235e38f;
        }

        o.points.reserve(rows.size() * 3);

        for (const uint8_t* row : rows) {
            const float c[3] = { CoA::Dbc::Table::F32(row, 8), CoA::Dbc::Table::F32(row, 0xC), CoA::Dbc::Table::F32(row, 0x10) };

            for (int k = 0; k < 3; ++k) {
                o.points.push_back(c[k]);
                o.min[k] = o.min[k] <= c[k] ? o.min[k] : c[k];
                o.max[k] = c[k] <= o.max[k] ? o.max[k] : c[k];
            }
        }

        o.mapId = CoA::Dbc::Table::U32(volume, 8);
        o.flags = CoA::Dbc::Table::U32(volume, 0xC);
        s_occluders.push_back(std::move(o));
    }

    if (s_occluders.empty()) {
        return;
    }

    if (s_deps.useOccluders) {
        s_deps.useOccluders(s_occluders);
    }
}

const std::vector<CoA::Graphics::Occluder>& CoA::Graphics::Occluders() {
    return s_occluders;
}

std::vector<uint32_t> CoA::Graphics::Triangulate(const std::vector<float>& points) {
    const size_t n = points.size() / 3;
    float nx = 0.0f;
    float ny = 0.0f;
    float nz = 0.0f;

    for (size_t i = 0; i < n; ++i) {
        const float* a = &points[i * 3];
        const float* b = &points[((i + 1) % n) * 3];
        nx = nx + (a[1] - b[1]) * (b[2] + a[2]);
        ny = (a[2] - b[2]) * (a[0] + b[0]) + ny;
        nz = (b[1] + a[1]) * (a[0] - b[0]) + nz;
    }

    nx = std::fabs(nx);
    ny = std::fabs(ny);
    nz = std::fabs(nz);
    int drop;

    if (1e-06f < nx || 1e-06f < ny || 1e-06f < nz) {
        drop = (nx < ny || nx < nz) ? (nz <= ny ? 1 : 0) : 2;
    } else {
        drop = 1;
    }

    std::vector<std::vector<std::array<float, 2>>> rings(1);
    rings[0].reserve(n);

    for (size_t i = 0; i < n; ++i) {
        const float* p = &points[i * 3];

        if (drop == 0) {
            rings[0].push_back({ p[0], p[1] });
        } else if (drop == 2) {
            rings[0].push_back({ p[1], p[2] });
        } else {
            rings[0].push_back({ p[0], p[2] });
        }
    }

    return mapbox::earcut<uint32_t>(rings);
}

void CoA::Graphics::DrawOcclusion() {
    if (!s_occlusionOverlay) {
        return;
    }

    const uint32_t map = s_deps.currentMap ? s_deps.currentMap() : 0;

    for (const Occluder& o : s_occluders) {
        const size_t n = o.points.size() / 3;

        if (o.mapId != map || n <= 2) {
            continue;
        }

        const std::vector<uint32_t> triangles = CoA::Graphics::Triangulate(o.points);

        if (s_deps.drawFill) {
            s_deps.drawFill(o, triangles, 0x4040FF20);
        }

        for (size_t i = 0; i < n; ++i) {
            if (s_deps.drawLine) {
                s_deps.drawLine(&o.points[i * 3], &o.points[((i + 1) % n) * 3], 0xB040FF20);
            }
        }
    }
}

bool CoA::Graphics::OcclusionOverlay() {
    return s_occlusionOverlay;
}

int CoA::Graphics::SetRenderOcclusion(lua_State* L) {
    if (lua_gettop(L) == 1) {
        s_occlusionOverlay = lua_toboolean(L, 1) != 0;
    }

    return 0;
}

// ---- packets --------------------------------------------------------------------------------------------------------
// FUN_10266530: u32 skybox (0 clears the override).
int32_t CoA::Graphics::OnSkyOverride(void*, uint32_t, uint32_t, CDataStore* msg) {
    s_skyOverride = CoA::Read::U32(msg);
    return 1;
}

// FUN_101e2810: {u32 id, u32 (unused), u32 map, u32 flags, C-string name}; the row is {id, name, map, flags}, overwritten
// in place or inserted, then the antiportal table is rebuilt. A short packet (the 7a guard) changes nothing.
int32_t CoA::Graphics::OnPatchVolume(void*, uint32_t, uint32_t, CDataStore* msg) {
    const uint32_t id = CoA::Read::U32(msg);
    CoA::Read::U32(msg);
    const uint32_t map = CoA::Read::U32(msg);
    const uint32_t flags = CoA::Read::U32(msg);
    const std::string name = CoA::Read::Str(msg);

    if (CoA::Read::Failed(msg)) {
        return 1;
    }

    const uint32_t row[4] = { id, Volumes().AddString(name.c_str()), map, flags };
    Volumes().Upsert(id, std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(row), reinterpret_cast<const uint8_t*>(row) + sizeof(row)));
    CoA::Graphics::BuildOccluders();
    return 1;
}

// FUN_101e2990: {u32 id, u32 volume, float x, y, z}, overwritten in place or inserted, then the table is rebuilt.
int32_t CoA::Graphics::OnPatchVolumePoint(void*, uint32_t, uint32_t, CDataStore* msg) {
    uint32_t row[5];

    for (uint32_t& v : row) {
        v = CoA::Read::U32(msg);
    }

    if (CoA::Read::Failed(msg)) {
        return 1;
    }

    VolumePoints().Upsert(row[0], std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(row), reinterpret_cast<const uint8_t*>(row) + sizeof(row)));
    CoA::Graphics::BuildOccluders();
    return 1;
}

uint32_t CoA::Graphics::SkyOverride() {
    return s_skyOverride;
}

CoA::Graphics::Deps& CoA::Graphics::Dependencies() {
    return s_deps;
}

void CoA::Graphics::Init() {
    // FUN_10265d20's FUN_10114540 half, in its order: no help, flags 1, category 1.
    // name, help, flags, default, callback, category, setCommand, slot, world
    COA_CVAR("entityCulling", nullptr, 1, "1", CoA::Graphics::OnEntityCulling, 1, false, &s_entityCullingVar, true);
    COA_CVAR("portalCulling", nullptr, 1, "1", CoA::Graphics::OnPortalCulling, 1, false, &s_portalCullingVar, true);
    COA_CVAR("terrainCulling", nullptr, 1, "1", CoA::Graphics::OnTerrainCulling, 1, false, &s_terrainCullingVar, true);
    COA_CVAR("lightCulling", nullptr, 1, "1", CoA::Graphics::OnLightCulling, 1, false, &s_lightCullingVar, true);
    COA_CVAR("wmoCulling", nullptr, 1, "1", CoA::Graphics::OnWmoCulling, 1, false, &s_wmoCullingVar, true);
    COA_CVAR("fogDensity", nullptr, 1, "1", CoA::Graphics::OnFogDensity, 1, false, &s_fogDensityVar, true);
    COA_CVAR("antiportal", nullptr, 1, "1", CoA::Graphics::OnAntiportal, 1, false, &s_antiportalVar, true);
    COA_CVAR("lightLimitFix", nullptr, 1, "0", CoA::Graphics::OnLightLimitFix, 1, false, &s_lightLimitFixVar, true);
    COA_CVAR("ambientGlow", nullptr, 1, "1", nullptr, 1, false, &s_ambientGlowVar, true);
    COA_CVAR("ambientShade", nullptr, 1, "1", nullptr, 1, false, &s_ambientShadeVar, true);
    COA_CVAR("lightLinearAttenuation", nullptr, 1, "1", CoA::Graphics::OnLightAttenuation, 1, false, &s_linearVar, true);
    COA_CVAR("lightQuadraticAttenuation", nullptr, 1, "1", CoA::Graphics::OnLightAttenuation, 1, false, &s_quadraticVar, true);
    COA_CVAR("overrideDayProgress", nullptr, 1, "0", CoA::Graphics::OnOverrideDayProgress, 1, false, &s_dayProgressVar, true);
    COA_CVAR("mistDensity", nullptr, 1, "12.0", CoA::Graphics::OnMistDensity, 1, false, &s_mistDensityVar, true);

    COA_PACKET(0x650, CoA::Graphics::OnSkyOverride);
    COA_PACKET(0x9D2, CoA::Graphics::OnPatchVolume);
    COA_PACKET(0x9D3, CoA::Graphics::OnPatchVolumePoint);

    // FUN_1026c7f0 (SetRenderOcclusion's world-state registration).
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));

    CoA::ClientHooks::initialSpellsAlternate.Add(CoA::Graphics::MapCap);
    CoA::ClientHooks::afterZoneFeedback.Add(ZoneFeedback);
    CoA::ClientHooks::enterWorld.Add(CoA::Graphics::CullingOn);
    CoA::ClientHooks::enterWorld.Add(CoA::Graphics::BuildOccluders);
    CoA::ClientHooks::afterClearTrackedObjects.Add(CoA::Graphics::DrawOcclusion);
}

void CoA::Graphics::Reset() {
    s_deps = Deps();
    s_entityShadows = true;
    s_lodObjectCull = true;
    s_entityCulling = true;
    s_portalCulling = true;
    s_terrainCulling = true;
    s_lightStamp = true;
    s_wmoCulling = true;
    s_fogDensityStores = true;
    s_fogEndStores = true;
    s_antiportal = true;
    s_lightLimitFix = false;
    s_dayOverride = false;
    s_dayProgress = 0.0f;
    s_mistDensity = 12.0f;
    s_occlusionOverlay = false;
    s_cloudsDirty = false;
    s_capApplied = false;
    s_capSaved = -1;
    s_skyOverride = 0;
    memset(s_skyParams, 0, sizeof(s_skyParams));
    s_lights.clear();
    s_occluders.clear();
}
