#ifndef COA_TESTS_RENDERMOTIONHELPERS_H
#define COA_TESTS_RENDERMOTIONHELPERS_H

// Camera / frame helpers shared by the render-motion and DLSS tests (WoW world: z up, camera-relative, row-vector matrices).

#include "CoA/Source/Runtime/RenderPipeline.h"
#include <cmath>

namespace CoA::TestHelpers {
    using RenderPipeline::Camera;
    using RenderPipeline::Mat4;

    inline Mat4 Identity() {
        Mat4 m{};
        for (int i = 0; i < 4; ++i) {
            m.m[i][i] = 1.0f;
        }
        return m;
    }

    using RenderPipeline::Frame;

    constexpr float kFovY = 0.6f * 1.57f;
    constexpr float kAspect = 1.6f;
    constexpr float kNear = 0.2f;
    constexpr float kFar = 777.0f;

    inline bool Near(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) <= eps; }
    inline float Yaw(float y) { return y; }

    // Row-vector transform of a 4-vector by m.
    inline void Transform(const float v[4], const Mat4& m, float out[4]) {
        for (int c = 0; c < 4; ++c) out[c] = v[0] * m.m[0][c] + v[1] * m.m[1][c] + v[2] * m.m[2][c] + v[3] * m.m[3][c];
    }

    // Row-vector product: a is applied first.
    inline Mat4 Mul(const Mat4& a, const Mat4& b) {
        Mat4 r{};
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                for (int k = 0; k < 4; ++k) r.m[i][j] += a.m[i][k] * b.m[k][j];
        return r;
    }

    inline Mat4 Translate(float x, float y, float z) {
        Mat4 m = Identity();
        m.m[3][0] = x; m.m[3][1] = y; m.m[3][2] = z;
        return m;
    }

    // WoW world: z up. Camera forward for a yaw, and its screen-right axis (the view x axis = cross(forward, up), see
    // GxuXformCreateLookAtSgCompat; for yaw 0 that is -y, not +x).
    inline void Forward(float yaw, float f[3]) { f[0] = std::cos(yaw); f[1] = std::sin(yaw); f[2] = 0.0f; }
    inline void Right(float yaw, float r[3]) { r[0] = std::sin(yaw); r[1] = -std::cos(yaw); r[2] = 0.0f; }

    // GxuXformCreateProjection_Exact(FOV * 0.6, aspect, near, far).
    inline Mat4 CamProj() {
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
    inline Mat4 CamView(float yaw) {
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

    inline Camera Cam(float x, float y, float z, float yaw) {
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

    inline Frame FrameWith(const Camera& prev, const Camera& cur, float depthMin = 0.0f, float depthMax = 0.94f) {
        Frame f{};
        f.prev = prev; f.cur = cur;
        f.depthMin = depthMin; f.depthMax = depthMax;
        return f;
    }

    // Stored depth of a point `yards` straight ahead.
    inline float DepthAt(const Frame& f, float yards) {
        const Mat4& p = f.cur.proj;
        const float ndcGl = (yards * p.m[2][2] + p.m[3][2]) / (yards * p.m[2][3]);
        return f.depthMin + (ndcGl + 1.0f) * 0.5f * (f.depthMax - f.depthMin);   // the device's D3D depth through the viewport
    }

    inline void CurClip(const Mat4& bone, const Frame& f, const float p[3], float out[4]) {
        const Mat4 m = Mul(bone, f.cur.proj);
        const float v[4] = { p[0], p[1], p[2], 1.0f };
        Transform(v, m, out);
    }
    inline float CurU(const Frame& f, const Mat4& bone, const float p[3]) { float c[4]; CurClip(bone, f, p, c); return (c[0] / c[3] + 1.0f) * 0.5f; }
    inline float CurV(const Frame& f, const Mat4& bone, const float p[3]) { float c[4]; CurClip(bone, f, p, c); return (1.0f - c[1] / c[3]) * 0.5f; }
    inline float CurDepth(const Frame& f, const Mat4& bone, const float p[3]) {
        float c[4];
        CurClip(bone, f, p, c);
        return f.depthMin + (c[2] / c[3] + 1.0f) * 0.5f * (f.depthMax - f.depthMin);
    }

    // Previous camera at the origin, current camera one yard along its right axis.
    inline Frame SidestepFrame(float depthMin = 0.0f, float depthMax = 0.94f) {
        float r[3];
        Right(0, r);
        return FrameWith(Cam(0, 0, 0, Yaw(0)), Cam(r[0], r[1], r[2], Yaw(0)), depthMin, depthMax);
    }
}

#endif
