#ifndef COA_SOURCE_RUNTIME_HD_H
#define COA_SOURCE_RUNTIME_HD_H

// Whether CoA's HD content may load: the eight HD archives (RealmDataClient.cpp) and the HD model tables
// (BridgeStoreClient.cpp). The original loads them unless "-hd 0"; CoA's HD patches only run on its DXVK (Vulkan)
// renderer, so CoA64 also needs that renderer active, which the renderer reports here. "-hd 0" still turns HD off.
namespace CoA::Hd {
    bool Available();
    void SetVulkanRenderer(bool active);
}

#endif
