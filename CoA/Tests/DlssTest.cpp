#include "CoA/Shaders/DepthRemapPS.h"
#include "CoA/Source/Runtime/Dlss.h"
#if __has_include(<sl_consts.h>)
#include "CoA/Source/Runtime/DlssFrame.h"
#endif
#include "CoA/Source/Runtime/RenderMotion.h"
#include "CoA/Tests/RenderMotionHelpers.h"
#include "CoA/Tests/TestMain.h"
#include <cmath>
#include <cstring>
#include <vector>

namespace {
    using namespace CoA::TestHelpers;
    using CoA::Dlss::Mode;
}

COA_TEST(Dlss_ModeFromCVarClamps) {
    CHECK(CoA::Dlss::ModeFromCVar(-1) == Mode::Off);
    CHECK(CoA::Dlss::ModeFromCVar(6) == Mode::Off);
    CHECK(CoA::Dlss::ModeFromCVar(0) == Mode::Off);
    CHECK(CoA::Dlss::ModeFromCVar(2) == Mode::Quality);
    CHECK(CoA::Dlss::ModeFromCVar(5) == Mode::UltraPerformance);
}

COA_TEST(Dlss_FallbackSizes) {
    uint32_t w = 0, h = 0;
    CoA::Dlss::FallbackRenderSize(Mode::Quality, 2560, 1440, w, h);
    CHECK(w == 1707 && h == 960);
    CoA::Dlss::FallbackRenderSize(Mode::Performance, 2560, 1440, w, h);
    CHECK(w == 1280 && h == 720);
    CoA::Dlss::FallbackRenderSize(Mode::Dlaa, 2560, 1440, w, h);
    CHECK(w == 2560 && h == 1440);
    CoA::Dlss::FallbackRenderSize(Mode::UltraPerformance, 2560, 1440, w, h);
    CHECK(w == 853 && h == 480);
    CoA::Dlss::FallbackRenderSize(Mode::UltraPerformance, 1, 1, w, h);
    CHECK(w == 1 && h == 1);
}

COA_TEST(Dlss_LodBias) {
    CHECK(Near(CoA::Dlss::LodBias(1280, 2560), -1.0f, 1e-6f));
    CHECK(Near(CoA::Dlss::LodBias(2560, 2560), 0.0f, 1e-6f));
}

COA_TEST(Dlss_ClipToPrevClipMatchesStaticMotion) {
    float r[3];
    Right(0, r);
    Frame f = FrameWith(Cam(0, 0, 0, Yaw(0)), Cam(r[0] * 1.5f, r[1] * 1.5f, 0.5f, Yaw(0.08f)));
    const auto s = CoA::Dlss::BuildMatrices(f);
    const float uvs[3][2] = { { 0.5f, 0.5f }, { 0.25f, 0.7f }, { 0.8f, 0.3f } };
    for (const auto& uv : uvs) {
        const float depth = DepthAt(f, 12.0f);
        const float ndcD3d = (depth - f.depthMin) / (f.depthMax - f.depthMin);
        const float clip[4] = { uv[0] * 2.0f - 1.0f, 1.0f - uv[1] * 2.0f, ndcD3d, 1.0f };   // D3D z: the remapped depth DLSS sees
        float prev[4];
        Transform(clip, s.clipToPrevClip, prev);
        const float pu = (prev[0] / prev[3] + 1.0f) * 0.5f;
        const float pv = (1.0f - prev[1] / prev[3]) * 0.5f;
        const auto m = CoA::RenderMotion::StaticMotion(f, uv[0], uv[1], depth);
        CHECK(Near(pu - uv[0], m.x, 1e-4f) && Near(pv - uv[1], m.y, 1e-4f));
    }
    const Mat4 id = Mul(s.cameraViewToClip, s.clipToCameraView);
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) CHECK(Near(id.m[i][j], i == j ? 1.0f : 0.0f, 1e-4f));
}

