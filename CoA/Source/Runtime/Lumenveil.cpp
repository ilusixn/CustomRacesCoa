// Lumenveil64.dll host: fills the x64 engine's input table and calls its frame events (docs/2026-11-13-lumenveil-x64.md).
#include "CoA/Source/Runtime/Lumenveil.h"
#include "CoA/Source/Runtime/LumenveilAbi.h"
#include "CoA/Source/Modules/Graphics.h"
#include "CoA/Source/Runtime/Log.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxDeviceD3d/CGxDeviceD3d.h"
#include "Engine/Source/Gx/Lumenveil.h"
#include "Engine/Source/Os/Gui.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include "WoW/Source/WorldClient/DayNight.h"
#include "WoW/Source/WorldClient/DayNight__DNInfo.h"
#include "WoW/Source/WorldClient/World.h"
#include <windows.h>
#include <cstdarg>
#include <cstring>
#include <string>

int32_t LoadingScreenMapId();

namespace {
    using namespace CoA::Lumenveil::Abi;

    Host s_host;    LumenveilCallbacks s_callbacks;
    void* s_device;
    EventFn s_frameBegin;
    EventFn s_frameEnd;
    EventFn s_opaqueBegin;
    EventFn s_opaqueDone;
    EventFn s_liquidBegin;
    EventFn s_liquidEnd;
    EventFn s_worldDone;
    EventFn s_present;
    DepthSurfaceFn s_depthSurface;
    WrapD3D9Fn s_wrapD3D9;

    std::wstring ExeDir() {
        wchar_t path[MAX_PATH];
        DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring dir(path, n);
        size_t slash = dir.find_last_of(L"\\/");
        return slash == std::wstring::npos ? std::wstring(L".") : dir.substr(0, slash);
    }

    // Always-on diagnostics: the ordinary CoA log needs "-extlog", and when the engine DLL does not load its own log
    // stays empty. This file says what the host tried.
    void HostLog(const char* fmt, ...) {
        const std::wstring path = ExeDir() + L"\\Lumenveil-host.log";
        FILE* file = nullptr;

        if (_wfopen_s(&file, path.c_str(), L"a") || !file) {
            return;
        }

        va_list args;
        va_start(args, fmt);
        vfprintf(file, fmt, args);
        va_end(args);
        fprintf(file, "\n");
        fclose(file);
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

    void ReadStockFog(void*, StockFog* fog) {
        if (!fog) {
            return;
        }

        fog->start[0] = DayNight::g_dnInfo.m_fog.m_start;
        fog->end[0] = DayNight::g_dnInfo.m_fog.m_end;
        fog->start[1] = DayNight::g_dnInfo.m_fogInterior.m_start;
        fog->end[1] = DayNight::g_dnInfo.m_fogInterior.m_end;
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
        if (s_device) {
            return s_device;
        }

        return D3dDevice();
    }

    void* Window(void*) {
        return OsGuiGetWindow(OSGUI_WINDOW_TYPE_GX);
    }

    struct LumenveilHost final : LumenveilCallbacks {
        static void OnFrameBegin() {
            static bool logged = false;

            // The client's main-scene depth is bound once at device creation. Point it at the engine's INTZ depth
            // before the world clears and renders, so the fog reads this frame's depth.
            if (s_depthSurface) {
                if (auto* device = static_cast<IDirect3DDevice9*>(D3dDevice())) {
                    IDirect3DSurface9* depth = s_depthSurface();

                    if (!logged) {
                        logged = true;
                        HostLog("frameBegin: device=%p depth=%p", reinterpret_cast<void*>(device),
                                reinterpret_cast<void*>(depth));
                    }

                    if (depth) {
                        device->SetDepthStencilSurface(depth);
                    }
                }
            }

            if (s_frameBegin) s_frameBegin();
        }
        static void OnFrameEnd() { if (s_frameEnd) s_frameEnd(); }
        static void OnOpaqueBegin() { if (s_opaqueBegin) s_opaqueBegin(); }
        static void OnOpaqueDone() { if (s_opaqueDone) s_opaqueDone(); }
        static void OnLiquidBegin() { if (s_liquidBegin) s_liquidBegin(); }
        static void OnLiquidEnd() { if (s_liquidEnd) s_liquidEnd(); }
        static void OnWorldDone() {
            // A late pass (water, portraits) may have left another depth bound; the engine needs its own.
            if (s_depthSurface) {
                if (auto* device = static_cast<IDirect3DDevice9*>(D3dDevice())) {
                    if (IDirect3DSurface9* depth = s_depthSurface()) {
                        static bool logged = false;
                        const HRESULT hr = device->SetDepthStencilSurface(depth);

                        if (FAILED(hr) && !logged) {
                            logged = true;
                            HostLog("worldDone: SetDepthStencilSurface(%p) failed 0x%08lX", reinterpret_cast<void*>(depth),
                                    static_cast<unsigned long>(hr));
                        }
                    }
                }
            }

            if (s_worldDone) s_worldDone();
        }
        static void OnPresent() {
            static bool logged = false;

            if (!logged) {
                logged = true;
                HostLog("present: first frame");
            }

            if (s_present) s_present();
        }
    };
}

LumenveilCallbacks* g_lumenveil = nullptr;

void CoA::Lumenveil::Load() {
    const std::wstring path = ExeDir() + L"\\Lumenveil64.dll";
    HostLog("Load: trying %ls", path.c_str());

    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        HostLog("Load: Lumenveil64.dll is not beside the exe");
        return;
    }

