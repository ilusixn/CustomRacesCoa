#include "CoA/Source/Runtime/RenderOptions.h"
#include "CoA/Tests/TestMain.h"
#include <string>

using CoA::RenderOptions::Facts;
using CoA::RenderOptions::Reason;

namespace {
    constexpr uint32_t kPascal = 0x130;
    constexpr uint32_t kAmpere = 0x170;
    constexpr uint32_t kAda = 0x190;
    constexpr uint32_t kBlackwell = 0x1B0;

    Facts Gpu(uint32_t arch, bool flip = false) {
        return { true, true, arch, false, false, {}, {}, flip };
    }

    // Streamline up with these plugins loaded (and supported unless `supported` says otherwise).
    Facts Loaded(CoA::Streamline::Features loaded, CoA::Streamline::Features supported) {
        Facts f = Gpu(0x1B0, true);
        f.slRequested = f.slLoaded = true;
        f.slLoadedFeatures = loaded;
        f.slSupported = supported;
        return f;
    }
}

// The system D3D9 path has no Streamline backend: all three greyed out.
COA_TEST(RenderOptions_NoDxvkGreysAll) {
    const auto s = CoA::RenderOptions::Decide({ false, true, kBlackwell, false, false, {}, {}, false });
    CHECK(!s.dlss.enabled && !s.frameGen.enabled && !s.reflex.enabled);
    CHECK(s.dlss.reason == Reason::NoDxvk && s.frameGen.reason == Reason::NoDxvk && s.reflex.reason == Reason::NoDxvk);
}

// DLSS and Reflex need Turing or newer, Frame Generation Ada or newer.
COA_TEST(RenderOptions_ArchitectureGates) {
    const auto pascal = CoA::RenderOptions::Decide(Gpu(kPascal));
    CHECK(!pascal.dlss.enabled && pascal.dlss.reason == Reason::NeedsRtx20);
    CHECK(!pascal.reflex.enabled && pascal.frameGen.reason == Reason::NeedsRtx20);
    const auto ampere = CoA::RenderOptions::Decide(Gpu(kAmpere, true));
    CHECK(ampere.dlss.enabled && ampere.reflex.enabled);
    CHECK(!ampere.frameGen.enabled && ampere.frameGen.reason == Reason::NeedsRtx40);
    const auto blackwell = CoA::RenderOptions::Decide(Gpu(kBlackwell));
    CHECK(blackwell.dlss.enabled && blackwell.dlss.maxValue == 5);
    CHECK(blackwell.reflex.enabled && blackwell.reflex.maxValue == 2);
    const auto notNvidia = CoA::RenderOptions::Decide({ true, false, 0, false, false, {}, {}, true });
    CHECK(!notNvidia.dlss.enabled && notNvidia.dlss.reason == Reason::NotNvidia);
}

// Frame Generation needs a flip present (stage 4); 3x / 4x need Blackwell.
COA_TEST(RenderOptions_FrameGenNeedsFlip) {
    const auto noFlip = CoA::RenderOptions::Decide(Gpu(kBlackwell, false));
    CHECK(!noFlip.frameGen.enabled && noFlip.frameGen.reason == Reason::NotAvailableYet);
    const auto blackwell = CoA::RenderOptions::Decide(Gpu(kBlackwell, true));
    CHECK(blackwell.frameGen.enabled && blackwell.frameGen.maxValue == 3);
    const auto ada = CoA::RenderOptions::Decide(Gpu(kAda, true));
    CHECK(ada.frameGen.enabled && ada.frameGen.maxValue == 1);
}

// Once Streamline is loaded its per-feature answer wins (e.g. a driver too old for DLSS).
COA_TEST(RenderOptions_StreamlineOverrides) {
    const auto s = CoA::RenderOptions::Decide(Loaded({ true, true, true, true }, { false, true, true, true }));
    CHECK(!s.dlss.enabled && s.dlss.reason == Reason::DriverUnsupported);
    CHECK(s.reflex.enabled && s.frameGen.enabled);
}

// Streamline loads only when an option is on at startup: from all-off, any choice waits for a restart.
COA_TEST(RenderOptions_RestartRule) {
    CHECK(CoA::RenderOptions::NeedsRestart(false, 2, 0, 0));
    CHECK(CoA::RenderOptions::NeedsRestart(false, 0, 0, 1));
    CHECK(!CoA::RenderOptions::NeedsRestart(false, 0, 0, 0));
    CHECK(!CoA::RenderOptions::NeedsRestart(true, 3, 0, 1));
    const auto off = CoA::RenderOptions::Decide(Gpu(kBlackwell));
    CHECK(off.dlss.restart && off.reflex.restart);
    const auto loaded = CoA::RenderOptions::Decide(Loaded({ true, false, true, true }, { true, false, true, true }));
    CHECK(!loaded.dlss.restart && !loaded.reflex.restart);
}

// A Reflex-only start loads no DLSS plugin: DLSS stays selectable and waits for a restart (not "unsupported").
COA_TEST(RenderOptions_ReflexOnlyStartKeepsDlssSelectable) {
    const auto s = CoA::RenderOptions::Decide(Loaded({ false, false, true, true }, { false, false, true, true }));
    CHECK(s.dlss.enabled && s.dlss.restart && s.dlss.reason == Reason::None);
    CHECK(s.reflex.enabled && !s.reflex.restart);
}

// An option was on at startup but Streamline did not load: a restart will not help, so nothing waits for one.
COA_TEST(RenderOptions_LoadFailureIsNotARestart) {
    Facts f = Gpu(kBlackwell, true);
    f.slRequested = true;
    const auto s = CoA::RenderOptions::Decide(f);
    CHECK(!s.dlss.enabled && s.dlss.reason == Reason::DriverUnsupported && !s.dlss.restart);
    CHECK(!s.reflex.enabled && !s.reflex.restart);
}

COA_TEST(RenderOptions_ReasonTextNonEmpty) {
    for (Reason r : { Reason::NoDxvk, Reason::NotNvidia, Reason::NeedsRtx20, Reason::NeedsRtx40, Reason::NotAvailableYet,
             Reason::DriverUnsupported }) {
        CHECK(std::string(CoA::RenderOptions::ReasonText(r)).size() > 0);
    }

    CHECK(std::string(CoA::RenderOptions::ReasonText(Reason::NotAvailableYet)) == "Not available yet");
}