COA_TEST(Dlss_PrevClipToClipIsInverse) {
    float r[3];
    Right(0, r);
    Frame f = FrameWith(Cam(0, 0, 0, Yaw(0)), Cam(r[0], r[1], 0.25f, Yaw(0.1f)));
    const auto s = CoA::Dlss::BuildMatrices(f);
    const Mat4 p = Mul(s.clipToPrevClip, s.prevClipToClip);
    for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) CHECK(Near(p.m[i][j], i == j ? 1.0f : 0.0f, 1e-3f));
}

COA_TEST(Dlss_DepthRemap) {
    Frame f = FrameWith(Cam(0, 0, 0, Yaw(0)), Cam(0, 0, 0, Yaw(0)), 0.0f, 0.94f);
    auto d = CoA::Dlss::DepthRemapFor(f);
    CHECK(Near(0.47f * d.scale + d.offset, 0.5f, 1e-5f));
    CHECK(Near(0.94f * d.scale + d.offset, 1.0f, 1e-5f));
    Frame g = FrameWith(Cam(0, 0, 0, Yaw(0)), Cam(0, 0, 0, Yaw(0)), 0.5f, 0.5f);
    d = CoA::Dlss::DepthRemapFor(g);
    CHECK(d.scale == 0.0f && d.offset == 1.0f);
}

COA_TEST(Dlss_CameraViewToClipUsesD3dDepth) {
    Frame f = FrameWith(Cam(0, 0, 0, Yaw(0)), Cam(0, 0, 0, Yaw(0)));
    const auto s = CoA::Dlss::BuildMatrices(f);
    const float nearP[4] = { 0.0f, 0.0f, kNear, 1.0f };   // view space: z forward
    const float farP[4] = { 0.0f, 0.0f, kFar, 1.0f };
    float c[4];
    Transform(nearP, s.cameraViewToClip, c);
    CHECK(Near(c[2] / c[3], 0.0f, 1e-4f));
    Transform(farP, s.cameraViewToClip, c);
    CHECK(Near(c[2] / c[3], 1.0f, 1e-4f));
}

COA_TEST(Dlss_RenderSizeRectRoundTrip) {
    const auto r = CoA::Dlss::RenderRect(1707, 960);
    CHECK(r.left == 0 && r.top == 0 && r.right == 1707 && r.bottom == 960);
    uint32_t w = 0, h = 0;
    CoA::Dlss::FallbackRenderSize(Mode::Performance, 2560, 1440, w, h);
    const auto p = CoA::Dlss::RenderRect(w, h);
    CHECK(static_cast<uint32_t>(p.right - p.left) == w && static_cast<uint32_t>(p.bottom - p.top) == h);
}

COA_TEST(Dlss_EmbeddedDepthRemapIsPs3) {
    auto token = [](const unsigned char* b) { return uint32_t(b[0]) | uint32_t(b[1]) << 8 | uint32_t(b[2]) << 16 | uint32_t(b[3]) << 24; };
    CHECK(token(kDepthRemapPS) == 0xFFFF0300u);   // ps_3_0
}

COA_TEST(Dlss_SlModeValues) {
    // sl::DLSSMode: eOff 0, eMaxPerformance 1, eBalanced 2, eMaxQuality 3, eUltraPerformance 4, eUltraQuality 5, eDLAA 6.
    CHECK(CoA::Dlss::SlMode(Mode::Off) == 0);
    CHECK(CoA::Dlss::SlMode(Mode::Performance) == 1);
    CHECK(CoA::Dlss::SlMode(Mode::Balanced) == 2);
    CHECK(CoA::Dlss::SlMode(Mode::Quality) == 3);
    CHECK(CoA::Dlss::SlMode(Mode::UltraPerformance) == 4);
    CHECK(CoA::Dlss::SlMode(Mode::Dlaa) == 6);
}

