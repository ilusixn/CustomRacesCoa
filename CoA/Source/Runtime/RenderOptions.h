#ifndef COA_SOURCE_RUNTIME_RENDEROPTIONS_H
#define COA_SOURCE_RUNTIME_RENDEROPTIONS_H

#include "CoA/Source/Runtime/Dlss.h"
#include <cstdint>

// The Graphics page's DLSS / Frame Generation / Reflex dropdowns (stage 5): which are usable, why not, which entries.
// Not part of the original (CoA64 addition).
namespace CoA::RenderOptions {
    enum class Reason { None, NoDxvk, NotNvidia, NeedsRtx20, NeedsRtx40, NotAvailableYet, DriverUnsupported };

    struct Facts {
        bool dxvk;                                  // DXVK renderer active
        bool nvidia;                                // NVAPI initialised on an NVIDIA GPU
        uint32_t arch;                              // NV_GPU_ARCHITECTURE_* (0: unknown)
        bool slRequested;                           // an option was on at startup (Streamline was to load)
        bool slLoaded;                              // Streamline loaded at startup
        CoA::Streamline::Features slLoadedFeatures; // the plugins it loaded (when slLoaded)
        CoA::Streamline::Features slSupported;      // per feature on this device (when slLoaded)
        bool flipPresent;                           // Frame Generation's present gate
    };

    // maxValue: the highest CVar value offered; restart: its plugin is not loaded, a choice waits for a restart.
    struct Option {
        bool enabled;
        Reason reason;
        uint32_t maxValue;
        bool restart;
    };

    struct State {
        Option dlss, frameGen, reflex;
    };

    State Decide(const Facts& f);
    // The saved values (dlss, fg, reflex) take effect only after a restart: Streamline was not loaded at startup
    // and at least one value is non-zero.
    bool NeedsRestart(bool slLoaded, int dlss, int frameGen, int reflex);
    const char* ReasonText(Reason r);
}

// RenderOptionsClient.cpp: the facts from NVAPI, DXVK and Streamline.
namespace CoA::RenderOptionsClient {
    CoA::RenderOptions::Facts Facts();
}

#endif
