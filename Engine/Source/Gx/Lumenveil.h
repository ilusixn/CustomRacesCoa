#pragma once

// Lumenveil's frame events, called from CGWorldFrame::RenderWorld at the same places LuxVelum's callbacks are
// (CoA/Source/Runtime/Lumenveil.cpp). Null while Lumenveil is not loaded.
using LumenveilEvent = void (*)();

struct LumenveilCallbacks {
    LumenveilEvent frameBegin;
    LumenveilEvent frameEnd;
    LumenveilEvent opaqueDone;
    LumenveilEvent liquidBegin;
    LumenveilEvent liquidEnd;
    LumenveilEvent worldDone;
};

extern LumenveilCallbacks* g_lumenveil;

// Calls a no-argument Lumenveil hook when the dll attached and provides it.
inline void LumenHook(LumenveilEvent LumenveilCallbacks::*hook) {
    if (g_lumenveil && (g_lumenveil->*hook) != nullptr) {
        (g_lumenveil->*hook)();
    }
}
