// SlFrame's Streamline / DXVK glue (stage 4): frame tokens, Reflex sleep and PCL markers on the engine's idle, paint and
// present hooks; DXVK's queue thread reports the present markers. CoA64 addition.
#ifndef COA_STREAMLINE
#include "CoA/Source/Runtime/Dlss.h"

void CoA::SlFrameClient::Install() {
}
#else
#include "CoA/Source/Runtime/SlFrame.h"
#include "CoA/Source/Runtime/StreamlineClient.h"
#include "CoA/Source/Runtime/Dlss.h"
#include "CoA/Source/Runtime/Dxvk.h"
#include "Engine/Source/Event/EvtSched.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "ThirdParty/dxvk/include/coa_dxvk_streamline.h"
#include <windows.h>

static_assert(static_cast<uint32_t>(sl::PCLMarker::eSimulationStart) == CoA::SlFrame::kSimulationStart
    && static_cast<uint32_t>(sl::PCLMarker::eRenderSubmitEnd) == CoA::SlFrame::kRenderSubmitEnd
    && static_cast<uint32_t>(sl::PCLMarker::ePresentStart) == CoA::SlFrame::kPresentStart
    && static_cast<uint32_t>(sl::PCLMarker::ePresentEnd) == CoA::SlFrame::kPresentEnd, "sl::PCLMarker changed");

namespace {
    // The main thread's token of the current frame (Token()).
    sl::FrameToken* s_token;
    PFN_CoaDxvkSetNextPresentFrame s_setNextPresentFrame;

    bool Token(uint64_t index) {
        s_token = nullptr;
        const CoA::StreamlineClient::Api* api = CoA::StreamlineClient::GetApi();

        if (!api) {
            return false;
        }

        uint32_t i = static_cast<uint32_t>(index);
        return api->getNewFrameToken(s_token, &i) == sl::Result::eOk && s_token;
    }

    void Sleep(uint64_t /* index */) {
        const CoA::StreamlineClient::Api* api = CoA::StreamlineClient::GetApi();

        if (api && api->reflexSleep && s_token) {
            api->reflexSleep(*s_token);
        }
    }

    void Marker(uint32_t marker, uint64_t /* index */) {
        const CoA::StreamlineClient::Api* api = CoA::StreamlineClient::GetApi();

        if (api && api->pclSetMarker && s_token) {
            api->pclSetMarker(static_cast<sl::PCLMarker>(marker), *s_token);
        }
    }

    void Stamp(uint64_t index) {
        if (s_setNextPresentFrame && CoA::StreamlineClient::GetApi()) {
            s_setNextPresentFrame(index);
        }
    }

    const CoA::SlFrame::Deps kDeps = { Token, Sleep, Marker, Stamp };

    void OnIdle() {
        CoA::SlFrame::BeginFrame(kDeps);
    }

    void OnPaint() {
        CoA::SlFrame::BeginRender(kDeps);
    }

    void OnScenePresent() {
        CoA::FrameGenClient::OnPresent();
        CoA::SlFrame::EndRender(kDeps);
    }

    // DXVK's queue thread: present start / end for a stamped frame (its own token, by index).
    void __stdcall OnPresentMarker(uint32_t marker, uint64_t frameIndex) {
        const CoA::StreamlineClient::Api* api = CoA::StreamlineClient::GetApi();

        if (!api || !api->pclSetMarker) {
            return;
        }

        sl::FrameToken* token = nullptr;
        uint32_t i = static_cast<uint32_t>(frameIndex);

        if (api->getNewFrameToken(token, &i) == sl::Result::eOk && token) {
            api->pclSetMarker(marker == 0 ? sl::PCLMarker::ePresentStart : sl::PCLMarker::ePresentEnd, *token);
        }

        if (marker != 0) {
            CoA::FrameGenClient::OnPresentThread();
        }
    }
}

void CoA::SlFrameClient::Install() {
    void* dxvk = CoA::Dxvk::D3d9Library();

    if (dxvk) {
        s_setNextPresentFrame = reinterpret_cast<PFN_CoaDxvkSetNextPresentFrame>(
            GetProcAddress(static_cast<HMODULE>(dxvk), "CoaDxvkSetNextPresentFrame"));

        if (auto setMarker = reinterpret_cast<PFN_CoaDxvkSetPresentMarkerCallback>(
                GetProcAddress(static_cast<HMODULE>(dxvk), "CoaDxvkSetPresentMarkerCallback"))) {
            setMarker(OnPresentMarker);
        }

        FreeLibrary(static_cast<HMODULE>(dxvk));
    }

    g_idleHook = OnIdle;
    g_paintHook = OnPaint;
    g_scenePresentHook = OnScenePresent;
}

const sl::FrameToken* CoA::SlFrameClient::Token() {
    return s_token;
}
#endif
