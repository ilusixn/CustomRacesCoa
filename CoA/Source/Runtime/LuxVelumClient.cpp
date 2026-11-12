// LuxVelum64.dll host: loads the dll beside the exe and hands it the client's state (docs/2026-11-12-luxvelum-host.md).
#include "CoA/Source/Runtime/LuxVelum.h"
#include "CoA/Source/Modules/Graphics.h"
#include "CoA/Source/Runtime/Log.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/FFX/EffectGlow.h"
#include "Engine/Source/FFX/FFX.h"
#include "Engine/Source/FFX/PassGlow.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxDeviceD3d/CGxDeviceD3d.h"
#include "Engine/Source/Gx/LuxVelum.h"
#include "Engine/Source/Model2/M2Light.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "WoW/Source/DB/Rec/LiquidTypeRec.h"
#include "WoW/Source/DB/WowClientDB.h"
#include "WoW/Source/LiquidSystem/Liquid.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include "WoW/Source/WorldClient/DayNight.h"
#include "WoW/Source/WorldClient/DayNight__DNInfo.h"
#include "WoW/Source/WorldClient/MapEntity.h"
#include "WoW/Source/WorldClient/MapLoad.h"
#include "WoW/Source/WorldClient/MapObj.h"
#include "WoW/Source/WorldClient/MapObjDef.h"
#include "WoW/Source/WorldClient/MapObjGroup.h"
#include "WoW/Source/WorldClient/World.h"
#include <windows.h>
#include <shellapi.h>
#include <cstring>
#include <cwctype>
#include <string>

int32_t LoadingScreenMapId();

namespace {
    LuxHostApi s_host;

    uint32_t Packed(const CImVector& color) {
        uint32_t value;
        std::memcpy(&value, &color, sizeof(value));
        return value;
    }

    std::wstring ExeDir() {
        wchar_t path[MAX_PATH];
        DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring dir(path, n);
        size_t slash = dir.find_last_of(L"\\/");
        return slash == std::wstring::npos ? std::wstring(L".") : dir.substr(0, slash);
    }

    bool NoLuxVelumArg() {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

        if (!argv) {
            return false;
        }

        bool found = false;

        for (int i = 1; i < argc && !found; ++i) {
            const wchar_t* arg = argv[i];

            if (*arg != L'-' && *arg != L'/') {
                continue;
            }

            ++arg;

            if (*arg == L'-') {
                ++arg;
            }

            found = _wcsicmp(arg, L"noluxvelum") == 0;
        }

        LocalFree(argv);
        return found;
    }

    void* D3dDevice() {
        CGxDevice* device = g_theGxDevicePtr;

        if (!device || device->m_api != GxApi_D3d9) {
            return nullptr;
        }

        return static_cast<CGxDeviceD3d*>(device)->m_d3dDevice;
    }

    int32_t GxMatrices(float* view, float* projection) {
        CGxDevice* device = g_theGxDevicePtr;

        if (!device) {
            return 0;
        }

        const auto& stack = device->m_xforms[GxXform_View];

        if (stack.m_level >= 4) {
            return 0;
        }

        std::memcpy(view, &stack.m_mtx[stack.m_level], 16 * sizeof(float));
        std::memcpy(projection, &device->m_projection, 16 * sizeof(float));
        return 1;
    }

    int32_t GxViewportDepthRange(float* range) {
        CGxDevice* device = g_theGxDevicePtr;

        if (!device) {
            return 0;
        }

        range[0] = device->m_viewport.m_z.m_l;
        range[1] = device->m_viewport.m_z.m_h;
        return 1;
    }

    void Camera(float* position, float* target) {
        position[0] = CWorldScene::s_activeWorldView.x;
        position[1] = CWorldScene::s_activeWorldView.y;
        position[2] = CWorldScene::s_activeWorldView.z;
        target[0] = CWorldScene::camTarget.x;
        target[1] = CWorldScene::camTarget.y;
        target[2] = CWorldScene::camTarget.z;
    }

    int32_t CameraLiquid() {
        return static_cast<int32_t>(s_liquidTypeId);
    }

    int32_t CurrentMap() {
        return LoadingScreenMapId();
    }

