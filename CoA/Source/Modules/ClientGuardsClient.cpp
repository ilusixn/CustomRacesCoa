// AscClientGuards.cpp's client half: the 0x68E4AD NOP as the Engine flag CGxDeviceD3d::DeviceSetBaseMipLevel reads.
#include "CoA/Source/Modules/ClientGuards.h"
#include "CoA/Source/Modules/Modules.h"
#include "Engine/Source/Gx/CGxDeviceD3d/CGxDeviceD3d.h"

namespace {
    void SkipBaseMipStore() {   // FUN_10a4c0c0
        g_gxD3dSkipBaseMipStore = true;
    }
}

void CoA::ClientGuardsClient::Init() {
    CoA::ClientGuards::Dependencies().skipBaseMipStore = SkipBaseMipStore;
}