    HMODULE module = LoadLibraryW(path.c_str());

    if (!module) {
        HostLog("Load: LoadLibraryW failed, error %u", static_cast<unsigned>(GetLastError()));
        return;
    }

    HostLog("Load: Lumenveil64.dll loaded at %p", reinterpret_cast<void*>(module));

    auto setHost = reinterpret_cast<SetHostFn>(GetProcAddress(module, "vf_x64_host"));
    auto init = reinterpret_cast<InitFn>(GetProcAddress(module, "vf_host_init"));
    s_frameBegin = reinterpret_cast<EventFn>(GetProcAddress(module, "vf_on_frame_begin"));
    s_frameEnd = reinterpret_cast<EventFn>(GetProcAddress(module, "vf_on_frame_end"));
    s_opaqueBegin = reinterpret_cast<EventFn>(GetProcAddress(module, "vf_on_opaque_begin"));
    s_opaqueDone = reinterpret_cast<EventFn>(GetProcAddress(module, "vf_on_opaque_done"));
    s_liquidBegin = reinterpret_cast<EventFn>(GetProcAddress(module, "vf_on_liquid_begin"));
    s_liquidEnd = reinterpret_cast<EventFn>(GetProcAddress(module, "vf_on_liquid_end"));
    s_worldDone = reinterpret_cast<EventFn>(GetProcAddress(module, "vf_on_world_done"));
    s_present = reinterpret_cast<EventFn>(GetProcAddress(module, "vf_on_present"));
    s_depthSurface = reinterpret_cast<DepthSurfaceFn>(GetProcAddress(module, "vf_host_depth_surface"));
    s_wrapD3D9 = reinterpret_cast<WrapD3D9Fn>(GetProcAddress(module, "vf_host_wrap_d3d9"));

    HostLog("Load: exports setHost=%p frameBegin=%p frameEnd=%p worldDone=%p present=%p depth=%p wrap=%p", reinterpret_cast<void*>(setHost), reinterpret_cast<void*>(s_frameBegin), reinterpret_cast<void*>(s_frameEnd), reinterpret_cast<void*>(s_worldDone), reinterpret_cast<void*>(s_present), reinterpret_cast<void*>(s_depthSurface), reinterpret_cast<void*>(s_wrapD3D9));

    if (!setHost || !init || !s_frameBegin || !s_frameEnd || !s_worldDone) {
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
    s_host.window = Window;
    s_host.readStockFog = ReadStockFog;
    setHost(&s_host);
    init();

    s_callbacks.frameBegin = &LumenveilHost::OnFrameBegin;
    s_callbacks.frameEnd = &LumenveilHost::OnFrameEnd;
    s_callbacks.opaqueBegin = &LumenveilHost::OnOpaqueBegin;
    s_callbacks.opaqueDone = &LumenveilHost::OnOpaqueDone;
    s_callbacks.liquidBegin = &LumenveilHost::OnLiquidBegin;
    s_callbacks.liquidEnd = &LumenveilHost::OnLiquidEnd;
    s_callbacks.worldDone = &LumenveilHost::OnWorldDone;
    s_callbacks.present = &LumenveilHost::OnPresent;
    g_lumenveil = &s_callbacks;

    HostLog("Load: host table set, engine ready");
    CoA::Log::Printf("Lumenveil: x64 engine loaded from Lumenveil64.dll");
}

IDirect3DSurface9* CoA::Lumenveil::DepthSurface() {
    return s_depthSurface ? s_depthSurface() : nullptr;
}

void CoA::Lumenveil::SetDevice(void* device) {
    s_device = device;
}

IDirect3D9* CoA::Lumenveil::WrapD3D9(IDirect3D9* real) {
    return s_wrapD3D9 ? s_wrapD3D9(real) : nullptr;
}






