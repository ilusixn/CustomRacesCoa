// Lumenveil64.dll host: fills the x64 engine's input table and calls its frame events (docs/2026-11-13-lumenveil-x64.md).
#include "CoA/Source/Runtime/Lumenveil.h"
#include "CoA/Source/Runtime/LumenveilAbi.h"
#include "CoA/Source/Modules/Graphics.h"
#include "CoA/Source/Runtime/Log.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxDeviceD3d/CGxDeviceD3d.h"
#include "Engine/Source/Gx/Lumenveil.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include "WoW/Source/WorldClient/DayNight.h"
#include "WoW/Source/WorldClient/DayNight__DNInfo.h"
#include "WoW/Source/WorldClient/World.h"
#include <windows.h>
#include <cstring>
#include <string>

int32_t LoadingScreenMapId();

namespace {
    using namespace CoA::Lumenveil::Abi;

    Host s_host;
    LumenveilCallbacks s_callbacks;
    EventFn s_frameBegin;
    EventFn s_frameEnd;
    EventFn s_opaqueDone;
    EventFn s_liquidBegin;
    EventFn s_liquidEnd;
    EventFn s_worldDone;

    std::wstring ExeDir() {
        wchar_t path[MAX_PATH];
        DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring dir(path, n);
        size_t slash = dir.find_last_of(L"\\/");
        return slash == std::wstring::npos ? std::wstring(L".") : dir.substr(0, slash);
    }

    uint32_t Packed(const CImVector& color) {
        uint32_t value;
        std::memcpy(&value, &color, sizeof(value));
        return value;
    }

    void* D3dDevice() {
        CGxDevice* device = g_theGxDevicePtr;

        if (!device || device->m_api != GxApi_D3d9) {
            return nullptr;
        }

        return static_cast<CGxDeviceD3d*>(device)->m_d3dDevice;
    }

    bool GxMatrices(float* view, float* projection) {
        CGxDevice* device = g_theGxDevicePtr;

        if (!device) {
            return false;
        }

        const auto& stack = device->m_xforms[GxXform_View];

        if (stack.m_level >= 4) {
            return false;
        }

        std::memcpy(view, &stack.m_mtx[stack.m_level], 16 * sizeof(float));
        std::memcpy(projection, &device->m_projection, 16 * sizeof(float));
        return true;
    }

    void Camera(float* position, float* target) {
        position[0] = CWorldScene::s_activeWorldView.x;
        position[1] = CWorldScene::s_activeWorldView.y;
        position[2] = CWorldScene::s_activeWorldView.z;
        target[0] = CWorldScene::camTarget.x;
        target[1] = CWorldScene::camTarget.y;
        target[2] = CWorldScene::camTarget.z;
    }

    bool CameraInLiquid(void*) {
        return false;
    }

    bool OpaqueViewport(void*, D3DVIEWPORT9* out) {
        auto* device = static_cast<IDirect3DDevice9*>(D3dDevice());

        if (!device || !out) {
            return false;
        }

        return SUCCEEDED(device->GetViewport(out));
    }

    bool ReadFrameInputs(void*, CoA::Lumenveil::Abi::FrameInputs* out) {
        if (!out || !GxMatrices(out->cameraRelativeView, out->glProjection)) {
            return false;
        }

        Camera(out->camPos, out->camTarget);
        OpaqueViewport(nullptr, &out->viewport);
        out->seconds = static_cast<double>(GetTickCount64()) / 1000.0;

        const DayNight::DNInfo& info = DayNight::g_dnInfo;
        const float day = CoA::Graphics::DayProgress(info.m_dayProgression);
        out->dayFraction = day;
        out->lightIsMoon = day < 0.20139f || day > 0.92361f;
        out->fogColor = Packed(info.m_fogInterior.m_color);
        out->ambientColor = Packed(info.m_bands.m_ambient);
        out->directColor = Packed(info.m_bands.m_diffuse);
        out->sunColor = Packed(info.m_bands.m_sunCloudColors[0]);
        out->fogStart = info.m_fog.m_start;
        out->fogEnd = info.m_fog.m_end;
        out->zoneFogDistance = info.m_bands.m_fogEnd;
        out->clientGlowAmount = info.m_bands.m_glow;
        out->skyHorizonColor = Packed(info.m_bands.m_skyColors[0]);
        out->skyAboveHorizonColor = Packed(info.m_bands.m_skyColors[2]);
        out->lightParams.stormBlend = info.m_stormBlend;
        out->lightParams.screenEffectSlot = static_cast<int>(info.m_overrideLightParamsId);
        out->farClip = World::s_farClip;
        out->clientFarClip = World::s_farClip;
        out->inLiquid = false;
        out->mapId = LoadingScreenMapId();
        return true;
    }

