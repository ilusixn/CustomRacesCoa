#pragma once

// Client side of streamline.patch (CoA64's DXVK d3d9.dll): the exports that hand Streamline's Vulkan requirements
// to DXVK before device creation, return the created device and route DXVK's present through Streamline, and
// ID3D9VkInteropDevice1, which records Vulkan commands into DXVK's own command stream. Declarations match the patch exactly (dxvk_coa_streamline.h and
// d3d9_interfaces.h); keep them in sync. Needs the Vulkan headers (include <vulkan/vulkan.h> first or have them on the
// include path). Everything is inert in DXVK unless CoaDxvkSetVulkanRequirements was called.

#include <d3d9.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include <cstdint>

// Copied by DXVK at the call; names are VkPhysicalDeviceVulkan12Features / 13Features member names (unknown or
// unsupported ones are logged and ignored, as are unsupported extensions). Call before Direct3DCreate9: each DXVK
// instance (one per Direct3DCreate9) latches the requirements when it is created and uses that snapshot for its
// extensions, features, queues, NVX, latency and binding-model (descriptor heap off: NGX writes zeros under it)
// decisions; later calls only affect instances created afterwards.
struct CoaVkRequirements {
    const char* const* instanceExtensions; uint32_t instanceExtensionCount;
    const char* const* deviceExtensions;   uint32_t deviceExtensionCount;
    const char* const* features12;         uint32_t features12Count;   // names as in VkPhysicalDeviceVulkan12Features
    const char* const* features13;         uint32_t features13Count;   // names as in VkPhysicalDeviceVulkan13Features
    uint32_t extraComputeQueues; uint32_t extraGraphicsQueues;
    uint32_t opticalFlowQueues;                                     // VK_QUEUE_OPTICAL_FLOW_BIT_NV family (Frame Generation)
};

// Passed to the device callback right after vkCreateDevice succeeded. Extra compute queues come from a dedicated
// compute family if the device has one (else DXVK's graphics family), extra graphics queues from DXVK's graphics
// family; reserved queues follow DXVK's own in that family. Family and index are 0xFFFFFFFF when none were requested;
// the index alone is 0xFFFFFFFF when the family could not provide them.
struct CoaVkDeviceInfo {
    VkInstance instance; VkPhysicalDevice physicalDevice; VkDevice device;
    uint32_t graphicsQueueFamily, graphicsQueueIndex;               // DXVK's own queue
    uint32_t extraComputeQueueFamily, extraComputeQueueFirstIndex;   // first queue reserved for the app
    uint32_t extraGraphicsQueueFamily, extraGraphicsQueueFirstIndex; // (0xFFFFFFFF: none reserved)
    uint32_t opticalFlowQueueFamily, opticalFlowQueueIndex;
};

// Streamline's proxies for the calls it must see (sl_hooks.h, Vulkan); a null member keeps the driver's.
struct CoaVkSwapchainProcs {
    PFN_vkCreateSwapchainKHR    createSwapchain;
    PFN_vkDestroySwapchainKHR   destroySwapchain;
    PFN_vkGetSwapchainImagesKHR getSwapchainImages;
    PFN_vkAcquireNextImageKHR   acquireNextImage;
    PFN_vkQueuePresentKHR       queuePresent;
    PFN_vkDeviceWaitIdle        deviceWaitIdle;
    PFN_vkCreateWin32SurfaceKHR createWin32Surface;
    PFN_vkDestroySurfaceKHR     destroySurface;
};

// DXVK's queue thread, around a present stamped with a client frame (marker 0 start, 1 end).
typedef void (__stdcall* PFN_CoaPresentMarker)(uint32_t marker, uint64_t frameIndex);

typedef void (__stdcall* PFN_CoaVkDeviceCallback)(const CoaVkDeviceInfo* info);
// From ~DxvkDevice, device idle, before the VkDevice is destroyed; also for a device whose D3D9 device never existed.
typedef void (__stdcall* PFN_CoaVkDeviceDestroyCallback)(VkDevice device);

// d3d9.dll exports (load with GetProcAddress)
typedef void (__stdcall* PFN_CoaDxvkSetVulkanRequirements)(const CoaVkRequirements* req);
typedef void (__stdcall* PFN_CoaDxvkSetDeviceCallback)(PFN_CoaVkDeviceCallback cb);
// vkQueuePresentKHR replacement for DXVK's presenter (Streamline's proxy, so its presentCommon runs every frame); null
// restores the driver's. Any time. The presenter calls it on DXVK's submission thread under the device queue lock, so
// clearing it and then taking and dropping that lock (ID3D9VkInteropDevice::LockSubmissionQueue) means no call into the
// old proc is still running.
typedef void (__stdcall* PFN_CoaDxvkSetPresentProc)(PFN_vkQueuePresentKHR proc);
typedef void (__stdcall* PFN_CoaDxvkSetDeviceDestroyCallback)(PFN_CoaVkDeviceDestroyCallback cb);
// Set before any swapchain exists and cleared only after the device's swapchains are gone (null: the driver's).
typedef void (__stdcall* PFN_CoaDxvkSetSwapchainProcs)(const CoaVkSwapchainProcs* procs);
typedef void (__stdcall* PFN_CoaDxvkSetNextPresentFrame)(uint64_t frameIndex);
typedef void (__stdcall* PFN_CoaDxvkSetPresentMarkerCallback)(PFN_CoaPresentMarker cb);

