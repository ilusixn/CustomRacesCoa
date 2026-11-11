#include "CoA/Source/Runtime/Hd.h"

namespace {
    bool s_vulkanRenderer = false;
}

// NOT IN BINARY: HD loads on any renderer ("-hd 0" still turns it off); the DXVK gate dropped HD where DXVK fails.
bool CoA::Hd::Available() {
    return true;
}

void CoA::Hd::SetVulkanRenderer(bool active) {
    s_vulkanRenderer = active;
}
