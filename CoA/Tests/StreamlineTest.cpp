#include "CoA/Source/Runtime/Dlss.h"
#include "CoA/Tests/TestMain.h"
#include <algorithm>
#include <initializer_list>
#include <string>
#include <vector>

namespace {
    using CoA::Dlss::Off;

    // A non-null stand-in for the module handle.
    int s_module;

    int s_mode;
    bool s_dxvk;
    bool s_signed;
    uint32_t s_loadError;
    bool s_initOk;
    int s_verifies;
    int s_inits;
    std::vector<std::wstring> s_loads;
    std::vector<std::string> s_logs;

    int Mode() {
        return s_mode;
    }

    bool DxvkActive() {
        return s_dxvk;
    }

    std::wstring ExeDir() {
        return L"D:\\WoW\\Coa";
    }

    bool Verify(const std::wstring&) {
        ++s_verifies;
        return s_signed;
    }

    void* Load(const std::wstring& path, uint32_t& err) {
        s_loads.push_back(path);
        err = s_loadError;
        return s_loadError ? nullptr : &s_module;
    }

    bool Init(void* module) {
        ++s_inits;
        return module == &s_module && s_initOk;
    }

    void Log(const char* line) {
        s_logs.emplace_back(line);
    }

    CoA::Streamline::Deps Fakes(int mode, bool dxvk = true, bool isSigned = true, uint32_t loadError = 0, bool initOk = true) {
        CoA::Streamline::Reset();
        s_mode = mode;
        s_dxvk = dxvk;
        s_signed = isSigned;
        s_loadError = loadError;
        s_initOk = initOk;
        s_verifies = 0;
        s_inits = 0;
        s_loads.clear();
        s_logs.clear();
        return { Mode, DxvkActive, ExeDir, Verify, Load, Init, Log };
    }
}

COA_TEST(Streamline_UnsupportedLeavesDlssOff) {
    using CoA::Dlss::WhyOff;
    CHECK(WhyOff(true, true, true, true, true, true) == Off::None);
    CHECK(WhyOff(false, true, true, true, true, true) == Off::ModeZero);
    CHECK(WhyOff(true, false, true, true, true, true) == Off::NoDxvk);
    CHECK(WhyOff(true, true, false, true, true, true) == Off::NotSigned);
    CHECK(WhyOff(true, true, true, false, true, true) == Off::InitFailed);
    CHECK(WhyOff(true, true, true, true, false, true) == Off::Unsupported);
    CHECK(WhyOff(true, true, true, true, true, false) == Off::NoDevice);
    // The first failing reason wins.
    CHECK(WhyOff(false, false, false, false, false, false) == Off::ModeZero);
    CHECK(WhyOff(true, true, false, false, false, false) == Off::NotSigned);
}

COA_TEST(Streamline_ModeZeroLoadsNothing) {
    CoA::Streamline::PreDevice(Fakes(0));
    CHECK(s_loads.empty());
    CHECK(s_verifies == 0 && s_inits == 0);
    CHECK(CoA::Streamline::State() == Off::ModeZero);
    CHECK(!CoA::Streamline::Ready());
    CoA::Streamline::Reset();
}

COA_TEST(Streamline_NoDxvkLoadsNothing) {
    CoA::Streamline::PreDevice(Fakes(2, false));
    CHECK(s_loads.empty() && s_verifies == 0 && s_inits == 0);
    CHECK(CoA::Streamline::State() == Off::NoDxvk);
    CHECK(s_logs.size() == 1);
    CoA::Streamline::Reset();
}

COA_TEST(Streamline_UnsignedInterposerIsNeverLoaded) {
    CoA::Streamline::PreDevice(Fakes(2, true, false));
    CHECK(s_verifies == 1 && s_loads.empty() && s_inits == 0);
    CHECK(CoA::Streamline::State() == Off::NotSigned);
    CoA::Streamline::Reset();
}

