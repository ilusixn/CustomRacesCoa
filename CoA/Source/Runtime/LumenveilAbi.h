#ifndef COA_SOURCE_RUNTIME_LUMENVEIL_ABI_H
#define COA_SOURCE_RUNTIME_LUMENVEIL_ABI_H

// Layout of the x64 Lumenveil engine's host interface (engine/src/host_x64.h and engine/src/engine.h of lumenveil/
// Lumenveil, GPLv2). Only the plain-data structures cross the boundary; the engine stays a separate DLL.
#include <cstdint>
#include <d3d9.h>

namespace CoA::Lumenveil::Abi {
    enum WeatherKind : int {
        Clear = 0,
        Rain = 1,
        Snow = 2,
        Sand = 3,
    };

    constexpr int kMaxCorpseInputs = 16;
    constexpr int kMaxUnitInputs = 32;
    constexpr int kDayNightFogGroupCount = 2;

    struct UnitInput {
        float feet[3];
        float boundingRadius;
        int target;
        uint64_t guid;
    };

    struct LightParamsSelection {
        float stormBlend;
        int screenEffectSlot;
    };

    struct FrameInputs {
        float cameraRelativeView[16];
        float glProjection[16];
        float camPos[3];
        float camTarget[3];
        D3DVIEWPORT9 viewport;
        double seconds;
        float dayFraction;
        float toLight[3];
        bool lightIsMoon;
        float sunElevation;
        float moonElevation;
        WeatherKind weather;
        float weatherIntensity;
        uint32_t fogColor;
        uint32_t sunColor;
        uint32_t directColor;
        uint32_t ambientColor;
        float fogStart;
        float fogEnd;
        float zoneFogDistance;
        float farClip;
        float clientFarClip;
        float clientGlowAmount;
        bool inLiquid;
        int mapId;
        int zoneId;
        uint32_t skyHorizonColor;
        uint32_t skyAboveHorizonColor;
        float dryLand;
        float greenLand;
        LightParamsSelection lightParams;
        float indoors;
        bool playerKnown;
        float playerPos[3];
        float playerFacing;
        uint32_t playerMoveFlags;
        uint32_t playerMountDisplay;
        uint32_t playerClass;
        bool targetKnown;
        float targetPos[3];
        float targetBoundingRadius;
        int corpseCount;
        float corpsePos[kMaxCorpseInputs][3];
        uint32_t corpseIds[kMaxCorpseInputs];
        int unitCount;
        UnitInput units[kMaxUnitInputs];
    };

    struct StockFog {
        float start[kDayNightFogGroupCount];
        float end[kDayNightFogGroupCount];
    };

    // Mirrors host_x64.h: what the engine asks the client for instead of the 32-bit build's memory reads.
    struct Host {
        unsigned size;
        void* context;
        void* d3dDevice;
        bool (*frameInputs)(void* context, FrameInputs* out);
        bool (*opaqueViewport)(void* context, D3DVIEWPORT9* out);
        bool (*cameraInLiquid)(void* context);
        void (*writeStockFog)(void* context, const StockFog* fog);
        // Clock and weather types are laid out as in the client's own structs (world_controls.h):
        struct ClockState { bool valid; int minute; float speed; };
        struct WeatherState { bool valid; int type; float target; float current; };
        ClockState (*readClock)(void* context);
        bool (*setClock)(void* context, int minute, float speed);
        WeatherState (*readWeather)(void* context);
        bool (*setWeather)(void* context, int type, float grade, bool instant);
        int (*areaId)(void* context);
        void (*refreshFarClip)(void* context);
        void* (*device)(void* context);
        void* (*window)(void* context);
    };

    // The engine's exports.
    using SetHostFn = void(__cdecl*)(const Host*);
    using InitFn = void(__cdecl*)();
    using EventFn = void(__cdecl*)();
    using DepthSurfaceFn = IDirect3DSurface9*(__cdecl*)();
    using LiquidMaterialFn = void(__cdecl*)(int index, int begin);
}

#endif


