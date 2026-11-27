#ifndef COA_SOURCE_RUNTIME_SLFRAME_H
#define COA_SOURCE_RUNTIME_SLFRAME_H

#include <cstdint>

// One Streamline frame per game frame (stage 4): the frame index, Reflex sleep and the PCL markers of the main thread.
// Pure core over Deps; SlFrameClient.cpp supplies Streamline and DXVK. Not part of the original (CoA64 addition).
namespace CoA::SlFrame {
    // sl::PCLMarker values used here (SlFrameClient.cpp static_asserts them).
    enum Marker : uint32_t { kSimulationStart = 0, kSimulationEnd = 1, kRenderSubmitStart = 2, kRenderSubmitEnd = 3,
        kPresentStart = 4, kPresentEnd = 5 };

    struct Deps {
        bool (*token)(uint64_t index);                     // slGetNewFrameToken for this index
        void (*sleep)(uint64_t index);                     // slReflexSleep
        void (*marker)(uint32_t marker, uint64_t index);   // slPCLSetMarker
        void (*stamp)(uint64_t index);                     // CoaDxvkSetNextPresentFrame
    };

    // Idle dispatch: a new index, Reflex sleep, simulation start.
    void BeginFrame(const Deps& deps);
    // Paint dispatch: simulation end, render submit start (only inside a frame BeginFrame opened).
    void BeginRender(const Deps& deps);
    // CGxDevice::ScenePresent, before the device's Present: render submit end, the index stamped on that present.
    void EndRender(const Deps& deps);
    // The current index (0 before the first frame).
    uint64_t Index();
    void Reset();
}

#endif
