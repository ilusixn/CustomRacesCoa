#ifndef COA_SOURCE_RUNTIME_DXVK_H
#define COA_SOURCE_RUNTIME_DXVK_H

#include <cstdint>
#include <string>

// CoA64's Vulkan renderer: DXVK's x64 d3d9.dll from <exe dir>\Vulkan\ (2.0) or <exe dir>\x64\ (1.9.x); the install's
// own d3d9.dll is the 32-bit DXVK of Ascension.exe. Preload runs at startup before the first archive opens, so the HD
// archive filter (RealmDataClient) already knows whether HD may load; both ILoadD3dLib then take the preloaded module.
// "-nodxvk" turns it off; a missing or unloadable dll falls back to the system d3d9. Not part of the original (CoA64
// addition).
namespace CoA::Dxvk {
    struct Deps {
        bool (*hasArg)(const char* name);
        std::wstring (*exeDir)();
        void* (*load)(const std::wstring& path, uint32_t& err);
        void (*log)(const char* line);
    };

    void Preload(const Deps& deps);
    // A new reference to the DXVK module (each caller frees its own), or nullptr when DXVK is not active.
    void* D3d9Library();
    bool Active();

    // Tests only.
    void Reset();
}

// DxvkClient.cpp: the Win32 Deps; also hands D3d9Library to the D3D9 devices (g_d3d9LibraryHook).
namespace CoA::DxvkClient {
    void Preload();
}

#endif
