#ifndef COA_SOURCE_RUNTIME_LUMENVEIL_H
#define COA_SOURCE_RUNTIME_LUMENVEIL_H

#include <d3d9.h>

// Loads the x64 Lumenveil engine (Lumenveil64.dll beside the exe) when the Visuals choice is Lumenveil. The engine
// stays a separate GPLv2 DLL; this host only fills its input table and calls its frame events at the places
// LuxVelum's callbacks sit (Engine/Source/Gx/Lumenveil.h). Not part of the original (CoA64 addition).
namespace CoA::Lumenveil {
    void Load();

    // The engine's INTZ scene depth while it renders; null otherwise.
    IDirect3DSurface9* DepthSurface();
}

#endif
