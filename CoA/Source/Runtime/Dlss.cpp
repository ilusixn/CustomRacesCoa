#include "CoA/Source/Runtime/Dlss.h"
#include "CoA/Source/Runtime/RenderMotion.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace CoA::Dlss {
    using RenderPipeline::Mat4;
    using RenderMotion::Mul;

    namespace {
        float Scale(Mode m) {
            switch (m) {
                case Mode::Quality: return 0.6667f;
                case Mode::Balanced: return 0.58f;
                case Mode::Performance: return 0.5f;
                case Mode::UltraPerformance: return 0.3333f;
                default: return 1.0f;
            }
        }

        uint32_t Axis(uint32_t out, float scale) {
            return std::max(1u, static_cast<uint32_t>(std::lround(static_cast<double>(out) * scale)));
        }
    }

    Mode ModeFromCVar(int value) {
        return value >= 0 && value <= 5 ? static_cast<Mode>(value) : Mode::Off;
    }

    void FallbackRenderSize(Mode m, uint32_t outW, uint32_t outH, uint32_t& w, uint32_t& h) {
        const float s = Scale(m);
        w = Axis(outW, s);
        h = Axis(outH, s);
    }

    RenderPipeline::Rect RenderRect(uint32_t w, uint32_t h) {
        return { 0, 0, static_cast<int32_t>(w), static_cast<int32_t>(h) };
    }

    uint32_t SlMode(Mode m) {
        switch (m) {
            case Mode::Performance: return 1;
            case Mode::Balanced: return 2;
            case Mode::Quality: return 3;
            case Mode::UltraPerformance: return 4;
            case Mode::Dlaa: return 6;
            default: return 0;
        }
    }

    float LodBias(uint32_t renderW, uint32_t outW) {
        if (renderW == 0 || outW == 0) {
            return 0.0f;
        }
        return std::min(0.0f, std::log2(static_cast<float>(renderW) / static_cast<float>(outW)));
    }

    SlMatrices BuildMatrices(const RenderPipeline::Frame& f) {
        SlMatrices s;
        // Streamline wants clip z to match the tagged depth, which is D3D-native [0, 1]: z' = 0.5 z + 0.5 w.
        Mat4 z{}, zInv{};
        for (int i = 0; i < 4; ++i) {
            z.m[i][i] = 1.0f;
            zInv.m[i][i] = 1.0f;
        }
        z.m[2][2] = 0.5f;
        z.m[3][2] = 0.5f;
        zInv.m[2][2] = 2.0f;
        zInv.m[3][2] = -1.0f;

        s.cameraViewToClip = Mul(f.cur.proj, z);
        s.clipToCameraView = RenderMotion::Inverse(s.cameraViewToClip);

        Mat4 t{};
        for (int i = 0; i < 4; ++i) {
            t.m[i][i] = 1.0f;
        }
        t.m[3][0] = f.cur.pos[0] - f.prev.pos[0];
        t.m[3][1] = f.cur.pos[1] - f.prev.pos[1];
        t.m[3][2] = f.cur.pos[2] - f.prev.pos[2];

        // GL-style clip to previous GL-style clip (the StaticMotion math), conjugated by Z to D3D clip depth.
        const Mat4 curInv = RenderMotion::Inverse(Mul(f.cur.view, f.cur.proj));
        const Mat4 gl = Mul(Mul(curInv, t), Mul(f.prev.view, f.prev.proj));
        s.clipToPrevClip = Mul(Mul(zInv, gl), z);
        s.prevClipToClip = RenderMotion::Inverse(s.clipToPrevClip);
        return s;
    }

    DepthRemap DepthRemapFor(const RenderPipeline::Frame& f) {
        const float range = f.depthMax - f.depthMin;
        if (range <= 0.0f) {
            return { 0.0f, 1.0f };
        }
        const float scale = 1.0f / range;
        return { scale, -f.depthMin * scale };
    }
}

namespace CoA::Dlss {
    Off WhyOff(bool modeNonZero, bool dxvkActive, bool signatureOk, bool initOk, bool featureSupported, bool deviceSet) {
        if (!modeNonZero) return Off::ModeZero;
        if (!dxvkActive) return Off::NoDxvk;
        if (!signatureOk) return Off::NotSigned;
        if (!initOk) return Off::InitFailed;
        if (!featureSupported) return Off::Unsupported;
        if (!deviceSet) return Off::NoDevice;
        return Off::None;
    }

    const char* OffName(Off reason) {
        switch (reason) {
            case Off::None: return "on";
            case Off::ModeZero: return "coaDlssMode, coaFrameGen and coaReflex all 0";
            case Off::NoDxvk: return "DXVK not active";
            case Off::NotSigned: return "sl.interposer.dll missing or not signed by NVIDIA";
            case Off::InitFailed: return "Streamline failed to load or initialise";
            case Off::Unsupported: return "DLSS not supported on this GPU / driver";
            case Off::NoDevice: return "no Vulkan device handed to Streamline";
        }
        return "?";
    }
}

namespace CoA::Streamline {
    namespace {
        bool s_mode, s_dxvk, s_signed, s_init, s_device, s_supported;
        // The Vulkan device Streamline holds (0: none); Streamline was shut down with a released device and must be
        // initialised again; Streamline was shut down for good (a device it could not use).
        uint64_t s_held;
        bool s_needsInit;
        bool s_dead;
        Features s_loaded;
        Features s_supportedFeatures;
        bool s_routed;

