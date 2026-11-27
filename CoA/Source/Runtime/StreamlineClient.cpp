// Streamline's Win32 half (DLSS, stage 3; CoA64 addition). Sequence: docs/2026-10-29-streamline-client-notes.md.
#ifndef COA_STREAMLINE
// Built without the Streamline SDK (x86): DLSS stays off.
#include "CoA/Source/Runtime/Dlss.h"

void CoA::StreamlineClient::PreDevice() {
}

bool CoA::StreamlineClient::Ready() {
    return false;
}
#else
#include "CoA/Source/Runtime/StreamlineClient.h"
#include "CoA/Source/Runtime/Dlss.h"
#include "CoA/Source/Runtime/Dxvk.h"
#include "CoA/Source/Runtime/FrameGen.h"
#include "CoA/Source/Runtime/Log.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "ThirdParty/dxvk/include/coa_dxvk_streamline.h"
#include "WaystoneVersion.h"
#include <windows.h>
#include <softpub.h>
#include <wintrust.h>
#include <sl_helpers.h>
#include <sl_security.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace {
    // Streamline's projectId for CoA64 (a fixed GUID of our own; no NVIDIA application id, engine type + version
    // identify the client).
    constexpr const char* kCoaProjectId = "e12b9f80-823f-453b-bce1-b9f235a81dca";
    // engineVersion: the client's build tag (WaystoneVersion.h, the build number shown on the login screen).
    constexpr const char* kEngineVersion = "CoA64 " WAYSTONE_BUILD_STRING " (" WAYSTONE_COMMIT ")";

    CoA::StreamlineClient::Api s_api;
    std::wstring s_pluginDir;
    std::wstring s_logDir;
    const wchar_t* s_pluginPaths[1];
    uint32_t s_computeQueues;
    uint32_t s_graphicsQueues;
    uint32_t s_opticalFlowQueues;
    uint32_t s_deviceCallbacks;
    // slInit has run and slShutdown has not (Streamline::DeviceCreated / DeviceReleasing decide when).
    bool s_slActive;
    // CoaDxvkSetPresentProc (DXVK presents through Streamline's vkQueuePresentKHR proxy, so its presentCommon runs every
    // frame); nullptr with a DXVK that lacks it.
    PFN_CoaDxvkSetPresentProc s_setPresentProc;
    // The Vulkan device Streamline was given (for vkDeviceWaitIdle before slShutdown).
    VkDevice s_vkDevice;

    void WriteLog(const char* line) {
        CoA::Log::Printf("%s", line);
    }

    std::wstring ExeDir() {
        wchar_t path[MAX_PATH];
        DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring dir(path, n);
        size_t slash = dir.find_last_of(L"\\/");
        return slash == std::wstring::npos ? std::wstring(L".") : dir.substr(0, slash);
    }

    // Read before the glue CVars register: Config.wtf already created them unregistered (absent: 0).
    int CVarInt(const char* name) {
        CVar* var = CVar::Lookup(name);
        return var ? var->m_intValue : 0;
    }

    int Mode() {
        const int dlss = static_cast<int>(CoA::Dlss::ModeFromCVar(CVarInt("coaDlssMode")));
        const int frameGen = static_cast<int>(CoA::FrameGen::ModeFromCVar(CVarInt("coaFrameGen")));
        const int reflex = static_cast<int>(CoA::FrameGen::ReflexFromCVar(CVarInt("coaReflex")));
        return CoA::FrameGen::AnyOptionOn(dlss, frameGen, reflex) ? 1 : 0;
    }

    // What slInit loads: Reflex and PCL always; DLSS for super resolution or Frame Generation (its tags); DLSS-G when
    // coaFrameGen is on at startup.
    CoA::Streamline::Features s_features;

    CoA::Streamline::Features LoadFeatures() {
        const bool frameGen = CoA::FrameGen::ModeFromCVar(CVarInt("coaFrameGen")) != CoA::FrameGen::Mode::Off;
        const bool dlss = CoA::Dlss::ModeFromCVar(CVarInt("coaDlssMode")) != CoA::Dlss::Mode::Off;
        s_features = { dlss || frameGen, frameGen, true, true };
        return s_features;
    }

    bool DxvkActive() {
        return CoA::Dxvk::Active();
    }

    // WinVerifyTrust: a valid Authenticode signature chaining to a trusted root.
    bool TrustedFile(const std::wstring& path) {
        WINTRUST_FILE_INFO file = {};
        file.cbStruct = sizeof(file);
        file.pcwszFilePath = path.c_str();

        WINTRUST_DATA data = {};
        data.cbStruct = sizeof(data);
        data.dwUIChoice = WTD_UI_NONE;
        data.fdwRevocationChecks = WTD_REVOKE_NONE;
        data.dwUnionChoice = WTD_CHOICE_FILE;
        data.pFile = &file;
        data.dwStateAction = WTD_STATEACTION_VERIFY;

        GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
        LONG status = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action, &data);
        data.dwStateAction = WTD_STATEACTION_CLOSE;
        WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action, &data);

        if (status != ERROR_SUCCESS) {
            CoA::Log::Printf("Streamline: WinVerifyTrust failed (0x%08lX)", static_cast<unsigned long>(status));
        }

        return status == ERROR_SUCCESS;
    }

    // Both checks, before the file is ever loaded.
    bool Verify(const std::wstring& path) {
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
            WriteLog("Streamline: Vulkan\\sl.interposer.dll not found");
            return false;
        }

        if (!sl::security::verifyEmbeddedSignature(path.c_str())) {
            WriteLog("Streamline: sl.interposer.dll is not signed by NVIDIA");
            return false;
        }

        if (!TrustedFile(path)) {
            return false;
        }

        WriteLog("Streamline: sl.interposer.dll signature ok");
        return true;
    }

    void* Load(const std::wstring& path, uint32_t& err) {
        HMODULE module = LoadLibraryW(path.c_str());
        err = module ? 0 : GetLastError();
        return module;
    }

    template <typename F>
    bool Resolve(HMODULE module, const char* name, F*& fn) {
        fn = reinterpret_cast<F*>(GetProcAddress(module, name));

        if (!fn) {
            CoA::Log::Printf("Streamline: sl.interposer.dll has no %s", name);
        }

        return fn != nullptr;
    }

    template <typename F>
    bool ResolveFeature(sl::Feature feature, const char* name, F*& fn) {
        void* p = nullptr;
        sl::Result r = s_api.getFeatureFunction(feature, name, p);
        fn = r == sl::Result::eOk ? reinterpret_cast<F*>(p) : nullptr;

        if (!fn) {
            CoA::Log::Printf("Streamline: slGetFeatureFunction(%s) %s", name, CoA::StreamlineClient::ResultName(r));
        }

        return fn != nullptr;
    }

    void SlLog(sl::LogType type, const char* msg) {
        if (!msg) {
            return;
        }

        size_t n = strlen(msg);

        while (n && (msg[n - 1] == '\n' || msg[n - 1] == '\r')) {
            --n;
        }

        const char* kind = type == sl::LogType::eError ? "error: " : type == sl::LogType::eWarn ? "warning: " : "";
        CoA::Log::Printf("SL: %s%.*s", kind, static_cast<int>(n), msg);
    }

    void LogNames(const char* what, const char* const* names, uint32_t count) {
        for (uint32_t i = 0; i < count; ++i) {
            CoA::Log::Printf("Streamline:   %s %s", what, names[i] ? names[i] : "(null)");
        }
    }

    // slInit with CoA64's preferences (Global Constraints); s_slActive on success.
    sl::Result SlInit() {
        static sl::Feature kFeatures[4];
        uint32_t count = 0;

        if (s_features.dlss) {
            kFeatures[count++] = sl::kFeatureDLSS;
        }

        if (s_features.frameGen) {
            kFeatures[count++] = sl::kFeatureDLSS_G;
        }

        if (s_features.reflex) {
            kFeatures[count++] = sl::kFeatureReflex;
        }

        if (s_features.pcl) {
            kFeatures[count++] = sl::kFeaturePCL;
        }
        const std::wstring exeDir = ExeDir();
        s_pluginDir = exeDir + L"\\Vulkan";
        s_logDir = exeDir + L"\\Logs";
        s_pluginPaths[0] = s_pluginDir.c_str();

        sl::Preferences pref {};
        pref.logLevel = sl::LogLevel::eDefault;
        pref.pathsToPlugins = s_pluginPaths;
        pref.numPathsToPlugins = 1;
        pref.pathToLogsAndData = s_logDir.c_str();
        pref.logMessageCallback = SlLog;
        // Manual hooking: DXVK creates the instance and device itself; no OTA updates or downloaded plugins.
        pref.flags = sl::PreferenceFlags::eUseManualHooking | sl::PreferenceFlags::eUseFrameBasedResourceTagging |
            sl::PreferenceFlags::eDisableCLStateTracking;
        pref.featuresToLoad = kFeatures;
        pref.numFeaturesToLoad = count;
        pref.engine = sl::EngineType::eCustom;
        pref.engineVersion = kEngineVersion;
        pref.projectId = kCoaProjectId;
        pref.renderAPI = sl::RenderAPI::eVulkan;

        const sl::Result r = s_api.init(pref, sl::kSDKVersion);
        s_slActive = r == sl::Result::eOk;
        return r;
    }

    // slShutdown, once (logged).
    void SlShutdown() {
        if (s_slActive && s_api.shutdown) {
            const sl::Result r = s_api.shutdown();
            CoA::Log::Printf("Streamline: slShutdown %s", CoA::StreamlineClient::ResultName(r));
        }

        s_slActive = false;
    }

    // DXVK presents through Streamline's vkQueuePresentKHR proxy (sl.interposer's vkGetDeviceProcAddr; it calls the
    // driver's after Streamline's present hooks), so Streamline's per-frame bookkeeping (presentCommon) runs.
    void RoutePresent(VkDevice device) {
        HMODULE interposer = GetModuleHandleW(L"sl.interposer.dll");
        auto getDeviceProcAddr = interposer ? reinterpret_cast<PFN_vkGetDeviceProcAddr>(GetProcAddress(interposer, "vkGetDeviceProcAddr")) : nullptr;
        auto present = getDeviceProcAddr ? reinterpret_cast<PFN_vkQueuePresentKHR>(getDeviceProcAddr(device, "vkQueuePresentKHR")) : nullptr;

        if (s_setPresentProc && present) {
            s_setPresentProc(present);
            WriteLog("Streamline: DXVK presents through Streamline's vkQueuePresentKHR");
        } else {
            CoA::Log::Printf("Streamline: present not routed through Streamline (%s)",
                !s_setPresentProc ? "Vulkan\\d3d9.dll lacks CoaDxvkSetPresentProc" : "no vkQueuePresentKHR proxy");
        }
    }

    // The device callback being handled (DeviceSet), and the interop device of the D3D device being released
    // (DeviceShutdown).
    const CoaVkDeviceInfo* s_info;
    ID3D9VkInteropDevice* s_releaseInterop;

    // Streamline::DeviceDeps: slInit again (after a release shut Streamline down).
    bool DeviceInit() {
        const sl::Result r = SlInit();
        CoA::Log::Printf("Streamline: slInit for device %u %s", s_deviceCallbacks, CoA::StreamlineClient::ResultName(r));
        return r == sl::Result::eOk;
    }

    // Streamline::DeviceDeps: slSetVulkanInfo for s_info's device.
    bool DeviceSet(uint64_t /* device */) {
        const CoaVkDeviceInfo* info = s_info;

        if ((s_computeQueues && info->extraComputeQueueFirstIndex == 0xFFFFFFFFu) ||
            (s_graphicsQueues && info->extraGraphicsQueueFirstIndex == 0xFFFFFFFFu)) {
            CoA::Log::Printf("Streamline: DXVK could not reserve the %u compute / %u graphics queues Streamline needs",
                s_computeQueues, s_graphicsQueues);
            return false;
        }

        sl::VulkanInfo vk {};
        vk.instance = info->instance;
        vk.physicalDevice = info->physicalDevice;
        vk.device = info->device;
        // No queues requested: DXVK's own graphics family and queue 0 (a valid queue) rather than the 0xFFFFFFFF
        // "none reserved" markers.
        vk.computeQueueFamily = s_computeQueues ? info->extraComputeQueueFamily : info->graphicsQueueFamily;
        vk.computeQueueIndex = s_computeQueues ? info->extraComputeQueueFirstIndex : 0;
        vk.graphicsQueueFamily = s_graphicsQueues ? info->extraGraphicsQueueFamily : info->graphicsQueueFamily;
        vk.graphicsQueueIndex = s_graphicsQueues ? info->extraGraphicsQueueFirstIndex : 0;

        // Frame Generation's native optical flow runs on its own queue when DXVK could reserve one.
        if (s_opticalFlowQueues && info->opticalFlowQueueIndex != 0xFFFFFFFFu) {
            vk.opticalFlowQueueFamily = info->opticalFlowQueueFamily;
            vk.opticalFlowQueueIndex = info->opticalFlowQueueIndex;
            vk.useNativeOpticalFlowMode = true;
        } else if (s_opticalFlowQueues) {
            WriteLog("Streamline: no optical flow queue reserved (Frame Generation may be unsupported)");
        }

        const sl::Result r = s_api.setVulkanInfo(vk);
        CoA::Log::Printf("Streamline: slSetVulkanInfo %s (compute %u/%u, graphics %u/%u)",
            CoA::StreamlineClient::ResultName(r), vk.computeQueueFamily, vk.computeQueueIndex, vk.graphicsQueueFamily,
            vk.graphicsQueueIndex);
        return r == sl::Result::eOk;
    }

    // The loaded features s_info's adapter runs (DeviceSupported), with their functions resolved.
    CoA::Streamline::Features s_deviceFeatures;

    bool FeatureSupported(sl::Feature feature, const char* name) {
        sl::AdapterInfo adapter {};
        adapter.vkPhysicalDevice = s_info->physicalDevice;
        const sl::Result r = s_api.isFeatureSupported(feature, adapter);
        CoA::Log::Printf("Streamline: slIsFeatureSupported(%s) %s", name, CoA::StreamlineClient::ResultName(r));
        return r == sl::Result::eOk;
    }

    // Streamline::DeviceDeps: at least one loaded feature runs on s_info's adapter.
    bool DeviceSupported() {
        CoA::Streamline::Features& f = s_deviceFeatures;
        f = {};
        f.dlss = s_features.dlss && FeatureSupported(sl::kFeatureDLSS, "DLSS")
            && (ResolveFeature(sl::kFeatureDLSS, "slDLSSGetOptimalSettings", s_api.dlssGetOptimalSettings)
            & ResolveFeature(sl::kFeatureDLSS, "slDLSSSetOptions", s_api.dlssSetOptions) & ResolveFeature(sl::kFeatureDLSS, "slDLSSGetState", s_api.dlssGetState));
        f.frameGen = s_features.frameGen && FeatureSupported(sl::kFeatureDLSS_G, "DLSS-G")
            && (ResolveFeature(sl::kFeatureDLSS_G, "slDLSSGSetOptions", s_api.dlssgSetOptions) & ResolveFeature(sl::kFeatureDLSS_G, "slDLSSGGetState", s_api.dlssgGetState));
        f.reflex = s_features.reflex && FeatureSupported(sl::kFeatureReflex, "Reflex")
            && (ResolveFeature(sl::kFeatureReflex, "slReflexSetOptions", s_api.reflexSetOptions) & ResolveFeature(sl::kFeatureReflex, "slReflexSleep", s_api.reflexSleep)
            & ResolveFeature(sl::kFeatureReflex, "slReflexGetState", s_api.reflexGetState));
        f.pcl = s_features.pcl && FeatureSupported(sl::kFeaturePCL, "PCL") && ResolveFeature(sl::kFeaturePCL, "slPCLSetMarker", s_api.pclSetMarker);
        return f.dlss || f.frameGen || f.reflex || f.pcl;
    }

    CoA::Streamline::Features DeviceFeatures() {
        return s_deviceFeatures;
    }

    // Streamline::DeviceDeps: DXVK's swapchain calls through Streamline's proxies (sl_hooks.h), or back to the driver's.
    PFN_CoaDxvkSetSwapchainProcs s_setSwapchainProcs;

    bool RouteSwapchain(bool on) {
        if (!s_setSwapchainProcs) {
            WriteLog("Streamline: Vulkan\\d3d9.dll lacks CoaDxvkSetSwapchainProcs (Frame Generation cannot present)");
            return false;
        }

        if (!on) {
            s_setSwapchainProcs(nullptr);
            WriteLog("Streamline: swapchain back on the driver");
            return true;
        }

        HMODULE interposer = GetModuleHandleW(L"sl.interposer.dll");
        auto gdpa = interposer ? reinterpret_cast<PFN_vkGetDeviceProcAddr>(GetProcAddress(interposer, "vkGetDeviceProcAddr")) : nullptr;
        auto gipa = interposer ? reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(interposer, "vkGetInstanceProcAddr")) : nullptr;

        if (!gdpa || !gipa || !s_info) {
            WriteLog("Streamline: no interposer proc address functions, swapchain not routed");
            return false;
        }

        VkDevice d = s_info->device;
        VkInstance i = s_info->instance;
        CoaVkSwapchainProcs procs {};
        procs.createSwapchain = reinterpret_cast<PFN_vkCreateSwapchainKHR>(gdpa(d, "vkCreateSwapchainKHR"));
        procs.destroySwapchain = reinterpret_cast<PFN_vkDestroySwapchainKHR>(gdpa(d, "vkDestroySwapchainKHR"));
        procs.getSwapchainImages = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(gdpa(d, "vkGetSwapchainImagesKHR"));
        procs.acquireNextImage = reinterpret_cast<PFN_vkAcquireNextImageKHR>(gdpa(d, "vkAcquireNextImageKHR"));
        procs.queuePresent = reinterpret_cast<PFN_vkQueuePresentKHR>(gdpa(d, "vkQueuePresentKHR"));
        procs.deviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(gdpa(d, "vkDeviceWaitIdle"));
        procs.createWin32Surface = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(gipa(i, "vkCreateWin32SurfaceKHR"));
        procs.destroySurface = reinterpret_cast<PFN_vkDestroySurfaceKHR>(gipa(i, "vkDestroySurfaceKHR"));
        // All or nothing: a swapchain created by Streamline but presented by the driver (or the reverse) breaks.
        if (!procs.createSwapchain || !procs.destroySwapchain || !procs.getSwapchainImages || !procs.acquireNextImage
            || !procs.queuePresent || !procs.deviceWaitIdle || !procs.createWin32Surface || !procs.destroySurface) {
            WriteLog("Streamline: a swapchain proxy is missing, swapchain not routed");
            return false;
        }

        s_setSwapchainProcs(&procs);
        WriteLog("Streamline: swapchain routed through Streamline");
        return true;
    }

    // Streamline::DeviceDeps: release DLSS, unroute present, drain DXVK, then slShutdown while the VkDevice exists.
    void DeviceShutdown() {
        CoA::DlssClient::Release();
        CoA::FrameGenClient::Release();

        if (s_setPresentProc) {
            s_setPresentProc(nullptr);
        }

        if (ID3D9VkInteropDevice* interop = s_releaseInterop) {
            HMODULE vulkan = GetModuleHandleW(L"vulkan-1.dll");
            auto deviceWaitIdle = vulkan ? reinterpret_cast<PFN_vkDeviceWaitIdle>(GetProcAddress(vulkan, "vkDeviceWaitIdle")) : nullptr;
            interop->FlushRenderingCommands();
            interop->LockSubmissionQueue();

            if (deviceWaitIdle && s_vkDevice) {
                deviceWaitIdle(s_vkDevice);
            }

            interop->ReleaseSubmissionQueue();
        }

        SlShutdown();
        s_vkDevice = nullptr;
    }

    const CoA::Streamline::DeviceDeps kDeviceDeps = { DeviceInit, DeviceSet, DeviceSupported, DeviceShutdown, WriteLog, DeviceFeatures, RouteSwapchain,
        CoA::StreamlineClient::FlipPresent };

    // DXVK, right after vkCreateDevice; Streamline::DeviceCreated decides (notes: OnDevice).
    void __stdcall OnDevice(const CoaVkDeviceInfo* info) {
        ++s_deviceCallbacks;
        CoA::Log::Printf("Streamline: device callback %u (graphics queue family %u index %u, reserved compute %u/%u, "
            "reserved graphics %u/%u)", s_deviceCallbacks, info->graphicsQueueFamily, info->graphicsQueueIndex,
            info->extraComputeQueueFamily, info->extraComputeQueueFirstIndex, info->extraGraphicsQueueFamily,
            info->extraGraphicsQueueFirstIndex);

        s_info = info;
        CoA::Streamline::DeviceCreated(kDeviceDeps, reinterpret_cast<uint64_t>(info->device));
        s_info = nullptr;

        if (CoA::Streamline::Ready()) {
            const CoA::Streamline::Features f = CoA::Streamline::Supported();
            CoA::Log::Printf("Streamline: ready (DLSS %s, Frame Generation %s, Reflex %s, PCL %s)", f.dlss ? "on" : "off",
                f.frameGen ? "on" : "off", f.reflex ? "on" : "off", f.pcl ? "on" : "off");
            s_vkDevice = info->device;
            RoutePresent(info->device);
        } else {
            CoA::Log::Printf("DLSS off: %s (Streamline's Vulkan requirements stay applied to DXVK for this process)",
                CoA::Dlss::OffName(CoA::Streamline::State()));
        }
    }

    // A DXVK device going away without a D3D9 release (failed InitialReset): free Streamline's hold on it.
    void __stdcall OnDeviceDestroying(VkDevice device) {
        if (CoA::Streamline::DeviceDestroying(kDeviceDeps, reinterpret_cast<uint64_t>(device))) {
            WriteLog("Streamline: shut down with the DXVK device");
        }
    }

    // After a successful slInit: undo it before reporting failure.
    bool Fail() {
        SlShutdown();
        s_api = {};
        return false;
    }

    bool Init(void* module) {
        HMODULE m = static_cast<HMODULE>(module);
        auto& a = s_api;
        a = {};
        bool ok = Resolve(m, "slInit", a.init) & Resolve(m, "slShutdown", a.shutdown) &
            Resolve(m, "slGetFeatureRequirements", a.getFeatureRequirements) &
            Resolve(m, "slSetVulkanInfo", a.setVulkanInfo) & Resolve(m, "slIsFeatureSupported", a.isFeatureSupported) &
            Resolve(m, "slGetFeatureFunction", a.getFeatureFunction) &
            Resolve(m, "slGetNewFrameToken", a.getNewFrameToken) & Resolve(m, "slSetConstants", a.setConstants) &
            Resolve(m, "slSetTagForFrame", a.setTagForFrame) & Resolve(m, "slEvaluateFeature", a.evaluateFeature) &
            Resolve(m, "slFreeResources", a.freeResources);

        if (!ok) {
            a = {};
            return false;
        }

        sl::Result r = SlInit();
        CoA::Log::Printf("Streamline: slInit %s (engine \"%s\")", CoA::StreamlineClient::ResultName(r), kEngineVersion);

        if (r != sl::Result::eOk) {
            a = {};
            return false;
        }

        // The union of every loaded feature's Vulkan requirements; a feature without them is dropped.
        static std::vector<std::string> names[4];
        static std::vector<const char*> ptrs[4];
        uint32_t computeQueues = 0;
        uint32_t graphicsQueues = 0;
        uint32_t opticalFlowQueues = 0;
        const struct { bool* on; sl::Feature feature; const char* name; } wanted[] = {
            { &s_features.dlss, sl::kFeatureDLSS, "DLSS" }, { &s_features.frameGen, sl::kFeatureDLSS_G, "DLSS-G" },
            { &s_features.reflex, sl::kFeatureReflex, "Reflex" }, { &s_features.pcl, sl::kFeaturePCL, "PCL" },
        };
        auto add = [](std::vector<std::string>& list, const char* const* items, uint32_t n) {
            for (uint32_t k = 0; k < n; ++k) {
                if (items[k] && std::find(list.begin(), list.end(), items[k]) == list.end()) {
                    list.emplace_back(items[k]);
                }
            }
        };

        for (const auto& w : wanted) {
            if (!*w.on) {
                continue;
            }

            sl::FeatureRequirements req {};
            r = a.getFeatureRequirements(w.feature, req);
            CoA::Log::Printf("Streamline: slGetFeatureRequirements(%s) %s", w.name, CoA::StreamlineClient::ResultName(r));

            if (r != sl::Result::eOk || !(static_cast<uint32_t>(req.flags) & static_cast<uint32_t>(sl::FeatureRequirementFlags::eVulkanSupported))) {
                *w.on = false;
                continue;
            }

            add(names[0], req.vkInstanceExtensions, req.vkNumInstanceExtensions);
            add(names[1], req.vkDeviceExtensions, req.vkNumDeviceExtensions);
            add(names[2], req.vkFeatures12, req.vkNumFeatures12);
            add(names[3], req.vkFeatures13, req.vkNumFeatures13);
            computeQueues = std::max(computeQueues, req.vkNumComputeQueuesRequired);
            graphicsQueues = std::max(graphicsQueues, req.vkNumGraphicsQueuesRequired);
            opticalFlowQueues = std::max(opticalFlowQueues, req.vkNumOpticalFlowQueuesRequired);
        }

        if (!s_features.dlss && !s_features.frameGen && !s_features.reflex && !s_features.pcl) {
            WriteLog("Streamline: no loaded feature supports Vulkan");
            return Fail();
        }

        for (int k = 0; k < 4; ++k) {
            ptrs[k].clear();

            for (const std::string& n : names[k]) {
                ptrs[k].push_back(n.c_str());
            }
        }

        void* dxvk = CoA::Dxvk::D3d9Library();

        if (!dxvk) {
            WriteLog("Streamline: no DXVK module");
            return Fail();
        }

        auto setRequirements = reinterpret_cast<PFN_CoaDxvkSetVulkanRequirements>(
            GetProcAddress(static_cast<HMODULE>(dxvk), "CoaDxvkSetVulkanRequirements"));
        auto setDeviceCallback = reinterpret_cast<PFN_CoaDxvkSetDeviceCallback>(
            GetProcAddress(static_cast<HMODULE>(dxvk), "CoaDxvkSetDeviceCallback"));

        s_setPresentProc = reinterpret_cast<PFN_CoaDxvkSetPresentProc>(
            GetProcAddress(static_cast<HMODULE>(dxvk), "CoaDxvkSetPresentProc"));
        s_setSwapchainProcs = reinterpret_cast<PFN_CoaDxvkSetSwapchainProcs>(
            GetProcAddress(static_cast<HMODULE>(dxvk), "CoaDxvkSetSwapchainProcs"));

        if (!setRequirements || !setDeviceCallback) {
            WriteLog("Streamline: Vulkan\\d3d9.dll lacks CoaDxvkSetVulkanRequirements / CoaDxvkSetDeviceCallback");
            FreeLibrary(static_cast<HMODULE>(dxvk));
            return Fail();
        }

        s_computeQueues = computeQueues;
        s_graphicsQueues = graphicsQueues;
        s_opticalFlowQueues = opticalFlowQueues;

        CoaVkRequirements vk {};
        vk.instanceExtensions = ptrs[0].data();
        vk.instanceExtensionCount = static_cast<uint32_t>(ptrs[0].size());
        vk.deviceExtensions = ptrs[1].data();
        vk.deviceExtensionCount = static_cast<uint32_t>(ptrs[1].size());
        vk.features12 = ptrs[2].data();
        vk.features12Count = static_cast<uint32_t>(ptrs[2].size());
        vk.features13 = ptrs[3].data();
        vk.features13Count = static_cast<uint32_t>(ptrs[3].size());
        vk.extraComputeQueues = computeQueues;
        vk.extraGraphicsQueues = graphicsQueues;
        vk.opticalFlowQueues = opticalFlowQueues;

        CoA::Log::Printf("Streamline: Vulkan requirements: %u instance extensions, %u device extensions, %u 1.2 "
            "features, %u 1.3 features, %u compute queues, %u graphics queues", vk.instanceExtensionCount,
            vk.deviceExtensionCount, vk.features12Count, vk.features13Count, vk.extraComputeQueues,
            vk.extraGraphicsQueues);
        LogNames("instance extension", vk.instanceExtensions, vk.instanceExtensionCount);
        LogNames("device extension", vk.deviceExtensions, vk.deviceExtensionCount);
        LogNames("1.2 feature", vk.features12, vk.features12Count);
        LogNames("1.3 feature", vk.features13, vk.features13Count);

        // DXVK copies the requirements at the call; from here they apply to every DXVK instance of this process.
        setDeviceCallback(OnDevice);

        if (auto setDestroy = reinterpret_cast<PFN_CoaDxvkSetDeviceDestroyCallback>(
                GetProcAddress(static_cast<HMODULE>(dxvk), "CoaDxvkSetDeviceDestroyCallback"))) {
            setDestroy(OnDeviceDestroying);
        }

        setRequirements(&vk);
        FreeLibrary(static_cast<HMODULE>(dxvk));
        WriteLog("Streamline: requirements and device callback passed to DXVK");
        return true;
    }
}

