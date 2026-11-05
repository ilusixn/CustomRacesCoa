// DLSS Frame Generation's options per frame (stage 4). DlssClient tags the inputs; this decides, before each present,
// whether Frame Generation runs (FrameGen::Decide) and sets DLSS-G's options when that changes. CoA64 addition.
#ifndef COA_STREAMLINE
#include "CoA/Source/Runtime/Dlss.h"

bool CoA::FrameGenClient::Wanted() {
    return false;
}

void CoA::FrameGenClient::OnTagged(uint32_t, uint32_t) {
}

void CoA::FrameGenClient::OnPresent() {
}

void CoA::FrameGenClient::OnPresentThread() {
}

void CoA::FrameGenClient::Release() {
}
#else
#include "CoA/Source/Runtime/Dlss.h"
#include "CoA/Source/Runtime/FrameGen.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/StreamlineClient.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include <atomic>
#include <string>
#include <windows.h>

namespace {
    // DLSS's viewport, which the tags and options share.
    constexpr uint32_t kViewport = 0;

    bool s_tagged;               // DlssClient tagged this frame's inputs
    bool s_resizing;             // the display size changed this frame
    uint32_t s_width, s_height;
    uint32_t s_applied;          // frames generated per frame as last set (0: eOff)
    bool s_optionsSent;
    std::atomic<uint32_t> s_maxFrames;  // DLSSGState::numFramesToGenerateMax (0: not queried yet), set on the present thread
    std::atomic<bool> s_logState;       // the present thread logs DLSS-G's state once
    CoA::FrameGen::Gate s_lastGate = CoA::FrameGen::Gate::On;
    CoA::FrameGen::ReflexTracker s_reflex;

    int CVarInt(const char* name) {
        CVar* var = CVar::Lookup(name);
        return var ? var->m_intValue : 0;
    }

    // coaReflex (at least On while Frame Generation runs), sent when it changes, also live.
    void ApplyReflex(const CoA::StreamlineClient::Api* api, bool frameGenOn) {
        if (!api->reflexSetOptions) {
            return;
        }

        const CoA::FrameGen::Reflex mode = CoA::FrameGen::EffectiveReflex(CoA::FrameGen::ReflexFromCVar(CVarInt("coaReflex")), frameGenOn);

        if (!s_reflex.Update(mode)) {
            return;
        }

        sl::ReflexOptions options {};
        options.mode = static_cast<sl::ReflexMode>(CoA::FrameGen::SlReflexMode(mode));
        const sl::Result r = api->reflexSetOptions(options);
        CoA::Log::Printf("Streamline: slReflexSetOptions(mode %u) %s", CoA::FrameGen::SlReflexMode(mode), CoA::StreamlineClient::ResultName(r));

        if (r != sl::Result::eOk) {
            s_reflex.Reset();
        }
    }
    uint64_t s_stateLogged;      // GetTickCount64 of the last DLSS-G state request

    // Every 5 s while generating: frames Streamline presented per rendered frame and its status (the in-game proof).
    void LogState() {
        const uint64_t now = GetTickCount64();

        if (now - s_stateLogged >= 5000) {
            s_stateLogged = now;
            s_logState = true;
        }
    }

    CoA::FrameGen::Mode ModeNow() {
        CVar* var = CVar::Lookup("coaFrameGen");
        return CoA::FrameGen::ModeFromCVar(var ? var->m_intValue : 0);
    }

    // 1 until the present thread has read DLSS-G's maximum.
    uint32_t MaxFrames() {
        const uint32_t max = s_maxFrames;
        return max ? max : 1;
    }
}

bool CoA::FrameGenClient::Wanted() {
    return CoA::StreamlineClient::Ready() && CoA::Streamline::Supported().frameGen && ModeNow() != CoA::FrameGen::Mode::Off;
}

