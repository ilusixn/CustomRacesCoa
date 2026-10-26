#include "CoA/Shaders/DepthViewPS.h"
#include "CoA/Shaders/FullscreenVS.h"
#include "CoA/Source/Runtime/RenderPipeline.h"
#include "CoA/Tests/TestMain.h"
#include <cmath>
#include <cstring>

namespace {
    using CoA::RenderPipeline::Mat4;
    using CoA::RenderPipeline::Camera;

    bool Near(float a, float b) {
        return std::fabs(a - b) < 1e-5f;
    }

    Mat4 Identity() {
        Mat4 m{};
        for (int i = 0; i < 4; ++i) {
            m.m[i][i] = 1.0f;
        }
        return m;
    }

    Camera CameraAt(float x, float y, float z) {
        Camera c{};
        c.view = Identity();
        c.proj = Identity();
        c.pos[0] = x;
        c.pos[1] = y;
        c.pos[2] = z;
        return c;
    }

    Mat4 SampleProjection() {
        Mat4 p{};
        p.m[0][0] = 1.2f;
        p.m[1][1] = 2.1f;
        p.m[2][0] = 0.01f;
        p.m[2][1] = -0.02f;
        p.m[2][2] = 1.001f;
        p.m[2][3] = 1.0f;
        p.m[3][2] = -0.1f;
        return p;
    }
}

COA_TEST(RenderPipeline_InactiveReasonsInOrder) {
    using namespace CoA::RenderPipeline;
    CHECK(WhyInactive({ false, 1, true, true }) == Inactive::CVarOff);
    CHECK(WhyInactive({ true, 4, true, true }) == Inactive::Multisample);
    CHECK(WhyInactive({ true, 1, false, true }) == Inactive::NoIntz);
    CHECK(WhyInactive({ true, 1, true, false }) == Inactive::NoShaders);
    CHECK(WhyInactive({ true, 1, true, true }) == Inactive::None);
    CHECK(WhyInactive({ true, 0, true, true }) == Inactive::None);   // 0 and 1 both mean "no MSAA"
}

COA_TEST(RenderPipeline_FirstFrameIsReset) {
    CoA::RenderPipeline::Reset();
    CoA::RenderPipeline::BeginFrame(CameraAt(0, 0, 0), false);
    CHECK(CoA::RenderPipeline::Current().reset);
    CHECK(CoA::RenderPipeline::Current().index == 1);
}

COA_TEST(RenderPipeline_SecondFrameKeepsPreviousCamera) {
    CoA::RenderPipeline::Reset();
    CoA::RenderPipeline::BeginFrame(CameraAt(0, 0, 0), false);
    CoA::RenderPipeline::BeginFrame(CameraAt(1, 0, 0), false);
    const auto& f = CoA::RenderPipeline::Current();
    CHECK(!f.reset);
    CHECK(f.prev.pos[0] == 0.0f && f.cur.pos[0] == 1.0f);
}

COA_TEST(RenderPipeline_TeleportAndForcedResetSetReset) {
    CoA::RenderPipeline::Reset();
    CoA::RenderPipeline::BeginFrame(CameraAt(0, 0, 0), false);
    CoA::RenderPipeline::BeginFrame(CameraAt(51, 0, 0), false);
    CHECK(CoA::RenderPipeline::Current().reset);
    CoA::RenderPipeline::BeginFrame(CameraAt(52, 0, 0), true);
    CHECK(CoA::RenderPipeline::Current().reset);
    CoA::RenderPipeline::BeginFrame(CameraAt(53, 0, 0), false);
    CHECK(!CoA::RenderPipeline::Current().reset);
}

COA_TEST(RenderPipeline_HaltonFirstValues) {
    float x, y;
    CoA::RenderPipeline::Halton23(1, x, y);
    CHECK(Near(x, 0.0f) && Near(y, -0.5f + 1.0f / 3.0f));          // 1/2 - 0.5, 1/3 - 0.5
    CoA::RenderPipeline::Halton23(2, x, y);
    CHECK(Near(x, -0.25f) && Near(y, -0.5f + 2.0f / 3.0f));
    for (uint32_t i = 1; i < 64; ++i) {
        CoA::RenderPipeline::Halton23(i, x, y);
        CHECK(x >= -0.5f && x < 0.5f && y >= -0.5f && y < 0.5f);
    }
}

COA_TEST(RenderPipeline_PhaseCount) {
    CHECK(CoA::RenderPipeline::JitterPhaseCount(2560, 2560) == 8);
    CHECK(CoA::RenderPipeline::JitterPhaseCount(1280, 2560) == 32);
    CHECK(CoA::RenderPipeline::JitterPhaseCount(1707, 2560) == 18);   // 8 * 1.4997^2 = 17.99
}

COA_TEST(RenderPipeline_ZeroJitterLeavesProjectionBitIdentical) {
    Mat4 p = SampleProjection();
    Mat4 q = CoA::RenderPipeline::ApplyJitter(p, 0.0f, 0.0f, 1920, 1080);
    CHECK(memcmp(&p, &q, sizeof p) == 0);
}

COA_TEST(RenderPipeline_JitterOffsetsClipSpaceByPixels) {
    Mat4 p = SampleProjection();
    Mat4 q = CoA::RenderPipeline::ApplyJitter(p, 0.5f, -0.25f, 1920, 1080);
    CHECK(Near(q.m[2][0], p.m[2][0] + 2.0f * 0.5f / 1920.0f));
    CHECK(Near(q.m[2][1], p.m[2][1] - 2.0f * -0.25f / 1080.0f));      // y flips: +pixel y is down, +clip y is up
    CHECK(q.m[0][0] == p.m[0][0] && q.m[3][2] == p.m[3][2]);
}

COA_TEST(RenderPipeline_CopyRectFullAndLetterbox) {
    auto r = CoA::RenderPipeline::CopyRect(0.0f, 1.0f, 0.0f, 1.0f, 1920, 1080);
    CHECK(r.left == 0 && r.top == 0 && r.right == 1920 && r.bottom == 1080);
    r = CoA::RenderPipeline::CopyRect(0.0f, 1.0f, 0.1f, 0.9f, 1920, 1000);
    CHECK(r.left == 0 && r.top == 100 && r.right == 1920 && r.bottom == 900);
}

COA_TEST(RenderPipeline_EmbeddedShadersAreSm3Bytecode) {
    auto token = [](const unsigned char* b) { return uint32_t(b[0]) | uint32_t(b[1]) << 8 | uint32_t(b[2]) << 16 | uint32_t(b[3]) << 24; };
    CHECK(token(kFullscreenVS) == 0xFFFE0300u);   // vs_3_0
    CHECK(token(kDepthViewPS) == 0xFFFF0300u);    // ps_3_0
}
