#include "CoA/Source/Runtime/RenderMotion.h"
#include <cmath>

namespace CoA::RenderMotion {
    using RenderPipeline::Mat4;

    namespace {
        void Transform(const float v[4], const Mat4& m, float out[4]) {
            for (int c = 0; c < 4; ++c) {
                out[c] = v[0] * m.m[0][c] + v[1] * m.m[1][c] + v[2] * m.m[2][c] + v[3] * m.m[3][c];
            }
        }

        Mat4 Mul(const Mat4& a, const Mat4& b) {
            Mat4 r{};
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
                }
            }
            return r;
        }

        // Clip position to UV (u right, v down).
        Vec2 ToUV(const float clip[4]) {
            const float invW = 1.0f / clip[3];
            return { (clip[0] * invW + 1.0f) * 0.5f, (1.0f - clip[1] * invW) * 0.5f };
        }
    }

    Mat4 Inverse(const Mat4& m) {
        // Gauss-Jordan with partial pivoting, in double.
        Mat4 identity{};
        for (int i = 0; i < 4; ++i) {
            identity.m[i][i] = 1.0f;
        }
        double a[4][8];
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                a[r][c] = m.m[r][c];
                a[r][4 + c] = (r == c) ? 1.0 : 0.0;
            }
        }
        for (int col = 0; col < 4; ++col) {
            int piv = col;
            for (int r = col + 1; r < 4; ++r) {
                if (std::fabs(a[r][col]) > std::fabs(a[piv][col])) {
                    piv = r;
                }
            }
            if (a[piv][col] == 0.0) {
                return identity;
            }
            if (piv != col) {
                for (int c = 0; c < 8; ++c) {
                    const double t = a[col][c];
                    a[col][c] = a[piv][c];
                    a[piv][c] = t;
                }
            }
            const double inv = 1.0 / a[col][col];
            for (int c = 0; c < 8; ++c) {
                a[col][c] *= inv;
            }
            for (int r = 0; r < 4; ++r) {
                if (r == col) {
                    continue;
                }
                const double f = a[r][col];
                for (int c = 0; c < 8; ++c) {
                    a[r][c] -= f * a[col][c];
                }
            }
        }
        Mat4 out{};
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                out.m[r][c] = static_cast<float>(a[r][4 + c]);
            }
        }
        return out;
    }

    Vec2 StaticMotion(const RenderPipeline::Frame& f, float u, float v, float storedDepth) {
        const float range = f.depthMax - f.depthMin;
        if (range == 0.0f) {
            return { 0.0f, 0.0f };
        }
        // The device writes the D3D depth (ndcGL + 1) / 2 through the viewport range; Frame.proj is the GL-style projection.
        const float ndcD3d = (storedDepth - f.depthMin) / range;
        const float ndc[4] = { u * 2.0f - 1.0f, 1.0f - v * 2.0f, ndcD3d * 2.0f - 1.0f, 1.0f };
        // Far plane / clear value (sky): a direction at infinity, so the eye translation does not apply (w = 0).
        const bool atInfinity = storedDepth >= f.depthMax - 1e-6f;

        // Camera-relative point: the view has no translation (eye at the origin).
        const Mat4 inv = Inverse(Mul(f.cur.view, f.cur.proj));
        float p[4];
        Transform(ndc, inv, p);
        const float invW = 1.0f / p[3];
        const float world[4] = {
            p[0] * invW + (atInfinity ? 0.0f : f.cur.pos[0] - f.prev.pos[0]),
            p[1] * invW + (atInfinity ? 0.0f : f.cur.pos[1] - f.prev.pos[1]),
            p[2] * invW + (atInfinity ? 0.0f : f.cur.pos[2] - f.prev.pos[2]),
            atInfinity ? 0.0f : 1.0f
        };

        float prevClip[4];
        Transform(world, Mul(f.prev.view, f.prev.proj), prevClip);
        const Vec2 prevUV = ToUV(prevClip);
        return { prevUV.x - u, prevUV.y - v };
    }

    Vec2 ModelMotion(const Mat4& curBoneView, const Mat4& prevBoneView, const float pos[3], const Mat4& curProj, const Mat4& prevProj) {
        const float p[4] = { pos[0], pos[1], pos[2], 1.0f };
        float curClip[4], prevClip[4];
        Transform(p, Mul(curBoneView, curProj), curClip);
        Transform(p, Mul(prevBoneView, prevProj), prevClip);
        const Vec2 c = ToUV(curClip);
        const Vec2 q = ToUV(prevClip);
        return { q.x - c.x, q.y - c.y };
    }
}
