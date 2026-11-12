#ifndef ENGINE_SOURCE_GX_LUXVELUM_H
#define ENGINE_SOURCE_GX_LUXVELUM_H

#include <cstdint>

// LuxVelum64.dll host API, version 1 (fog, light and water add-on of the CoA Ultra-HD pack). The layout matches the
// LuxVelum_Attach export of that dll; see docs/2026-11-12-luxvelum-host.md. Not part of the original (CoA64 addition).

struct LuxDayNight {
    float dayFraction;
    uint32_t lightIsMoon;
    float skyCentre[3];
    float sunPosition[3];
    float moonPosition[3];
    uint32_t fogColor;
    float fogStart[2];
    float fogEnd[2];
    uint32_t ambientColor;
    uint32_t directColor;
    uint32_t sunColor;
    float zoneFogDistance;
    float glow;
    float stormBlend;
    int32_t screenEffectLightSlot;
    float interiorFogBlend;
    uint32_t skyColors[6];
    uint32_t oceanColors[2];
    uint32_t riverColors[2];
};

struct LuxScreenEffects {
    uint64_t current;
    uint64_t glow;
    uint64_t death;
    uint32_t ffxEnabled;
    uint32_t currentEnabled;
};

struct LuxPointLight {
    float position[3];
    float color[3];
    float attenuation[3];
    uint32_t type;
    uint32_t enabled;
    uint64_t id;
};

struct LuxLiquidInfo {
    uint32_t inBank;
    uint32_t hasRow;
    uint32_t id;
    uint32_t flags;
    uint32_t soundBank;
    uint32_t materialId;
    char texture0[128];
};

struct LuxWaterContact {
    uint64_t guid;
    float position[3];
    float surface;
    float radius;
    float height;
    uint32_t swimming;
    uint32_t onTransport;
};

struct LuxM2Fog {
    float start;
    float end;
    float exponent;
    const uint32_t* colour;
};

struct LuxHostApi {
    uint32_t size;
    uint32_t version;
    const char* hostName;
    void* (*d3dDevice)();
    int32_t (*gxMatrices)(float* view, float* projection);
    int32_t (*gxViewportDepthRange)(float* range);
    void (*camera)(float* position, float* target);
    int32_t (*cameraLiquid)();
    int32_t (*currentMap)();
    float (*worldFarClip)();
    void (*dayNight)(LuxDayNight* out);
    void (*readStockFog)(float* start, float* end);
    void (*writeStockFog)(const float* start, const float* end);
    void (*screenEffects)(LuxScreenEffects* out);
    int32_t (*glowColourAlpha)(uint8_t** out);
    int32_t (*enumPointLights)(int32_t (*callback)(void* context, const LuxPointLight* light), void* context);
    int32_t (*cameraInterior)(uint32_t* interior);
    int32_t (*describeLiquid)(const void* settings, LuxLiquidInfo* out);
    int32_t (*enumWaterUnits)(int32_t (*callback)(void* context, const LuxWaterContact* contact), void* context);
    const int32_t* waterRipplesValue;
    void (*renderGlares)();
};

struct LuxCallbacks {
    uint32_t size;
    uint32_t version;
    void* (*filterD3D9Proc)(const char* name, void* proc);
    void (*frameBegin)();
    void (*frameEnd)();
    void (*opaqueDone)();
    void (*liquidBegin)();
    void (*liquidEnd)();
    int32_t (*glareBegin)();
    void (*glareEnd)();
    void (*worldTextBegin)();
    void (*worldTextEnd)();
    void (*worldDone)();
    int32_t observeM2Fog;
    void (*m2BatchFog)(LuxM2Fog* fog);
    void (*waterPassBegin)(int32_t hasTransparent);
    void (*waterPassEnd)();
    int32_t (*waterDrawBegin)(const void* settings);
    void (*waterDrawEnd)(int32_t token);
    float (*farClipClamp)(float requested, int32_t mapId, float clamped);
    int32_t keepStaleLights;
};

static_assert(sizeof(LuxDayNight) == 136, "LuxDayNight layout");
static_assert(sizeof(LuxScreenEffects) == 32, "LuxScreenEffects layout");
static_assert(sizeof(LuxPointLight) == 56, "LuxPointLight layout");
static_assert(sizeof(LuxLiquidInfo) == 152, "LuxLiquidInfo layout");
static_assert(sizeof(LuxWaterContact) == 40, "LuxWaterContact layout");
static_assert(sizeof(LuxM2Fog) == 24, "LuxM2Fog layout");
static_assert(sizeof(LuxHostApi) == 160, "LuxHostApi layout");
static_assert(sizeof(LuxCallbacks) == 160, "LuxCallbacks layout");

constexpr uint32_t kLuxApiVersion = 1;

// Set by CoA::LuxVelum::Load when LuxVelum64.dll attached; null otherwise.
extern LuxCallbacks* g_luxVelum;

// Calls a no-argument LuxVelum hook when the dll attached and provides it.
inline void LuxHook(void (*LuxCallbacks::*hook)()) {
    if (g_luxVelum && g_luxVelum->*hook) {
        (g_luxVelum->*hook)();
    }
}

#endif
