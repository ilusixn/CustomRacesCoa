// AscGraphics.cpp's client half: the Engine hook pointers (CM2Light, CM2Scene::SelectLights), the client state the CVar
// callbacks and lifecycle write (CWorld::enables, DNInfo's fog, the shadow mode, the far clip), the antiportal table and
// the occlusion overlay's draws.
#include "CoA/Source/Modules/Graphics.h"
#include "CoA/Source/Modules/CrashContext.h"
#include "CoA/Source/Modules/Modules.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "Engine/Source/Model2/M2Light.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/ShaderEffect/ShadowCache.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/CImVector.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include "WoW/Source/UIUtil/Camera.h"
#include "WoW/Source/WorldClient/CWorldOcclusion.h"
#include "WoW/Source/WorldClient/DayNight.h"
#include "WoW/Source/WorldClient/DayNight__DNInfo.h"
#include "WoW/Source/WorldClient/Map.h"
#include "WoW/Source/WorldClient/Types.h"
#include "WoW/Source/WorldClient/World.h"
#include "WoW/Source/WorldClient/WorldParam.h"
#include <cstring>
#include <vector>

namespace {
    // The DLL's antiportal records (DAT_10be2748) in the client's layout, and their points (DAT_10be2754).
    std::vector<OcclusionVolume> s_volumes;
    std::vector<std::vector<C3Vector>> s_points;

    bool InWorld() {   // 0xBD0792
        return CGGameUI::s_inWorld != 0;
    }

    void SetAttenuation(void* light, float linear, float quadratic) {   // +0x58 / +0x5C
        CM2Light* l = static_cast<CM2Light*>(light);
        l->m_linearAttenuation = linear;
        l->m_quadraticAttenuation = quadratic;
    }

    void SetCullingEnable(bool on) {   // 0xCD774C bit 0x20
        if (on) {
            CWorld::enables |= Enable_Culling;
        } else {
            CWorld::enables &= ~static_cast<uint32_t>(Enable_Culling);
        }
    }

    void SetFogDensity(float density) {   // 0xD38B98
        DayNight::g_dnInfo.m_fog.m_density = density;
    }

    void SetFogEnd(float end) {   // 0xD38C1C
        DayNight::g_dnInfo.m_bands.m_fogEnd = end;
    }

    uint32_t MapId() {   // 0xBD088C
        return static_cast<uint32_t>(CGGameUI::m_iCurrentMapID);
    }

    CVar* ShadowCVar() {   // *0xCD85DC
        return CWorldParam::cvar_extShadowQuality;
    }

    void SetShadowMode(int32_t mode) {   // 0x874210
        CShadowCache::SetShadowMode(static_cast<EShadowMode>(mode));
    }

    void SetFarClip(float farClip) {   // 0x780800
        World::SetFarClip(farClip);
    }

    void UseOccluders(const std::vector<CoA::Graphics::Occluder>& occluders) {
        s_points.clear();
        s_points.reserve(occluders.size());
        s_volumes.clear();
        s_volumes.reserve(occluders.size());

        for (const CoA::Graphics::Occluder& o : occluders) {
            s_points.emplace_back();
            std::vector<C3Vector>& pts = s_points.back();

            for (size_t i = 0; i + 2 < o.points.size(); i += 3) {
                pts.push_back(C3Vector(o.points[i], o.points[i + 1], o.points[i + 2]));
            }

            OcclusionVolume v = {};
            v.m_mapID = static_cast<int32_t>(o.mapId);
            v.m_flags = o.flags;
            v.m_data.m_bounds.b = C3Vector(o.min[0], o.min[1], o.min[2]);
            v.m_data.m_bounds.t = C3Vector(o.max[0], o.max[1], o.max[2]);
            v.m_data.m_positions = pts.data();
            v.m_data.m_positionCount = static_cast<uint32_t>(pts.size());
            s_volumes.push_back(v);
        }

        // The patched operands (0x7CCD30 / 0x7CCD35 / 0x7CD8A0 / 0x7CD8A5), 0xD2DCD4 = 0, then 0x7CCD20.
        CWorldOcclusion_UseVolumes(s_volumes.data(), static_cast<uint32_t>(s_volumes.size()));
    }

    uint32_t CurrentMap() {   // 0xADFBC4
        return static_cast<uint32_t>(s_mapId);
    }