COA_TEST(Streamline_LoadsTheInterposerBesideDxvkAndWaitsForTheDevice) {
    CoA::Streamline::PreDevice(Fakes(2));
    CHECK(s_loads.size() == 1 && s_loads[0] == L"D:\\WoW\\Coa\\x64\\sl.interposer.dll");
    CHECK(s_inits == 1);
    CHECK(CoA::Streamline::State() == Off::NoDevice);
    CHECK(!CoA::Streamline::Ready());
    CoA::Streamline::OnDevice(true, true);
    CHECK(CoA::Streamline::State() == Off::None);
    CHECK(CoA::Streamline::Ready());
    CoA::Streamline::Reset();
}

COA_TEST(Streamline_LoadOrInitFailureIsInitFailed) {
    CoA::Streamline::PreDevice(Fakes(1, true, true, 126));
    CHECK(s_inits == 0 && CoA::Streamline::State() == Off::InitFailed);
    CoA::Streamline::PreDevice(Fakes(1, true, true, 0, false));
    CHECK(s_inits == 1 && CoA::Streamline::State() == Off::InitFailed);
    // A device that arrives anyway does not turn it on.
    CoA::Streamline::OnDevice(true, true);
    CHECK(CoA::Streamline::State() == Off::InitFailed && !CoA::Streamline::Ready());
    CoA::Streamline::Reset();
}

COA_TEST(Streamline_DeviceOutcomeNamesTheReason) {
    CoA::Streamline::PreDevice(Fakes(3));
    CoA::Streamline::OnDevice(true, false);
    CHECK(CoA::Streamline::State() == Off::Unsupported && !CoA::Streamline::Ready());
    CoA::Streamline::PreDevice(Fakes(3));
    CoA::Streamline::OnDevice(false, false);
    CHECK(CoA::Streamline::State() == Off::NoDevice && !CoA::Streamline::Ready());
    CoA::Streamline::Reset();
}

namespace {
    // The device lifecycle's Streamline calls, in order ("init", "set:<device>", "supported", "shutdown").
    std::vector<std::string> s_calls;
    bool s_reinitOk;
    bool s_setOk;
    bool s_supportedOk;

    bool DeviceInit() {
        s_calls.emplace_back("init");
        return s_reinitOk;
    }

    bool DeviceSet(uint64_t device) {
        s_calls.push_back("set:" + std::to_string(device));
        return s_setOk;
    }

    bool DeviceSupported() {
        s_calls.emplace_back("supported");
        return s_supportedOk;
    }

    void DeviceShutdown() {
        s_calls.emplace_back("shutdown");
    }

    // PreDevice succeeded (Streamline initialised, waiting for the device), fakes reset.
    CoA::Streamline::DeviceDeps DeviceFakes() {
        CoA::Streamline::PreDevice(Fakes(2));
        s_calls.clear();
        s_reinitOk = true;
        s_setOk = true;
        s_supportedOk = true;
        return { DeviceInit, DeviceSet, DeviceSupported, DeviceShutdown, Log };
    }

    std::vector<std::string> Calls(std::initializer_list<const char*> calls) {
        return std::vector<std::string>(calls.begin(), calls.end());
    }
}

COA_TEST(Streamline_ReleaseBeforeAnyDeviceDoesNothing) {
    const auto deps = DeviceFakes();
    // CGxDeviceD3d::DeviceSetFormat starts with IDestroyD3d while there is no device yet.
    CHECK(!CoA::Streamline::DeviceReleasing(deps, 0));
    CHECK(!CoA::Streamline::DeviceReleasing(deps, 0x1234));
    CHECK(s_calls.empty());
    // The first device is handed over with the slInit of PreDevice (no second init).
    CoA::Streamline::DeviceCreated(deps, 0x1234);
    CHECK(s_calls == Calls({ "set:4660", "supported" }));
    CHECK(CoA::Streamline::Ready());
    CoA::Streamline::Reset();
}

