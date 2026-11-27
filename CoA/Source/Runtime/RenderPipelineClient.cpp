// RenderPipeline's engine half: the g_renderPipelineHook callbacks (CGWorldFrame::OnWorldRender begin/end,
// (continued: docs/2026-10-29-render-pipeline-notes.md, RenderPipelineClient.cpp #1)
#include "CoA/Shaders/DepthCopyPS.h"
#include "CoA/Shaders/DepthViewPS.h"
#include "CoA/Shaders/FullscreenVS.h"
#include "CoA/Source/Runtime/Dlss.h"
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
    // DLSS evaluates this world render (DlssClient::Begin succeeded and the world target began), and the world camera's
    // projection carries this frame's jitter.
    bool s_dlss;
    bool s_jittered;
    // This world render is upscaled or jittered: unit names are drawn after it at display size (DrawWorldNames).
    bool s_deferNames;
    // The device may hold world textures (released when the pipeline turns off).
    bool s_targetHeld;
    // Device frame (CGxDevice::m_frameCount) and size of the last active world render: s_width x s_height is the
    // (continued: docs/2026-10-29-render-pipeline-notes.md, RenderPipelineClient.cpp #2)
    int32_t s_lastFrame;
    uint32_t s_width;
    uint32_t s_height;
    uint32_t s_renderWidth;
    uint32_t s_renderHeight;
    // The device renders the world at s_renderWidth x s_renderHeight with a texture LOD bias (WorldSized to EndWorld).
    bool s_sized;
    // The camera of the latest SetupWorldProjection (hit tests call it too, so the last call before the world
    // render is the frame's camera); committed with BeginFrame when the world target begins.
    Camera s_pending;
    bool s_havePending;
    // SetupWorldProjection ran since the last world render began: this world render has a new camera record. DLSS only
    // evaluates such renders (a repeated record would hand Streamline last frame's camera and jitter again).
    bool s_pendingFresh;

    // The depth debug view's D3D9 objects, created lazily on first use and released with the world target (device
    // reset / destroy through g_worldTargetReleaseHook, pipeline off through Deactivate).
    IDirect3DVertexShader9* s_debugVS;
    IDirect3DPixelShader9* s_debugPS;
    IDirect3DPixelShader9* s_depthCopyPS;
    IDirect3DVertexDeclaration9* s_debugDecl;
    // Shader constants DrawDepthView writes (VS c0, PS c0..c2), re-sent from the engine's shadow copy afterwards.
    constexpr uint32_t kDebugVsConstants = 1;
    constexpr uint32_t kDebugPsConstants = 3;
    // Creation failed (logged once): coaRenderDebug falls back to the normal image until the objects are released.
    bool s_debugFailed;

    void ReleaseDebug() {
        IUnknown* objects[] = { s_debugVS, s_debugPS, s_depthCopyPS, s_debugDecl };

        for (IUnknown* object : objects) {
            if (object) {
                object->Release();
            }
        }

        s_debugVS = nullptr;
        s_debugPS = nullptr;
        s_depthCopyPS = nullptr;
        s_debugDecl = nullptr;
        s_debugFailed = false;
    }

    // The fullscreen passes' objects (depth view, names' depth copy), created once; false after a failure (logged).
    bool EnsureFullscreenObjects(IDirect3DDevice9* device) {
        if (s_debugDecl) {
            return true;
        }

        if (s_debugFailed) {
            return false;
        }

        const D3DVERTEXELEMENT9 elements[] = {
            { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };

        if (FAILED(device->CreateVertexShader(reinterpret_cast<const DWORD*>(kFullscreenVS), &s_debugVS))
            || FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(kDepthViewPS), &s_debugPS))
            || FAILED(device->CreatePixelShader(reinterpret_cast<const DWORD*>(kDepthCopyPS), &s_depthCopyPS))
            || FAILED(device->CreateVertexDeclaration(elements, &s_debugDecl))) {
            ReleaseDebug();
            s_debugFailed = true;
            CoA::Log::Printf("RenderPipeline: fullscreen passes unavailable (D3D9 object creation failed)");
            return false;
        }

        return true;
    }

    // Size of the bound render target (the back buffer after WorldTargetEnd); the display size as fallback.
    void BoundTargetSize(IDirect3DDevice9* device, uint32_t& width, uint32_t& height) {
        width = s_width;
        height = s_height;
        IDirect3DSurface9* target = nullptr;

        if (SUCCEEDED(device->GetRenderTarget(0, &target)) && target) {
            D3DSURFACE_DESC desc;

            if (SUCCEEDED(target->GetDesc(&desc)) && desc.Width && desc.Height) {
                width = desc.Width;
                height = desc.Height;
            }

            target->Release();
        }
    }

    // A quad over the whole bound target (uv 0..1, point / clamp on s0), viewport depth 0..1. The caller sets the
    // pixel shader, its constants, s0 and the depth / colour write states.
    void DrawFullscreen(IDirect3DDevice9* device, uint32_t targetWidth, uint32_t targetHeight) {
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
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        device->SetVertexDeclaration(s_debugDecl);
        device->SetVertexShader(s_debugVS);
        device->SetVertexShaderConstantF(0, offset, 1);
        device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));
    }

    // Draws the world's INTZ depth as greyscale over the full target rect, then ExternalStateRestore. Runs
    // after WorldTargetEnd, so the original back buffer and depth surface are bound again. False: nothing drawn.
    bool DrawDepthView(CGxDevice* gx) {
        IDirect3DDevice9* device = static_cast<IDirect3DDevice9*>(gx->D3dDevice());
        IDirect3DBaseTexture9* depth = static_cast<IDirect3DBaseTexture9*>(gx->WorldDepthTexture());

        if (!device || !depth || !EnsureFullscreenObjects(device)) {
            return false;
        }

        uint32_t targetWidth = 0;
        uint32_t targetHeight = 0;
        BoundTargetSize(device, targetWidth, targetHeight);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        device->SetPixelShader(s_debugPS);
        // c0.x: 1 = depth view, 2 = diagnostic (uv gradient, blue where the depth is still at the clear value).
        // (continued: docs/2026-10-29-render-pipeline-notes.md, RenderPipelineClient.cpp #3)
        const Frame& frame = Current();
        const float range = frame.depthMax - frame.depthMin;
        const float nearZ = frame.cur.nearZ;
        const float farZ = frame.cur.farZ;
        const float depthConstants[8] = { frame.depthMin, range > 1e-6f ? 1.0f / range : 1.0f, 0.0f, 0.0f, nearZ * farZ, farZ, farZ - nearZ, 1.0f / 16.0f };
        const float mode[4] = { (*CoA::RenderPipelineClient::DebugSlot())->m_intValue == 2 ? 2.0f : 1.0f, 0.0f, 0.0f, 0.0f };
        device->SetPixelShaderConstantF(0, mode, 1);
        device->SetPixelShaderConstantF(1, depthConstants, 2);
        device->SetTexture(0, depth);
        DrawFullscreen(device, targetWidth, targetHeight);

        // The INTZ must not stay bound (it would fail the next Begin / Reset); the engine re-binds its own textures.
        device->SetTexture(0, nullptr);
        gx->ExternalStateRestore(kDebugVsConstants, kDebugPsConstants);

        return true;
    }

    // The world's INTZ upscaled (point) into the bound default depth surface: what that surface held after the world
    // before the pipeline existed, and the occlusion the deferred names test against. False: nothing written.
    bool CopyWorldDepth(CGxDevice* gx) {
        IDirect3DDevice9* device = static_cast<IDirect3DDevice9*>(gx->D3dDevice());
        IDirect3DBaseTexture9* depth = static_cast<IDirect3DBaseTexture9*>(gx->WorldDepthTexture());
        IDirect3DSurface9* depthSurface = nullptr;

        if (!device || !depth || !EnsureFullscreenObjects(device) || FAILED(device->GetDepthStencilSurface(&depthSurface)) || !depthSurface) {
            return false;
        }

        depthSurface->Release();
        uint32_t targetWidth = 0;
        uint32_t targetHeight = 0;
        BoundTargetSize(device, targetWidth, targetHeight);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
        device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        device->SetRenderState(D3DRS_DEPTHBIAS, 0);
        device->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
        device->SetPixelShader(s_depthCopyPS);
        device->SetTexture(0, depth);
        DrawFullscreen(device, targetWidth, targetHeight);
        device->SetTexture(0, nullptr);
        gx->ExternalStateRestore(kDebugVsConstants, 0);

        return true;
    }

    bool CVarOn(CVar** slot) {
        return *slot && (*slot)->m_intValue != 0;
    }

    // coaRenderPipeline 1, or DLSS wanted (forces the pipeline on).
    bool PipelineOn() {
        return CVarOn(CoA::RenderPipelineClient::PipelineSlot()) || CoA::DlssClient::Wanted();
    }

    const char* InactiveName(Inactive why) {
        switch (why) {
            case Inactive::CVarOff: return "cvar off";
            case Inactive::Multisample: return "multisample (MSAA is on)";
            case Inactive::NoIntz: return "no INTZ / 24-bit depth";
            case Inactive::NoShaders: return "shaders disabled";
            default: return "?";
        }
    }

    void LogState(int state) {
        if (state == s_logged) {
            return;
        }

        s_logged = state;

        switch (state) {
            case static_cast<int>(Inactive::None):
                if (s_renderWidth != s_width || s_renderHeight != s_height) {
                    CoA::Log::Printf("RenderPipeline: active (%ux%u, world at %ux%u)", s_width, s_height, s_renderWidth, s_renderHeight);
                } else {
                    CoA::Log::Printf("RenderPipeline: active (%ux%u)", s_width, s_height);
                }
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

        CoA::DlssClient::Release();
        ReleaseDebug();
        CoA::RenderMotionClient::Release();
        s_beginFailed = false;
        g_worldTargetReleaseHook = nullptr;
        g_renderPipelineHook = nullptr;
        LogState(static_cast<int>(Inactive::CVarOff));
    }

    // The device is releasing its D3DPOOL_DEFAULT resources (reset / destroy): DLSS's Streamline feature and textures
    // (continued: docs/2026-10-29-render-pipeline-notes.md, RenderPipelineClient.cpp #4)
    void OnWorldTargetRelease() {
        CoA::DlssClient::Release();
        ReleaseDebug();
        CoA::RenderMotionClient::Release();
        s_beginFailed = false;
    }

    // The world target's size for a display size: coaRenderScale (developer; 1 = the display size, the default).
    void RenderSize(uint32_t width, uint32_t height, uint32_t& renderWidth, uint32_t& renderHeight) {
        CVar* scale = *CoA::RenderPipelineClient::ScaleSlot();
        ScaledRenderSize(width, height, scale ? scale->m_floatValue : 1.0f, renderWidth, renderHeight);
    }

    void BeginWorld() {
        CGxDevice* device = g_theGxDevicePtr;

        if (!device) {
            return;
        }

        s_dlss = false;
        s_jittered = false;
        s_deferNames = false;
        ActivationInput in;
        in.cvarOn = PipelineOn();
        in.msaaSamples = device->m_format.m_multisampleCount;
        in.intzSupported = in.cvarOn && device->WorldTargetSupported();
        in.shadersEnabled = CShaderEffect::s_enableShaders != 0;
        const Inactive why = WhyInactive(in);

        if (why != Inactive::None) {
            LogState(static_cast<int>(why));
            CoA::DlssClient::NotActive(InactiveName(why));
            Deactivate(device);
            return;
        }

        // CoA64: no device context means the device is lost (between a failed Present / Reset and recovery): nothing to
        // create, and not a creation failure either.
        if (!device->m_context) {
            LogState(kDeviceLost);
            CoA::DlssClient::NotActive("device lost");
            s_wasActive = false;
            return;
        }

        // A texture render target bound around the world render: the world target only replaces the default
        // surfaces, so leave this frame alone.
        CGxTex* renderTarget = nullptr;
        device->DeviceGetRenderTarget(GxBuffers_Color, renderTarget);

        if (renderTarget) {
            s_wasActive = false;
            CoA::DlssClient::NotActive("a texture render target is bound around the world");
            return;
        }

        CRect window = { 0.0f, 0.0f, 0.0f, 0.0f };
        device->CapsWindowSize(window);
        const uint32_t width = static_cast<uint32_t>(std::lround(window.maxX - window.minX));
        const uint32_t height = static_cast<uint32_t>(std::lround(window.maxY - window.minY));
        uint32_t renderWidth = width;
        uint32_t renderHeight = height;
        // DLSS: Streamline's render size for the mode and display size (overrides coaRenderScale).
        const bool dlss = CoA::DlssClient::Begin(device, width, height, renderWidth, renderHeight);

        if (!dlss) {
            RenderSize(width, height, renderWidth, renderHeight);
        }

        // Temporal history restarts on the first world frame after a gap in presented frames (a loading screen,
        // entering the world, the pipeline turning on) and when the display or target size changes.
        const int32_t frame = device->m_frameCount;
        const bool renderSizeChanged = renderWidth != s_renderWidth || renderHeight != s_renderHeight;
        const bool sizeChanged = width != s_width || height != s_height || renderSizeChanged;
        const bool forceReset = !s_wasActive || frame != s_lastFrame + 1 || sizeChanged;
        s_width = width;
        s_height = height;
        s_renderWidth = renderWidth;
        s_renderHeight = renderHeight;
        s_lastFrame = frame;

        // A creation that failed at this size is not retried every frame (the size changing or a device reset retries).
        if (s_beginFailed && !sizeChanged) {
            CoA::DlssClient::NotActive("world target creation failed");
            Deactivate(device);
            return;
        }

        if (!device->WorldTargetBegin(renderWidth, renderHeight)) {
            CoA::DlssClient::NotActive("world target creation failed");
            LogState(kBeginFailed);
            Deactivate(device);
            // After Deactivate: its release hook clears s_beginFailed.
            s_beginFailed = true;
            return;
        }

        s_beginFailed = false;

        // A new world size while already active is logged too (LogState only logs a change of state).
        if (renderSizeChanged && s_logged == static_cast<int>(Inactive::None)) {
            s_logged = -1;
        }

        LogState(static_cast<int>(Inactive::None));
        s_begun = true;
        s_wasActive = true;
        s_targetHeld = true;
        const bool fresh = s_havePending && s_pendingFresh;
        s_pendingFresh = false;
        s_dlss = dlss && fresh;
        s_deferNames = dlss || renderWidth != width || renderHeight != height;

        if (dlss && !fresh) {
            CoA::DlssClient::Skip();
        }

        if (!s_havePending) {
            return;
        }

        // The world viewport (WorldFrame::OnWorldRender, right after this) is [the current minZ, CWorld::s_unk_ADEEE4].
        BeginFrame(s_pending, forceReset, device->m_viewport.m_z.m_l, CWorld::s_unk_ADEEE4);

        // Jitter only the world camera of a frame that is rendering into the world target: the device must still hold
        // (continued: docs/2026-10-29-render-pipeline-notes.md, RenderPipelineClient.cpp #5)
        if (!((dlss && CoA::DlssClient::Jitters()) || CVarOn(CoA::RenderPipelineClient::JitterSlot())) || memcmp(&device->m_projection, &s_pending.proj, sizeof(s_pending.proj)) != 0) {
            return;
        }

        // Jitter is in render pixels (the world target's), with more phases the more the display outnumbers them.
        const uint32_t phases = JitterPhaseCount(renderWidth, width);
        float jx = 0.0f;
        float jy = 0.0f;
        Halton23(Current().index % phases + 1, jx, jy);
        const Mat4 jittered = ApplyJitter(s_pending.proj, jx, jy, renderWidth, renderHeight);
        C44Matrix projection;
        memcpy(&projection, &jittered, sizeof(projection));
        device->XformSetProjection(projection);
        SetJitter(jx, jy);
        s_jittered = true;
        s_deferNames = true;
    }

    // Right after FFX::Start (which sizes its textures from the display): a world target smaller than the display
    // becomes the device's window for the rest of the world render, with the matching texture LOD bias.
    void WorldSized() {
        CGxDevice* device = g_theGxDevicePtr;

        if (!s_begun || s_sized || !device || (s_renderWidth == s_width && s_renderHeight == s_height)) {
            return;
        }

        const Rect rect = CoA::Dlss::RenderRect(s_renderWidth, s_renderHeight);
        device->WorldRenderSizeBegin(static_cast<uint32_t>(rect.right - rect.left), static_cast<uint32_t>(rect.bottom - rect.top));
        device->WorldLodBiasSet(CoA::Dlss::LodBias(s_renderWidth, s_width));
        s_sized = true;
    }

    // Back to the display's window and no LOD bias, before anything after the world draws (passes, copy-back, UI).
    void EndSized(CGxDevice* device) {
        if (s_sized) {
            s_sized = false;
            device->WorldLodBiasSet(0.0f);
            device->WorldRenderSizeEnd();
        }
    }

    void EndWorld(const CRect& /* viewportNDC */) {
        if (s_begun) {
            s_begun = false;
            EndSized(g_theGxDevicePtr);

            // coaRenderMotion 1 (or DLSS): the motion and mask passes, while the world's colour and depth are still bound.
            CoA::RenderMotionClient::BeforeCopyBack();

            const int debug = CVarOn(CoA::RenderPipelineClient::DebugSlot()) ? (*CoA::RenderPipelineClient::DebugSlot())->m_intValue : 0;
            CGxDevice* device = g_theGxDevicePtr;

            // DLSS: the depth remap pass and Streamline's evaluate recorded into DXVK's command stream; its display-size
            // (continued: docs/2026-10-29-render-pipeline-notes.md, RenderPipelineClient.cpp #6)
            void* dlssOutput = s_dlss ? CoA::DlssClient::Evaluate(device, Current(), s_jittered, s_width, s_height, debug) : nullptr;
            s_dlss = false;
            bool copied;

            if (dlssOutput && debug != 7) {
                copied = device->WorldTargetEndWith(dlssOutput);
            } else {
                // The whole target, not only the viewport: it started as a copy of the back buffer and the
                // (continued: docs/2026-10-29-render-pipeline-notes.md, RenderPipelineClient.cpp #7)
                copied = device->WorldTargetEnd(0, 0, static_cast<int32_t>(s_width), static_cast<int32_t>(s_height));
            }

            // What follows the world (names, chat bubbles) projects with the camera as the original did, unjittered.
            if (s_jittered && s_havePending) {
                C44Matrix projection;
                memcpy(&projection, &s_pending.proj, sizeof(projection));
                device->XformSetProjection(projection);
                s_jittered = false;
            }

            if (!copied) {
                if (!s_copyFailLogged) {
                    s_copyFailLogged = true;
                    CoA::Log::Printf("RenderPipeline: copy of the world image to the back buffer failed (this frame's world is lost)");
                }
            } else {
                s_copyFailLogged = false;
            }

            // coaRenderDebug 1 / 2: the depth view is drawn over the copied world (the same full rect), so a failure
            // (continued: docs/2026-10-29-render-pipeline-notes.md, RenderPipelineClient.cpp #8)
            if (debug >= 3 && debug <= 6) {
                CoA::RenderMotionClient::DrawDebugView(debug);
            } else if (debug == 1 || debug == 2) {
                DrawDepthView(device);
            }
        }

        // Turned off since the hook was installed (and DLSS not wanted): done with this frame, so release and uninstall now.
        if (!PipelineOn()) {
            Uninstall();
        }
    }

    void SetupWorldProjection(C44Matrix& projection, const C44Matrix& view, const C3Vector& position, float fovY, float aspect, float nearZ, float farZ, const C3Vector& forward, const C3Vector& up) {
        // coaRenderPipeline 0 (and no DLSS): nothing recorded, projection untouched.
        if (!PipelineOn()) {
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
        s_pendingFresh = true;
        // The projection is left as is; BeginWorld jitters the world camera's once the world target has begun.
    }

    // The opaque world is drawn (WorldFrame, after the detail doodads): only meaningful inside a begun world render.
    void OpaqueDone() {
        if (s_begun) {
            CoA::RenderMotionClient::OpaqueDone();
        }
    }

    bool DeferWorldNames() {
        return s_begun && s_deferNames;
    }

    // After EndWorld, before the deferred names draw: the world's depth for their occlusion test.
    bool DrawWorldNames() {
        const bool deferred = s_deferNames;
        s_deferNames = false;
        return deferred && g_theGxDevicePtr && CopyWorldDepth(g_theGxDevicePtr);
    }

    RenderPipelineHook s_hook = { BeginWorld, EndWorld, SetupWorldProjection, OpaqueDone, WorldSized, DeferWorldNames, DrawWorldNames };

    // coaRenderPipeline's callback; also runs at registration with the value loaded from Config.wtf, so a 1 there
    // (continued: docs/2026-10-29-render-pipeline-notes.md, RenderPipelineClient.cpp #9)
    bool OnPipeline(CVar* /* var */, const char* /* oldValue */, const char* value, void* /* arg */) {
        if ((value && atoi(value) != 0) || CoA::DlssClient::Wanted()) {
            g_renderPipelineHook = &s_hook;
            g_worldTargetReleaseHook = OnWorldTargetRelease;
        } else if (g_renderPipelineHook && !s_begun) {
            Uninstall();
        }

        return true;
    }

    // coaDlssMode's callback (also at registration, after the device exists, so Streamline's readiness is known): a
    // (continued: docs/2026-10-29-render-pipeline-notes.md, RenderPipelineClient.cpp #10)
    bool OnDlssMode(CVar* /* var */, const char* /* oldValue */, const char* value, void* /* arg */) {
        if (CoA::DlssClient::WantedFor(value ? atoi(value) : 0)) {
            g_renderPipelineHook = &s_hook;
            g_worldTargetReleaseHook = OnWorldTargetRelease;
        } else if (g_renderPipelineHook && !s_begun && !CVarOn(CoA::RenderPipelineClient::PipelineSlot())) {
            Uninstall();
        }

        return true;
    }
}

void CoA::RenderPipelineClient::Install() {
    CoA::RenderPipelineClient::QueueCVars();
    CoA::CVars::SetCallback("coaRenderPipeline", OnPipeline);
    CoA::CVars::SetCallback("coaDlssMode", OnDlssMode);
}
