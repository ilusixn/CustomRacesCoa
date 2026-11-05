# Render Pipeline Stage 3 (Streamline + DLSS Super Resolution / DLAA) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Behind console CVar `coaDlssMode` (0 off · 1 DLAA · 2 Quality · 3 Balanced · 4 Performance · 5 Ultra
Performance), render the 3D world at DLSS's render resolution and upscale it with NVIDIA DLSS (Streamline v2.14.16,
Vulkan, manual hooking, inside DXVK) to full resolution before post-processing and UI.

**Architecture:** Streamline is loaded by CoA64 itself (only when `coaDlssMode` ≠ 0 at startup), initialised before the
D3D9 device exists, and hands its Vulkan requirements to DXVK through a small DXVK patch (exported setter + device
callback). Per frame the world renders at render size (engine window-rect override + negative texture LOD bias), the
stage 1/2 targets supply colour/depth/motion/mask, a remap pass converts depth to [0,1], and DLSS is recorded into
DXVK's own command stream through a new interop call (`RecordCallback`); its full-size output is copied to the back
buffer in place of stage 1's copy-back.

**Tech Stack:** C++ (MSVC x64), Direct3D 9 → DXVK v3.1.1 (Raz0r1337 fork, patched) → Vulkan, NVIDIA Streamline SDK
v2.14.16 (prebuilt **production** binaries: `sl.interposer.dll`, `sl.common.dll`, `sl.dlss.dll`, `nvngx_dlss.dll`),
HLSL via fxc, CoATests, in-game checks in `D:\WoW\CoA-dev`.

**Spec:** `docs/superpowers/specs/2026-10-26-streamline-rendering-design.md` (sections 4, 6.2, 6.5, 7 stage 3, 9 Q-A/Q-B).

## Global Constraints

- `coaDlssMode 0` at startup (default): no Streamline DLL is loaded, DXVK is called exactly as today (no requirements
  set, no callback), and stages 1/2 behave as before. Switching 0 → non-zero needs a client restart (spec Q-A);
  switching between non-zero modes and back to 0 applies live once Streamline is loaded.
- Streamline only on: DXVK active, NVIDIA GPU supported (`slIsFeatureSupported(kFeatureDLSS)` true), pipeline
  activatable (no MSAA, INTZ, 24-bit depth, shaders). Otherwise DLSS stays off with one log line naming the reason.
- Only NVIDIA's signed **production** DLLs are staged/packaged; `sl.interposer.dll` is verified with
  `sl::security::verifyEmbeddedSignature(fullPath)` (and `WinVerifyTrust`) before `LoadLibraryW`; a failed check
  leaves DLSS off. All DLLs live in `<exe dir>\x64\` beside DXVK's `d3d9.dll`.
- `slInit`: `renderAPI = eVulkan`, `featuresToLoad = { kFeatureDLSS }`, flags `eUseManualHooking |
  eUseFrameBasedResourceTagging | eDisableCLStateTracking` (no `eAllowOTA`, no `eLoadDownloadedPlugins`),
  `engine = EngineType::eCustom`, `engineVersion` = the CoA64 build tag, `projectId = kCoaProjectId` (one constant, a fixed
  GUID; no NVIDIA application id is needed — `applicationId` is optional when engine type + version are given), `pathsToPlugins = { <exe dir>\x64 }`, logs to `<exe dir>\Logs\`.
- `sl::Constants` per the SDK header (`sl_consts.h`): row-major matrices **without jitter**;
  `clipToPrevClip = clipToView * viewToViewPrev * viewToClipPrev`; `jitterOffset` in pixels; `mvecScale` normalises
  motion to [-1,1]; `cameraMotionIncluded = eTrue`; `depthInverted = eFalse`; `motionVectorsJittered = eFalse`;
  `reset` from `Frame::reset`; `renderingGameFrames = eTrue` only on frames where DLSS evaluates.
- Our motion texture is `prevUV − curUV` in UV units (stage 2) → `mvecScale = (1, 1)` with sign multiplier
  `coaDlssMvecSign` (debug CVar, default 1). Jitter passed as `(jitterX, jitterY)` from `Frame` times
  `coaDlssJitterSign` (debug CVar, default 1). Both debug CVars exist so the convention can be confirmed in-game
  without a rebuild; the confirmed values become the defaults.
- Depth tagged to DLSS = a remapped R32F texture holding `saturate((stored − depthMin) / (depthMax − depthMin))`
  (D3D native [0,1]); sky / clear depth ≥ depthMax maps to 1.
- New CVars (glue, flags 1, category 1): `coaDlssMode` default "0", `coaDlssMvecSign` default "1", `coaDlssJitterSign`
  default "1". `coaDlssMode` ≠ 0 implies the pipeline and motion are on (they are forced active internally; the
  user-facing `coaRenderPipeline`/`coaRenderMotion` CVars are not rewritten).
- Never write into `D:\WoW\CoA`. Both D3D9 device classes get identical engine changes (`// CoA64:` comments).

