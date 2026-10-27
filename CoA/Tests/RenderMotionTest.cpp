#include "CoA/Shaders/BiasMaskPS.h"
#include "CoA/Shaders/MotionPS.h"
#include "CoA/Shaders/MotionViewPS.h"
#include "CoA/Shaders/SkinnedMotionVS.h"
#include "CoA/Shaders/StaticMotionPS.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/MotionCache.h"
#include "CoA/Source/Runtime/RenderMotion.h"
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

// --- Motion reference math (RenderMotion) ---------------------------------------------------------------------------

namespace {
    using CoA::RenderPipeline::Frame;

    constexpr float kFovY = 0.6f * 1.57f;
    constexpr float kAspect = 1.6f;
    constexpr float kNear = 0.2f;
    constexpr float kFar = 777.0f;

    bool Near(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) <= eps; }
    float Yaw(float y) { return y; }

    // Row-vector product: a is applied first.
    Mat4 Mul(const Mat4& a, const Mat4& b) {
        Mat4 r{};
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                for (int k = 0; k < 4; ++k) r.m[i][j] += a.m[i][k] * b.m[k][j];
        return r;
    }

    Mat4 Translate(float x, float y, float z) {
        Mat4 m = Identity();
        m.m[3][0] = x; m.m[3][1] = y; m.m[3][2] = z;
        return m;
    }

    // WoW world: z up. Camera forward for a yaw, and its screen-right axis (the view x axis = cross(forward, up), see
    // GxuXformCreateLookAtSgCompat; for yaw 0 that is -y, not +x).
    void Forward(float yaw, float f[3]) { f[0] = std::cos(yaw); f[1] = std::sin(yaw); f[2] = 0.0f; }
    void Right(float yaw, float r[3]) { r[0] = std::sin(yaw); r[1] = -std::cos(yaw); r[2] = 0.0f; }

    // GxuXformCreateProjection_Exact(FOV * 0.6, aspect, near, far).
    Mat4 CamProj() {
        const float halfH = std::tan(kFovY * 0.5f) * kNear;
        const float halfW = halfH * kAspect;
        Mat4 m{};
        m.m[0][0] = kNear / halfW;
        m.m[1][1] = kNear / halfH;
        m.m[2][2] = (kNear + kFar) / (kFar - kNear);
        m.m[3][2] = kNear * (kFar * -2.0f) / (kFar - kNear);
        m.m[2][3] = 1.0f;
        return m;
    }

    // GxuXformCreateLookAtSgCompat(eye at origin, forward, up = z).
    Mat4 CamView(float yaw) {
        float zv[3];
        Forward(yaw, zv);
        const float up[3] = { 0, 0, 1 };
        float xv[3] = { zv[1] * up[2] - zv[2] * up[1], zv[2] * up[0] - zv[0] * up[2], zv[0] * up[1] - zv[1] * up[0] };
        const float xl = std::sqrt(xv[0] * xv[0] + xv[1] * xv[1] + xv[2] * xv[2]);
        for (float& c : xv) c /= xl;
        float yv[3] = { xv[1] * zv[2] - xv[2] * zv[1], xv[2] * zv[0] - xv[0] * zv[2], xv[0] * zv[1] - xv[1] * zv[0] };
        const float yl = std::sqrt(yv[0] * yv[0] + yv[1] * yv[1] + yv[2] * yv[2]);
        for (float& c : yv) c /= yl;
        Mat4 m = Identity();
        for (int i = 0; i < 3; ++i) {
            m.m[i][0] = xv[i];
            m.m[i][1] = yv[i];
            m.m[i][2] = zv[i];
        }
        return m;
    }

    Camera Cam(float x, float y, float z, float yaw) {
        Camera c{};
        c.view = CamView(yaw);
        c.proj = CamProj();
        c.pos[0] = x; c.pos[1] = y; c.pos[2] = z;
        Forward(yaw, c.forward);
        c.up[2] = 1.0f;
        Right(yaw, c.right);
        c.nearZ = kNear; c.farZ = kFar; c.fovY = kFovY; c.aspect = kAspect;
        return c;
    }

    Frame FrameWith(const Camera& prev, const Camera& cur, float depthMin = 0.0f, float depthMax = 0.94f) {
        Frame f{};
        f.prev = prev; f.cur = cur;
        f.depthMin = depthMin; f.depthMax = depthMax;
        return f;
    }

    // Stored depth of a point `yards` straight ahead.
    float DepthAt(const Frame& f, float yards) {
        const Mat4& p = f.cur.proj;
        const float ndcGl = (yards * p.m[2][2] + p.m[3][2]) / (yards * p.m[2][3]);
        return f.depthMin + (ndcGl + 1.0f) * 0.5f * (f.depthMax - f.depthMin);   // the device's D3D depth through the viewport
    }

    void CurClip(const Mat4& bone, const Frame& f, const float p[3], float out[4]) {
        const Mat4 m = Mul(bone, f.cur.proj);
        const float v[4] = { p[0], p[1], p[2], 1.0f };
        for (int c = 0; c < 4; ++c) out[c] = v[0] * m.m[0][c] + v[1] * m.m[1][c] + v[2] * m.m[2][c] + v[3] * m.m[3][c];
    }
    float CurU(const Frame& f, const Mat4& bone, const float p[3]) { float c[4]; CurClip(bone, f, p, c); return (c[0] / c[3] + 1.0f) * 0.5f; }
    float CurV(const Frame& f, const Mat4& bone, const float p[3]) { float c[4]; CurClip(bone, f, p, c); return (1.0f - c[1] / c[3]) * 0.5f; }
    float CurDepth(const Frame& f, const Mat4& bone, const float p[3]) {
        float c[4];
        CurClip(bone, f, p, c);
        return f.depthMin + (c[2] / c[3] + 1.0f) * 0.5f * (f.depthMax - f.depthMin);
    }

    // Previous camera at the origin, current camera one yard along its right axis.
    Frame SidestepFrame(float depthMin = 0.0f, float depthMax = 0.94f) {
        float r[3];
        Right(0, r);
        return FrameWith(Cam(0, 0, 0, Yaw(0)), Cam(r[0], r[1], r[2], Yaw(0)), depthMin, depthMax);
    }
}

