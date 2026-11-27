# StreamlineClient.cpp notes (2026-10-29)

Rationale moved out of `CoA/Source/Runtime/StreamlineClient.cpp` inline comments (comment budget).

## File overview

Streamline's Win32 half (DLSS, stage 3): coaDlssMode from the loaded Config.wtf, NVIDIA's signature on
sl.interposer.dll, LoadLibraryW, slInit with manual hooking for Vulkan, Streamline's Vulkan requirements and the
device callback handed to CoA64's DXVK (ThirdParty/dxvk/include/coa_dxvk_streamline.h) before Direct3DCreate9, then
slSetVulkanInfo / slIsFeatureSupported when DXVK has created its device. The decisions are CoA::Streamline's
(Dlss.cpp). Not part of the original (CoA64 addition).

## Mode()

Read before the glue CVars are registered: CVar::Initialize (InitializeGlobal) has already executed Config.wtf's
SET lines (or -config's file), which create the CVar unregistered with the stored value; it is absent when
the file has no line for it (mode 0).

## DeviceShutdown()

Streamline::DeviceDeps: DLSS's resources released (DXVK's stream drained, its output finished, slFreeResources),
DXVK's present taken off Streamline's proxy, and, when a D3D device is being released, DXVK's CS thread
synchronised and the Vulkan device idle under DXVK's queue lock (no submission or present meanwhile); then
slShutdown, while the Vulkan device still exists.

## OnDevice()

DXVK calls this right after vkCreateDevice (inside Direct3DCreate9's CreateDevice). Streamline::DeviceCreated
decides: the first device after PreDevice's slInit, a later one after a release shut Streamline down (slInit again
first), none while Streamline still holds another device. Requirements: a device re-created under the same
IDirect3D9 (DeviceSetFormat) belongs to the same DxvkInstance, which latched them; a new Direct3DCreate9 (a new
DxvkInstance) latches DXVK's process-wide copy, which CoaDxvkSetVulkanRequirements set once and never clears.

## BeforeDeviceRelease()

g_d3dDeviceReleaseHook (both D3D9 devices' IDestroyD3d, right before the IDirect3DDevice9 is released: device
re-creation and client shutdown, and also DeviceSetFormat's IDestroyD3d before the first device exists, with a null
device). Streamline shuts down (DeviceShutdown) only when the Vulkan device behind this D3D device is the one it was
handed; any other call does nothing. Without the shutdown NGX's own teardown ran at process exit against the
destroyed device (the crash in nvcuda64.dll from nvngx_dlssd.dll's DLL detach). Nothing calls Streamline at process
exit (no atexit / DLL detach path): a device that is never released leaves Streamline as it is.