    float WorldFarClip() {
        return World::s_farClip;
    }

    void DayNightInputs(LuxDayNight* out) {
        std::memset(out, 0, sizeof(*out));
        const DayNight::DNInfo& info = DayNight::g_dnInfo;
        const float day = CoA::Graphics::DayProgress(info.m_dayProgression);

        out->dayFraction = day;
        // Night hours as the stock sky draws the moon.
        out->lightIsMoon = (day < 0.20139f || day > 0.92361f) ? 1 : 0;
        out->skyCentre[0] = info.m_cameraPos.x;
        out->skyCentre[1] = info.m_cameraPos.y;
        out->skyCentre[2] = info.m_cameraPos.z;
        out->sunPosition[0] = DayNight::g_planets.m_sun.m_position.x;
        out->sunPosition[1] = DayNight::g_planets.m_sun.m_position.y;
        out->sunPosition[2] = DayNight::g_planets.m_sun.m_position.z;
        out->moonPosition[0] = DayNight::g_planets.m_moon.m_position.x;
        out->moonPosition[1] = DayNight::g_planets.m_moon.m_position.y;
        out->moonPosition[2] = DayNight::g_planets.m_moon.m_position.z;
        out->fogColor = Packed(info.m_fogInterior.m_color);
        out->fogStart[0] = info.m_fog.m_start;
        out->fogStart[1] = info.m_fogInterior.m_start;
        out->fogEnd[0] = info.m_fog.m_end;
        out->fogEnd[1] = info.m_fogInterior.m_end;
        out->ambientColor = Packed(info.m_bands.m_ambient);
        out->directColor = Packed(info.m_bands.m_diffuse);
        out->sunColor = Packed(info.m_bands.m_sunCloudColors[0]);
        out->zoneFogDistance = info.m_bands.m_fogEnd;
        out->glow = info.m_bands.m_glow;
        out->stormBlend = info.m_stormBlend;
        out->screenEffectLightSlot = static_cast<int32_t>(info.m_overrideLightParamsId);
        out->interiorFogBlend = info.m_interiorFogBlend;

        for (int i = 0; i < 6; ++i) {
            out->skyColors[i] = Packed(info.m_bands.m_skyColors[i]);
        }

        out->oceanColors[0] = Packed(info.m_bands.m_waterColors[0]);
        out->oceanColors[1] = Packed(info.m_bands.m_waterColors[1]);
        out->riverColors[0] = Packed(info.m_bands.m_waterColors[2]);
        out->riverColors[1] = Packed(info.m_bands.m_waterColors[3]);
    }

    void ReadStockFog(float* start, float* end) {
        start[0] = DayNight::g_dnInfo.m_fog.m_start;
        end[0] = DayNight::g_dnInfo.m_fog.m_end;
        start[1] = DayNight::g_dnInfo.m_fogInterior.m_start;
        end[1] = DayNight::g_dnInfo.m_fogInterior.m_end;
    }

    void WriteStockFog(const float* start, const float* end) {
        DayNight::g_dnInfo.m_fog.m_start = start[0];
        DayNight::g_dnInfo.m_fog.m_end = end[0];
        DayNight::g_dnInfo.m_fogInterior.m_start = start[1];
        DayNight::g_dnInfo.m_fogInterior.m_end = end[1];
    }

    void ScreenEffects(LuxScreenEffects* out) {
        FFX::Effect* current = FFX::s_effect;
        out->currentEnabled = 0;
        out->current = reinterpret_cast<uint64_t>(current);
        out->glow = reinterpret_cast<uint64_t>(CGWorldFrame::ffxEffectGlow);
        out->death = reinterpret_cast<uint64_t>(CGWorldFrame::ffxEffectDeath);
        out->ffxEnabled = FFX::s_cvarFFX && FFX::s_cvarFFX->m_intValue != 0 ? 1 : 0;

        if (current && current->m_cvar && current->m_cvar->m_intValue != 0) {
            out->currentEnabled = 1;
        }
    }