// FrameGen::SlReflexMode's values are sl::ReflexMode's enumerators.
static_assert(static_cast<uint32_t>(sl::ReflexMode::eOff) == 0 && static_cast<uint32_t>(sl::ReflexMode::eLowLatency) == 1 &&
    static_cast<uint32_t>(sl::ReflexMode::eLowLatencyWithBoost) == 2, "sl::ReflexMode changed: update FrameGen::SlReflexMode");

// Dlss::SlMode's values are sl::DLSSMode's enumerators.
static_assert(static_cast<uint32_t>(sl::DLSSMode::eOff) == 0 && static_cast<uint32_t>(sl::DLSSMode::eMaxPerformance) == 1 &&
    static_cast<uint32_t>(sl::DLSSMode::eBalanced) == 2 && static_cast<uint32_t>(sl::DLSSMode::eMaxQuality) == 3 &&
    static_cast<uint32_t>(sl::DLSSMode::eUltraPerformance) == 4 && static_cast<uint32_t>(sl::DLSSMode::eDLAA) == 6,
    "sl::DLSSMode changed: update Dlss::SlMode");

const char* CoA::StreamlineClient::ResultName(sl::Result result) {
    const char* name = sl::getResultAsStr(result);
    return strncmp(name, "Result::", 8) ? name : name + 8;
}

