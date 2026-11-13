#pragma once

// Lumenveil's frame events, called from CGWorldFrame::RenderWorld at the same places LuxVelum's callbacks are
// (CoA/Source/Runtime/Lumenveil.cpp). Null while Lumenveil is not loaded.
using LumenveilEvent = void (*)();
using LumenveilLiquidMaterial = void (*)(int index, int begin);
using LumenveilFarClip = float (*)(float requested, int mapId, float clamped);

struct LumenveilCallbacks {
    LumenveilEvent frameBegin;
    LumenveilEvent frameEnd;
    LumenveilEvent opaqueBegin;
    LumenveilEvent opaqueDone;
    LumenveilEvent liquidBegin;
    LumenveilEvent liquidEnd;
    LumenveilEvent worldDone;
    LumenveilEvent present;
    LumenveilEvent liquidInstancesDone;
    LumenveilLiquidMaterial liquidMaterial;
    LumenveilFarClip farClipClamp;
};

extern LumenveilCallbacks* g_lumenveil;

// Calls a no-argument Lumenveil hook when the dll attached and provides it.
inline void LumenHook(LumenveilEvent LumenveilCallbacks::*hook) {
    if (g_lumenveil && (g_lumenveil->*hook) != nullptr) {
        (g_lumenveil->*hook)();
    }
}

inline void LumenLiquidMaterial(int index, int begin) {
    if (g_lumenveil && g_lumenveil->liquidMaterial) {
        g_lumenveil->liquidMaterial(index, begin);
    }
}

inline void LumenLiquidInstancesDone() {
    if (g_lumenveil && g_lumenveil->liquidInstancesDone) {
        g_lumenveil->liquidInstancesDone();
    }
}

inline float LumenFarClipClamp(float requested, int mapId, float clamped) {
    return g_lumenveil && g_lumenveil->farClipClamp ? g_lumenveil->farClipClamp(requested, mapId, clamped) : clamped;
}