COA_TEST(StaticMotion_StillCameraIsZero) {
    auto f = FrameWith(Cam(0, 0, 0, Yaw(0)), Cam(0, 0, 0, Yaw(0)));
    auto m = CoA::RenderMotion::StaticMotion(f, 0.3f, 0.7f, 0.9f);
    CHECK(Near(m.x, 0, 1e-4f) && Near(m.y, 0, 1e-4f));
}

COA_TEST(StaticMotion_SidestepMovesPointsOpposite) {
    auto f = SidestepFrame();
    auto m = CoA::RenderMotion::StaticMotion(f, 0.5f, 0.5f, DepthAt(f, 10.0f));
    CHECK(m.x > 0 && Near(m.y, 0, 1e-4f));   // camera moved right: the point was further right on screen last frame
}

COA_TEST(StaticMotion_DepthRangeHonoured) {
    auto a = SidestepFrame(0.1f, 0.94f);
    auto b = SidestepFrame(0.0f, 1.0f);
    auto ma = CoA::RenderMotion::StaticMotion(a, 0.4f, 0.5f, DepthAt(a, 10.0f));
    auto mb = CoA::RenderMotion::StaticMotion(b, 0.4f, 0.5f, DepthAt(b, 10.0f));
    CHECK(Near(ma.x, mb.x, 1e-4f) && Near(ma.y, mb.y, 1e-4f) && ma.x > 0);
}

COA_TEST(ModelMotion_StaticModelUnderStillCameraIsZero) {
    float fw[3];
    Forward(0, fw);
    Mat4 bone = Mul(Translate(fw[0] * 10, fw[1] * 10, fw[2] * 10), CamView(Yaw(0)));   // world * view
    float p[3] = { 0.5f, 0.2f, 0.0f };
    auto m = CoA::RenderMotion::ModelMotion(bone, bone, p, CamProj(), CamProj());
    CHECK(Near(m.x, 0) && Near(m.y, 0));
}

COA_TEST(ModelMotion_CameraMotionIncludedOnce) {     // model still in the world, camera sidestepped 1 yd to its right
    auto f = SidestepFrame();
    float fw[3], r[3];
    Forward(0, fw);
    Right(0, r);
    Mat4 world = Translate(fw[0] * 10, fw[1] * 10, fw[2] * 10);                       // camera-relative to the prev eye
    Mat4 prevBone = Mul(world, f.prev.view);
    Mat4 curBone = Mul(Mul(world, Translate(-r[0], -r[1], -r[2])), f.cur.view);       // same point, re-centred on new eye
    float p[3] = { 0, 0, 0 };
    auto mm = CoA::RenderMotion::ModelMotion(curBone, prevBone, p, f.cur.proj, f.prev.proj);
    auto ms = CoA::RenderMotion::StaticMotion(f, CurU(f, curBone, p), CurV(f, curBone, p), CurDepth(f, curBone, p));
    CHECK(Near(mm.x, ms.x, 1e-4f) && Near(mm.y, ms.y, 1e-4f) && mm.x > 0);
}

