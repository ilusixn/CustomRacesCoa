// Frame Generation / Reflex pure logic (see FrameGen.h).
#include "CoA/Source/Runtime/FrameGen.h"

namespace CoA::FrameGen {
    Mode ModeFromCVar(int value) {
        return value >= 1 && value <= 3 ? static_cast<Mode>(value) : Mode::Off;
    }

    uint32_t FramesToGenerate(Mode m, uint32_t maxSupported) {
        const uint32_t wanted = static_cast<uint32_t>(m);
        return wanted < maxSupported ? wanted : maxSupported;
    }

    Reflex ReflexFromCVar(int value) {
        return value >= 1 && value <= 2 ? static_cast<Reflex>(value) : Reflex::Off;
    }

    Reflex EffectiveReflex(Reflex wanted, bool fgActive) {
        return fgActive && wanted == Reflex::Off ? Reflex::On : wanted;
    }

    uint32_t SlReflexMode(Reflex r) {
        return static_cast<uint32_t>(r);
    }

    Gate Decide(Mode /* m */, const FrameState& s) {
        if (!s.loaded) {
            return Gate::NotLoaded;
        }

        if (!s.supported) {
            return Gate::Unsupported;
        }

        if (!s.worldFrame) {
            return Gate::NotWorldFrame;
        }

        if (s.minimised) {
            return Gate::Minimised;
        }

        if (s.resizing) {
            return Gate::Resizing;
        }

        return s.hasDepthAndMotion ? Gate::On : Gate::NoDepthOrMotion;
    }

    const char* GateName(Gate g) {
        switch (g) {
            case Gate::On: return "on";
            case Gate::NotLoaded: return "Frame Generation not loaded (coaFrameGen was 0 at startup)";
            case Gate::Unsupported: return "Frame Generation unsupported on this GPU / driver";
            case Gate::NotWorldFrame: return "not a world frame";
            case Gate::Minimised: return "minimised";
            case Gate::Resizing: return "resizing";
            case Gate::NoDepthOrMotion: return "no depth or motion this frame";
        }

        return "?";
    }

    bool AnyOptionOn(int dlssMode, int frameGen, int reflex) {
        return dlssMode != 0 || frameGen != 0 || reflex != 0;
    }

    FramePlan PlanFrame(bool dlssWanted, bool frameGenWanted) {
        return { dlssWanted || frameGenWanted, dlssWanted, dlssWanted, frameGenWanted };
    }
}
