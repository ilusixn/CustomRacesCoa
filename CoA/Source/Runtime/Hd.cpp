#include "CoA/Source/Runtime/Hd.h"

namespace {
    bool s_vulkanRenderer = false;
}

bool CoA::Hd::Available() {
    return s_vulkanRenderer;
}

void CoA::Hd::SetVulkanRenderer(bool active) {
    s_vulkanRenderer = active;
}
