#include "CoA/Source/Runtime/Dxvk.h"
#include "CoA/Source/Runtime/Hd.h"
#include <cstdio>

namespace {
    CoA::Dxvk::Deps s_deps;
    std::wstring s_path;
    bool s_active = false;

    // For the log line only: non-ASCII characters become '?'.
    std::string Narrow(const std::wstring& text) {
        std::string out;

        for (wchar_t c : text) {
            out += c < 0x80 ? static_cast<char>(c) : '?';
        }

        return out;
    }
}

void CoA::Dxvk::Preload(const Deps& deps) {
    s_deps = deps;

    if (deps.hasArg("nodxvk")) {
        deps.log("DXVK disabled (-nodxvk)");
        return;
    }

    std::wstring path = deps.exeDir() + L"\\x64\\d3d9.dll";
    uint32_t err = 0;

    if (!deps.load(path, err)) {
        char line[96];
        snprintf(line, sizeof(line), "DXVK not loaded (error %u), using system d3d9", err);
        deps.log(line);
        return;
    }

    // The preload's own reference stays for the life of the process, so a device reset that frees its copy
    // (IUnloadD3dLib) never unloads DXVK.
    s_path = path;
    s_active = true;
    CoA::Hd::SetVulkanRenderer(true);
    deps.log(("DXVK loaded: " + Narrow(path)).c_str());
}

void* CoA::Dxvk::D3d9Library() {
    if (!s_active) {
        return nullptr;
    }

    uint32_t err = 0;
    return s_deps.load(s_path, err);
}

bool CoA::Dxvk::Active() {
    return s_active;
}

void CoA::Dxvk::Reset() {
    s_deps = {};
    s_path.clear();
    s_active = false;
}
