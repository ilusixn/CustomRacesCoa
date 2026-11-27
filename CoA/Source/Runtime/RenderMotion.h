#ifndef COA_SOURCE_RUNTIME_RENDERMOTION_H
#define COA_SOURCE_RUNTIME_RENDERMOTION_H

#include "CoA/Source/Runtime/RenderPipeline.h"
#include <cstdint>
#include <utility>
#include <vector>

// Reference math for the screen-space motion vectors of CoA64's render pipeline (stage 2). The motion shaders must match
// these functions exactly. Pure; not part of the original (CoA64 addition).
namespace CoA::RenderMotion {
    struct Vec2 {
        float x, y;
    };

    // Static pass: motion of a static world point seen at pixel (u, v) (0..1, v down) with the stored depth, as prevUV - curUV.
    // (continued: docs/2026-10-29-render-pipeline-notes.md, RenderMotion.h #1)
    Vec2 StaticMotion(const RenderPipeline::Frame& f, float u, float v, float storedDepth);

    // Model pass, single bone: clip = (pos, 1) * bone * proj (row vectors); returns prevUV - curUV. The bone matrices
    // include the world and view transforms of their frame.
    Vec2 ModelMotion(const RenderPipeline::Mat4& curBoneView, const RenderPipeline::Mat4& prevBoneView, const float pos[3],
                     const RenderPipeline::Mat4& curProj, const RenderPipeline::Mat4& prevProj);

    // Motion limits every motion shader applies: DLSS turns an Inf / NaN motion into spreading black blocks.
    constexpr float kMinClipW = 1e-4f;      // clip w at or below: behind / at that frame's eye
    constexpr float kMotionLimit = 2.0f;    // |motion| in UV; beyond is off-screen anyway
    // prevUV - curUV from the two clip .xyw; a point with no valid w or a non-finite result moves off-screen
    // (kMotionLimit, kMotionLimit), the rest is clamped to +-kMotionLimit.
    Vec2 ClipMotion(const float cur[3], const float prev[3]);

    // Row-vector matrix product: a is applied first.
    RenderPipeline::Mat4 Mul(const RenderPipeline::Mat4& a, const RenderPipeline::Mat4& b);

    // General 4x4 inverse. A singular matrix returns the identity.
    RenderPipeline::Mat4 Inverse(const RenderPipeline::Mat4& m);

    // The model pass draws a model only when it was animated this frame, is not culled, has last frame's bones cached,
    // and is not a static (AnimateMTSimple) model whose world transform stayed put (the static pass already covers it).
    bool ModelNeedsPass(bool animatedThisFrame, bool culled, bool staticSimple, bool worldChanged, bool havePrevious);

    // How the model pass treats an M2 batch: blend mode (M2BLEND_*) and its first texture's transform index.
    enum class BatchKind { Opaque, Keyed, Skip };
    BatchKind ClassifyBatch(uint32_t blendMode, uint32_t transform, uint32_t transformCount);

    // CM2SceneRender::SetupMaterial's alpha-key reference for an element's alpha.
    float KeyAlphaRef(float elementAlpha);

    // The bone palette is a stream of texels, kPaletteStreamWidth per texture row, in one of kPaletteBuckets textures
    // (heights kPaletteBucketRows); a frame uses the smallest that holds its texels, as DXVK uploads the whole texture.
    constexpr uint32_t kPaletteStreamWidth = 1024;
    constexpr uint32_t kPaletteBuckets = 4;
    constexpr uint32_t kPaletteBucketRows[kPaletteBuckets] = { 32, 64, 128, 256 };
    // The bucket for this many texels, or kPaletteBuckets when none holds them.
    uint32_t PaletteBucket(uint32_t texels);

    // Palette slots per frame: batches of the same model and skin section share one slot (same bones).
    class PaletteSlots {
    public:
        explicit PaletteSlots(uint32_t capacity) : capacity_(capacity) {}
        void Clear();
        // The pair's slot and whether it is new this frame (its rows still need packing); false when full.
        bool Assign(const void* model, const void* section, uint32_t& slot, bool& fresh);
        uint32_t Count() const { return static_cast<uint32_t>(this->keys_.size()); }

    private:
        uint32_t capacity_;
        std::vector<std::pair<const void*, const void*>> keys_;
    };
}

// The motion passes' engine glue (RenderMotionClient.cpp), driven by RenderPipelineClient while the world target is
// (continued: docs/2026-10-29-render-pipeline-notes.md, RenderMotion.h #2)
namespace CoA::RenderMotionClient {
    // After the opaque world (WorldFrame, right after CWorldScene::RenderDetailDoodads): snapshots the world colour for
    // the translucency mask.
    void OpaqueDone();
    // EndWorld, before WorldTargetEnd: the static, model and mask passes, then this frame's bones into MotionCache.
    void BeforeCopyBack();
    // coaRenderDebug 3 (motion) / 4 (mask) / 5 (motion, the model pass writing a constant: its coverage) / 6 (motion,
    // 16x brighter), after WorldTargetEnd: draws the view over the bound target. False: nothing drawn.
    bool DrawDebugView(int mode);
    // The world target is being released (device reset / destroy, pipeline off): frees every D3D9 object of the
    // passes and clears MotionCache.
    void Release();
}

#endif
