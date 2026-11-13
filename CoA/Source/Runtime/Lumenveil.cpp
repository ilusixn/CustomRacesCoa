// Lumenveil64.dll host: fills the x64 engine's input table and calls its frame events (docs/2026-11-13-lumenveil-x64.md).
#include "CoA/Source/Runtime/Lumenveil.h"
#include "CoA/Source/Runtime/LumenveilAbi.h"
#include "CoA/Source/Modules/Graphics.h"
#include "CoA/Source/Runtime/Log.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxDeviceD3d/CGxDeviceD3d.h"
#include "Engine/Source/Gx/Lumenveil.h"
#include "Engine/Source/Os/Gui.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/FFX/EffectGlow.h"
#include "Engine/Source/FFX/FFX.h"
#include "Engine/Source/FFX/PassGlow.h"
#include "WoW/Source/Game/GameTime.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Object/Movement.h"
#include "WoW/Source/Object/ObjectClient/Object_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Object/Types.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/WorldClient/MapWeather.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include "WoW/Source/WorldClient/DayNight.h"
#include "WoW/Source/WorldClient/DayNight__DNInfo.h"
#include "WoW/Source/WorldClient/World.h"
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
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
    EventFn s_liquidInstancesDone;
    LiquidMaterialFn s_liquidMaterial;
    FarClipFn s_farClipLift;
    EventFn s_worldDone;
    EventFn s_present;
    DepthSurfaceFn s_depthSurface;
    WrapD3D9Fn s_wrapD3D9;

    // The 32-bit engine captures the view/projection at opaqueDone and uses that snapshot at worldDone. Taking
    // the matrices later (after name plates and chat bubbles) can differ slightly and makes the temporal fog
    // history misalign.
    float s_opaqueView[16];
    float s_opaqueProj[16];
    D3DVIEWPORT9 s_opaqueViewport;
    bool s_opaqueValid = false;
    bool s_loggedLateView = false;

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

    float ClientGlowAmount() {
        FFX::Effect* effect = FFX::s_effect;

        if (!effect || effect != CGWorldFrame::ffxEffectGlow) {
            return 0.0f;
        }

        auto* glow = static_cast<EffectGlow*>(effect);

        if (glow->m_passes.Count() <= 2 || glow->m_altPasses.Count() <= 2) {
            return 0.0f;
        }

        auto* pass = static_cast<PassGlowBase*>(glow->m_passes[2]);

        return pass ? static_cast<float>(pass->m_color.a) / 255.0f : 0.0f;
    }

    // The camera bobs at the water surface; toggling the fog on and off with every wave makes the distance
    // haze flicker. Enter the underwater state only when clearly below the surface and leave it near the top.
    bool s_cameraInLiquid = false;

    bool CameraInLiquid(void*) {
        float depth = 0.0f;
        const int type = World::SceneCamLiquidStatus(&depth);

        if (type != 0 && depth > 0.5f) {
            s_cameraInLiquid = true;
        } else if (type == 0 || depth < 0.1f) {
            s_cameraInLiquid = false;
        }

        return s_cameraInLiquid;
    }

    bool OpaqueViewport(void*, D3DVIEWPORT9* out) {
        auto* device = static_cast<IDirect3DDevice9*>(D3dDevice());

        if (!device || !out) {
            return false;
        }

        return SUCCEEDED(device->GetViewport(out));
    }

    constexpr float kUnitScanYards = 80.0f;
    constexpr float kCorpseScanYards = 60.0f;
    constexpr int kMaxVisibleObjects = 4096;
    constexpr uint32_t kUnitDynamicLootable = 0x1;

    WOWGUID s_playerGuid = {};
    WOWGUID s_targetGuid = {};

    struct VisibleUnits {
        CoA::Lumenveil::Abi::FrameInputs* out;
        WOWGUID unitTargets[CoA::Lumenveil::Abi::kMaxUnitInputs];
        int visited;
    };

    int __cdecl VisitVisibleUnit(WOWGUID guid, void* param) {
        auto& scan = *static_cast<VisibleUnits*>(param);

        if (++scan.visited > kMaxVisibleObjects) {
            return 0;
        }

        auto* object = ClntObjMgrObjectPtr(guid, TYPE_UNIT, "Lumenveil.cpp", __LINE__);

        if (!object || !object->m_obj) {
            return 1;
        }

        const OBJECT_TYPE_ID typeId = object->m_typeId;

        if (typeId != ID_UNIT && typeId != ID_PLAYER) {
            return 1;
        }

        auto* unit = static_cast<CGUnit_C*>(object);

        if (!unit->m_unit) {
            return 1;
        }

        const C3Vector at = unit->GetPosition();

        if (!std::isfinite(at.x) || !std::isfinite(at.y) || !std::isfinite(at.z)) {
            return 1;
        }

        const float dx = at.x - scan.out->camPos[0];
        const float dy = at.y - scan.out->camPos[1];
        const float dz = at.z - scan.out->camPos[2];
        const float squared = dx * dx + dy * dy + dz * dz;
        const bool alive = unit->m_unit->m_unitFieldHealth != 0;
        const bool creature = typeId == ID_UNIT;
        const bool lootable = creature && !alive && (unit->m_unit->m_unitDynamicFlags & kUnitDynamicLootable) != 0;

        if (alive) {
            if (squared > kUnitScanYards * kUnitScanYards ||
                scan.out->unitCount >= CoA::Lumenveil::Abi::kMaxUnitInputs) {
                return 1;
            }

            auto& input = scan.out->units[scan.out->unitCount];
            input.feet[0] = at.x;
            input.feet[1] = at.y;
            input.feet[2] = at.z;
            input.boundingRadius = unit->m_unit->m_unitFieldBoundingradius;
            input.target = -1;
            input.guid = guid.m_guid;
            scan.unitTargets[scan.out->unitCount] = unit->m_unit->m_unitFieldTarget;
            ++scan.out->unitCount;
            return 1;
        }

        if (!lootable || squared > kCorpseScanYards * kCorpseScanYards ||
            scan.out->corpseCount >= CoA::Lumenveil::Abi::kMaxCorpseInputs) {
            return 1;
        }

        auto& corpse = scan.out->corpsePos[scan.out->corpseCount];
        corpse[0] = at.x;
        corpse[1] = at.y;
        corpse[2] = at.z;
        scan.out->corpseIds[scan.out->corpseCount] = static_cast<uint32_t>(guid.m_guid);
        ++scan.out->corpseCount;
        return 1;
    }

    struct WorldScan {
        int unitCount;
        CoA::Lumenveil::Abi::UnitInput units[CoA::Lumenveil::Abi::kMaxUnitInputs];
        int corpseCount;
        float corpsePos[CoA::Lumenveil::Abi::kMaxCorpseInputs][3];
        uint32_t corpseIds[CoA::Lumenveil::Abi::kMaxCorpseInputs];
        double seconds;
    };

    WorldScan s_worldScan;

    void ReadPlayer(CoA::Lumenveil::Abi::FrameInputs& out) {
        out.playerKnown = false;
        out.targetKnown = false;
        s_playerGuid = {};
        s_targetGuid = {};

        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player || !player->m_unit || !player->m_obj) {
            return;
        }

        const C3Vector at = player->GetPosition();
        const float facing = player->GetFacing();

        if (!std::isfinite(at.x) || !std::isfinite(at.y) || !std::isfinite(at.z) || !std::isfinite(facing)) {
            return;
        }

        out.playerKnown = true;
        out.playerPos[0] = at.x;
        out.playerPos[1] = at.y;
        out.playerPos[2] = at.z;
        out.playerFacing = facing;
        out.playerMoveFlags = player->m_movement ? player->m_movement->m_flags : 0;
        out.playerMountDisplay = player->m_unit->m_unitFieldMountdisplayid;
        out.playerClass = player->m_unit->m_unitFieldBytes0[1];
        s_playerGuid = player->m_obj->m_guid;
        s_targetGuid = player->m_unit->m_unitFieldTarget;

        if (!s_targetGuid) {
            return;
        }

        auto* target = ClntObjMgrObjectPtr(s_targetGuid, TYPE_UNIT, "Lumenveil.cpp", __LINE__);

        if (!target || !target->m_obj) {
            return;
        }

        const C3Vector targetAt = target->GetPosition();

        if (!std::isfinite(targetAt.x) || !std::isfinite(targetAt.y) || !std::isfinite(targetAt.z)) {
            return;
        }

        out.targetKnown = true;
        out.targetPos[0] = targetAt.x;
        out.targetPos[1] = targetAt.y;
        out.targetPos[2] = targetAt.z;

        if (target->m_typeId == ID_UNIT || target->m_typeId == ID_PLAYER) {
            auto* targetUnit = static_cast<CGUnit_C*>(target);

            if (targetUnit->m_unit) {
                out.targetBoundingRadius = targetUnit->m_unit->m_unitFieldBoundingradius;
            }
        }
    }

    void RefreshUnitFeet(CoA::Lumenveil::Abi::FrameInputs& out) {
        for (int i = 0; i < out.unitCount; ++i) {
            WOWGUID guid = {};
            guid.m_guid = out.units[i].guid;
            auto* object = ClntObjMgrObjectPtr(guid, TYPE_UNIT, "Lumenveil.cpp", __LINE__);

            if (!object || !object->m_obj ||
                (object->m_typeId != ID_UNIT && object->m_typeId != ID_PLAYER)) {
                continue;
            }

            const C3Vector at = object->GetPosition();

            if (std::isfinite(at.x) && std::isfinite(at.y) && std::isfinite(at.z)) {
                out.units[i].feet[0] = at.x;
                out.units[i].feet[1] = at.y;
                out.units[i].feet[2] = at.z;
            }
        }
    }

    void ReadWorldUnits(CoA::Lumenveil::Abi::FrameInputs& out) {        constexpr double kScanInterval = 0.1;

        if (s_worldScan.seconds < 0.0 || out.seconds < s_worldScan.seconds ||
            out.seconds - s_worldScan.seconds >= kScanInterval) {
            s_worldScan.seconds = out.seconds;
            CoA::Lumenveil::Abi::FrameInputs scratch = out;
            scratch.unitCount = 0;
            scratch.corpseCount = 0;
            VisibleUnits scan = {};
            scan.out = &scratch;
            ClntObjMgrEnumVisibleObjects(VisitVisibleUnit, &scan);

            for (int i = 0; i < scratch.unitCount; ++i) {
                for (int j = 0; j < scratch.unitCount; ++j) {
                    if (i != j && scan.unitTargets[i].m_guid == scratch.units[j].guid) {
                        scratch.units[i].target = j;
                        break;
                    }
                }
            }

            s_worldScan.unitCount = scratch.unitCount;
            std::memcpy(s_worldScan.units, scratch.units, sizeof(s_worldScan.units));
            s_worldScan.corpseCount = scratch.corpseCount;
            std::memcpy(s_worldScan.corpsePos, scratch.corpsePos, sizeof(s_worldScan.corpsePos));
            std::memcpy(s_worldScan.corpseIds, scratch.corpseIds, sizeof(s_worldScan.corpseIds));

            static bool s_loggedUnits = false;

            if (!s_loggedUnits && (scratch.unitCount > 0 || scratch.corpseCount > 0)) {
                s_loggedUnits = true;
                HostLog("units: %d alive within 80 yd, %d lootable corpses within 60 yd", scratch.unitCount,
                        scratch.corpseCount);
            }
        }

        out.unitCount = s_worldScan.unitCount;
        std::memcpy(out.units, s_worldScan.units, sizeof(out.units));
        out.corpseCount = s_worldScan.corpseCount;
        std::memcpy(out.corpsePos, s_worldScan.corpsePos, sizeof(out.corpsePos));
        std::memcpy(out.corpseIds, s_worldScan.corpseIds, sizeof(out.corpseIds));
        RefreshUnitFeet(out);
    }

    Host::WeatherState ReadWeather(void*);

    bool ReadFrameInputs(void*, CoA::Lumenveil::Abi::FrameInputs* out) {
        if (!out) {
            return false;
        }

        if (s_opaqueValid) {
            std::memcpy(out->cameraRelativeView, s_opaqueView, sizeof(out->cameraRelativeView));
            std::memcpy(out->glProjection, s_opaqueProj, sizeof(out->glProjection));
            out->viewport = s_opaqueViewport;

            if (!s_loggedLateView) {
                float lateView[16];
                float lateProj[16];

                if (GxMatrices(lateView, lateProj) &&
                    (std::memcmp(lateView, s_opaqueView, sizeof(lateView)) != 0 ||
                     std::memcmp(lateProj, s_opaqueProj, sizeof(lateProj)) != 0)) {
                    s_loggedLateView = true;
                    HostLog("worldDone: the view/projection differ from the opaqueDone capture; using the capture");
                }
            }
        } else if (!GxMatrices(out->cameraRelativeView, out->glProjection)) {
            return false;
        }

        Camera(out->camPos, out->camTarget);

        if (!s_opaqueValid && !OpaqueViewport(nullptr, &out->viewport)) {
            return false;
        }

        out->seconds = static_cast<double>(GetTickCount64()) / 1000.0;

        const DayNight::DNInfo& info = DayNight::g_dnInfo;
        const float day = CoA::Graphics::DayProgress(info.m_dayProgression);
        out->dayFraction = day;
        out->lightIsMoon = day < 0.20139f || day > 0.92361f;
        out->fogColor = Packed(info.m_fogInterior.m_color);
        out->ambientColor = Packed(info.m_bands.m_ambient);
        out->directColor = Packed(info.m_bands.m_diffuse);
        out->sunColor = Packed(info.m_bands.m_sunCloudColors[1]);
        out->fogStart = info.m_fog.m_start;
        out->fogEnd = info.m_fog.m_end;
        out->zoneFogDistance = info.m_bands.m_fogEnd;
        out->clientGlowAmount = ClientGlowAmount();
        out->skyHorizonColor = Packed(info.m_bands.m_skyColors[0]);
        out->skyAboveHorizonColor = Packed(info.m_bands.m_skyColors[2]);
        out->lightParams.stormBlend = info.m_stormBlend;
        out->lightParams.screenEffectSlot = static_cast<int>(info.m_overrideLightParamsId);
        out->farClip = World::s_farClip;
        out->clientFarClip = World::s_farClip;
        out->inLiquid = CameraInLiquid(nullptr);
        out->mapId = LoadingScreenMapId();
        out->zoneId = CGGameUI::m_areaID;
        out->indoors = std::clamp(info.m_interiorFogBlend, 0.0f, 1.0f);

        const Host::WeatherState weather = ReadWeather(nullptr);

        if (weather.valid) {
            out->weather = static_cast<WeatherKind>(std::clamp(weather.type, 0, 3));
            out->weatherIntensity = std::clamp(weather.current, 0.0f, 1.0f);
        }

        const C3Vector& sun = DayNight::g_planets.m_sun.m_position;
        const C3Vector& moon = DayNight::g_planets.m_moon.m_position;
        const C3Vector sunRelative = {sun.x - info.m_cameraPos.x, sun.y - info.m_cameraPos.y,
                                      sun.z - info.m_cameraPos.z};
        const C3Vector moonRelative = {moon.x - info.m_cameraPos.x, moon.y - info.m_cameraPos.y,
                                       moon.z - info.m_cameraPos.z};
        const float sunLength = std::sqrt(sunRelative.x * sunRelative.x + sunRelative.y * sunRelative.y +
                                          sunRelative.z * sunRelative.z);
        const float moonLength = std::sqrt(moonRelative.x * moonRelative.x + moonRelative.y * moonRelative.y +
                                           moonRelative.z * moonRelative.z);

        if (sunLength > 1.0e-6f) {
            out->sunElevation = sunRelative.z / sunLength;
        }

        if (moonLength > 1.0e-6f) {
            out->moonElevation = moonRelative.z / moonLength;
        }

        const bool moonLight = day < 0.20139f || day > 0.92361f;
        out->lightIsMoon = moonLight;

        const C3Vector& light = moonLight ? moonRelative : sunRelative;
        const float lightLength = moonLight ? moonLength : sunLength;

        if (lightLength > 1.0e-6f) {
            out->toLight[0] = light.x / lightLength;
            out->toLight[1] = light.y / lightLength;
            out->toLight[2] = light.z / lightLength;
        }

        out->unitCount = 0;
        out->corpseCount = 0;
        ReadPlayer(*out);
        ReadWorldUnits(*out);
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
        Host::ClockState state = {};
        state.valid = true;
        state.minute = g_clientGameTime.GetHourAndMinutes();
        state.speed = g_clientGameTime.m_minutesPerSecond;
        return state;
    }

    bool SetClock(void*, int minute, float speed) {
        WowTime time = g_clientGameTime;
        time.SetHourAndMinutes(minute);
        g_clientGameTime.GameTimeSetTime(time, true);
        g_clientGameTime.GameTimeSetMinutesPerSecond(std::clamp(speed, 1.0f / 60.0f, 60.0f));
        return true;
    }

    Host::WeatherState ReadWeather(void*) {
        Weather* weather = World::s_weather;

        if (!weather) {
            return Host::WeatherState{false, 0, 0.0f, 0.0f};
        }

        return Host::WeatherState{true, static_cast<int>(weather->GetType()), weather->m_targetIntensity,
                                 weather->m_intensity};
    }

    bool SetWeather(void*, int type, float grade, bool instant) {
        Weather* weather = World::s_weather;

        if (!weather) {
            return false;
        }

        weather->SetType(static_cast<uint32_t>(std::clamp(type, 0, 3)), std::clamp(grade, 0.0f, 1.0f), nullptr,
                         instant, 1.0f);
        return true;
    }

    int AreaId(void*) {
        return CGGameUI::m_subzoneID;
    }

    void RefreshFarClip(void*) {}

    void SetFarClip(void*, float setting) {
        char text[32];
        snprintf(text, sizeof(text), "%.1f", setting);

        if (CVar* cvar = CVar::Lookup("farclip")) {
            cvar->Set(text, true, false, false, true);
        }

        World::SetFarClip(setting);
    }

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
        static void OnOpaqueDone() {
            float view[16];
            float proj[16];
            D3DVIEWPORT9 viewport;

            s_opaqueValid = GxMatrices(view, proj) && OpaqueViewport(nullptr, &viewport);

            if (s_opaqueValid) {
                std::memcpy(s_opaqueView, view, sizeof(s_opaqueView));
                std::memcpy(s_opaqueProj, proj, sizeof(s_opaqueProj));
                s_opaqueViewport = viewport;
            }

            if (s_opaqueDone) s_opaqueDone();
        }
        static void OnLiquidBegin() { if (s_liquidBegin) s_liquidBegin(); }
        static void OnLiquidEnd() { if (s_liquidEnd) s_liquidEnd(); }
        static void OnLiquidInstancesDone() { if (s_liquidInstancesDone) s_liquidInstancesDone(); }
        static void OnLiquidMaterial(int index, int begin) { if (s_liquidMaterial) s_liquidMaterial(index, begin); }
        static float OnFarClipClamp(float requested, int mapId, float clamped) {
            return s_farClipLift ? s_farClipLift(requested, mapId, clamped) : clamped;
        }
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
    s_liquidInstancesDone = reinterpret_cast<EventFn>(GetProcAddress(module, "vf_on_liquid_instances_done"));
    s_liquidMaterial = reinterpret_cast<LiquidMaterialFn>(GetProcAddress(module, "vf_on_liquid_material"));
    s_farClipLift = reinterpret_cast<FarClipFn>(GetProcAddress(module, "vf_far_clip_lift"));
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
    s_host.setFarClip = SetFarClip;
    setHost(&s_host);
    init();

    s_callbacks.frameBegin = &LumenveilHost::OnFrameBegin;
    s_callbacks.frameEnd = &LumenveilHost::OnFrameEnd;
    s_callbacks.opaqueBegin = &LumenveilHost::OnOpaqueBegin;
    s_callbacks.opaqueDone = &LumenveilHost::OnOpaqueDone;
    s_callbacks.liquidBegin = &LumenveilHost::OnLiquidBegin;
    s_callbacks.liquidEnd = &LumenveilHost::OnLiquidEnd;
    s_callbacks.liquidInstancesDone = &LumenveilHost::OnLiquidInstancesDone;
    s_callbacks.liquidMaterial = &LumenveilHost::OnLiquidMaterial;
    s_callbacks.farClipClamp = &LumenveilHost::OnFarClipClamp;
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






