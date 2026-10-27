#ifndef COA_SOURCE_RUNTIME_RENDERPIPELINE_H
#define COA_SOURCE_RUNTIME_RENDERPIPELINE_H

#include <cstdint>

// Pure logic of CoA64's render pipeline (off-screen world target, camera record, sub-pixel jitter). No Win32/D3D here;
// the device-facing parts live elsewhere. Not part of the original (CoA64 addition).
namespace CoA::RenderPipeline {
    // Row-vector, D3D layout; same memory layout as C44Matrix.
    struct Mat4 {
        float m[4][4];
    };

    struct Camera {
        Mat4 view;
        Mat4 proj;
        float pos[3];
        float forward[3];
        float up[3];
        float right[3];
        float nearZ, farZ, fovY, aspect;
    };

    struct Frame {
        Camera cur;
        Camera prev;
        uint32_t index;
        bool reset;
        float jitterX, jitterY;
        // The world viewport's depth range: the Z buffer holds minZ + ndcDepth * (maxZ - minZ), not ndcDepth (the world
        // viewport's MaxZ is CWorld::s_unk_ADEEE4, 0.94). Kept per frame for the depth debug view and stage 3 (DLSS).
        float depthMin, depthMax;
    };

    struct ActivationInput {
        bool cvarOn;
        uint32_t msaaSamples;
        bool intzSupported;
        bool shadersEnabled;
    };

    enum class Inactive { None, CVarOff, Multisample, NoIntz, NoShaders };

    // Yards; a camera that moved farther than this in one frame resets temporal history.
    constexpr float kTeleportDistance = 50.0f;
    // Degrees; a camera that turned more than this in one frame resets temporal history.
    constexpr float kCameraCutDegrees = 60.0f;

    struct Rect {
        int32_t left, top, right, bottom;
    };

    Inactive WhyInactive(const ActivationInput& in);

    // Rolls cur -> prev, increments index, sets reset (first frame, forceReset, a teleport, or a camera cut), zeroes the jitter.
    // depthMin / depthMax: the world viewport's Z range for this frame (defaults: the full 0..1).
    void BeginFrame(const Camera& cam, bool forceReset, float depthMin = 0.0f, float depthMax = 1.0f);
    const Frame& Current();
    // Records the jitter (pixels) applied to this frame's projection; BeginFrame zeroes it, so unjittered frames stay 0.
    void SetJitter(float jx, float jy);

    // Halton(2,3) sample for the given index, in [-0.5, 0.5).
    void Halton23(uint32_t index, float& x, float& y);
    // 8 * (display/render)^2, rounded, at least 8.
    uint32_t JitterPhaseCount(uint32_t renderWidth, uint32_t displayWidth);
    // Offsets the projection by (jx, jy) pixels; returned unchanged when both are 0.
    Mat4 ApplyJitter(const Mat4& proj, float jx, float jy, uint32_t width, uint32_t height);
    // NDC viewport (y up) to pixel rect (y down).
    Rect CopyRect(float vpMinX, float vpMaxX, float vpMinY, float vpMaxY, uint32_t width, uint32_t height);

    // Tests only.
    void Reset();
}

struct CVar;

// The render pipeline's engine glue (RenderPipelineClient.cpp): CVars and the g_renderPipelineHook callbacks.
namespace CoA::RenderPipelineClient {
    // Queues coaRenderPipeline / coaRenderDebug / coaRenderJitter / coaRenderMotion as glue CVars (flags 1, category 1, default "0").
    void QueueCVars();
    // The CVars' registration slots (filled by CoA::CVars::RegisterGlue).
    CVar** PipelineSlot();
    CVar** DebugSlot();
    CVar** JitterSlot();
    CVar** MotionSlot();
    // Queues the CVars with coaRenderPipeline's callback, which installs g_renderPipelineHook while the CVar is 1
    // (including a 1 loaded from Config.wtf). Called once at client init, before the glue CVars register.
    void Install();
}

#endif