## Review Focus

1. **Window resize / mode change while DLSS is on** — render size, targets and DLSS resources are recreated
   (`slFreeResources` then re-evaluate), no stale-size image, no crash. (Task 6 resize handling; Task 7 in-game.)
2. **Device reset / alt-tab** — all DXVK-created images (output texture, remapped depth) released before Reset; DLSS
   resumes after. (Task 6; Task 7 in-game.)
3. **DLSS unsupported at runtime** (old driver, non-RTX, signature failure, `slInit` error) — DLSS off, game renders
   exactly as with `coaDlssMode 0`, one log line. (Task 5 test `Streamline_UnsupportedLeavesDlssOff`.)
4. **Mode 0 at startup** — `sl.*.dll` never loaded (checked by enumerating process modules in-game). (Task 7.)
5. **Things sized in window pixels inside the world** (sand/weather point sizes, sun-glare occlusion, sky scissor)
   — use render size while the world renders; FFX still sees display size. (Task 4 tests on the size override.)

---

## File Structure

| File | Responsibility |
|---|---|
| `ThirdParty/streamline/CMakeLists.txt` (new) | Download pinned SDK zip (SHA256), expose headers, stage production DLLs into `bin/Win/x64`. |
| `ThirdParty/dxvk/streamline.patch` (new) + `ThirdParty/dxvk/CMakeLists.txt` | DXVK patch, applied after `hqx-msvc.patch`. |
| `CoA/Source/Runtime/Dlss.h/.cpp` (new) | Pure logic: mode table, render size fallback, LOD bias, `sl::Constants` math (clipToPrevClip etc.), activation reasons. |
| `CoA/Source/Runtime/StreamlineClient.cpp` (new) | Loading/verifying SL, `slInit`, requirements → DXVK, device callback → `slSetVulkanInfo`, support check, per-frame DLSS evaluate. |
| `CoA/Source/Render/Shaders/DepthRemapPS.hlsl` (new) | Depth → [0,1] R32F. |
| Engine `CGxDevice.h/.cpp`, both D3D9 classes | Render-size override (window rect), LOD bias in `ISetTexture`, world target at render size. |
| `WoW/Source/Ui/WorldFrame.cpp` | Hook `worldSized()` right after `FFX::Start()`. |
| `CoA/Tests/DlssTest.cpp` (new) | Tests for `Dlss` pure logic and the Streamline client's decision logic (with fakes). |

---

### Task 1: Streamline SDK in the build

**Files:** Create `ThirdParty/streamline/CMakeLists.txt`; modify `ThirdParty/CMakeLists.txt`, `BuildCoA64.bat`
(package the four DLLs from `bin/Win/x64`).

**Interfaces:** Produces include dir target `streamline_headers` (INTERFACE library: `include/` of the SDK) and the
staged DLLs `bin/Win/x64/{sl.interposer,sl.common,sl.dlss,nvngx_dlss}.dll` (production variants from the SDK's
`bin/x64/`, never `bin/x64/development`).