COA_TEST(RenderMotion_InverseRoundTrip) {
    Mat4 m = Mul(CamView(Yaw(0.7f)), CamProj());
    Mat4 i = Mul(m, CoA::RenderMotion::Inverse(m));
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) CHECK(Near(i.m[r][c], r == c ? 1.0f : 0.0f, 1e-4f));
}

COA_TEST(StaticMotion_SkyIgnoresTranslation) {
    auto f = SidestepFrame();
    auto m = CoA::RenderMotion::StaticMotion(f, 0.3f, 0.6f, f.depthMax);
    CHECK(Near(m.x, 0, 1e-4f) && Near(m.y, 0, 1e-4f));
    auto g = FrameWith(Cam(0, 0, 0, Yaw(0)), Cam(0, 0, 0, Yaw(0.1f)));
    auto n = CoA::RenderMotion::StaticMotion(g, 0.5f, 0.5f, g.depthMax);
    CHECK(std::fabs(n.x) > 1e-3f);
}

COA_TEST(StaticMotion_MagnitudeMatchesHandComputation) {
    auto f = SidestepFrame();
    auto m = CoA::RenderMotion::StaticMotion(f, 0.5f, 0.5f, DepthAt(f, 10.0f));
    CHECK(Near(m.x, 0.5f * CamProj().m[0][0] * 0.1f, 1e-4f));
}

COA_TEST(StaticMotion_CameraUpMovesPointsDown) {
    auto f = FrameWith(Cam(0, 0, 0, Yaw(0)), Cam(0, 0, 1, Yaw(0)));
    auto m = CoA::RenderMotion::StaticMotion(f, 0.5f, 0.5f, DepthAt(f, 10.0f));
    CHECK(m.y < 0 && Near(m.x, 0, 1e-4f));
}

COA_TEST(StaticMotion_EmptyDepthRangeIsZero) {
    auto f = SidestepFrame(0.5f, 0.5f);
    auto m = CoA::RenderMotion::StaticMotion(f, 0.5f, 0.5f, 0.5f);
    CHECK(m.x == 0 && m.y == 0);
}

namespace {
    Mat4 Mat(float k) {
        Mat4 m{};
        m.m[0][0] = k;
        return m;
    }
    int s_m1, s_m2;
}

COA_TEST(MotionCache_PreviousIsLastFrame) {
    CoA::MotionCache::Clear();
    Mat4 a[2] = { Mat(1), Mat(2) }, b[2] = { Mat(3), Mat(4) };
    CHECK(CoA::MotionCache::Store({ &s_m1, 7 }, 10, Mat(0), a, 2));
    uint32_t n = 0;
    CHECK(CoA::MotionCache::Previous({ &s_m1, 7 }, 10, n) == nullptr);
    CoA::MotionCache::EndFrame(10);
    CHECK(CoA::MotionCache::Store({ &s_m1, 7 }, 11, Mat(0), b, 2));
    const Mat4* p = CoA::MotionCache::Previous({ &s_m1, 7 }, 11, n);
    CHECK(p && n == 2 && p[0].m[0][0] == 1.0f && p[1].m[0][0] == 2.0f);
    // Rolls on across further frames.
    CoA::MotionCache::EndFrame(11);
    CHECK(CoA::MotionCache::Store({ &s_m1, 7 }, 12, Mat(0), a, 2));
    p = CoA::MotionCache::Previous({ &s_m1, 7 }, 12, n);
    CHECK(p && p[0].m[0][0] == 3.0f);
}

COA_TEST(MotionCache_GapDropsHistory) {
    CoA::MotionCache::Clear();
    Mat4 a[1] = { Mat(1) };
    CHECK(CoA::MotionCache::Store({ &s_m1, 7 }, 10, Mat(0), a, 1));
    uint32_t n = 0;
    CHECK(CoA::MotionCache::Previous({ &s_m1, 7 }, 12, n) == nullptr);
}