    int32_t GlowColourAlpha(uint8_t** out) {
        FFX::Effect* effect = FFX::s_effect;

        if (!effect || effect != CGWorldFrame::ffxEffectGlow) {
            return 0;
        }

        auto* glow = static_cast<EffectGlow*>(effect);

        if (glow->m_passes.Count() <= 2 || glow->m_altPasses.Count() <= 2) {
            return 0;
        }

        auto* pass = static_cast<PassGlowBase*>(glow->m_passes[2]);
        auto* alt = static_cast<PassGlowBase*>(glow->m_altPasses[2]);

        if (!pass || !alt) {
            return 0;
        }

        out[0] = &pass->m_color.a;
        out[1] = &alt->m_color.a;
        return 1;
    }

    int32_t EnumPointLights(int32_t (*callback)(void*, const LuxPointLight*), void* context) {
        CM2Scene* scene = World::s_m2Scene;

        if (!scene || !scene->m_lightGrid) {
            return 0;
        }

        for (uint32_t cell = 0; cell < 64 * 64; ++cell) {
            for (CM2Light* light = scene->m_lightGrid[cell]; light; light = light->m_lightNext) {
                LuxPointLight entry;
                entry.position[0] = light->m_pos.x;
                entry.position[1] = light->m_pos.y;
                entry.position[2] = light->m_pos.z;
                entry.color[0] = light->m_dirColor.x;
                entry.color[1] = light->m_dirColor.y;
                entry.color[2] = light->m_dirColor.z;
                entry.attenuation[0] = light->m_constantAttenuation;
                entry.attenuation[1] = light->m_linearAttenuation;
                entry.attenuation[2] = light->m_quadraticAttenuation;
                entry.type = static_cast<uint32_t>(light->m_type);
                entry.enabled = light->m_visible;
                entry.id = reinterpret_cast<uint64_t>(light);

                if (!callback(context, &entry)) {
                    return 1;
                }
            }
        }

        return 0;
    }

    int32_t CameraInterior(uint32_t* interior) {
        *interior = 0;
        CMapObjDef* def = CWorldScene::s_viewerMapObjDef;
        const uint32_t count = CWorldScene::s_viewerMapObjGroups.Count();

        if (!def || !count) {
            return 1;
        }

        if (count > 64) {
            return 0;
        }

        CMapObj* mapObj = def->m_owner;

        if (!mapObj || !mapObj->m_isGroupLoaded) {
            return 0;
        }

        const uint32_t groupCount = static_cast<uint32_t>(mapObj->m_mapObjGroupCount);

        if (groupCount - 1 > 0x1FF) {
            return 0;
        }

        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t index = CWorldScene::s_viewerMapObjGroups[i];

            if (index >= groupCount) {
                return 0;
            }

            CMapObjGroup* group = mapObj->m_mapObjGroupArray[index];

            if (!group) {
                return 0;
            }

            // Loaded group that is neither exterior-lit nor outdoor.
            if ((group->m_unkLoadedFlag & 1) && !(group->m_flags & 0x48)) {
                *interior = 1;
            }
        }

