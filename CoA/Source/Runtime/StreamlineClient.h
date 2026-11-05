#ifndef COA_SOURCE_RUNTIME_STREAMLINECLIENT_H
#define COA_SOURCE_RUNTIME_STREAMLINECLIENT_H

// The Streamline functions CoA64 calls (sl.interposer.dll, NVIDIA Streamline SDK). Needs the SDK and Vulkan headers
// (continued: docs/2026-10-29-render-pipeline-notes.md, StreamlineClient.h #1)
#include <vulkan/vulkan.h>
#include <sl.h>
#include <sl_dlss.h>
#include <sl_dlss_g.h>
#include <sl_pcl.h>
#include <sl_reflex.h>
#include <sl_helpers_vk.h>

namespace CoA::StreamlineClient {
    struct Api {
        // Interposer exports (GetProcAddress, at PreDevice).
        PFun_slInit* init;
        PFun_slShutdown* shutdown;
        PFun_slGetFeatureRequirements* getFeatureRequirements;
        PFun_slSetVulkanInfo* setVulkanInfo;
        PFun_slIsFeatureSupported* isFeatureSupported;
        PFun_slGetFeatureFunction* getFeatureFunction;
        PFun_slGetNewFrameToken* getNewFrameToken;
        PFun_slSetConstants* setConstants;
        PFun_slSetTagForFrame* setTagForFrame;
        PFun_slEvaluateFeature* evaluateFeature;
        PFun_slFreeResources* freeResources;
        // DLSS feature functions (slGetFeatureFunction, once the device is set and DLSS is supported).
        PFun_slDLSSGetOptimalSettings* dlssGetOptimalSettings;
        PFun_slDLSSSetOptions* dlssSetOptions;
        PFun_slDLSSGetState* dlssGetState;
        // Stage 4 feature functions; null when the feature is not loaded or not supported on the device.
        PFun_slReflexSetOptions* reflexSetOptions;
        PFun_slReflexSleep* reflexSleep;
        PFun_slReflexGetState* reflexGetState;
        PFun_slPCLSetMarker* pclSetMarker;
        PFun_slDLSSGSetOptions* dlssgSetOptions;
        PFun_slDLSSGGetState* dlssgGetState;
    };

    // The function table once Ready() (every pointer non-null), else nullptr.
    const Api* GetApi();
    // sl::Result as its enumerator name ("eOk", "eErrorNoPlugins", ...).
    const char* ResultName(sl::Result result);
}

namespace CoA::SlFrameClient {
    // The current game frame's token (main thread), nullptr outside a frame or without Streamline.
    const sl::FrameToken* Token();
}

#endif