// DXVK's existing interop types (d3d9_interfaces.h), needed for the ID3D9VkInteropDevice vtable

MIDL_INTERFACE("d56344f5-8d35-46fd-806d-94c351b472c1")
ID3D9VkInteropTexture : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetVulkanImageInfo(
            VkImage*              pHandle,
            VkImageLayout*        pLayout,
            VkImageCreateInfo*    pInfo) = 0;
};

struct D3D9VkExtImageDesc {
    D3DRESOURCETYPE     Type;
    UINT                Width;
    UINT                Height;
    UINT                Depth;
    UINT                MipLevels;
    DWORD               Usage;
    D3DFORMAT           Format;
    D3DPOOL             Pool;
    D3DMULTISAMPLE_TYPE MultiSample;
    DWORD               MultiSampleQuality;
    bool                Discard;
    bool                IsAttachmentOnly;
    bool                IsLockable;
    VkImageUsageFlags   ImageUsage;
};

MIDL_INTERFACE("2eaa4b89-0107-4bdb-87f7-0f541c493ce0")
ID3D9VkInteropDevice : public IUnknown {
    virtual void STDMETHODCALLTYPE GetVulkanHandles(
            VkInstance*           pInstance,
            VkPhysicalDevice*     pPhysDev,
            VkDevice*             pDevice) = 0;

    virtual void STDMETHODCALLTYPE GetSubmissionQueue(
            VkQueue*              pQueue,
            uint32_t*             pQueueIndex,
            uint32_t*             pQueueFamilyIndex) = 0;

    virtual void STDMETHODCALLTYPE TransitionTextureLayout(
            ID3D9VkInteropTexture*    pTexture,
      const VkImageSubresourceRange*  pSubresources,
            VkImageLayout             OldLayout,
            VkImageLayout             NewLayout) = 0;

    virtual void STDMETHODCALLTYPE FlushRenderingCommands() = 0;

    virtual void STDMETHODCALLTYPE LockSubmissionQueue() = 0;

    virtual void STDMETHODCALLTYPE ReleaseSubmissionQueue() = 0;

    virtual void STDMETHODCALLTYPE LockDevice() = 0;

    virtual void STDMETHODCALLTYPE UnlockDevice() = 0;

    virtual bool STDMETHODCALLTYPE WaitForResource(
            IDirect3DResource9*  pResource,
            DWORD                MapFlags) = 0;

    virtual HRESULT STDMETHODCALLTYPE CreateImage(
            const D3D9VkExtImageDesc* desc,
            IDirect3DResource9**      ppResult) = 0;
};

// In: resource (texture or surface), the layout the callback needs it in (VK_IMAGE_LAYOUT_UNDEFINED: the image's
// default DXVK layout) and the usage of the view to create (0: SAMPLED). Out, at callback time: image, a view of mip 0 /
// layer 0 (VK_NULL_HANDLE if the image lacks the usage; depth-only for depth-stencil images unless the usage is
// DEPTH_STENCIL_ATTACHMENT), format, extent and the layout the image is actually in, as tracked by DXVK right before
// the callback (DXVK may substitute GENERAL).
// The callback must leave each image in that layout.
struct D3D9VkImageAccess {
    IDirect3DResource9* pResource; VkImageLayout requiredLayout; VkImageUsageFlags viewUsage;
    VkImage image; VkImageView view; VkFormat format; VkExtent3D extent; VkImageLayout layout;
};

// Called later on DXVK's CS thread, not inside RecordCallback, with DXVK's current command buffer (outside any render
// pass, barriers flushed, a full memory barrier recorded just before), a copy of the image array with the out fields
// filled, and the copied user blob (or `user` if no blob was given). `images` and the blob pointer are only valid
// during the callback; `user` itself must stay valid until the callback has run.
typedef void (__stdcall* PFN_D3D9VkRecordCallback)(VkCommandBuffer cmd, D3D9VkImageAccess* images, UINT count, void* user);

// QueryInterface on the D3D9 device. RecordCallback returns D3D_OK, or D3DERR_INVALIDCALL for a null callback, a null
// image array / blob with a non-zero count / size, or a resource without a Vulkan image. After the callback DXVK records
// an ALL_COMMANDS memory barrier and tracks the images as written.
MIDL_INTERFACE("f9e3883a-e7ac-42ed-9a1d-d8f50e43aea9")
ID3D9VkInteropDevice1 : public ID3D9VkInteropDevice {
    virtual HRESULT STDMETHODCALLTYPE RecordCallback(
            PFN_D3D9VkRecordCallback  fn,
            void*                     user,
      const D3D9VkImageAccess*        images,
            UINT                      count,
      const void*                     userBlob,
            UINT                      userBlobSize) = 0;
};
