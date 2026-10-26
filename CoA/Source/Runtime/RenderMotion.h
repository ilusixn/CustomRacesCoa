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
    // The stored depth is what the device writes: minZ + ndcD3D * (maxZ - minZ) with ndcD3D = (ndcGL + 1) / 2, where ndcGL
    // is the clip z / w of Frame.proj (GL-style, -1..1). A stored depth >= depthMax (sky / clear) is a direction at
    // infinity: only the rotation applies. An empty depth range returns (0, 0). Camera-relative: the unprojected point is
    // relative to the current eye and is moved into the previous eye's frame with cur.pos - prev.pos.
    Vec2 StaticMotion(const RenderPipeline::Frame& f, float u, float v, float storedDepth);

    // Model pass, single bone: clip = (pos, 1) * bone * proj (row vectors); returns prevUV - curUV. The bone matrices
    // include the world and view transforms of their frame.
    Vec2 ModelMotion(const RenderPipeline::Mat4& curBoneView, const RenderPipeline::Mat4& prevBoneView, const float pos[3],
                     const RenderPipeline::Mat4& curProj, const RenderPipeline::Mat4& prevProj);

    // General 4x4 inverse. A singular matrix returns the identity.
    RenderPipeline::Mat4 Inverse(const RenderPipeline::Mat4& m);
}

#endif
