#ifndef COA_SOURCE_RUNTIME_RENDERMOTION_H
#define COA_SOURCE_RUNTIME_RENDERMOTION_H

#include "CoA/Source/Runtime/RenderPipeline.h"

// Reference math for the screen-space motion vectors of CoA64's render pipeline (stage 2). The motion shaders must match
// these functions exactly. Pure; not part of the original (CoA64 addition).
namespace CoA::RenderMotion {
    struct Vec2 {
        float x, y;
    };

    // Static pass: motion of a static world point seen at pixel (u, v) (0..1, v down) with the stored depth, as prevUV - curUV.
    // The stored depth is minZ + ndc * (maxZ - minZ) of the frame's depth range. Camera-relative: the unprojected point is
    // relative to the current eye and is moved into the previous eye's frame with cur.pos - prev.pos.
    Vec2 StaticMotion(const RenderPipeline::Frame& f, float u, float v, float storedDepth);

    // Model pass, single bone: clip = (pos, 1) * bone * proj (row vectors); returns prevUV - curUV. The bone matrices
    // include the world and view transforms of their frame.
    Vec2 ModelMotion(const RenderPipeline::Mat4& curBoneView, const RenderPipeline::Mat4& prevBoneView, const float pos[3],
                     const RenderPipeline::Mat4& curProj, const RenderPipeline::Mat4& prevProj);

    // General 4x4 inverse (identity if singular).
    RenderPipeline::Mat4 Inverse(const RenderPipeline::Mat4& m);
}

#endif