COA_TEST(Streamline_ReleasingTheHandedDeviceShutsDown) {
    const auto deps = DeviceFakes();
    CoA::Streamline::DeviceCreated(deps, 0xA);
    s_calls.clear();
    // Another device (or none) going away leaves Streamline alone.
    CHECK(!CoA::Streamline::DeviceReleasing(deps, 0xB));
    CHECK(!CoA::Streamline::DeviceReleasing(deps, 0));
    CHECK(s_calls.empty() && CoA::Streamline::Ready());
    CHECK(CoA::Streamline::DeviceReleasing(deps, 0xA));
    CHECK(s_calls == Calls({ "shutdown" }));
    CHECK(!CoA::Streamline::Ready() && CoA::Streamline::State() == Off::NoDevice);
    // Once only.
    CHECK(!CoA::Streamline::DeviceReleasing(deps, 0xA));
    CHECK(s_calls.size() == 1);
    CoA::Streamline::Reset();
}

COA_TEST(Streamline_NewDeviceAfterReleaseInitialisesAgain) {
    const auto deps = DeviceFakes();
    CoA::Streamline::DeviceCreated(deps, 0xA);
    CoA::Streamline::DeviceReleasing(deps, 0xA);
    s_calls.clear();
    // slInit before the new device's slSetVulkanInfo.
    CoA::Streamline::DeviceCreated(deps, 0xB);
    CHECK(s_calls == Calls({ "init", "set:11", "supported" }));
    CHECK(CoA::Streamline::Ready());
    CHECK(CoA::Streamline::DeviceReleasing(deps, 0xB));
    // A failed re-init leaves DLSS off without handing the device over.
    s_reinitOk = false;
    s_calls.clear();
    CoA::Streamline::DeviceCreated(deps, 0xC);
    CHECK(s_calls == Calls({ "init" }));
    CHECK(!CoA::Streamline::Ready());
    CHECK(!CoA::Streamline::DeviceReleasing(deps, 0xC));
    CoA::Streamline::Reset();
}

COA_TEST(Streamline_SecondDeviceWithoutReleaseStaysOff) {
    const auto deps = DeviceFakes();
    CoA::Streamline::DeviceCreated(deps, 0xA);
    s_calls.clear();
    // Streamline still holds 0xA: no second slSetVulkanInfo.
    CoA::Streamline::DeviceCreated(deps, 0xB);
    CHECK(s_calls.empty());
    CHECK(!CoA::Streamline::Ready());
    // 0xA is still the device to shut down with.
    CHECK(CoA::Streamline::DeviceReleasing(deps, 0xA));
    CoA::Streamline::Reset();
}

// DXVK's destroy callback for a device whose D3D9 device failed InitialReset frees the hold; the retry gets DLSS.
COA_TEST(Streamline_DeviceDestroyedWithoutD3dLetsTheRetryIn) {
    const auto deps = DeviceFakes();
    CoA::Streamline::DeviceCreated(deps, 0xA);
    CHECK(CoA::Streamline::DeviceReleasing(deps, 0xA));
    s_calls.clear();
    CoA::Streamline::DeviceCreated(deps, 0xB);
    CHECK(s_calls == Calls({ "init", "set:11", "supported" }));
    CHECK(CoA::Streamline::Ready());
    // The normal path already released 0xB; its later destroy callback does nothing.
    CHECK(CoA::Streamline::DeviceReleasing(deps, 0xB));
    CHECK(!CoA::Streamline::DeviceReleasing(deps, 0xB));
    CoA::Streamline::Reset();
}