void CoA::FrameGenClient::OnTagged(uint32_t displayW, uint32_t displayH) {
    s_resizing = s_resizing || (s_width && (displayW != s_width || displayH != s_height));
    s_width = displayW;
    s_height = displayH;
    s_tagged = true;
}

void CoA::FrameGenClient::OnPresent() {
    const CoA::StreamlineClient::Api* api = CoA::StreamlineClient::GetApi();
    const CoA::FrameGen::Mode mode = ModeNow();

    if (api) {
        ApplyReflex(api, CoA::Streamline::Supported().frameGen && mode != CoA::FrameGen::Mode::Off);
    }

    if (!api || !api->dlssgSetOptions || (mode == CoA::FrameGen::Mode::Off && !s_applied && s_optionsSent)) {
        s_tagged = s_resizing = false;
        return;
    }

    const CoA::Streamline::Features loaded = CoA::Streamline::Loaded();
    const CoA::Streamline::Features supported = CoA::Streamline::Supported();
    const CoA::FrameGen::FrameState state { loaded.frameGen, supported.frameGen, s_tagged, false, s_resizing, s_tagged };
    const CoA::FrameGen::Gate gate = mode == CoA::FrameGen::Mode::Off ? CoA::FrameGen::Gate::NotLoaded : CoA::FrameGen::Decide(mode, state);
    const uint32_t frames = gate == CoA::FrameGen::Gate::On ? CoA::FrameGen::FramesToGenerate(mode, MaxFrames()) : 0;
    s_tagged = s_resizing = false;

    if (s_applied) {
        LogState();
    }

    if (s_optionsSent && frames == s_applied) {
        return;
    }

    sl::DLSSGOptions options {};
    options.mode = frames ? sl::DLSSGMode::eOn : sl::DLSSGMode::eOff;
    options.numFramesToGenerate = frames ? frames : 1;
    const sl::Result r = api->dlssgSetOptions(sl::ViewportHandle(kViewport), options);

    if (frames || gate != s_lastGate || !s_optionsSent) {
        CoA::Log::Printf("Frame Generation: %s (%u generated) %s%s", frames ? "on" : "off", frames, CoA::StreamlineClient::ResultName(r),
            frames ? "" : (std::string(" -- ") + CoA::FrameGen::GateName(gate)).c_str());
    }

    s_lastGate = gate;
    s_applied = r == sl::Result::eOk ? frames : s_applied;
    s_optionsSent = r == sl::Result::eOk;
}

// slDLSSGGetState must run on the present thread (DLSS-G guide 15.1); from the main thread the pacer hung.
void CoA::FrameGenClient::OnPresentThread() {
    const CoA::StreamlineClient::Api* api = CoA::StreamlineClient::GetApi();
    const bool log = s_logState.exchange(false);

    if (!api || !api->dlssgGetState || (s_maxFrames && !log)) {
        return;
    }

    sl::DLSSGState state {};
    const sl::Result r = api->dlssgGetState(sl::ViewportHandle(kViewport), state, nullptr);

    if (!s_maxFrames) {
        s_maxFrames = r == sl::Result::eOk && state.numFramesToGenerateMax ? state.numFramesToGenerateMax : 1;
        CoA::Log::Printf("Frame Generation: slDLSSGGetState %s, up to %u generated frames", CoA::StreamlineClient::ResultName(r), s_maxFrames.load());
    }

    if (log) {
        CoA::Log::Printf("Frame Generation: state %s, %u frames presented per frame, status 0x%x, VRAM %llu MB",
            CoA::StreamlineClient::ResultName(r), state.numFramesActuallyPresented, static_cast<unsigned>(state.status),
            static_cast<unsigned long long>(state.estimatedVRAMUsageInBytes >> 20));
    }
}

void CoA::FrameGenClient::Release() {
    s_reflex.Reset();
    s_logState = false;
    s_applied = 0;
    s_optionsSent = false;
    s_maxFrames = 0;
    s_tagged = s_resizing = false;
}
#endif