        return 1;
    }

    int32_t DescribeLiquid(const void* settings, LuxLiquidInfo* out) {
        std::memset(out, 0, sizeof(*out));

        if (!settings) {
            return 0;
        }

        auto& bank = Liquid::CSettingsBank::s_settings;

        for (uint32_t i = 0; i < bank.Count(); ++i) {
            if (bank[i] == settings) {
                out->inBank = 1;
                out->id = i;
                break;
            }
        }

        if (!out->inBank) {
            return 1;
        }

        const LiquidTypeRec* rec = Liquid::s_liquidTypeDB ? Liquid::s_liquidTypeDB->GetRecord(static_cast<int32_t>(out->id)) : nullptr;

        if (!rec) {
            return 1;
        }

        out->hasRow = 1;
        out->flags = static_cast<uint32_t>(rec->m_flags);
        out->soundBank = static_cast<uint32_t>(rec->m_soundBank);
        out->materialId = static_cast<uint32_t>(rec->m_materialID);

        const char* texture = static_cast<const Liquid::CMaterialSettings*>(settings)->m_textureNames[0];

        if (std::memchr(texture, 0, sizeof(out->texture0))) {
            std::memcpy(out->texture0, texture, sizeof(out->texture0));
        }

        return 1;
    }

    int32_t EnumWaterUnits(int32_t (*callback)(void*, const LuxWaterContact*), void* context) {
        ObjectMgr* mgr = ClntObjMgrGetCurrent();

        if (!mgr) {
            return 1;
        }

        for (CGObject_C* object = mgr->m_visibleObjects.Head(); object; object = mgr->m_visibleObjects.Next(object)) {
            CGObjectData* data = object->m_obj;

            if (!data || !(static_cast<uint32_t>(data->m_type) & TYPE_UNIT)) {
                continue;
            }

            CMapEntity* entity = object->m_worldObject;

            // In liquid and touching its surface.
            if (!entity || (entity->m_staticFlags & 0x120) != 0x120) {
                continue;
            }

            auto* unit = static_cast<CGUnit_C*>(object);
            const CMovement_C& move = unit->m_movementData;
            const bool transported = static_cast<bool>(move.m_transportGuid);
            const C3Vector& position = transported ? entity->m_position : move.m_position;

            LuxWaterContact contact;
            contact.guid = data->m_guid.m_guid;
            contact.position[0] = position.x;
            contact.position[1] = position.y;
            contact.position[2] = position.z;
            contact.surface = entity->m_liquidHeight;
            contact.radius = move.m_collisionRadius;
            contact.height = move.m_collisionHeight;
            contact.swimming = (move.m_flags >> 21) & 1;
            contact.onTransport = transported ? 1 : 0;

            if (!callback(context, &contact)) {
                break;
            }
        }

        return 0;
    }

    void RenderGlares() {
        DayNight::RenderGlares();
    }
}

void CoA::LuxVelum::Load() {
    if (NoLuxVelumArg()) {
        CoA::Log::Printf("LuxVelum: skipped (-noluxvelum)");
        return;
    }

    const std::wstring path = ExeDir() + L"\\LuxVelum64.dll";

    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return;
    }

    HMODULE module = LoadLibraryW(path.c_str());

    if (!module) {
        CoA::Log::Printf("LuxVelum: LuxVelum64.dll not loaded (error %u)", static_cast<unsigned>(GetLastError()));
        return;
    }

    using AttachProc = LuxCallbacks* (*)(const LuxHostApi*);
    auto attach = reinterpret_cast<AttachProc>(GetProcAddress(module, "LuxVelum_Attach"));

    if (!attach) {
        CoA::Log::Printf("LuxVelum: LuxVelum64.dll has no LuxVelum_Attach export; unloaded");
        FreeLibrary(module);
        return;
    }

    s_host.size = sizeof(LuxHostApi);
    s_host.version = kLuxApiVersion;
    s_host.hostName = "CoA64";
    s_host.d3dDevice = D3dDevice;
    s_host.gxMatrices = GxMatrices;
    s_host.gxViewportDepthRange = GxViewportDepthRange;
    s_host.camera = Camera;
    s_host.cameraLiquid = CameraLiquid;
    s_host.currentMap = CurrentMap;
    s_host.worldFarClip = WorldFarClip;
    s_host.dayNight = DayNightInputs;
    s_host.readStockFog = ReadStockFog;
    s_host.writeStockFog = WriteStockFog;
    s_host.screenEffects = ScreenEffects;
    s_host.glowColourAlpha = GlowColourAlpha;
    s_host.enumPointLights = EnumPointLights;
    s_host.cameraInterior = CameraInterior;
    s_host.describeLiquid = DescribeLiquid;
    s_host.enumWaterUnits = EnumWaterUnits;
    s_host.waterRipplesValue = &CWorld::s_waterRipples;
    s_host.renderGlares = RenderGlares;

    LuxCallbacks* callbacks = attach(&s_host);

    if (callbacks && callbacks->version == kLuxApiVersion && callbacks->size >= sizeof(LuxCallbacks)) {
        g_luxVelum = callbacks;
        CoA::Log::Printf("LuxVelum: LuxVelum64.dll attached");
    } else {
        CoA::Log::Printf("LuxVelum: LuxVelum64.dll stayed detached (different API version)");
    }
}
