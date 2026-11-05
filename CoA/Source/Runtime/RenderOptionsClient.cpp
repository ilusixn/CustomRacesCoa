// RenderOptions' facts: DXVK active, the NVIDIA GPU's architecture through NVAPI (no Streamline needed) and, once
// loaded, what Streamline supports on the device. CoA64 addition.
#include "CoA/Source/Runtime/RenderOptions.h"
#include "CoA/Source/Runtime/Dxvk.h"
#include "CoA/Source/Runtime/Log.h"
#include <nvapi.h>

namespace {
    bool s_queried;
    bool s_nvidia;
    uint32_t s_arch;

    void QueryGpu() {
        s_queried = true;
        NvPhysicalGpuHandle gpus[NVAPI_MAX_PHYSICAL_GPUS] = {};
        NvU32 count = 0;

        if (NvAPI_Initialize() != NVAPI_OK || NvAPI_EnumPhysicalGPUs(gpus, &count) != NVAPI_OK || count == 0) {
            return;
        }

        NV_GPU_ARCH_INFO info = {};
        info.version = NV_GPU_ARCH_INFO_VER;

        if (NvAPI_GPU_GetArchInfo(gpus[0], &info) == NVAPI_OK) {
            s_nvidia = true;
            s_arch = info.architecture;
        }
    }
}

CoA::RenderOptions::Facts CoA::RenderOptionsClient::Facts() {
    if (!s_queried) {
        QueryGpu();
        CoA::Log::Printf("RenderOptions: NVIDIA %s, arch 0x%x, DXVK %s", s_nvidia ? "yes" : "no", s_arch,
            CoA::Dxvk::Active() ? "active" : "inactive");
    }

    const CoA::Streamline::Features loaded = CoA::Streamline::Loaded();
    const bool slLoaded = loaded.dlss || loaded.frameGen || loaded.reflex || loaded.pcl;
    return { CoA::Dxvk::Active(), s_nvidia, s_arch, CoA::Streamline::Requested(), slLoaded, loaded, CoA::Streamline::Supported(),
        CoA::StreamlineClient::FlipPresent() };
}
