#ifndef COA_SOURCE_RUNTIME_FRAMEGEN_H
#define COA_SOURCE_RUNTIME_FRAMEGEN_H

#include <cstdint>

// Pure logic for DLSS Frame Generation and NVIDIA Reflex (stage 4, via Streamline): CVar values, the per-frame gate and
// Reflex forcing. Not part of the original (CoA64 addition).
namespace CoA::FrameGen {
    // coaFrameGen: 0 Off, 1 = 2x, 2 = 3x, 3 = 4x.
    enum class Mode { Off = 0, X2 = 1, X3 = 2, X4 = 3 };
    Mode ModeFromCVar(int value);
    // Frames generated per rendered frame, capped by what Streamline reports (numFramesToGenerateMax).
    uint32_t FramesToGenerate(Mode m, uint32_t maxSupported);

    // coaReflex: 0 Off, 1 On, 2 On + Boost.
    enum class Reflex { Off = 0, On = 1, Boost = 2 };
    Reflex ReflexFromCVar(int value);
    // Frame Generation needs Reflex: Off becomes On while it runs.
    Reflex EffectiveReflex(Reflex wanted, bool fgActive);
    // sl::ReflexMode (eOff 0, eLowLatency 1, eLowLatencyWithBoost 2).
    uint32_t SlReflexMode(Reflex r);

    enum class Gate { On, NotLoaded, Unsupported, NotWorldFrame, Minimised, Resizing, NoDepthOrMotion };

    struct FrameState {
        bool loaded, supported, worldFrame, minimised, resizing, hasDepthAndMotion;
    };

    // The first failing condition (loaded, supported, worldFrame, !minimised, !resizing, hasDepthAndMotion), else On.
    // Callers ask only for a mode other than Off.
    Gate Decide(Mode m, const FrameState& s);
    const char* GateName(Gate g);

    // Streamline is loaded when any option is non-zero at startup.
    bool AnyOptionOn(int dlssMode, int frameGen, int reflex);

    // What a world render does for DLSS / Frame Generation: run the pipeline and motion passes, jitter the camera,
    // evaluate DLSS, tag Frame Generation's inputs (hudless, depth, motion until present).
    struct FramePlan {
        bool pipeline, jitter, evaluateDlss, tagFrameGen;
    };

    FramePlan PlanFrame(bool dlssWanted, bool frameGenWanted);

    // The Reflex mode last sent to Streamline: Update is true when `r` must be sent (first time or a change).
    struct ReflexTracker {
        int applied = -1;

        bool Update(Reflex r) {
            const int value = static_cast<int>(r);
            const bool changed = value != this->applied;
            this->applied = value;
            return changed;
        }

        void Reset() {
            this->applied = -1;
        }
    };
}

#endif