- [ ] **Step 1:** `file(DOWNLOAD https://github.com/NVIDIA-RTX/Streamline/releases/download/v2.14.16/streamline-sdk-v2.14.16.zip ... EXPECTED_HASH SHA256=<pin>)` into the build dir (cache: skip when present), extract, `add_library(streamline_headers INTERFACE)`; compute the SHA256 once and pin it in the file. Option `WOWDECOMP_STREAMLINE_SDK_DIR` (local extracted SDK) to build offline, mirroring DXVK's `DXVK_SOURCE_DIR`.
- [ ] **Step 2:** stage the four production DLLs with `configure_file(... COPYONLY)` or a custom command; `BuildCoA64.bat` adds them to the zip.
- [ ] **Step 3:** Build `Wow`; verify the four files exist in `bin/Win/x64` and `dumpbin /headers sl.interposer.dll` shows x64; verify `sl::security::verifyEmbeddedSignature` header compiles in a CoA TU.
- [ ] **Step 4: Commit** — `Build: Streamline SDK v2.14.16 (headers, production DLLs staged beside DXVK)`

---

### Task 2: DXVK patch (`ThirdParty/dxvk/streamline.patch`)

**Files:** Create `ThirdParty/dxvk/streamline.patch`; modify `ThirdParty/dxvk/CMakeLists.txt` (apply after
`hqx-msvc.patch`; for a local `DXVK_SOURCE_DIR` document that the patch must be pre-applied).

