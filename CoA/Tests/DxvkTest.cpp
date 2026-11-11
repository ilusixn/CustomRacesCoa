#include "CoA/Source/Runtime/Dxvk.h"
#include "CoA/Source/Runtime/Hd.h"
#include "CoA/Tests/TestMain.h"
#include <string>
#include <vector>

namespace {
    // A non-null stand-in for the module handle.
    int s_module;

    bool s_noDxvk;
    uint32_t s_loadError;
    std::vector<std::wstring> s_loads;
    std::vector<std::string> s_logs;

    bool HasArg(const char* name) {
        return s_noDxvk && std::string(name) == "nodxvk";
    }

    std::wstring ExeDir() {
        return L"D:\\WoW\\Coa";
    }

    void* Load(const std::wstring& path, uint32_t& err) {
        s_loads.push_back(path);
        err = s_loadError;
        return s_loadError ? nullptr : &s_module;
    }

    void Log(const char* line) {
        s_logs.emplace_back(line);
    }

    CoA::Dxvk::Deps Fakes(bool noDxvk, uint32_t loadError) {
        CoA::Dxvk::Reset();
        CoA::Hd::SetVulkanRenderer(false);
        s_noDxvk = noDxvk;
        s_loadError = loadError;
        s_loads.clear();
        s_logs.clear();
        return { HasArg, ExeDir, Load, Log };
    }

    void Done() {
        CoA::Dxvk::Reset();
        CoA::Hd::SetVulkanRenderer(false);
    }
}

COA_TEST(Dxvk_NoDxvkSkipsTheLoadAndHdStaysOn) {
    CoA::Dxvk::Preload(Fakes(true, 0));
    CHECK(s_loads.empty());
    CHECK(!CoA::Dxvk::Active());
    CHECK(CoA::Hd::Available());
    CHECK(s_logs.size() == 1 && s_logs[0] == "DXVK disabled (-nodxvk)");
    Done();
}

COA_TEST(Dxvk_LoadsX64D3d9NextToTheExeAndTurnsHdOn) {
    CoA::Dxvk::Preload(Fakes(false, 0));
    CHECK(s_loads.size() == 1 && s_loads[0] == L"D:\\WoW\\Coa\\x64\\d3d9.dll");
    CHECK(CoA::Dxvk::Active());
    CHECK(CoA::Hd::Available());
    CHECK(s_logs.size() == 1 && s_logs[0] == "DXVK loaded: D:\\WoW\\Coa\\x64\\d3d9.dll");
    Done();
}

// 193 = ERROR_BAD_EXE_FORMAT: the install's own (32-bit) DXVK in the x64 folder.
COA_TEST(Dxvk_LoadFailureKeepsSystemD3d9AndHdOn) {
    CoA::Dxvk::Preload(Fakes(false, 193));
    CHECK(s_loads.size() == 1);
    CHECK(!CoA::Dxvk::Active());
    CHECK(CoA::Hd::Available());
    CHECK(s_logs.size() == 1 && s_logs[0] == "DXVK not loaded (error 193), using system d3d9");
    CHECK(CoA::Dxvk::D3d9Library() == nullptr);
    Done();
}

COA_TEST(Dxvk_D3d9LibraryNullWhenInactive) {
    Fakes(false, 0);
    CHECK(CoA::Dxvk::D3d9Library() == nullptr);
    Done();
}

// Each ILoadD3dLib frees what it gets back, so every call takes its own reference (a second load of the same path).
COA_TEST(Dxvk_D3d9LibraryTakesANewReferenceToTheSamePath) {
    CoA::Dxvk::Preload(Fakes(false, 0));
    CHECK(CoA::Dxvk::D3d9Library() == &s_module);
    CHECK(s_loads.size() == 2 && s_loads[1] == L"D:\\WoW\\Coa\\x64\\d3d9.dll");
    Done();
}
