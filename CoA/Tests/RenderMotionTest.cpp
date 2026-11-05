#include "CoA/Shaders/BiasMaskPS.h"
#include "CoA/Shaders/MotionKeyPS.h"
#include "CoA/Shaders/MotionPS.h"
#include "CoA/Shaders/MotionViewPS.h"
#include "CoA/Shaders/SkinnedMotionVS.h"
#include "CoA/Shaders/StaticMotionPS.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/MotionCache.h"
#include "CoA/Source/Runtime/RenderMotion.h"
#include "CoA/Source/Runtime/RenderPipeline.h"
#include "CoA/Tests/RenderMotionHelpers.h"
#include "CoA/Tests/TestMain.h"
#include <cmath>
#include <cstring>

namespace {
    using CoA::RenderPipeline::Camera;
    using CoA::RenderPipeline::Mat4;
    using namespace CoA::TestHelpers;

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
    static Mat4 big[CoA::MotionCache::kMaxModelBones + 1];
    CHECK(!CoA::MotionCache::Store({ &s_m1, 7 }, 10, Mat(0), big, CoA::MotionCache::kMaxModelBones + 1));
    CHECK(!CoA::MotionCache::Store({ &s_m1, 7 }, 10, Mat(0), big, 0));
    CHECK(CoA::MotionCache::Size() == 0);
    CHECK(CoA::MotionCache::Store({ &s_m1, 7 }, 10, Mat(0), big, CoA::MotionCache::kMaxModelBones));
    CHECK(CoA::MotionCache::Size() == 1);
}

COA_TEST(MotionCache_StoresModelsAbovePaletteSlots) {   // character / creature models have more bones than 85 slots
    CoA::MotionCache::Clear();
    static Mat4 bones[200];
    for (int i = 0; i < 200; ++i) {
        bones[i] = Mat(static_cast<float>(i + 1));
    }
    CHECK(CoA::MotionCache::Store({ &s_m1, 7 }, 10, Mat(0), bones, 200));
    uint32_t n = 0;
    const Mat4* p = CoA::MotionCache::Previous({ &s_m1, 7 }, 11, n);
    CHECK(p && n == 200 && p[0].m[0][0] == 1.0f && p[85].m[0][0] == 86.0f && p[199].m[0][0] == 200.0f);
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
    CHECK(token(kMotionKeyPS) == 0xFFFF0300u);
}

// Alpha-keyed batches join the model pass unless their first texture's UVs animate; blended ones never do.
COA_TEST(RenderMotion_KeyedBatchUsable) {
    using CoA::RenderMotion::BatchKind;
    using CoA::RenderMotion::ClassifyBatch;
    CHECK(ClassifyBatch(0, 5, 3) == BatchKind::Opaque);
    CHECK(ClassifyBatch(1, 0xFFFF, 3) == BatchKind::Keyed);   // no texture transform
    CHECK(ClassifyBatch(1, 3, 3) == BatchKind::Keyed);        // index past the transform table: identity
    CHECK(ClassifyBatch(1, 2, 3) == BatchKind::Skip);         // animated UVs
    for (uint32_t blend = 2; blend < 7; ++blend) {
        CHECK(ClassifyBatch(blend, 0xFFFF, 3) == BatchKind::Skip);
    }
}

// CM2SceneRender::SetupMaterial's alpha-key reference: element alpha x 224/255.
COA_TEST(RenderMotion_KeyAlphaRef) {
    CHECK(CoA::RenderMotion::KeyAlphaRef(1.0f) == 0.87843138f);
    CHECK(CoA::RenderMotion::KeyAlphaRef(0.5f) == 0.5f * 0.87843138f);
    CHECK(CoA::RenderMotion::KeyAlphaRef(0.0f) == 0.0f);
}

COA_TEST(RenderMotion_ModelEligibility) {
    // Drawn only when animated && !culled && havePrevious && (!staticSimple || worldChanged).
    for (int bits = 0; bits < 32; ++bits) {
        const bool animated = bits & 1, culled = bits & 2, staticSimple = bits & 4, worldChanged = bits & 8, havePrevious = bits & 16;
        const bool want = animated && !culled && havePrevious && (!staticSimple || worldChanged);
        CHECK(CoA::RenderMotion::ModelNeedsPass(animated, culled, staticSimple, worldChanged, havePrevious) == want);
    }
}

// Batches sharing a model and skin section share one palette slot; a full palette refuses new pairs.
COA_TEST(RenderMotion_PaletteSlotsShareSections) {
    CoA::RenderMotion::PaletteSlots slots(2);
    int m1, m2, s1, s2;
    uint32_t slot = 99;
    bool fresh = false;
    CHECK(slots.Assign(&m1, &s1, slot, fresh) && slot == 0 && fresh);
    CHECK(slots.Assign(&m1, &s1, slot, fresh) && slot == 0 && !fresh);
    CHECK(slots.Assign(&m1, &s2, slot, fresh) && slot == 1 && fresh);
    CHECK(!slots.Assign(&m2, &s1, slot, fresh));
    CHECK(slots.Assign(&m1, &s2, slot, fresh) && slot == 1 && !fresh);
    CHECK(slots.Count() == 2);
    slots.Clear();
    CHECK(slots.Assign(&m2, &s1, slot, fresh) && slot == 0 && fresh);
}

// The smallest palette texture that holds a frame's texels; the largest holds every slot at the most bones.
COA_TEST(RenderMotion_PaletteBucket) {
    using namespace CoA::RenderMotion;
    CHECK(PaletteBucket(0) == 0);
    CHECK(PaletteBucket(32 * 1024) == 0);
    CHECK(PaletteBucket(32 * 1024 + 1) == 1);
    CHECK(PaletteBucket(100 * 1024) == 2);
    CHECK(PaletteBucket(256 * 1024) == 3);
    CHECK(PaletteBucket(256 * 1024 + 1) == kPaletteBuckets);
    CHECK(512u * 2u * 3u * 85u <= kPaletteBucketRows[kPaletteBuckets - 1] * kPaletteStreamWidth);
}
