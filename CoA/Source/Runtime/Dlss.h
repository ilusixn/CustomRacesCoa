#ifndef COA_SOURCE_RUNTIME_DLSS_H
#define COA_SOURCE_RUNTIME_DLSS_H

#include "CoA/Source/Runtime/RenderPipeline.h"
#include <cstdint>
#include <string>

// Pure logic for CoA64's DLSS integration (stage 3, via NVIDIA Streamline): modes, render sizes, texture LOD bias, the camera
// (continued: docs/2026-10-29-render-pipeline-notes.md, Dlss.h #1)
namespace CoA::Dlss {
    enum class Mode { Off = 0, Dlaa = 1, Quality = 2, Balanced = 3, Performance = 4, UltraPerformance = 5 };

    // The coaDlssMode CVar value to a mode; anything outside 0..5 is Off.
    Mode ModeFromCVar(int value);

    // Render size for an output size, used only when slDLSSGetOptimalSettings fails: the per-axis scale of the mode
    // (DLAA 1, Quality 0.6667, Balanced 0.58, Performance 0.5, UltraPerformance 0.3333), rounded to the nearest pixel, at least 1.
    void FallbackRenderSize(Mode m, uint32_t outW, uint32_t outH, uint32_t& w, uint32_t& h);

    // Texture LOD bias for a render width below the output width: log2(renderW / outW), <= 0 (0 for DLAA).
    float LodBias(uint32_t renderW, uint32_t outW);

    // The render size as a pixel rect {0, 0, w, h} (the world viewport's window rect while the world renders at it).
    RenderPipeline::Rect RenderRect(uint32_t w, uint32_t h);

    // The matrices of sl::Constants (row-major, no jitter). The world is camera-relative (the view has no translation).
    // (continued: docs/2026-10-29-render-pipeline-notes.md, Dlss.h #2)
    struct SlMatrices {
        RenderPipeline::Mat4 cameraViewToClip;   // cur.proj * Z, unjittered (Z: clip z -> 0.5 z + 0.5 w)
        RenderPipeline::Mat4 clipToCameraView;   // its inverse
        RenderPipeline::Mat4 clipToPrevClip;     // current clip to previous clip, for a static world point
        RenderPipeline::Mat4 prevClipToClip;     // its inverse
    };

    // clipToPrevClip = Z^-1 * inverse(cur.view * cur.proj) * T(cur.pos - prev.pos) * (prev.view * prev.proj) * Z: the same math
    // (continued: docs/2026-10-29-render-pipeline-notes.md, Dlss.h #3)
    SlMatrices BuildMatrices(const RenderPipeline::Frame& f);

    // The world depth buffer holds minZ + ndcDepth * (maxZ - minZ) (maxZ 0.94 in the world viewport), not 0..1.
    // Remapped = stored * scale + offset (saturated by the shader) brings it back to 0..1.
    struct DepthRemap {
        float scale, offset;
    };

    // scale = 1 / (depthMax - depthMin), offset = -depthMin * scale. An empty range returns {0, 1} (everything far).
    DepthRemap DepthRemapFor(const RenderPipeline::Frame& f);

    // Streamline's sl::DLSSMode value for a mode (sl_dlss.h: eOff 0, eMaxPerformance 1, eBalanced 2, eMaxQuality 3,
    // eUltraPerformance 4, eUltraQuality 5, eDLAA 6). StreamlineClient.cpp static_asserts the enumerators.
    uint32_t SlMode(Mode m);

    // Why DLSS is off for this process (None: Streamline is up and DLSS is supported on the device).
    enum class Off { None, ModeZero, NoDxvk, NotSigned, InitFailed, Unsupported, NoDevice };

    // The first false argument's reason, in argument order; None when all are true.
    Off WhyOff(bool modeNonZero, bool dxvkActive, bool signatureOk, bool initOk, bool featureSupported, bool deviceSet);
    // For the log: "mode 0", "DXVK not active", ...
    const char* OffName(Off reason);
}

// Streamline's startup sequence, decided once before the D3D9 device exists (Win32 / SL halves in StreamlineClient.cpp).
// coaDlssMode 0 at startup loads nothing and leaves DXVK untouched; switching 0 to non-zero needs a restart.
namespace CoA::Streamline {
    // The Streamline features a process loads / a device runs (stage 4 adds Frame Generation, Reflex, PCL).
    struct Features {
        bool dlss, frameGen, reflex, pcl;
    };

    struct Deps {
        int (*mode)();                                            // non-zero: any of the three options on at startup
        bool (*dxvkActive)();
        std::wstring (*exeDir)();
        bool (*verify)(const std::wstring& path);                 // NVIDIA's signature on the file
        void* (*load)(const std::wstring& path, uint32_t& err);
        // Resolves the interposer's functions, slInit, hands Streamline's Vulkan requirements and the device callback
        // to DXVK. False leaves DLSS off.
        bool (*init)(void* module);
        void (*log)(const char* line);
        Features (*features)() = nullptr;                         // what slInit loads (null: DLSS only)
    };

    // Mode 0 or no DXVK: nothing is verified or loaded. Otherwise verifies and loads <exe dir>\x64\sl.interposer.dll
    // and runs deps.init; on success the state is NoDevice until OnDevice.
    void PreDevice(const Deps& deps);
    // The device callback's outcome (slSetVulkanInfo succeeded / slIsFeatureSupported(DLSS) is eOk). Ignored unless
    // PreDevice succeeded.
    void OnDevice(bool deviceSet, bool featureSupported);
    Dlss::Off State();
    bool Ready();

