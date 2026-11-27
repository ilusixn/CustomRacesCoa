#include "CoA/Source/Runtime/FrameGen.h"
#include "CoA/Source/Runtime/SlFrame.h"
#include "CoA/Tests/TestMain.h"
#include <string>
#include <vector>

using CoA::FrameGen::Gate;
using CoA::FrameGen::Mode;
using CoA::FrameGen::Reflex;

// coaFrameGen 1/2/3 are 2x/3x/4x; generated frames never exceed what the GPU supports.
COA_TEST(FrameGen_Modes) {
    using namespace CoA::FrameGen;
    CHECK(ModeFromCVar(-1) == Mode::Off);
    CHECK(ModeFromCVar(4) == Mode::Off);
    CHECK(ModeFromCVar(2) == Mode::X3);
    CHECK(FramesToGenerate(Mode::X4, 1) == 1);
    CHECK(FramesToGenerate(Mode::X3, 3) == 2);
    CHECK(FramesToGenerate(Mode::Off, 3) == 0);
}

// Reflex is at least On while Frame Generation runs; Boost stays Boost.
COA_TEST(FrameGen_ReflexForcedUnderFg) {
    using namespace CoA::FrameGen;
    CHECK(EffectiveReflex(Reflex::Off, true) == Reflex::On);
    CHECK(EffectiveReflex(Reflex::Boost, true) == Reflex::Boost);
    CHECK(EffectiveReflex(Reflex::Off, false) == Reflex::Off);
    CHECK(ReflexFromCVar(2) == Reflex::Boost);
    CHECK(ReflexFromCVar(7) == Reflex::Off);
    CHECK(SlReflexMode(Reflex::Boost) == 2);
}

// Loading screens, glue and cinematics are not world frames: no generated frames there.
COA_TEST(FrameGen_OffOutsideWorld) {
    using namespace CoA::FrameGen;
    CHECK(Decide(Mode::X2, { true, true, false, false, false, true }) == Gate::NotWorldFrame);
    CHECK(Decide(Mode::X2, { false, true, true, false, false, true }) == Gate::NotLoaded);
    CHECK(Decide(Mode::X2, { true, false, true, false, false, true }) == Gate::Unsupported);
}

// Resizing / window-mode changes and a minimised window turn Frame Generation off.
COA_TEST(FrameGen_OffAroundResize) {
    using namespace CoA::FrameGen;
    CHECK(Decide(Mode::X2, { true, true, true, false, true, true }) == Gate::Resizing);
    CHECK(Decide(Mode::X2, { true, true, true, true, false, true }) == Gate::Minimised);
    CHECK(Decide(Mode::X2, { true, true, true, false, false, false }) == Gate::NoDepthOrMotion);
    CHECK(Decide(Mode::X4, { true, true, true, false, false, true }) == Gate::On);
    CHECK(std::string(GateName(Gate::Resizing)) == "resizing");
}

// Streamline loads when any of the three options is on at startup.
COA_TEST(FrameGen_AnyOption) {
    using namespace CoA::FrameGen;
    CHECK(!AnyOptionOn(0, 0, 0));
    CHECK(AnyOptionOn(0, 0, 1));
    CHECK(AnyOptionOn(0, 2, 0));
    CHECK(AnyOptionOn(3, 0, 0));
}

namespace {
    std::vector<std::string> s_frameCalls;

    bool FrameToken(uint64_t index) {
        s_frameCalls.push_back("token:" + std::to_string(index));
        return true;
    }

    void FrameSleep(uint64_t index) {
        s_frameCalls.push_back("sleep:" + std::to_string(index));
    }

    void FrameMarker(uint32_t marker, uint64_t index) {
        s_frameCalls.push_back("m" + std::to_string(marker) + ":" + std::to_string(index));
    }

    void FrameStamp(uint64_t index) {
        s_frameCalls.push_back("stamp:" + std::to_string(index));
    }
}

// One Streamline frame per game frame: idle opens it (Reflex sleep, simulation start), paint ends simulation and starts
// render submit, the present ends it and stamps the index for DXVK. A paint or present outside that order adds nothing.
COA_TEST(SlFrame_MarkerOrder) {
    CoA::SlFrame::Reset();
    const CoA::SlFrame::Deps deps { FrameToken, FrameSleep, FrameMarker, FrameStamp };
    s_frameCalls.clear();
    CoA::SlFrame::BeginFrame(deps);
    CoA::SlFrame::BeginRender(deps);
    CoA::SlFrame::EndRender(deps);
    CHECK((s_frameCalls == std::vector<std::string> { "token:1", "sleep:1", "m0:1", "m1:1", "m2:1", "m3:1", "stamp:1" }));
    CHECK(CoA::SlFrame::Index() == 1);
    s_frameCalls.clear();
    CoA::SlFrame::BeginRender(deps);
    CoA::SlFrame::EndRender(deps);
    CHECK(s_frameCalls.empty());
    CoA::SlFrame::BeginFrame(deps);
    CHECK(CoA::SlFrame::Index() == 2);
    CHECK(s_frameCalls.front() == "token:2");
    CoA::SlFrame::Reset();
}

// Frame Generation without DLSS still needs depth and motion: the pipeline runs at display size, unjittered, with
// Frame Generation's tags and no DLSS evaluate. DLSS alone jitters and evaluates without Frame Generation's tags.
COA_TEST(FrameGen_WithoutDlssRunsPipelineNative) {
    using CoA::FrameGen::PlanFrame;
    const auto fgOnly = PlanFrame(false, true);
    CHECK(fgOnly.pipeline && !fgOnly.jitter && !fgOnly.evaluateDlss && fgOnly.tagFrameGen);
    const auto dlssOnly = PlanFrame(true, false);
    CHECK(dlssOnly.pipeline && dlssOnly.jitter && dlssOnly.evaluateDlss && !dlssOnly.tagFrameGen);
    const auto both = PlanFrame(true, true);
    CHECK(both.pipeline && both.jitter && both.evaluateDlss && both.tagFrameGen);
    const auto none = PlanFrame(false, false);
    CHECK(!none.pipeline && !none.jitter && !none.evaluateDlss && !none.tagFrameGen);
}

// Reflex options follow live changes of coaReflex / coaFrameGen: sent once per change, not every frame.
COA_TEST(FrameGen_ReflexTrackerSendsChangesOnly) {
    CoA::FrameGen::ReflexTracker t;
    CHECK(t.Update(Reflex::On));
    CHECK(!t.Update(Reflex::On));
    CHECK(t.Update(Reflex::Boost));
    CHECK(t.Update(Reflex::Off));
    t.Reset();
    CHECK(t.Update(Reflex::Off));
}