const CoA::StreamlineClient::Api* CoA::StreamlineClient::GetApi() {
    return Ready() && s_slActive ? &s_api : nullptr;
}

// g_d3dDeviceReleaseHook: shut Streamline down only with the device it was handed (notes: BeforeDeviceRelease).
void CoA::StreamlineClient::BeforeDeviceRelease(void* d3dDevice) {
    IDirect3DDevice9* device = static_cast<IDirect3DDevice9*>(d3dDevice);
    void* object = nullptr;

    if (!device || FAILED(device->QueryInterface(__uuidof(ID3D9VkInteropDevice), &object)) || !object) {
        return;
    }

    ID3D9VkInteropDevice* interop = static_cast<ID3D9VkInteropDevice*>(object);
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice vkDevice = VK_NULL_HANDLE;
    interop->GetVulkanHandles(&instance, &physicalDevice, &vkDevice);

    s_releaseInterop = interop;

    if (CoA::Streamline::DeviceReleasing(kDeviceDeps, reinterpret_cast<uint64_t>(vkDevice))) {
        WriteLog("Streamline: shut down with the D3D device");
    }

    s_releaseInterop = nullptr;
    interop->Release();
}

void CoA::StreamlineClient::PreDevice() {
    CoA::Streamline::PreDevice({ Mode, DxvkActive, ExeDir, Verify, Load, Init, WriteLog, LoadFeatures });

    if (CoA::Streamline::Loaded().reflex || CoA::Streamline::Loaded().pcl) {
        CoA::SlFrameClient::Install();
    }

    if (s_slActive) {
        g_d3dDeviceReleaseHook = BeforeDeviceRelease;
    }
}

bool CoA::StreamlineClient::Ready() {
    return CoA::Streamline::Ready();
}
#endif