#if __has_include(<sl_consts.h>)
COA_TEST(Dlss_ConstantsBlobLayout) {
    float r[3];
    Right(0, r);
    Frame f = FrameWith(Cam(0, 0, 0, Yaw(0)), Cam(r[0], r[1], 0.25f, Yaw(0.1f)));
    f.jitterX = 0.25f;
    f.jitterY = -0.375f;
    f.reset = false;

    CoA::Dlss::FrameSetup s{};
    s.frameIndex = 1234;
    s.viewport = 0;
    s.mode = CoA::Dlss::SlMode(Mode::Quality);
    s.renderWidth = 1707;
    s.renderHeight = 960;
    s.displayWidth = 2560;
    s.displayHeight = 1440;
    s.generation = 7;
    s.mvecSign = -1.0f;
    s.jitterSign = -1.0f;
    s.jittered = true;
    s.reset = true;   // a mode / size change resets even when the frame does not

    CoA::Dlss::FrameBlob blob;
    CoA::Dlss::FillFrameBlob(blob, f, s);

    // DXVK copies the blob into a byte vector and hands the callback its data pointer.
    std::vector<uint8_t> bytes(reinterpret_cast<const uint8_t*>(&blob), reinterpret_cast<const uint8_t*>(&blob) + sizeof(blob));
    const auto* b = reinterpret_cast<const CoA::Dlss::FrameBlob*>(bytes.data());

    CHECK(b->frameIndex == 1234 && b->viewport == 0 && b->mode == 3 && b->generation == 7);
    CHECK(b->renderWidth == 1707 && b->renderHeight == 960 && b->displayWidth == 2560 && b->displayHeight == 1440);
    const sl::Constants& c = b->constants;
    CHECK(c.next == nullptr && c.structType == sl::Constants::s_structType);
    CHECK(c.jitterOffset.x == -0.25f && c.jitterOffset.y == 0.375f);
    CHECK(c.mvecScale.x == -1.0f && c.mvecScale.y == -1.0f);
    CHECK(c.reset == sl::Boolean::eTrue);
    CHECK(c.depthInverted == sl::Boolean::eFalse && c.cameraMotionIncluded == sl::Boolean::eTrue);
    CHECK(c.motionVectorsJittered == sl::Boolean::eFalse && c.motionVectors3D == sl::Boolean::eFalse);
    CHECK(c.cameraPos.x == f.cur.pos[0] && c.cameraPos.y == f.cur.pos[1] && c.cameraPos.z == f.cur.pos[2]);
    CHECK(c.cameraFwd.x == f.cur.forward[0] && c.cameraUp.z == f.cur.up[2] && c.cameraRight.y == f.cur.right[1]);
    CHECK(c.cameraNear == kNear && c.cameraFar == kFar && c.cameraFOV == kFovY && c.cameraAspectRatio == kAspect);

    const auto m = CoA::Dlss::BuildMatrices(f);
    CHECK(memcmp(&c.cameraViewToClip, &m.cameraViewToClip, 64) == 0);
    CHECK(memcmp(&c.clipToCameraView, &m.clipToCameraView, 64) == 0);
    CHECK(memcmp(&c.clipToPrevClip, &m.clipToPrevClip, 64) == 0);
    CHECK(memcmp(&c.prevClipToClip, &m.prevClipToClip, 64) == 0);
    CHECK(c.clipToLensClip.row[0].x == 1.0f && c.clipToLensClip.row[3].w == 1.0f && c.clipToLensClip.row[0].y == 0.0f);

    // An unjittered render reports no jitter; Frame::reset alone also resets.
    s.jittered = false;
    s.reset = false;
    f.reset = true;
    CoA::Dlss::FillFrameBlob(blob, f, s);
    CHECK(blob.constants.jitterOffset.x == 0.0f && blob.constants.jitterOffset.y == 0.0f);
    CHECK(blob.constants.reset == sl::Boolean::eTrue);
    f.reset = false;
    CoA::Dlss::FillFrameBlob(blob, f, s);
    CHECK(blob.constants.reset == sl::Boolean::eFalse);
}
#endif
