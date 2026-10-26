#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/RenderPipeline.h"
#include "CoA/Tests/TestMain.h"
#include <cmath>
#include <cstring>

namespace {
    using CoA::RenderPipeline::Camera;
    using CoA::RenderPipeline::Mat4;

    Mat4 Identity() {
        Mat4 m{};
        for (int i = 0; i < 4; ++i) {
            m.m[i][i] = 1.0f;
        }
        return m;
    }

    // Identity matrices, position 0, forward = normalised (x, y, z).
    Camera CameraFacing(float x, float y, float z) {
        Camera c{};
        c.view = Identity();
        c.proj = Identity();
        const float len = std::sqrt(x * x + y * y + z * z);
        c.forward[0] = x / len;
        c.forward[1] = y / len;
        c.forward[2] = z / len;
        return c;
    }
}

COA_TEST(RenderPipeline_CameraCutResets) {
    CoA::RenderPipeline::Reset();
    CoA::RenderPipeline::BeginFrame(CameraFacing(1, 0, 0), false);
    CoA::RenderPipeline::BeginFrame(CameraFacing(1, 0, 0), false);
    CHECK(!CoA::RenderPipeline::Current().reset);
    CoA::RenderPipeline::BeginFrame(CameraFacing(0.6f, 0.8f, 0), false);    // ~53 degrees: no cut
    CHECK(!CoA::RenderPipeline::Current().reset);
    CoA::RenderPipeline::BeginFrame(CameraFacing(-0.6f, 0.8f, 0), false);   // ~74 degrees: cut
    CHECK(CoA::RenderPipeline::Current().reset);
}

COA_TEST(RenderPipeline_SetJitterStoresPixels) {
    CoA::RenderPipeline::Reset();
    CoA::RenderPipeline::BeginFrame(CameraFacing(1, 0, 0), false);
    CoA::RenderPipeline::SetJitter(0.25f, -0.125f);
    CHECK(CoA::RenderPipeline::Current().jitterX == 0.25f && CoA::RenderPipeline::Current().jitterY == -0.125f);
    CoA::RenderPipeline::BeginFrame(CameraFacing(1, 0, 0), false);
    CHECK(CoA::RenderPipeline::Current().jitterX == 0.0f && CoA::RenderPipeline::Current().jitterY == 0.0f);
}

COA_TEST(RenderPipeline_MotionCVarQueuedOff) {
    CoA::CVars::Reset();
    CoA::RenderPipelineClient::QueueCVars();
    size_t n = 0;
    const CoA::CVarEntry* e = CoA::CVars::Glue(n);
    const CoA::CVarEntry* v = nullptr;
    for (size_t i = 0; i < n; ++i) if (!strcmp(e[i].name, "coaRenderMotion")) v = &e[i];
    CHECK(v && !strcmp(v->defaultValue, "0") && v->flags == 1 && v->category == 1);
    CoA::CVars::Reset();
}