    // FUN_10a3aac0: world position relative to the active camera (0x4F5960, its position). Without a camera the
    // original raises the client's fatal error (FUN_10112f70 -> 0x8C51D0).
    C3Vector ToDrawPosition(const float p[3]) {
        CGCamera* camera = CGWorldFrame::GetActiveCamera();

        if (!camera) {
            CoA::CrashContext::Assert("WorldRender::ToDrawPosition requires an active camera", nullptr, 0);
            return C3Vector(0.0f, 0.0f, 0.0f);
        }

        const C3Vector& c = camera->m_position;
        return C3Vector(p[0] - c.x, p[1] - c.y, p[2] - c.z);
    }

    // The render state both draws set (0x409670 push, then 0x408240 / 0x408BF0); `depthTest` is state 0xD.
    void OverlayState(int32_t depthTest) {
        g_theGxDevicePtr->RsPush();
        GxRsSet(GxRs_VertexShader, static_cast<void*>(nullptr));
        GxRsSet(GxRs_PixelShader, static_cast<void*>(nullptr));
        GxRsSet(GxRs_Texture0, static_cast<void*>(nullptr));
        GxRsSet(GxRs_Culling, 0);
        GxRsSet(GxRs_Lighting, 0);
        GxRsSet(GxRs_Fog, 0);
        GxRsSet(GxRs_BlendingMode, 2);
        GxRsSet(static_cast<EGxRenderState>(7), 0);
        GxRsSet(GxRs_DepthTest, depthTest);
        GxRsSet(GxRs_DepthWrite, 0);
    }

    // The fill (FUN_10266560's first draw): triangles, depth test off.
    void DrawFill(const CoA::Graphics::Occluder& o, const std::vector<uint32_t>& triangles, uint32_t color) {
        std::vector<C3Vector> draw;
        draw.reserve(o.points.size() / 3);

        for (size_t i = 0; i + 2 < o.points.size(); i += 3) {
            draw.push_back(ToDrawPosition(&o.points[i]));
        }

        std::vector<uint16_t> indices;
        indices.reserve(triangles.size());

        for (uint32_t t : triangles) {
            indices.push_back(static_cast<uint16_t>(t));
        }

        OverlayState(0);
        CImVector c;
        memcpy(&c, &color, 4);
        GxPrimLockVertexPtrs(static_cast<uint32_t>(draw.size()), draw.data(), sizeof(C3Vector), nullptr, 0, &c, 0, nullptr, 0, nullptr, 0, nullptr, 0);
        GxDrawLockedElements(GxPrim_Triangles, static_cast<uint32_t>(indices.size()), indices.data());
        GxPrimUnlockVertexPtrs();
        g_theGxDevicePtr->RsPop();
    }

    // FUN_10a3a900: one outline segment, depth test on.
    void DrawLine(const float a[3], const float b[3], uint32_t color) {
        const C3Vector v[2] = { ToDrawPosition(a), ToDrawPosition(b) };
        const uint16_t indices[2] = { 0, 1 };
        OverlayState(1);
        CImVector c;
        memcpy(&c, &color, 4);
        GxPrimLockVertexPtrs(2, v, sizeof(C3Vector), nullptr, 0, &c, 0, nullptr, 0, nullptr, 0, nullptr, 0);
        GxDrawLockedElements(GxPrim_Lines, 2, indices);
        GxPrimUnlockVertexPtrs();
        g_theGxDevicePtr->RsPop();
    }

    void LightCreated(CM2Light* light) {
        CoA::Graphics::LightCreated(light);
    }

    void LightUnlinked(CM2Light* light) {
        CoA::Graphics::LightUnlinked(light);
    }
}

void CoA::GraphicsClient::Init() {
    CoA::Graphics::Deps& d = CoA::Graphics::Dependencies();
    d.inWorld = InWorld;
    d.setAttenuation = SetAttenuation;
    d.setCullingEnable = SetCullingEnable;
    d.setFogDensity = SetFogDensity;
    d.setFogEnd = SetFogEnd;
    d.mapId = MapId;
    d.shadowCVar = ShadowCVar;
    d.setShadowMode = SetShadowMode;
    d.setFarClip = SetFarClip;
    d.useOccluders = UseOccluders;
    d.currentMap = CurrentMap;
    d.drawFill = DrawFill;
    d.drawLine = DrawLine;

    // AscRuntime::Detour(0x834A40 / 0x834AB0) and the 0x81E548 rewrite, as Engine hook pointers.
    g_m2LightCreatedHook = LightCreated;
    g_m2LightUnlinkHook = LightUnlinked;
    g_m2LightStampCullingHook = CoA::Graphics::LightStampCulling;
}
