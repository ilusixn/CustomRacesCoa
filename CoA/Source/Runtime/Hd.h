#ifndef COA_SOURCE_RUNTIME_HD_H
#define COA_SOURCE_RUNTIME_HD_H

// Whether CoA's HD content may load: the eight HD archives (RealmDataClient.cpp) and the HD model tables
// (BridgeStoreClient.cpp). They load unless "-hd 0", on DXVK and on system D3D9 alike (docs/2026-11-11-hd-without-dxvk.md).
namespace CoA::Hd {
    bool Available();
    void SetVulkanRenderer(bool active);
}

#endif