    // Streamline's Vulkan device across D3D device creation and release (DXVK's device callback, the engine's
    // (continued: docs/2026-10-29-render-pipeline-notes.md, Dlss.h #4)
    struct DeviceDeps {
        bool (*init)();                      // slInit again (only after a DeviceReleasing shutdown)
        bool (*setDevice)(uint64_t device);  // slSetVulkanInfo for the new Vulkan device
        bool (*supported)();                 // slIsFeatureSupported(DLSS) and the DLSS functions
        void (*shutdown)();                  // DXVK drained and idle, slShutdown
        void (*log)(const char* line);
        Features (*supportedFeatures)() = nullptr;   // per loaded feature on this device (null: all loaded ones)
        bool (*routeSwapchain)(bool on) = nullptr;   // DXVK's swapchain through Streamline's proxies (false: not routed)
        bool (*flipPresent)() = nullptr;             // the driver presents Vulkan with flip model (null: assume so)
    };

    // What PreDevice loaded (nothing unless Streamline initialised), and what the current device runs (nothing
    // unless Ready()).
    Features Loaded();
    Features Supported();
    // An option was on when PreDevice ran (Streamline was to load, whether it did or not).
    bool Requested();

    // A device callback (VkDevice as an integer). Ignored unless PreDevice succeeded. After a DeviceReleasing shutdown:
    // (continued: docs/2026-10-29-render-pipeline-notes.md, Dlss.h #5)
    void DeviceCreated(const DeviceDeps& deps, uint64_t device);
    // The D3D device whose Vulkan device is `device` (0: none, e.g. DeviceSetFormat's IDestroyD3d before the first
    // device) is about to be released. True (after deps.shutdown) only when Streamline holds exactly that device.
    bool DeviceReleasing(const DeviceDeps& deps, uint64_t device);
    // DXVK destroys `device` (idle, its swapchains gone). With the swapchain routed (Frame Generation) DeviceReleasing
    // defers to here: un-route, then shut down. True when Streamline was shut down.
    bool DeviceDestroying(const DeviceDeps& deps, uint64_t device);

    // Tests only.
    void Reset();
}

// StreamlineClient.cpp: the Win32 / Streamline glue (the SL function table is in StreamlineClient.h).
namespace CoA::StreamlineClient {
    // Called at client init right after DxvkClient::Preload, before ConsoleDeviceInitialize creates the device.
    void PreDevice();
    // The whole chain succeeded: Streamline initialised, the device set, DLSS supported.
    bool Ready();
    // g_d3dDeviceReleaseHook: the D3D device (IDirect3DDevice9*) is about to be released (device re-creation, client
    // (continued: docs/2026-10-29-render-pipeline-notes.md, Dlss.h #6)
    void BeforeDeviceRelease(void* d3dDevice);
    // DXVK's Vulkan window is presented with flip model (Frame Generation's gate; false today, see the notes).
    bool FlipPresent();
}

namespace CoA::RenderPipeline {
    struct Frame;
}

class CGxDevice;

// DlssClient.cpp: DLSS Super Resolution evaluated every world frame inside DXVK's command stream (stage 3). Driven by
// RenderPipelineClient while the world target is begun; everything is inert unless Wanted().
namespace CoA::DlssClient {
    // StreamlineClient::Ready() and coaDlssMode is not 0: the render pipeline and the motion passes are forced on.
    bool Wanted();
    // The same for a coaDlssMode value about to be set (its CVar callback).
    bool WantedFor(int modeValue);
    // This world render jitters the camera (DLSS evaluates; not for Frame Generation alone).
    bool Jitters();
    // BeginWorld, once the pipeline is active, before the world target begins: decides whether DLSS evaluates this
    // (continued: docs/2026-10-29-render-pipeline-notes.md, Dlss.h #7)
    bool Begin(CGxDevice* device, uint32_t displayW, uint32_t displayH, uint32_t& renderW, uint32_t& renderH);
    // This world render evaluates DLSS (Begin returned true, until Evaluate / Release).
    bool Active();
    // DLSS is not active this world render for a pipeline reason (logged once per change, only while Wanted()).
    void NotActive(const char* why);
    // EndWorld, after the motion passes, before the copy-back: the depth remap pass, then Streamline's tags, constants
    // (continued: docs/2026-10-29-render-pipeline-notes.md, Dlss.h #8)
    void* Evaluate(CGxDevice* device, const RenderPipeline::Frame& frame, bool jittered, uint32_t displayW, uint32_t displayH, int debug);
    // Begin succeeded but this world render will not evaluate (no new camera record): DLSS's history resets at the next.
    void Skip();
    // The device's D3DPOOL_DEFAULT resources go (reset / destroy) or the pipeline turns off: waits for DXVK's command
    // stream, slFreeResources, releases the remap / output textures and the shaders.
    void Release();
}

// SlFrameClient.cpp: one Streamline frame per game frame (stage 4). Install hooks the engine's idle / paint / present and
// DXVK's present markers; every call does nothing until Streamline is ready.
namespace CoA::SlFrameClient {
    void Install();
}

// FrameGenClient.cpp: DLSS Frame Generation options per frame (stage 4). The tags come from DlssClient's frame path.
namespace CoA::FrameGenClient {
    // Streamline ready, Frame Generation supported on the device and coaFrameGen is not 0.
    bool Wanted();
    // DlssClient tagged Frame Generation's inputs for this world render at this display size.
    void OnTagged(uint32_t displayW, uint32_t displayH);
    // Before the frame's present (SlFrameClient): decides the gate and sets DLSS-G's options when they change.
    void OnPresent();
    // DXVK's queue thread after a present: DLSS-G's state queries, which must run on the present thread.
    void OnPresentThread();
    // Streamline's options are gone (device release): the next frame sets them again.
    void Release();
}

#endif