COA_TEST(MotionCache_HandleChangeDropsHistory) {
    CoA::MotionCache::Clear();
    Mat4 a[1] = { Mat(1) };
    CHECK(CoA::MotionCache::Store({ &s_m1, 7 }, 10, Mat(0), a, 1));
    uint32_t n = 0;
    CHECK(CoA::MotionCache::Previous({ &s_m1, 8 }, 11, n) == nullptr);
    // Storing under the new handle discards the old history for good.
    CHECK(CoA::MotionCache::Store({ &s_m1, 8 }, 11, Mat(0), a, 1));
    CHECK(CoA::MotionCache::Previous({ &s_m1, 8 }, 11, n) == nullptr);
}

COA_TEST(MotionCache_RejectsOversizedPalette) {
    CoA::MotionCache::Clear();
    static Mat4 big[CoA::MotionCache::kMaxBones + 1];
    CHECK(!CoA::MotionCache::Store({ &s_m1, 7 }, 10, Mat(0), big, CoA::MotionCache::kMaxBones + 1));
    CHECK(!CoA::MotionCache::Store({ &s_m1, 7 }, 10, Mat(0), big, 0));
    CHECK(CoA::MotionCache::Size() == 0);
    CHECK(CoA::MotionCache::Store({ &s_m1, 7 }, 10, Mat(0), big, CoA::MotionCache::kMaxBones));
    CHECK(CoA::MotionCache::Size() == 1);
}

COA_TEST(MotionCache_EndFrameEvictsUnseen) {
    CoA::MotionCache::Clear();
    Mat4 a[1] = { Mat(1) };
    CHECK(CoA::MotionCache::Store({ &s_m1, 1 }, 10, Mat(0), a, 1));
    CHECK(CoA::MotionCache::Store({ &s_m2, 1 }, 10, Mat(0), a, 1));
    CoA::MotionCache::EndFrame(10);
    CHECK(CoA::MotionCache::Size() == 2);
    CHECK(CoA::MotionCache::Store({ &s_m1, 1 }, 11, Mat(0), a, 1));
    CoA::MotionCache::EndFrame(11);
    CHECK(CoA::MotionCache::Size() == 1);
    uint32_t n = 0;
    CHECK(CoA::MotionCache::Previous({ &s_m1, 1 }, 12, n) != nullptr);
}

COA_TEST(MotionCache_PreviousWorldFollowsSameRules) {
    CoA::MotionCache::Clear();
    Mat4 a[1] = { Mat(1) };
    CHECK(CoA::MotionCache::Store({ &s_m1, 7 }, 10, Mat(42), a, 1));
    const Mat4* w = CoA::MotionCache::PreviousWorld({ &s_m1, 7 }, 11);
    CHECK(w && w->m[0][0] == 42.0f);
    CHECK(CoA::MotionCache::PreviousWorld({ &s_m1, 7 }, 12) == nullptr);
    CHECK(CoA::MotionCache::PreviousWorld({ &s_m1, 8 }, 11) == nullptr);
    CHECK(CoA::MotionCache::PreviousWorld({ &s_m1, 7 }, 10) == nullptr);
}

COA_TEST(RenderMotion_EmbeddedShadersAreSm3Bytecode) {
    auto token = [](const unsigned char* b) { return uint32_t(b[0]) | uint32_t(b[1]) << 8 | uint32_t(b[2]) << 16 | uint32_t(b[3]) << 24; };
    CHECK(token(kSkinnedMotionVS) == 0xFFFE0300u);  // vs_3_0
    CHECK(token(kStaticMotionPS) == 0xFFFF0300u);   // ps_3_0
    CHECK(token(kMotionPS) == 0xFFFF0300u);
    CHECK(token(kBiasMaskPS) == 0xFFFF0300u);
    CHECK(token(kMotionViewPS) == 0xFFFF0300u);
}

COA_TEST(RenderMotion_ModelEligibility) {
    // Drawn only when animated && !culled && havePrevious && (!staticSimple || worldChanged).
    for (int bits = 0; bits < 32; ++bits) {
        const bool animated = bits & 1, culled = bits & 2, staticSimple = bits & 4, worldChanged = bits & 8, havePrevious = bits & 16;
        const bool want = animated && !culled && havePrevious && (!staticSimple || worldChanged);
        CHECK(CoA::RenderMotion::ModelNeedsPass(animated, culled, staticSimple, worldChanged, havePrevious) == want);
    }
}
