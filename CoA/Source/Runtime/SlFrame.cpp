// One Streamline frame per game frame (see SlFrame.h).
#include "CoA/Source/Runtime/SlFrame.h"

namespace CoA::SlFrame {
    namespace {
        enum class State { Idle, Simulating, Rendering };

        uint64_t s_index;
        State s_state = State::Idle;
    }

    void BeginFrame(const Deps& deps) {
        ++s_index;

        if (!deps.token(s_index)) {
            s_state = State::Idle;
            return;
        }

        deps.sleep(s_index);
        deps.marker(kSimulationStart, s_index);
        s_state = State::Simulating;
    }

    void BeginRender(const Deps& deps) {
        if (s_state != State::Simulating) {
            return;
        }

        deps.marker(kSimulationEnd, s_index);
        deps.marker(kRenderSubmitStart, s_index);
        s_state = State::Rendering;
    }

    void EndRender(const Deps& deps) {
        if (s_state != State::Rendering) {
            return;
        }

        deps.marker(kRenderSubmitEnd, s_index);
        deps.stamp(s_index);
        s_state = State::Idle;
    }

    uint64_t Index() {
        return s_index;
    }

    void Reset() {
        s_index = 0;
        s_state = State::Idle;
    }
}