        // The device's feature support, each unsupported loaded feature logged by name.
        Features DeviceFeatures(const DeviceDeps& deps) {
            Features f = deps.supportedFeatures ? deps.supportedFeatures() : s_loaded;
            f.dlss = f.dlss && s_loaded.dlss;
            f.frameGen = f.frameGen && s_loaded.frameGen;
            f.reflex = f.reflex && s_loaded.reflex;
            f.pcl = f.pcl && s_loaded.pcl;
            const struct { bool loaded, ok; const char* name; } named[] = {
                { s_loaded.dlss, f.dlss, "DLSS" }, { s_loaded.frameGen, f.frameGen, "Frame Generation" },
                { s_loaded.reflex, f.reflex, "Reflex" }, { s_loaded.pcl, f.pcl, "PCL" },
            };

            for (const auto& n : named) {
                if (n.loaded && !n.ok) {
                    deps.log((std::string(n.name) + " unsupported on this GPU / driver").c_str());
                }
            }

            return f;
        }
    }

    bool Requested() {
        return s_mode;
    }

    Features Loaded() {
        return s_init ? s_loaded : Features {};
    }

    Features Supported() {
        return Ready() ? s_supportedFeatures : Features {};
    }

    void PreDevice(const Deps& deps) {
        Reset();
        s_mode = deps.mode() != 0;
        s_dxvk = s_mode && deps.dxvkActive();
        s_loaded = deps.features ? deps.features() : Features { true, false, false, false };
        bool loaded = false;

        if (s_dxvk) {
            const std::wstring path = deps.exeDir() + L"\\Vulkan\\sl.interposer.dll";
            s_signed = deps.verify(path);

            if (s_signed) {
                uint32_t err = 0;
                void* module = deps.load(path, err);
                loaded = module != nullptr;

                if (!loaded) {
                    char line[96];
                    snprintf(line, sizeof(line), "Streamline: sl.interposer.dll not loaded (error %u)", err);
                    deps.log(line);
                }

                // The interposer stays loaded for the life of the process, even when init fails (DXVK may already
                // hold its requirements).
                s_init = loaded && deps.init(module);
            }
        }

        if (State() == Dlss::Off::NoDevice) {
            deps.log("Streamline: initialised, waiting for the Vulkan device");
        } else {
            deps.log((std::string("DLSS off: ") + Dlss::OffName(State())).c_str());
        }
    }

    void OnDevice(bool deviceSet, bool featureSupported) {
        if (!s_init) {
            return;
        }

        s_device = deviceSet;
        s_supported = featureSupported;
    }

    Dlss::Off State() {
        // Before the device arrives (or when it could not be set) DLSS support is unknown, not unsupported.
        return Dlss::WhyOff(s_mode, s_dxvk, s_signed, s_init, s_device ? s_supported : true, s_device);
    }

    bool Ready() {
        return State() == Dlss::Off::None;
    }

    void DeviceCreated(const DeviceDeps& deps, uint64_t device) {
        if (!s_init || s_dead) {
            return;
        }

        if (s_held) {
            s_device = false;
            deps.log("DLSS off: a new D3D device was created while Streamline still holds the old one");
            return;
        }

        if (s_needsInit) {
            if (!deps.init()) {
                s_dead = true;
                s_device = false;
                deps.log("DLSS off: Streamline failed to initialise again for the new device");
                return;
            }

            s_needsInit = false;
        }

        const bool set = deps.setDevice(device);
        const bool supported = set && deps.supported();
        OnDevice(set, supported);

        if (Ready()) {
            s_held = device;
            s_supportedFeatures = DeviceFeatures(deps);

            // A copy present flushes the device, which deadlocks DLSS-G's pacer against its own queues.
            if (s_supportedFeatures.frameGen && deps.flipPresent && !deps.flipPresent()) {
                s_supportedFeatures.frameGen = false;
                deps.log("Frame Generation off: the NVIDIA driver presents CoA64's Vulkan window by copy, not flip");
            }

            // Frame Generation needs the swapchain Streamline creates; without it, it stays off.
            if (s_supportedFeatures.frameGen && !(deps.routeSwapchain && deps.routeSwapchain(true))) {
                s_supportedFeatures.frameGen = false;
                deps.log("Frame Generation off: DXVK's swapchain could not be routed through Streamline");
            }

            s_routed = s_supportedFeatures.frameGen;

            return;
        }

        deps.shutdown();
        s_dead = true;
    }

    bool DeviceReleasing(const DeviceDeps& deps, uint64_t device) {
        if (!s_held || device != s_held || s_routed) {
            return false;
        }

        deps.shutdown();
        s_held = 0;
        s_needsInit = true;
        s_device = false;
        return true;
    }

    bool DeviceDestroying(const DeviceDeps& deps, uint64_t device) {
        if (!s_held || device != s_held) {
            return false;
        }

        if (s_routed && deps.routeSwapchain) {
            deps.routeSwapchain(false);
        }

        s_routed = false;
        deps.shutdown();
        s_held = 0;
        s_needsInit = true;
        s_device = false;
        return true;
    }

    void Reset() {
        s_loaded = s_supportedFeatures = Features {};
        s_routed = false;
        s_mode = s_dxvk = s_signed = s_init = s_device = s_supported = false;
        s_held = 0;
        s_needsInit = false;
        s_dead = false;
    }
}

// NVIDIA presents DXVK's Vulkan window by copy, even with the profile's "layered on DXGI" (PresentMon, 2026-10-29).
bool CoA::StreamlineClient::FlipPresent() {
    return false;
}
