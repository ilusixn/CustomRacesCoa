#ifndef COA_SOURCE_RUNTIME_DLSSFRAME_H
#define COA_SOURCE_RUNTIME_DLSSFRAME_H

// The per-frame data DlssClient hands to DXVK's RecordCallback as its user blob (copied by DXVK at the call, read by the
// (continued: docs/2026-10-29-render-pipeline-notes.md, DlssFrame.h (b) #1)
#include "CoA/Source/Runtime/Dlss.h"
#include "CoA/Source/Runtime/RenderPipeline.h"
#include <sl_consts.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace CoA::Dlss {
    // Byte layout (x64, SDK 2.14.16): sl::Constants at 0 (its BaseStructure `next` must stay nullptr), then eight
    // (continued: docs/2026-10-29-render-pipeline-notes.md, DlssFrame.h (b) #2)
    struct FrameBlob {
        sl::Constants constants;
        uint32_t frameIndex;
        uint32_t viewport;
        uint32_t mode;
        uint32_t renderWidth;
        uint32_t renderHeight;
        uint32_t displayWidth;
        uint32_t displayHeight;
        uint32_t generation;
    };

    static_assert(std::is_trivially_copyable_v<FrameBlob>, "the blob is copied as bytes by DXVK");
    static_assert(sizeof(sl::float4x4) == sizeof(RenderPipeline::Mat4), "sl::float4x4 and Mat4 must share a layout");
    static_assert(sizeof(sl::Constants) == 456, "sl::Constants changed size: check the SDK version and the blob");
    static_assert(offsetof(FrameBlob, frameIndex) == sizeof(sl::Constants), "blob: frameIndex follows the constants");
    static_assert(offsetof(FrameBlob, generation) == sizeof(sl::Constants) + 7 * sizeof(uint32_t), "blob: eight uint32_t");
    static_assert(sizeof(FrameBlob) == sizeof(sl::Constants) + 8 * sizeof(uint32_t), "blob: no padding");

    // What the frame does not record: the per-frame settings of the evaluate.
    struct FrameSetup {
        uint32_t frameIndex;
        uint32_t viewport;
        uint32_t mode;
        uint32_t renderWidth, renderHeight;
        uint32_t displayWidth, displayHeight;
        uint32_t generation;
        float mvecSign;     // coaDlssMvecSign
        float jitterSign;   // coaDlssJitterSign
        bool jittered;      // this world render's projection carries Frame::jitterX / jitterY
        bool reset;         // a mode / size change (Frame::reset resets too)
        bool evaluate;      // DLSS evaluates (else only constants and tags)
        bool frameGen;      // Frame Generation's tags (hudless, depth, motion until present)
    };

    // FrameBlob::mode carries the DLSS mode in its low byte and these flags above it.
    constexpr uint32_t kBlobEvaluate = 1u << 8;
    constexpr uint32_t kBlobFrameGen = 1u << 9;

    // Global Constraints: row-major matrices without jitter (BuildMatrices), jitterOffset in pixels times the sign,
    // (continued: docs/2026-10-29-render-pipeline-notes.md, DlssFrame.h (b) #3)
    inline void FillFrameBlob(FrameBlob& b, const RenderPipeline::Frame& f, const FrameSetup& s) {
        b.constants = sl::Constants{};
        sl::Constants& c = b.constants;
        const SlMatrices m = BuildMatrices(f);
        memcpy(&c.cameraViewToClip, &m.cameraViewToClip, sizeof(c.cameraViewToClip));
        memcpy(&c.clipToCameraView, &m.clipToCameraView, sizeof(c.clipToCameraView));
        memcpy(&c.clipToPrevClip, &m.clipToPrevClip, sizeof(c.clipToPrevClip));
        memcpy(&c.prevClipToClip, &m.prevClipToClip, sizeof(c.prevClipToClip));

        // No lens distortion.
        RenderPipeline::Mat4 identity{};
        for (int i = 0; i < 4; ++i) {
            identity.m[i][i] = 1.0f;
        }
        memcpy(&c.clipToLensClip, &identity, sizeof(c.clipToLensClip));

        c.jitterOffset = s.jittered ? sl::float2(f.jitterX * s.jitterSign, f.jitterY * s.jitterSign) : sl::float2(0.0f, 0.0f);
        c.mvecScale = sl::float2(s.mvecSign, s.mvecSign);
        c.cameraPinholeOffset = sl::float2(0.0f, 0.0f);
        c.cameraPos = sl::float3(f.cur.pos[0], f.cur.pos[1], f.cur.pos[2]);
        c.cameraUp = sl::float3(f.cur.up[0], f.cur.up[1], f.cur.up[2]);
        c.cameraRight = sl::float3(f.cur.right[0], f.cur.right[1], f.cur.right[2]);
        c.cameraFwd = sl::float3(f.cur.forward[0], f.cur.forward[1], f.cur.forward[2]);
        c.cameraNear = f.cur.nearZ;
        c.cameraFar = f.cur.farZ;
        c.cameraFOV = f.cur.fovY;
        c.cameraAspectRatio = f.cur.aspect;
        c.depthInverted = sl::Boolean::eFalse;
        c.cameraMotionIncluded = sl::Boolean::eTrue;
        c.motionVectors3D = sl::Boolean::eFalse;
        c.reset = (s.reset || f.reset) ? sl::Boolean::eTrue : sl::Boolean::eFalse;
        c.orthographicProjection = sl::Boolean::eFalse;
        c.motionVectorsDilated = sl::Boolean::eFalse;
        c.motionVectorsJittered = sl::Boolean::eFalse;

        b.frameIndex = s.frameIndex;
        b.viewport = s.viewport;
        b.mode = s.mode | (s.evaluate ? kBlobEvaluate : 0u) | (s.frameGen ? kBlobFrameGen : 0u);
        b.renderWidth = s.renderWidth;
        b.renderHeight = s.renderHeight;
        b.displayWidth = s.displayWidth;
        b.displayHeight = s.displayHeight;
        b.generation = s.generation;
    }
}

#endif
