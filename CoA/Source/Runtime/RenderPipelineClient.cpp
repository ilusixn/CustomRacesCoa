// RenderPipeline's engine half: the g_renderPipelineHook callbacks (CGWorldFrame::OnWorldRender begin/end,
// CGCamera::SetupWorldProjection), the activation rules' inputs and the device's world target. The hook is installed
// only while coaRenderPipeline is 1, so at 0 the engine runs its original paths. Not part of the original (CoA64
// addition).
#include "CoA/Source/Runtime/RenderPipeline.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/Log.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/ShaderEffect/ShaderEffect.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include "Engine/Source/Tempest/CRect.h"
#include <cmath>
#include <cstdlib>
#include <cstring>

static_assert(sizeof(C44Matrix) == sizeof(CoA::RenderPipeline::Mat4), "C44Matrix and Mat4 must share a layout");

namespace {
    using namespace CoA::RenderPipeline;

    // Not an Inactive value: the device refused WorldTargetBegin.
    constexpr int kBeginFailed = 100;

    // The last logged state (an Inactive value or kBeginFailed); -1 until the first world frame.
    int s_logged = -1;
    // The world target is begun for the current world render (between beginWorld and endWorld).
    bool s_begun;
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
            default:
                CoA::Log::Printf("RenderPipeline: inactive: world target creation failed (%ux%u)", s_width, s_height);
                break;
        }
    }

    void Deactivate(CGxDevice* device) {
        s_wasActive = false;

        if (s_targetHeld) {
            device->WorldTargetRelease();
            s_targetHeld = false;
        }
    }

    // coaRenderPipeline went to 0: frees the world target and removes the hook. Only outside a world render (from
    // EndWorld, or from the CVar callback while not begun).
    void Uninstall() {
        if (g_theGxDevicePtr) {
            Deactivate(g_theGxDevicePtr);
        }

        g_renderPipelineHook = nullptr;
        LogState(static_cast<int>(Inactive::CVarOff));
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
        const bool forceReset = !s_wasActive || frame != s_lastFrame + 1 || width != s_width || height != s_height;
        s_width = width;
        s_height = height;
        s_lastFrame = frame;

        if (!device->WorldTargetBegin(width, height)) {
            LogState(kBeginFailed);
            Deactivate(device);
            return;
        }

        LogState(static_cast<int>(Inactive::None));
        s_begun = true;
        s_wasActive = true;
        s_targetHeld = true;

        if (!s_havePending) {
            return;
        }

        BeginFrame(s_pending, forceReset);

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
    }

    void EndWorld(const CRect& /* viewportNDC */) {
        if (s_begun) {
            s_begun = false;

            // The whole target, not only the viewport: it started as a copy of the back buffer and the partial-viewport
            // GxSceneClear cleared all of it, so a full copy reproduces the original back buffer exactly.
            // coaRenderDebug 1 (the depth view) replaces this copy in a later stage-1 step.
            g_theGxDevicePtr->WorldTargetEnd(0, 0, static_cast<int32_t>(s_width), static_cast<int32_t>(s_height));
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

    RenderPipelineHook s_hook = { BeginWorld, EndWorld, SetupWorldProjection };

    // coaRenderPipeline's callback; also runs at registration with the value loaded from Config.wtf, so a 1 there
    // installs the hook at startup. 1 installs; 0 releases the world target and uninstalls at once outside a world
    // render, else (begun) at that render's EndWorld.
    bool OnPipeline(CVar* /* var */, const char* /* oldValue */, const char* value, void* /* arg */) {
        if (value && atoi(value) != 0) {
            g_renderPipelineHook = &s_hook;
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