    void WriteStockFog(void*, const StockFog* fog) {
        if (!fog) {
            return;
        }

        DayNight::g_dnInfo.m_fog.m_start = fog->start[0];
        DayNight::g_dnInfo.m_fog.m_end = fog->end[0];
        DayNight::g_dnInfo.m_fogInterior.m_start = fog->start[1];
        DayNight::g_dnInfo.m_fogInterior.m_end = fog->end[1];
    }

    Host::ClockState ReadClock(void*) {
        return Host::ClockState{false, 0, 0.0f};
    }

    bool SetClock(void*, int, float) {
        return false;
    }

    Host::WeatherState ReadWeather(void*) {
        return Host::WeatherState{false, 0, 0.0f, 0.0f};
    }

    bool SetWeather(void*, int, float, bool) {
        return false;
    }

    int AreaId(void*) {
        return 0;
    }

    void RefreshFarClip(void*) {}

    void* Device(void*) {
        return D3dDevice();
    }

    struct LumenveilHost final : LumenveilCallbacks {
        static void OnFrameBegin() { if (s_frameBegin) s_frameBegin(); }
        static void OnFrameEnd() { if (s_frameEnd) s_frameEnd(); }
        static void OnOpaqueDone() { if (s_opaqueDone) s_opaqueDone(); }
        static void OnLiquidBegin() { if (s_liquidBegin) s_liquidBegin(); }
        static void OnLiquidEnd() { if (s_liquidEnd) s_liquidEnd(); }
        static void OnWorldDone() { if (s_worldDone) s_worldDone(); }
    };
}

LumenveilCallbacks* g_lumenveil = nullptr;

void CoA::Lumenveil::Load() {
    const std::wstring path = ExeDir() + L"\\Lumenveil64.dll";

    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        CoA::Log::Printf("Lumenveil: Lumenveil64.dll is not beside the exe");
        return;
    }

    HMODULE module = LoadLibraryW(path.c_str());

    if (!module) {
        CoA::Log::Printf("Lumenveil: Lumenveil64.dll not loaded (error %u)", static_cast<unsigned>(GetLastError()));
        return;
    }

    auto setHost = reinterpret_cast<SetHostFn>(GetProcAddress(module, "vf_x64_host"));
    s_frameBegin = reinterpret_cast<EventFn>(GetProcAddress(module, "vf_on_frame_begin"));
    s_frameEnd = reinterpret_cast<EventFn>(GetProcAddress(module, "vf_on_frame_end"));
    s_opaqueDone = reinterpret_cast<EventFn>(GetProcAddress(module, "vf_on_opaque_done"));
    s_liquidBegin = reinterpret_cast<EventFn>(GetProcAddress(module, "vf_on_liquid_begin"));
    s_liquidEnd = reinterpret_cast<EventFn>(GetProcAddress(module, "vf_on_liquid_end"));
    s_worldDone = reinterpret_cast<EventFn>(GetProcAddress(module, "vf_on_world_done"));

    if (!setHost || !s_frameBegin || !s_frameEnd || !s_worldDone) {
        CoA::Log::Printf("Lumenveil: Lumenveil64.dll is not the x64 host build (exports missing); unloaded");
        FreeLibrary(module);
        return;
    }

    s_host.size = sizeof(Host);
    s_host.d3dDevice = D3dDevice();
    s_host.frameInputs = ReadFrameInputs;
    s_host.opaqueViewport = OpaqueViewport;
    s_host.cameraInLiquid = CameraInLiquid;
    s_host.writeStockFog = WriteStockFog;
    s_host.readClock = ReadClock;
    s_host.setClock = SetClock;
    s_host.readWeather = ReadWeather;
    s_host.setWeather = SetWeather;
    s_host.areaId = AreaId;
    s_host.refreshFarClip = RefreshFarClip;
    s_host.device = Device;
    setHost(&s_host);

    s_callbacks.frameBegin = &LumenveilHost::OnFrameBegin;
    s_callbacks.frameEnd = &LumenveilHost::OnFrameEnd;
    s_callbacks.opaqueDone = &LumenveilHost::OnOpaqueDone;
    s_callbacks.liquidBegin = &LumenveilHost::OnLiquidBegin;
    s_callbacks.liquidEnd = &LumenveilHost::OnLiquidEnd;
    s_callbacks.worldDone = &LumenveilHost::OnWorldDone;
    g_lumenveil = &s_callbacks;

    CoA::Log::Printf("Lumenveil: x64 engine loaded from Lumenveil64.dll");
}



