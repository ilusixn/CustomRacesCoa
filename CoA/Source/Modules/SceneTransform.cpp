// AscSceneTransform.cpp (see SceneTransform.h): a movie frame renders in its own rect.
#include "CoA/Source/Modules/SceneTransform.h"
#include "CoA/Source/Modules/Modules.h"

namespace {
    CoA::SceneTransform::Deps s_deps;
    float s_kept[4] = {};   // 0x10BCA734, 738, 73C, 740

    float Convert(float (*fn)(float), float v) {
        return fn ? fn(v) : v;
    }
}

CoA::SceneTransform::Deps& CoA::SceneTransform::Dependencies() {
    return s_deps;
}

void CoA::SceneTransform::Keep(const float* rect) {
    s_kept[0] = Convert(s_deps.toNdcHeight, rect[0]);
    s_kept[1] = Convert(s_deps.toNdcWidth, rect[1]);
    s_kept[2] = Convert(s_deps.toNdcHeight, rect[2]);
    s_kept[3] = Convert(s_deps.toNdcWidth, rect[3]);
}

void CoA::SceneTransform::ViewportArgs(float& minX, float& maxX, float& minY, float& maxY) {
    minX = s_kept[1];
    maxX = s_kept[3];
    minY = s_kept[0];
    maxY = s_kept[2];
}

void CoA::SceneTransform::Init() {
    // The hooks are the Engine sites in CSimpleMovieFrame::Render (SceneTransformClient.cpp installs them).
}

void CoA::SceneTransform::Reset() {
    s_deps = Deps();

    for (float& v : s_kept) {
        v = 0.0f;
    }
}
