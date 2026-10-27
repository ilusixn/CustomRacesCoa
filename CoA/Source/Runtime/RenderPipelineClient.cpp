// RenderPipeline's engine half: the g_renderPipelineHook callbacks (CGWorldFrame::OnWorldRender begin/end,
// CGCamera::SetupWorldProjection), the activation rules' inputs and the device's world target. The hook is installed
// only while coaRenderPipeline is 1, so at 0 the engine runs its original paths. Not part of the original (CoA64
// addition).
#include "CoA/Shaders/DepthViewPS.h"
#include "CoA/Shaders/FullscreenVS.h"
#include "CoA/Source/Runtime/RenderMotion.h"
#include "CoA/Source/Runtime/RenderPipeline.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/Log.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/ShaderEffect/ShaderEffect.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Tempest/CRect.h"
#include "WoW/Source/WorldClient/World.h"
#include <d3d9.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

static_assert(sizeof(C44Matrix) == sizeof(CoA::RenderPipeline::Mat4), "C44Matrix and Mat4 must share a layout");

namespace {
    using namespace CoA::RenderPipeline;

    // Not an Inactive value: the device refused WorldTargetBegin.
    constexpr int kBeginFailed = 100;
    // Not an Inactive value: the device is lost (no device context), so no world target is attempted.
    constexpr int kDeviceLost = 101;

    // The last logged state (an Inactive value or kBeginFailed); -1 until the first world frame.
    int s_logged = -1;
    // The world target is begun for the current world render (between beginWorld and endWorld).
    bool s_begun;
    // WorldTargetBegin failed for s_width x s_height: not retried until the size changes or the device is reset (the
    // world target release hook runs before every reset).
    bool s_beginFailed;
    // The failed copy-back was logged (cleared by the next good one).
    bool s_copyFailLogged;
    // The world target was begun at the last world render (a temporal-history reset follows when it was not).
    bool s_wasActive;
    // The device may hold world textures (released when the pipeline turns off).
    bool s_targetHeld;
    // Device frame (CGxDevice::m_frameCount) and size of the last active world render.
    int32_t s_lastFrame;
    uint32_t s_width;
    uint32_t s_height;
    // The camera of the latest SetupWorldProjection (hit tests call it too, so the last call before the world
    // render is the frame's camera); committed with BeginFrame when the world target begins.
    Camera s_pending;
    bool s_havePending;

    // The depth debug view's D3D9 objects, created lazily on first use and released with the world target (device
    // reset / destroy through g_worldTargetReleaseHook, pipeline off through Deactivate).
    IDirect3DVertexShader9* s_debugVS;
    IDirect3DPixelShader9* s_debugPS;
    IDirect3DVertexDeclaration9* s_debugDecl;
    IDirect3DStateBlock9* s_debugBlock;
    // Creation failed (logged once): coaRenderDebug falls back to the normal image until the objects are released.
    bool s_debugFailed;

    void ReleaseDebug() {
        IUnknown* objects[] = { s_debugVS, s_debugPS, s_debugDecl, s_debugBlock };

        for (IUnknown* object : objects) {
            if (object) {
                object->Release();
            }
        }

        s_debugVS = nullptr;
        s_debugPS = nullptr;
        s_debugDecl = nullptr;
        s_debugBlock = nullptr;
        s_debugFailed = false;
    }