COA_TEST(Streamline_DeviceThatFailsShutsDownForGood) {
    auto deps = DeviceFakes();
    s_supportedOk = false;
    CoA::Streamline::DeviceCreated(deps, 0xA);
    CHECK(s_calls == Calls({ "set:10", "supported", "shutdown" }));
    CHECK(CoA::Streamline::State() == Off::Unsupported);
    s_calls.clear();
    CHECK(!CoA::Streamline::DeviceReleasing(deps, 0xA));
    CoA::Streamline::DeviceCreated(deps, 0xB);
    CHECK(s_calls.empty() && !CoA::Streamline::Ready());

    deps = DeviceFakes();
    s_setOk = false;
    CoA::Streamline::DeviceCreated(deps, 0xA);
    CHECK(s_calls == Calls({ "set:10", "shutdown" }));
    CHECK(CoA::Streamline::State() == Off::NoDevice);
    CoA::Streamline::Reset();
}

COA_TEST(Streamline_DeviceCallbacksIgnoredWithoutInit) {
    auto deps = DeviceFakes();
    CoA::Streamline::PreDevice(Fakes(0));
    s_calls.clear();
    CoA::Streamline::DeviceCreated(deps, 0xA);
    CHECK(!CoA::Streamline::DeviceReleasing(deps, 0xA));
    CHECK(s_calls.empty());
    CoA::Streamline::Reset();
}

namespace {
    CoA::Streamline::Features s_loadFeatures;
    CoA::Streamline::Features s_deviceFeatures;

    CoA::Streamline::Features LoadFeatures() {
        return s_loadFeatures;
    }

    CoA::Streamline::Features DeviceFeatures() {
        s_calls.emplace_back("features");
        return s_deviceFeatures;
    }

    bool Route(bool on) {
        s_calls.emplace_back(on ? "route" : "unroute");
        return true;
    }
}

// Any option on loads Streamline; a Reflex-only start loads Reflex and PCL without DLSS.
COA_TEST(Streamline_AnyOptionLoads) {
    auto deps = Fakes(1);
    s_loadFeatures = { false, false, true, true };
    deps.features = LoadFeatures;
    CoA::Streamline::PreDevice(deps);
    CHECK(s_loads.size() == 1);
    const CoA::Streamline::Features loaded = CoA::Streamline::Loaded();
    CHECK(loaded.reflex && loaded.pcl && !loaded.dlss && !loaded.frameGen);
    CoA::Streamline::Reset();
}

// A loaded feature the device cannot run stays off and is named; the others keep working.
COA_TEST(Streamline_FeatureGating) {
    auto deps = Fakes(1);
    s_loadFeatures = { true, true, true, true };
    deps.features = LoadFeatures;
    CoA::Streamline::PreDevice(deps);
    auto device = DeviceFakes();
    CoA::Streamline::PreDevice(deps);
    s_deviceFeatures = { true, false, true, true };
    device.supportedFeatures = DeviceFeatures;
    s_logs.clear();
    CoA::Streamline::DeviceCreated(device, 0xA);
    const CoA::Streamline::Features ok = CoA::Streamline::Supported();
    CHECK(ok.dlss && ok.reflex && ok.pcl && !ok.frameGen);
    CHECK(CoA::Streamline::Ready());
    bool named = false;
    for (const std::string& line : s_logs) {
        named = named || line.find("Frame Generation unsupported") != std::string::npos;
    }
    CHECK(named);
    CoA::Streamline::Reset();
}

// With Frame Generation the swapchain goes through Streamline: the D3D release defers, and DXVK's device
// destruction (swapchains gone) un-routes before the shutdown; the next device routes again.
COA_TEST(Streamline_DeviceCycleClearsProcs) {
    auto deps = Fakes(1);
    s_loadFeatures = { true, true, true, true };
    deps.features = LoadFeatures;
    auto device = DeviceFakes();
    CoA::Streamline::PreDevice(deps);
    s_deviceFeatures = { true, true, true, true };
    device.supportedFeatures = DeviceFeatures;
    device.routeSwapchain = Route;
    s_calls.clear();
    CoA::Streamline::DeviceCreated(device, 0xA);
    CHECK(s_calls == Calls({ "set:10", "supported", "features", "route" }));
    s_calls.clear();
    CHECK(!CoA::Streamline::DeviceReleasing(device, 0xA));
    CHECK(s_calls.empty());
    CHECK(CoA::Streamline::DeviceDestroying(device, 0xA));
    CHECK(s_calls == Calls({ "unroute", "shutdown" }));
    s_calls.clear();
    CoA::Streamline::DeviceCreated(device, 0xB);
    CHECK(s_calls == Calls({ "init", "set:11", "supported", "features", "route" }));
    CoA::Streamline::Reset();
}

