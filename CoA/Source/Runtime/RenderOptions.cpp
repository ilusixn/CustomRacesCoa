#include "CoA/Source/Runtime/RenderOptions.h"

namespace CoA::RenderOptions {
    namespace {
        constexpr uint32_t kTuring = 0x160;     // NV_GPU_ARCHITECTURE_TU100
        constexpr uint32_t kAda = 0x190;        // NV_GPU_ARCHITECTURE_AD100
        constexpr uint32_t kBlackwell = 0x1B0;  // NV_GPU_ARCHITECTURE_GB200

        Option On(uint32_t maxValue) {
            return { true, Reason::None, maxValue, false };
        }

        Option Off(Reason r) {
            return { false, r, 0, false };
        }

        // Streamline's part: a loaded plugin's device support, a plugin not loaded waits for a restart, and a
        // requested Streamline that did not load cannot be fixed by one.
        void ApplyStreamline(Option& o, const Facts& f, bool loaded, bool supported) {
            if (!o.enabled) {
                return;
            }

            if (f.slRequested && !f.slLoaded) {
                o = Off(Reason::DriverUnsupported);
            } else if (!f.slLoaded || !loaded) {
                o.restart = true;
            } else if (!supported) {
                o = Off(Reason::DriverUnsupported);
            }
        }

        // DXVK, NVIDIA and the architecture, shared by all three options.
        Reason Platform(const Facts& f, uint32_t minArch) {
            if (!f.dxvk) {
                return Reason::NoDxvk;
            }

            if (!f.nvidia) {
                return Reason::NotNvidia;
            }

            if (f.arch < kTuring) {
                return Reason::NeedsRtx20;
            }

            return f.arch < minArch ? Reason::NeedsRtx40 : Reason::None;
        }
    }

    State Decide(const Facts& f) {
        State s {};
        const Reason base = Platform(f, kTuring);
        s.dlss = base != Reason::None ? Off(base) : On(5);
        s.reflex = base != Reason::None ? Off(base) : On(2);

        const Reason fg = Platform(f, kAda);
        s.frameGen = fg != Reason::None ? Off(fg) : !f.flipPresent ? Off(Reason::NotAvailableYet) : On(f.arch >= kBlackwell ? 3 : 1);

        ApplyStreamline(s.dlss, f, f.slLoadedFeatures.dlss, f.slSupported.dlss);
        ApplyStreamline(s.reflex, f, f.slLoadedFeatures.reflex, f.slSupported.reflex);
        ApplyStreamline(s.frameGen, f, f.slLoadedFeatures.frameGen, f.slSupported.frameGen);
        return s;
    }

    bool NeedsRestart(bool slLoaded, int dlss, int frameGen, int reflex) {
        return !slLoaded && (dlss != 0 || frameGen != 0 || reflex != 0);
    }

    const char* ReasonText(Reason r) {
        switch (r) {
            case Reason::None: return "";
            case Reason::NoDxvk: return "Requires the Vulkan renderer";
            case Reason::NotNvidia: return "Requires an NVIDIA graphics card";
            case Reason::NeedsRtx20: return "Requires an RTX 20 series or newer";
            case Reason::NeedsRtx40: return "Requires an RTX 40 series or newer";
            case Reason::NotAvailableYet: return "Not available yet";
            case Reason::DriverUnsupported: return "Not supported by your driver";
        }

        return "";
    }
}