    // Draws the world's INTZ depth as greyscale over the full target rect, leaving every D3D9 state as it was. Runs
    // after WorldTargetEnd, so the original back buffer and depth surface are bound again. False: nothing drawn.
    bool DrawDepthView(CGxDevice* gx) {
        IDirect3DDevice9* device = static_cast<IDirect3DDevice9*>(gx->D3dDevice());
        IDirect3DBaseTexture9* depth = static_cast<IDirect3DBaseTexture9*>(gx->WorldDepthTexture());

        if (!device || !depth || s_debugFailed) {
            return false;
        }

        if (!s_debugBlock) {
            const D3DVERTEXELEMENT9 elements[] = {
                { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
                { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
                D3DDECL_END()
            };

            if (FAILED(device->CreateVertexShader(reinterpret_cast<const DWORD*>(kFullscreenVS), &s_debugVS))
                || FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(kDepthViewPS), &s_debugPS))
                || FAILED(device->CreateVertexDeclaration(elements, &s_debugDecl))
                || FAILED(device->CreateStateBlock(D3DSBT_ALL, &s_debugBlock))) {
                ReleaseDebug();
                s_debugFailed = true;
                CoA::Log::Printf("RenderPipeline: depth debug view unavailable (D3D9 object creation failed)");
                return false;
            }
        }

        // Everything the draw touches is restored by the state block (textures, shaders, stream, render states,
        // samplers, viewport, scissor); render targets are not part of it and are not changed.
        if (FAILED(s_debugBlock->Capture())) {
            return false;
        }

        // The quad covers the bound render target (the back buffer) exactly, so uv runs 0..1 over what is visible; the
        // world target's size is the fallback. With both the same size the point sampling hits texel centres.
        uint32_t targetWidth = s_width;
        uint32_t targetHeight = s_height;
        IDirect3DSurface9* target = nullptr;

        if (SUCCEEDED(device->GetRenderTarget(0, &target)) && target) {
            D3DSURFACE_DESC desc;

            if (SUCCEEDED(target->GetDesc(&desc)) && desc.Width && desc.Height) {
                targetWidth = desc.Width;
                targetHeight = desc.Height;
            }

            target->Release();
        }

        const float w = static_cast<float>(targetWidth);
        const float h = static_cast<float>(targetHeight);
        D3DVIEWPORT9 viewport = { 0, 0, targetWidth, targetHeight, 0.0f, 1.0f };
        // Clip-space strip with the D3D9 half-pixel offset applied by the vertex shader (c0).
        const float offset[4] = { -1.0f / w, 1.0f / h, 0.0f, 0.0f };
        const float quad[4][6] = {
            { -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f },
            { 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f },
            { -1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f },
            { 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f }
        };

        device->SetViewport(&viewport);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
        device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetRenderState(D3DRS_WRAP0, 0);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);
        device->SetVertexDeclaration(s_debugDecl);
        device->SetVertexShader(s_debugVS);
        device->SetPixelShader(s_debugPS);
        device->SetVertexShaderConstantF(0, offset, 1);
        // Explicitly not fetch4 (D3DSAMP_MIPMAPLODBIAS == 'GET4' enables it in DXVK), whatever the engine left on sampler 0.
        device->SetSamplerState(0, D3DSAMP_MIPMAPLODBIAS, 0);
        device->SetSamplerState(0, D3DSAMP_MAXMIPLEVEL, 0);
        device->SetSamplerState(0, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);
        // c0.x: 1 = depth view, 2 = diagnostic (uv gradient, blue where the depth is still at the clear value).
        // c1 = (viewport minZ, 1 / (maxZ - minZ)): the Z buffer holds minZ + ndc * (maxZ - minZ). c2 = (near * far, far,
        // far - near, 1 / 16) linearises the NDC depth to yards for the greyscale 1 / (1 + yards / 16).
        const Frame& frame = Current();
        const float range = frame.depthMax - frame.depthMin;
        const float nearZ = frame.cur.nearZ;
        const float farZ = frame.cur.farZ;
        const float depthConstants[8] = { frame.depthMin, range > 1e-6f ? 1.0f / range : 1.0f, 0.0f, 0.0f, nearZ * farZ, farZ, farZ - nearZ, 1.0f / 16.0f };
        const float mode[4] = { (*CoA::RenderPipelineClient::DebugSlot())->m_intValue == 2 ? 2.0f : 1.0f, 0.0f, 0.0f, 0.0f };
        device->SetPixelShaderConstantF(0, mode, 1);
        device->SetPixelShaderConstantF(1, depthConstants, 2);
        device->SetTexture(0, depth);
        device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));

        // Apply puts back whatever was bound at stage 0 when captured (the INTZ texture never was, so it is unbound
        // again here); the explicit check keeps the device from ever being left holding it, which would fail the next
        // Begin / Reset.
        s_debugBlock->Apply();

        IDirect3DBaseTexture9* bound = nullptr;

        if (SUCCEEDED(device->GetTexture(0, &bound)) && bound) {
            if (bound == depth) {
                device->SetTexture(0, nullptr);
            }

            bound->Release();
        }

        return true;
    }

    bool CVarOn(CVar** slot) {
        return *slot && (*slot)->m_intValue != 0;
    }

    void LogState(int state) {
        if (state == s_logged) {
            return;
        }

        s_logged = state;

        switch (state) {
            case static_cast<int>(Inactive::None):
                CoA::Log::Printf("RenderPipeline: active (%ux%u)", s_width, s_height);
                break;
            case static_cast<int>(Inactive::CVarOff):
                CoA::Log::Printf("RenderPipeline: inactive: cvar off");
                break;
            case static_cast<int>(Inactive::Multisample):
                CoA::Log::Printf("RenderPipeline: inactive: multisample");
                break;
            case static_cast<int>(Inactive::NoIntz):
                CoA::Log::Printf("RenderPipeline: inactive: no INTZ / 24-bit depth");
                break;
            case static_cast<int>(Inactive::NoShaders):
                CoA::Log::Printf("RenderPipeline: inactive: shaders disabled");
                break;
            case kDeviceLost:
                CoA::Log::Printf("RenderPipeline: inactive: device lost");
                break;
            default:
                CoA::Log::Printf("RenderPipeline: inactive: world target creation failed (%ux%u)", s_width, s_height);
                break;
        }
    }

    void Deactivate(CGxDevice* device) {
        s_wasActive = false;

        if (s_targetHeld) {
            device->WorldTargetRelease();   // also releases the debug view's objects
            s_targetHeld = false;
        }
    }

    // coaRenderPipeline went to 0: frees the world target and removes the hook. Only outside a world render (from
    // EndWorld, or from the CVar callback while not begun).
    void Uninstall() {
        if (g_theGxDevicePtr) {
            Deactivate(g_theGxDevicePtr);
        }

        ReleaseDebug();
        CoA::RenderMotionClient::Release();
        s_beginFailed = false;
        g_worldTargetReleaseHook = nullptr;
        g_renderPipelineHook = nullptr;
        LogState(static_cast<int>(Inactive::CVarOff));
    }

    // The device is releasing its D3DPOOL_DEFAULT resources (reset / destroy): the debug objects go, and a failed world
    // target creation may be tried again on the new device.
    void OnWorldTargetRelease() {
        ReleaseDebug();
        CoA::RenderMotionClient::Release();
        s_beginFailed = false;
    }

    void BeginWorld() {
        CGxDevice* device = g_theGxDevicePtr;

        if (!device) {
            return;
        }

        ActivationInput in;
        in.cvarOn = CVarOn(CoA::RenderPipelineClient::PipelineSlot());
        in.msaaSamples = device->m_format.m_multisampleCount;
        in.intzSupported = in.cvarOn && device->WorldTargetSupported();
        in.shadersEnabled = CShaderEffect::s_enableShaders != 0;
        const Inactive why = WhyInactive(in);

        if (why != Inactive::None) {
            LogState(static_cast<int>(why));
            Deactivate(device);
            return;
        }

        // CoA64: no device context means the device is lost (between a failed Present / Reset and recovery): nothing to
        // create, and not a creation failure either.
        if (!device->m_context) {
            LogState(kDeviceLost);
            s_wasActive = false;
            return;
        }

        // A texture render target bound around the world render: the world target only replaces the default
        // surfaces, so leave this frame alone.
        CGxTex* renderTarget = nullptr;
        device->DeviceGetRenderTarget(GxBuffers_Color, renderTarget);

        if (renderTarget) {
            s_wasActive = false;
            return;
        }

        CRect window = { 0.0f, 0.0f, 0.0f, 0.0f };
        device->CapsWindowSize(window);
        const uint32_t width = static_cast<uint32_t>(std::lround(window.maxX - window.minX));
        const uint32_t height = static_cast<uint32_t>(std::lround(window.maxY - window.minY));

        // Temporal history restarts on the first world frame after a gap in presented frames (a loading screen,
        // entering the world, the pipeline turning on) and when the target size changes.
        const int32_t frame = device->m_frameCount;
        const bool sizeChanged = width != s_width || height != s_height;
        const bool forceReset = !s_wasActive || frame != s_lastFrame + 1 || sizeChanged;
        s_width = width;
        s_height = height;
        s_lastFrame = frame;

        // A creation that failed at this size is not retried every frame (the size changing or a device reset retries).
        if (s_beginFailed && !sizeChanged) {
            Deactivate(device);
            return;
        }

        if (!device->WorldTargetBegin(width, height)) {
            LogState(kBeginFailed);
            Deactivate(device);
            // After Deactivate: its release hook clears s_beginFailed.
            s_beginFailed = true;
            return;
        }

        s_beginFailed = false;

        LogState(static_cast<int>(Inactive::None));
        s_begun = true;
        s_wasActive = true;
        s_targetHeld = true;

        if (!s_havePending) {
            return;
        }

        // The world viewport (WorldFrame::OnWorldRender, right after this) is [the current minZ, CWorld::s_unk_ADEEE4].
        BeginFrame(s_pending, forceReset, device->m_viewport.m_z.m_l, CWorld::s_unk_ADEEE4);

        // Jitter only the world camera of a frame that is rendering into the world target: the device must still hold
        // the projection SetupWorldProjection just set for it (unchanged, so not a hit test's or another camera's).
        // CShaderEffect::UpdateProjMatrix and the world draws run after this, so they all see the jittered one.
        if (!CVarOn(CoA::RenderPipelineClient::JitterSlot()) || memcmp(&device->m_projection, &s_pending.proj, sizeof(s_pending.proj)) != 0) {
            return;
        }

        // Stage 1 renders at display size.
        const uint32_t phases = JitterPhaseCount(width, width);
        float jx = 0.0f;
        float jy = 0.0f;
        Halton23(Current().index % phases + 1, jx, jy);
        const Mat4 jittered = ApplyJitter(s_pending.proj, jx, jy, width, height);
        C44Matrix projection;
        memcpy(&projection, &jittered, sizeof(projection));
        device->XformSetProjection(projection);
        SetJitter(jx, jy);
    }

    void EndWorld(const CRect& /* viewportNDC */) {
        if (s_begun) {
            s_begun = false;

            // coaRenderMotion 1: the motion and mask passes, while the world's colour and depth are still bound.
            CoA::RenderMotionClient::BeforeCopyBack();

            // The whole target, not only the viewport: it started as a copy of the back buffer and the partial-viewport
            // GxSceneClear cleared all of it, so a full copy reproduces the original back buffer exactly.
            CGxDevice* device = g_theGxDevicePtr;
            if (!device->WorldTargetEnd(0, 0, static_cast<int32_t>(s_width), static_cast<int32_t>(s_height))) {
                if (!s_copyFailLogged) {
                    s_copyFailLogged = true;
                    CoA::Log::Printf("RenderPipeline: copy of the world image to the back buffer failed (this frame's world is lost)");
                }
            } else {
                s_copyFailLogged = false;
            }

            // coaRenderDebug 1 / 2: the depth view is drawn over the copied world (the same full rect), so a failure
            // leaves the normal image; 3 / 4: the motion / translucency mask view, 5: the motion view with the model pass
            // writing a constant motion (its coverage), 6: the motion view 16x brighter (nothing without coaRenderMotion
            // 1). The UI draws on top afterwards as usual.
            const int debug = CVarOn(CoA::RenderPipelineClient::DebugSlot()) ? (*CoA::RenderPipelineClient::DebugSlot())->m_intValue : 0;

            if (debug >= 3 && debug <= 6) {
                CoA::RenderMotionClient::DrawDebugView(debug);
            } else if (debug != 0) {
                DrawDepthView(device);
            }
        }

        // Turned off since the hook was installed: done with this frame, so release and uninstall now.
        if (!CVarOn(CoA::RenderPipelineClient::PipelineSlot())) {
            Uninstall();
        }
    }

    void SetupWorldProjection(C44Matrix& projection, const C44Matrix& view, const C3Vector& position, float fovY, float aspect, float nearZ, float farZ, const C3Vector& forward, const C3Vector& up) {
        // coaRenderPipeline 0: nothing recorded, projection untouched.
        if (!CVarOn(CoA::RenderPipelineClient::PipelineSlot())) {
            return;
        }

        Camera& cam = s_pending;
        memcpy(&cam.view, &view, sizeof(cam.view));
        memcpy(&cam.proj, &projection, sizeof(cam.proj));
        cam.pos[0] = position.x;
        cam.pos[1] = position.y;
        cam.pos[2] = position.z;
        cam.forward[0] = forward.x;
        cam.forward[1] = forward.y;
        cam.forward[2] = forward.z;
        cam.up[0] = up.x;
        cam.up[1] = up.y;
        cam.up[2] = up.z;
        // CSimpleCamera's facing rows are forward, right, up with right = up x forward.
        cam.right[0] = up.y * forward.z - up.z * forward.y;
        cam.right[1] = up.z * forward.x - up.x * forward.z;
        cam.right[2] = up.x * forward.y - up.y * forward.x;
        cam.nearZ = nearZ;
        cam.farZ = farZ;
        cam.fovY = fovY;
        cam.aspect = aspect;
        s_havePending = true;
        // The projection is left as is; BeginWorld jitters the world camera's once the world target has begun.
    }

    // The opaque world is drawn (WorldFrame, after the detail doodads): only meaningful inside a begun world render.
    void OpaqueDone() {
        if (s_begun) {
            CoA::RenderMotionClient::OpaqueDone();
        }
    }

    RenderPipelineHook s_hook = { BeginWorld, EndWorld, SetupWorldProjection, OpaqueDone };

    // coaRenderPipeline's callback; also runs at registration with the value loaded from Config.wtf, so a 1 there
    // installs the hook at startup. 1 installs; 0 releases the world target and uninstalls at once outside a world
    // render, else (begun) at that render's EndWorld.
    bool OnPipeline(CVar* /* var */, const char* /* oldValue */, const char* value, void* /* arg */) {
        if (value && atoi(value) != 0) {
            g_renderPipelineHook = &s_hook;
            g_worldTargetReleaseHook = OnWorldTargetRelease;
        } else if (g_renderPipelineHook && !s_begun) {
            Uninstall();
        }

        return true;
    }
}

void CoA::RenderPipelineClient::Install() {
    CoA::RenderPipelineClient::QueueCVars();
    CoA::CVars::SetCallback("coaRenderPipeline", OnPipeline);
}