**Interfaces (exported from DXVK's d3d9.dll, C linkage, all inert unless the setter was called):**
```cpp
struct CoaVkRequirements {
    const char* const* instanceExtensions; uint32_t instanceExtensionCount;
    const char* const* deviceExtensions;   uint32_t deviceExtensionCount;
    const char* const* features12;         uint32_t features12Count;   // names as in VkPhysicalDeviceVulkan12Features
    const char* const* features13;         uint32_t features13Count;
    uint32_t extraComputeQueues; uint32_t extraGraphicsQueues;
};
struct CoaVkDeviceInfo {
    VkInstance instance; VkPhysicalDevice physicalDevice; VkDevice device;
    uint32_t graphicsQueueFamily, graphicsQueueIndex;         // DXVK's own queue
    uint32_t extraComputeQueueFamily, extraComputeQueueFirstIndex;   // first queue reserved for the app
    uint32_t extraGraphicsQueueFamily, extraGraphicsQueueFirstIndex;
};
extern "C" __declspec(dllexport) void __stdcall CoaDxvkSetVulkanRequirements(const CoaVkRequirements* req);
extern "C" __declspec(dllexport) void __stdcall CoaDxvkSetDeviceCallback(void (__stdcall* cb)(const CoaVkDeviceInfo*));
```
and a new interop interface (new UUID) on the D3D9 device:
```cpp
struct D3D9VkImageAccess {           // in: resource + required layout; out: handles + layout at callback time
    IDirect3DResource9* pResource; VkImageLayout requiredLayout; VkImageUsageFlags viewUsage;
    VkImage image; VkImageView view; VkFormat format; VkExtent3D extent; VkImageLayout layout;
};
typedef void (__stdcall* PFN_D3D9VkRecordCallback)(VkCommandBuffer cmd, D3D9VkImageAccess* images, UINT count, void* user);
MIDL_INTERFACE("<new uuid>") ID3D9VkInteropDevice1 : public ID3D9VkInteropDevice {
    virtual HRESULT STDMETHODCALLTYPE RecordCallback(PFN_D3D9VkRecordCallback fn, void* user,
        const D3D9VkImageAccess* images, UINT count, const void* userBlob, UINT userBlobSize) = 0;
};
```
(`userBlob` is copied into the CS lambda so per-frame data — constants, frame token — travels with the command.)

Behaviour (facts from the DXVK survey, file:line in the brief context):
- Requirements: an extension provider (like the Platform/OpenVR providers, `dxvk_instance.cpp:43-46`) returns the
  instance/device extension lists; feature names are merged into the Vulkan 1.2/1.3 feature structs before
  `vkCreateDevice` (`dxvk_adapter.cpp:212-306`); `extraComputeQueues` / `extraGraphicsQueues` raise `queueCount`
  of the chosen families in `DxvkDeviceCapabilities::enableQueues` (`dxvk_device_info.cpp:663-731`) with priority
  arrays sized accordingly; when requirements are set, `VK_NVX_binary_import` and `VK_NVX_image_view_handle` are no
  longer gated on `isWineVulkan()` (`dxvk_device_info.cpp:548-553`) and the latency tracker is forced off
  (`dxvk.latencySleep` behaves as False; `dxvk_device.cpp:587-600`).
- Device callback: called once right after `vkCreateDevice` succeeds with the handles and the reserved queue
  slots.
- `RecordCallback`: on the API thread resolve each resource to its `DxvkImage` (as `GetDxvkResource` does,
  `d3d9_interop.cpp:256-268`), copy `userBlob`, `EmitCs`; in the CS lambda: `transformImage` each image to its
  `requiredLayout`, `beginExternalRendering()` (ends the pass, flushes barriers), fill handles (create views with the
  requested usage via `DxvkImageViewKey`), call `fn(cmdList->getExecBuffer(), ...)` (add the public
  `getExecBuffer()` wrapper to `DxvkCommandList`), then an ALL_COMMANDS memory barrier and
  `track(image, DxvkAccess::Write)` for every image.

- [ ] **Step 1:** Write the patch against the pinned commit (develop in the fetched checkout, `git diff > streamline.patch`).
- [ ] **Step 2:** Clean rebuild of the `dxvk` target from scratch (delete `dxvk-prefix` / `dxvk-build`) so the patch applies via CMake; build succeeds.
- [ ] **Step 3:** `dumpbin /exports bin\Win\x64\d3d9.dll` lists `CoaDxvkSetVulkanRequirements`, `CoaDxvkSetDeviceCallback` (plus DXVK's usual exports).
- [ ] **Step 4:** In-game smoke (controller, CoA-dev, `coaDlssMode 0`): the game runs exactly as before with the patched d3d9.dll (stage 1 static-pixel check pipeline off/on).
- [ ] **Step 5: Commit** — `DXVK: Streamline hooks (Vulkan requirements, device callback, RecordCallback interop)`

---

### Task 3: DLSS pure logic

**Files:** Create `CoA/Source/Runtime/Dlss.h/.cpp`; Test `CoA/Tests/DlssTest.cpp`.

**Interfaces (namespace `CoA::Dlss`, uses `RenderPipeline::Mat4/Frame`, `RenderMotion::Inverse`):**
- `enum class Mode { Off = 0, Dlaa = 1, Quality = 2, Balanced = 3, Performance = 4, UltraPerformance = 5 };`
- `Mode ModeFromCVar(int value);` (out-of-range → Off)
- `void FallbackRenderSize(Mode m, uint32_t outW, uint32_t outH, uint32_t& w, uint32_t& h);` — scale per axis
  DLAA 1.0, Quality 0.6667, Balanced 0.58, Performance 0.5, UltraPerformance 0.3333, rounded to the nearest pixel,
  min 1 (used only if `slDLSSGetOptimalSettings` fails).
- `float LodBias(uint32_t renderW, uint32_t outW);` — `log2(renderW / outW)` (≤ 0; 0 for DLAA).
- `struct SlMatrices { Mat4 cameraViewToClip, clipToCameraView, clipToPrevClip, prevClipToClip; };`
- `SlMatrices BuildMatrices(const RenderPipeline::Frame& f);` — cameraViewToClip = `cur.proj` (unjittered);
  clipToCameraView = inverse; clipToPrevClip = `inverse(cur.view*cur.proj) * T(cur.pos − prev.pos) * (prev.view*prev.proj)`
  (camera-relative world, same math as `RenderMotion::StaticMotion`); prevClipToClip = inverse of clipToPrevClip.
- `struct DepthRemap { float scale, offset; };` `DepthRemap DepthRemapFor(const Frame& f);` — `scale = 1/(depthMax−depthMin)`, `offset = −depthMin*scale`; empty range → `{0, 1}` (everything far).

- [ ] **Step 1: Write failing tests** in `CoA/Tests/DlssTest.cpp`:
  `Dlss_ModeFromCVarClamps` (−1, 6 → Off; 2 → Quality), `Dlss_FallbackSizes` (2560×1440 Quality → 1707×960,
  Performance → 1280×720, DLAA → 2560×1440, UltraPerformance → 853×480), `Dlss_LodBias` (1280 of 2560 → −1;
  DLAA → 0), `Dlss_ClipToPrevClipMatchesStaticMotion` (a point projected with clipToPrevClip lands at the UV
  `RenderMotion::StaticMotion` predicts, for a sidestep+yaw frame, tolerance 1e-4),
  `Dlss_PrevClipToClipIsInverse` (product ≈ identity), `Dlss_DepthRemap` (minZ 0, maxZ 0.94: 0.47 → 0.5, 0.94 → 1;
  empty range → scale 0 offset 1).
- [ ] **Step 2–4:** fail; implement; pass (full suite).
- [ ] **Step 5: Commit** — `Dlss: modes, render sizes, LOD bias, Streamline matrices, depth remap (pure)`

---

### Task 4: Engine — world at render size, LOD bias

**Files:** Modify `Engine/Source/Gx/CGxDevice/CGxDevice.h/.cpp`, both D3D9 device classes, `WoW/Source/Ui/WorldFrame.cpp`
(new hook call right after `FFX::Start();`), `CoA/Source/Runtime/RenderPipelineClient.cpp`.

**Interfaces:**
- `RenderPipelineHook` gains `void (*worldSized)();` (called after `FFX::Start()` so FFX still sees display size).
- `virtual void WorldRenderSizeBegin(uint32_t w, uint32_t h); virtual void WorldRenderSizeEnd();` on CGxDevice:
  while active, `m_defWindowRect` is the render rect (so `DeviceCurWindow`, `CapsWindowSize`, viewport, scissor and
  point-scale math use render pixels, including after world-internal `SetRenderTarget(nullptr)` resets,
  `CGxDevice.cpp:282-295`); End restores the saved rect and marks viewport/scissor dirty.
- `WorldTargetBegin(w, h)` is called with the **render** size when DLSS is active (stage 1/2 targets follow it; the
  motion/mask/snapshot targets already size from the world target).
- `virtual void WorldLodBiasSet(float bias);` — while ≠ 0, `ISetTexture` additionally sets `D3DSAMP_MIPMAPLODBIAS`
  (float bits; never the 'GET4' pattern) on every texture stage it binds; 0 restores the engine's default (no bias).
- Glue: when DLSS is active, `beginWorld` begins the world target at render size; `worldSized` calls
  `WorldRenderSizeBegin` + `WorldLodBiasSet(Dlss::LodBias(...))`; `endWorld` calls `WorldLodBiasSet(0)` +
  `WorldRenderSizeEnd()` before any copy/upscale. Jitter uses the render size (`ApplyJitter(..., renderW, renderH)`,
  phase count `JitterPhaseCount(renderW, outW)`) and is ALWAYS applied while DLSS is active (not gated on
  `coaRenderJitter`).

- [ ] **Step 1: Write failing test** — `Dlss_RenderSizeRectRoundTrip` in DlssTest.cpp for the pure helper the glue
  uses: `RenderPipeline::Rect RenderRect(uint32_t w, uint32_t h)` → `{0,0,w,h}`; (the device override itself is
  verified in-game).
- [ ] **Step 2–4:** implement (both classes identical); build `Wow` + `CoATests`; suite passes.
- [ ] **Step 5: In-game (controller):** with a temporary debug CVar `coaRenderScale` (0.5) — **add it in this task**
  (glue only, default "1", ignored unless the pipeline is active; it renders at the scale and the stage 1 copy-back
  stretches with `D3DTEXF_LINEAR`) — the world appears at half resolution stretched to full screen: correctly framed,
  sky/glare/weather correct, UI sharp. Remove nothing: `coaRenderScale` stays as a developer CVar.
- [ ] **Step 6: Commit** — `Gx: world renders at a chosen size (window-rect override), texture LOD bias; coaRenderScale`

---

### Task 5: Streamline client — load, init, device, support

**Files:** Create `CoA/Source/Runtime/StreamlineClient.cpp` (+ declarations in `Dlss.h`:
`namespace CoA::StreamlineClient { void PreDevice(); bool Ready(); }`); modify the CoA init next to
`DxvkClient::Preload()` (call `PreDevice()` after it), `RenderPipeline.cpp` (`QueueCVars` adds the three DLSS CVars).

**Interfaces:**
- Pure decision helper in `Dlss.cpp` (tested): `enum class Off { None, ModeZero, NoDxvk, NotSigned, InitFailed, Unsupported, NoDevice };`
  `Off WhyOff(bool modeNonZero, bool dxvkActive, bool signatureOk, bool initOk, bool featureSupported, bool deviceSet);` (first failing reason in that order).
- `PreDevice()`: reads `coaDlssMode` from `WTF\Config.wtf` directly (the CVar system is not loaded yet when the device
  is created — verify the order and use the CVar if it is), and if non-zero: verify + `LoadLibraryW(<exe>\x64\sl.interposer.dll)`,
  resolve `slInit`, `slShutdown`, `slGetFeatureRequirements`, `slSetVulkanInfo`, `slIsFeatureSupported`,
  `slGetNewFrameToken`, `slSetConstants`, `slSetTagForFrame`, `slEvaluateFeature`, `slFreeResources`,
  `slDLSSGetOptimalSettings`, `slDLSSSetOptions` via `GetProcAddress` (feature functions via
  `slGetFeatureFunction` if the SDK requires it — check `sl_dlss.h`), `slInit` per Global Constraints,
  `slGetFeatureRequirements(kFeatureDLSS)` → `CoaDxvkSetVulkanRequirements` (resolved from DXVK's module via
  `Dxvk::D3d9Library()`), and `CoaDxvkSetDeviceCallback(OnDevice)`.
- `OnDevice(info)`: `slSetVulkanInfo` (queue indices from the reserved slots), then
  `slIsFeatureSupported(kFeatureDLSS, {vkPhysicalDevice})`; logs the result.
- `Ready()`: the whole chain succeeded.

- [ ] **Step 1: Write failing tests** — `Streamline_UnsupportedLeavesDlssOff` (each argument false in turn → that
  reason; all true → None) and `Streamline_ModeZeroLoadsNothing` (with a fake loader dependency injected the way
  `Dxvk::Deps` does it: mode 0 → the fake loader is never called).
- [ ] **Step 2–4:** implement; build; suite passes.
- [ ] **Step 5: In-game (controller):** `coaDlssMode 2` in CoA-dev's Config.wtf → log shows signature ok, slInit ok,
  requirements passed, device callback, `DLSS supported` on the RTX 5080; `coaDlssMode 0` → no `sl.*` module loaded.
- [ ] **Step 6: Commit** — `Streamline: load, verify, init (manual hooking, Vulkan), DXVK requirements and device`

---

### Task 6: DLSS evaluate in the frame

**Files:** Create `CoA/Source/Render/Shaders/DepthRemapPS.hlsl`; modify `StreamlineClient.cpp`,
`RenderPipelineClient.cpp` (EndWorld path), `RenderMotionClient.cpp` (expose the motion/mask textures it already owns
through the device accessors — no change if accessors suffice).

**Interfaces / behaviour:**
- Active when `StreamlineClient::Ready()`, `coaDlssMode` ≠ 0, the pipeline and motion are active this frame.
- Per frame: render size from `slDLSSGetOptimalSettings(mode, displayW, displayH)` (fallback `FallbackRenderSize`),
  `slDLSSSetOptions` when mode or size changes (`outputWidth/Height` = display); on a change also `slFreeResources`
  first and set `reset`.
- Depth remap pass (render size, `DepthRemapPS`, c0 = (scale, offset) from `DepthRemapFor`) into an R32F render
  target created by the client (own texture, released on the release path).
- Output texture: display size, created with `ID3D9VkInteropDevice::CreateImage` with
  `VK_IMAGE_USAGE_STORAGE_BIT | SAMPLED | TRANSFER_SRC` (A8R8G8B8 / B8G8R8A8_UNORM), released on the release path.
- `RecordCallback` with images: world colour (render, SHADER_READ_ONLY), remapped depth (SHADER_READ_ONLY), motion
  (SHADER_READ_ONLY), output (GENERAL, storage view); userBlob = `sl::Constants` (from `Dlss::BuildMatrices`, Frame,
  jitter × `coaDlssJitterSign`, `mvecScale = (coaDlssMvecSign, coaDlssMvecSign)`) + frame index. Inside the callback
  (CS thread): `slGetNewFrameToken(token, &frameIndex)`, `slSetConstants`, `slSetTagForFrame` (depth,
  motion, scaling input/output; lifecycle `eOnlyValidNow`; extents = render rect / display rect), `slEvaluateFeature(kFeatureDLSS, ...)`
  with the callback's `VkCommandBuffer`.
- Then `WorldTargetEnd` is replaced for that frame by: restore default surfaces (as `WorldTargetEnd` does) and
  `StretchRect(output → back buffer, full, D3DTEXF_NONE)`. Device method: `virtual bool WorldTargetEndWith(void* texture)`
  (both classes) — same as `WorldTargetEnd` but copies from the given full-size texture.
- Any failure (callback error, SL error result) → this frame falls back to the stage 1 copy-back (stretch with
  `D3DTEXF_LINEAR`), logged once per streak.
- Debug: `coaRenderDebug 7` = show the raw render-size world stretched (no DLSS) for A/B in-game.

- [ ] **Step 1: Write failing test** — `Dlss_EmbeddedDepthRemapIsPs3` (token test) and `Dlss_ConstantsBlobLayout`
  (the blob struct round-trips: `static_assert` on size + a test that fields land where the callback reads them).
- [ ] **Step 2–4:** implement; build `Wow` + `CoATests` + `dxvk`; suite passes.
- [ ] **Step 5: Commit** — `DLSS: depth remap, Streamline tags and evaluate inside DXVK's command stream`

---

### Task 7: In-game proof (controller) and docs

- [ ] **Step 1 (CoA-dev, `coaDlssMode` set in Config.wtf before launch):**
  - DLAA (1): image sharper / less aliased than native (visual), no ghosting on walking NPCs.
  - Quality (2) and Performance (4): stable image, UI sharp, correct framing; FPS higher than native in the same spot
    (record numbers, uncapped).
  - Sign check: run with `coaDlssMvecSign` ±1 and `coaDlssJitterSign` ±1 while turning and while NPCs walk; pick the
    combination without ghosting/shimmer; set those as defaults (commit) if they differ from 1.
  - Resize ×3, mode switches 2→4→1→0→2 live, `RestartGx`: no crash, correct size each time.
  - `coaDlssMode 0` from startup: process has no `sl.*` / `nvngx_*` modules.
- [ ] **Step 2:** `CoA/DIFFERENCES.md` entry (CoA64 addition: DLSS via Streamline, console CVar, restart rule, files in `x64\`); commit.
- [ ] **Step 3:** Merge only with the user's consent.