// Without Frame Generation nothing is routed: the D3D release shuts down, the later destruction does nothing.
COA_TEST(Streamline_DestroyAfterReleaseWithoutFgIsNoOp) {
    auto device = DeviceFakes();
    CoA::Streamline::DeviceCreated(device, 0xA);
    CHECK(CoA::Streamline::DeviceReleasing(device, 0xA));
    s_calls.clear();
    CHECK(!CoA::Streamline::DeviceDestroying(device, 0xA));
    CHECK(s_calls.empty());
    CoA::Streamline::Reset();
}

// Streamline stays unloaded only when all three options are 0 at startup; the log says so.
COA_TEST(Streamline_ModeZeroReasonNamesAllOptions) {
    CHECK(std::string(CoA::Dlss::OffName(Off::ModeZero)) == "coaDlssMode, coaFrameGen and coaReflex all 0");
}

namespace {
    bool RouteFails(bool on) {
        s_calls.emplace_back(on ? "route" : "unroute");
        return !on;
    }
}

// A swapchain that could not be routed through Streamline cannot run Frame Generation: it is turned off and nothing
// stays routed; the D3D release then shuts down as without Frame Generation.
COA_TEST(Streamline_RouteFailureTurnsFrameGenOff) {
    auto deps = Fakes(1);
    s_loadFeatures = { true, true, true, true };
    deps.features = LoadFeatures;
    auto device = DeviceFakes();
    CoA::Streamline::PreDevice(deps);
    s_deviceFeatures = { true, true, true, true };
    device.supportedFeatures = DeviceFeatures;
    device.routeSwapchain = RouteFails;
    CoA::Streamline::DeviceCreated(device, 0xA);
    CHECK(!CoA::Streamline::Supported().frameGen);
    CHECK(CoA::Streamline::Supported().dlss && CoA::Streamline::Supported().reflex);
    s_calls.clear();
    CHECK(CoA::Streamline::DeviceReleasing(device, 0xA));
    CHECK(s_calls == Calls({ "shutdown" }));
    CoA::Streamline::Reset();
}

namespace {
    bool NoFlipPresent() {
        s_calls.emplace_back("flip");
        return false;
    }
}

// Without a flip-model present (the driver copies Vulkan windows) DLSS-G's pacer deadlocks in the present's device
// flush: Frame Generation stays off and the swapchain is not routed.
COA_TEST(Streamline_CopyPresentTurnsFrameGenOff) {
    auto deps = Fakes(1);
    s_loadFeatures = { true, true, true, true };
    deps.features = LoadFeatures;
    auto device = DeviceFakes();
    CoA::Streamline::PreDevice(deps);
    s_deviceFeatures = { true, true, true, true };
    device.supportedFeatures = DeviceFeatures;
    device.routeSwapchain = Route;
    device.flipPresent = NoFlipPresent;
    s_calls.clear();
    CoA::Streamline::DeviceCreated(device, 0xA);
    CHECK(!CoA::Streamline::Supported().frameGen);
    CHECK(CoA::Streamline::Supported().dlss && CoA::Streamline::Supported().reflex);
    CHECK(std::find(s_calls.begin(), s_calls.end(), "flip") != s_calls.end());
    CHECK(std::find(s_calls.begin(), s_calls.end(), "route") == s_calls.end());
    CoA::Streamline::DeviceReleasing(device, 0xA);
    CoA::Streamline::Reset();
}
