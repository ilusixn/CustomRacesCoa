#include "CoA/Source/Runtime/RenderPipeline.h"
#include "CoA/Source/Runtime/CVars.h"
#include <cmath>

namespace CoA::RenderPipeline {
    namespace {
        Frame s_frame;

        float RadicalInverse(uint32_t index, uint32_t base) {
            double result = 0.0;
            double f = 1.0 / base;
            while (index > 0) {
                result += f * (index % base);
                index /= base;
                f /= base;
            }
            return static_cast<float>(result);
        }
    }

    Inactive WhyInactive(const ActivationInput& in) {
        if (!in.cvarOn) {
            return Inactive::CVarOff;
        }
        if (in.msaaSamples > 1) {
            return Inactive::Multisample;
        }
        if (!in.intzSupported) {
            return Inactive::NoIntz;
        }
        if (!in.shadersEnabled) {
            return Inactive::NoShaders;
        }
        return Inactive::None;
    }

    void BeginFrame(const Camera& cam, bool forceReset, float depthMin, float depthMax) {
        const bool first = s_frame.index == 0;
        bool reset = first || forceReset;
        if (!first) {
            const float dx = cam.pos[0] - s_frame.cur.pos[0];
            const float dy = cam.pos[1] - s_frame.cur.pos[1];
            const float dz = cam.pos[2] - s_frame.cur.pos[2];
            if (dx * dx + dy * dy + dz * dz > kTeleportDistance * kTeleportDistance) {
                reset = true;
            }
        }
        s_frame.prev = first ? cam : s_frame.cur;
        s_frame.cur = cam;
        s_frame.index++;
        s_frame.reset = reset;
        s_frame.depthMin = depthMin;
        s_frame.depthMax = depthMax;
    }

    const Frame& Current() {
        return s_frame;
    }

    void Halton23(uint32_t index, float& x, float& y) {
        x = RadicalInverse(index, 2) - 0.5f;
        y = RadicalInverse(index, 3) - 0.5f;
    }

    uint32_t JitterPhaseCount(uint32_t renderWidth, uint32_t displayWidth) {
        if (renderWidth == 0) {
            return 8;
        }
        const double ratio = static_cast<double>(displayWidth) / renderWidth;
        const double count = std::round(8.0 * ratio * ratio);
        return count < 8.0 ? 8u : static_cast<uint32_t>(count);
    }

    Mat4 ApplyJitter(const Mat4& proj, float jx, float jy, uint32_t width, uint32_t height) {
        Mat4 out = proj;
        if (jx == 0.0f && jy == 0.0f) {
            return out;
        }
        out.m[2][0] += 2.0f * jx / static_cast<float>(width);
        out.m[2][1] -= 2.0f * jy / static_cast<float>(height);
        return out;
    }

    Rect CopyRect(float vpMinX, float vpMaxX, float vpMinY, float vpMaxY, uint32_t width, uint32_t height) {
        const float w = static_cast<float>(width);
        const float h = static_cast<float>(height);
        Rect r;
        r.left = static_cast<int32_t>(std::lround(vpMinX * w));
        r.right = static_cast<int32_t>(std::lround(vpMaxX * w));
        r.top = static_cast<int32_t>(std::lround((1.0f - vpMaxY) * h));
        r.bottom = static_cast<int32_t>(std::lround((1.0f - vpMinY) * h));
        return r;
    }

    void Reset() {
        s_frame = Frame{};
    }
}

namespace CoA::RenderPipelineClient {
    namespace {
        CVar* s_pipeline;
        CVar* s_debug;
        CVar* s_jitter;
    }

    void QueueCVars() {
        // name, help, flags, default, callback, category, setCommand, slot, world
        COA_CVAR("coaRenderPipeline", nullptr, 1, "0", nullptr, 1, false, &s_pipeline, false);
        COA_CVAR("coaRenderDebug", nullptr, 1, "0", nullptr, 1, false, &s_debug, false);
        COA_CVAR("coaRenderJitter", nullptr, 1, "0", nullptr, 1, false, &s_jitter, false);
    }

    CVar** PipelineSlot() {
        return &s_pipeline;
    }

    CVar** DebugSlot() {
        return &s_debug;
    }

    CVar** JitterSlot() {
        return &s_jitter;
    }
}
