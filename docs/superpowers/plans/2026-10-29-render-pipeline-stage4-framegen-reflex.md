# Render Pipeline Stage 4 (DLSS Frame Generation 2x/3x/4x + NVIDIA Reflex) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** DLSS Frame Generation (2x/3x/4x) and NVIDIA Reflex (Off / On / On + Boost) on top of stage 3, behind console CVars, on DXVK v3.1.1 via Streamline v2.14.16.

**Architecture:** Streamline stays in manual-hooking mode (stage 3): DXVK keeps its own Vulkan loader, and the client hands DXVK Streamline's proxies. Stage 3 routed only `vkQueuePresentKHR`; DLSS-G needs all eight mandatory Vulkan hooks of `sl_hooks.h`, an optical-flow queue at device creation, and a frame token that is the same from simulation start to the present. The client owns one Streamline frame per game frame (Reflex sleep + PCL markers on the main thread), stamps that frame's index on DXVK's next present, and DXVK's queue thread emits the present markers through a client callback.

**Tech Stack:** C++20 (MSVC, x64), DXVK v3.1.1 WoW build (Raz0r1337, `4165a8ec`) + `ThirdParty/dxvk/streamline.patch`, Streamline SDK v2.14.16 (prebuilt, signed), `CoA/Tests` (COA_TEST).

**Spec:** `docs/superpowers/specs/2026-10-26-streamline-rendering-design.md` (sections 2, 4, 6.5, 7 stage 4, 9 Q-A). Streamline docs used: `docs/ProgrammingGuideDLSS_G.md`, `ProgrammingGuideReflex.md`, `ProgrammingGuidePCL.md`, `ProgrammingGuideManualHooking.md` inside `streamline-sdk-v2.14.16.zip` (cached in `out/build/<tree>/ThirdParty/streamline/`).

## Global Constraints

- C++ only; no MPQ / data-file / game-shader changes. Nothing written into `D:\WoW\CoA`; tests in `D:\WoW\CoA-dev`.
- Off means unchanged: with `coaDlssMode`, `coaFrameGen`, `coaReflex` all `0` at startup, Streamline is not loaded and DXVK behaves exactly as without the patch (stage 3 rule, kept).
- Streamline is loaded only when at least one of the three CVars is non-zero at startup; going from all-0 to any non-zero needs a restart. Once loaded, mode changes apply live.
- New CVars (saved in `Config.wtf`, default `0`, names are final): `coaFrameGen` 0 Off, 1 = 2x, 2 = 3x, 3 = 4x; `coaReflex` 0 Off, 1 On, 2 On + Boost.
- Reflex is forced to at least On while Frame Generation is on (spec goal 4).
- Frame Generation needs RTX 40+ (2x) / RTX 50 (3x, 4x); a request above what Streamline reports (`numFramesToGenerateMax`) is clamped and logged once.
- DLSS-G is set to `eOff` whenever the frame is not a world frame (glue, loading screen, cinematic/movie, world not rendered), while minimised, and around resolution / window-mode changes (DLSS_G guide 0.0 checklist).
- `slReflexSleep` and `slPCLSetMarker` are called every frame while Streamline is loaded, even with Reflex Off (Reflex guide 4.0); `slReflexSetOptions` at least once.
- Production packaging ships only the signed `bin/x64` DLLs, never `bin/x64/development`.
- Inline comments: 1-2 lines, "why" only (`CLAUDE.md`, `Tools/comment_budget.mjs`); long rationale goes to a dated `docs/` page.

## Review Focus

1. FG on, then a loading screen / zone change: generated frames must stop (DLSS-G `eOff`) for the load and resume after, with no stale world frames interpolated into the loading screen. → Task 2 test `FrameGen_OffOutsideWorld` (gate) + Task 6 wiring + in-game check (Task 7).
2. Window resize / fullscreen toggle / alt-tab with FG on: no crash, no swapchain error loop (the v3.1.1 presenter already recreates swapchains; now through SL's proxies). → Task 2 test `FrameGen_OffAroundResize` (gate) + Task 6 `OnResize` wiring + Task 7.
3. FG requested on a GPU / driver that cannot do it (RTX 30, HAGS off, old driver): falls back to Off with one log line, DLSS SR and Reflex keep working. → Task 4 test `Streamline_FeatureGating`.
4. `coaFrameGen` set while `coaDlssMode` is 0: FG still needs depth + motion, so the pipeline runs at display size without jitter. → Task 6 test `FrameGen_WithoutDlssRunsPipelineNative`.
5. Device re-creation (`gxRestart`, device lost) with FG on: the swapchain proc table is cleared before the old device dies and set again for the new one; no call into a freed proxy. → Task 3 (patch) + Task 4 test `Streamline_DeviceCycleClearsProcs`.

---

## File Structure

| File | Responsibility |
|---|---|
| `ThirdParty/streamline/CMakeLists.txt` | stage the DLSS-G / Reflex / PCL DLLs |
| `ThirdParty/dxvk/streamline.patch` | swapchain proc table, optical-flow queue, present frame id + present markers |
| `CoA/Source/Runtime/FrameGen.h/.cpp` (new) | pure logic: CVar → modes, per-frame FG gate, Reflex forcing, SL enum mapping |
| `CoA/Source/Runtime/Dlss.h` | `Streamline::Deps::mode` becomes "any option non-zero"; feature set to load |
| `CoA/Source/Runtime/StreamlineClient.h/.cpp` | load DLSS-G / Reflex / PCL, requirements union, optical-flow queue in `VulkanInfo`, proc table hand-off |
| `CoA/Source/Runtime/SlFrameClient.h/.cpp` (new) | one Streamline frame per game frame: token, Reflex sleep, PCL markers, present stamping |
| `CoA/Source/Runtime/DlssClient.cpp` | use the shared frame index (drop `s_slFrame`), tag lifecycles, hudless tag |
| `CoA/Source/Runtime/FrameGenClient.cpp` (new) | DLSS-G options per frame on the main thread, eOff gating |
| `CoA/Source/Runtime/RenderPipelineClient.cpp` | pipeline forced on when FG is wanted |
| `CoA/Tests/FrameGenTest.cpp` (new), `CoA/Tests/StreamlineTest.cpp` | unit tests |
| `CoA/DIFFERENCES.md`, `NOTICE.md`, `docs/2026-10-29-coa-module-notes.md` or a new dated page | docs |

---

### Task 1: Streamline DLLs for Frame Generation and Reflex

**Files:**
- Modify: `ThirdParty/streamline/CMakeLists.txt:12` (`STREAMLINE_DLLS`)
- Modify: `NOTICE.md` (Streamline entry lists DLSS Frame Generation and Reflex components)

- [ ] **Step 1:** Add `sl.dlss_g.dll nvngx_dlssg.dll sl.reflex.dll sl.pcl.dll NvLowLatencyVk.dll` to `STREAMLINE_DLLS` (production `bin/x64` only). The extract stamp is keyed on the zip hash, so bump the stamp name (e.g. append `-fg`) so existing build trees re-extract.
- [ ] **Step 2:** Run `cmd /c out\claude_rp.bat`. Expected: `BUILD OK`, and the five DLLs next to `d3d9.dll` in the staged `x64` directory.
- [ ] **Step 3:** Commit `Streamline: stage DLSS Frame Generation, Reflex and PCL DLLs`.

### Task 2: Frame Generation / Reflex pure logic

**Files:**
- Create: `CoA/Source/Runtime/FrameGen.h`, `CoA/Source/Runtime/FrameGen.cpp` (add to `CoA/Source/CMakeLists.txt`)
- Test: `CoA/Tests/FrameGenTest.cpp` (add to `CoA/Tests/CMakeLists.txt`)

**Interfaces:**
- Produces (namespace `CoA::FrameGen`):
  - `enum class Mode { Off = 0, X2 = 1, X3 = 2, X4 = 3 };` `Mode ModeFromCVar(int)` (outside 0..3 → Off)
  - `uint32_t FramesToGenerate(Mode m, uint32_t maxSupported)` → `min(int(m), maxSupported)` (0 for Off)
  - `enum class Reflex { Off = 0, On = 1, Boost = 2 };` `Reflex ReflexFromCVar(int)`; `Reflex EffectiveReflex(Reflex wanted, bool fgActive)` (Off → On when fgActive)
  - `uint32_t SlReflexMode(Reflex)` (`sl::ReflexMode`: eOff 0, eLowLatency 1, eLowLatencyWithBoost 2; static_asserted in the client)
  - `enum class Gate { On, NotLoaded, Unsupported, NotWorldFrame, Minimised, Resizing, NoDepthOrMotion };`
  - `struct FrameState { bool loaded, supported, worldFrame, minimised, resizing, hasDepthAndMotion; };`
  - `Gate Decide(Mode m, const FrameState& s)` — the first failing condition in this order: `loaded`, `supported`, `worldFrame`, `!minimised`, `!resizing`, `hasDepthAndMotion`; else `On`. Callers ask only for `m != Off`.
  - `const char* GateName(Gate)` for the log.
  - `bool AnyOptionOn(int dlssMode, int frameGen, int reflex)`.

- [ ] **Step 1: Write the failing tests** in `FrameGenTest.cpp`:
  - `FrameGen_Modes`: `ModeFromCVar(-1|4) == Off`, `ModeFromCVar(2) == X3`; `FramesToGenerate(X4, 1) == 1`, `FramesToGenerate(X3, 3) == 2`, `FramesToGenerate(Off, 3) == 0`.
  - `FrameGen_ReflexForcedUnderFg`: `EffectiveReflex(Off, true) == On`, `EffectiveReflex(Boost, true) == Boost`, `EffectiveReflex(Off, false) == Off`; `SlReflexMode(Boost) == 2`.
  - `FrameGen_OffOutsideWorld`: `Decide(X2, {true,true,false,false,false,true}) == NotWorldFrame`.
  - `FrameGen_OffAroundResize`: `Decide(X2, {true,true,true,false,true,true}) == Resizing`; minimised → `Minimised`; all true/false-as-ok → `On`.
  - `FrameGen_AnyOption`: `AnyOptionOn(0,0,0) == false`, `AnyOptionOn(0,0,1) == true`.
- [ ] **Step 2:** Build `CoATests`; expected: compile failure (no `FrameGen.h`).
- [ ] **Step 3:** Implement `FrameGen.h/.cpp` to the signatures above.
- [ ] **Step 4:** `cmd /c out\claude_rp.bat`; expected `0 failed checks`.
- [ ] **Step 5:** Commit `Frame Generation and Reflex: mode, gate and Reflex-forcing logic`.

### Task 3: DXVK patch — swapchain hooks, optical-flow queue, present frame id

**Files:**
- Modify: `ThirdParty/dxvk/streamline.patch` (regenerate from a patched checkout: `git -C <dxvk-src> diff > ThirdParty/dxvk/streamline.patch`, LF)

**Interfaces:**
- Replaces stage 3's `CoaDxvkSetPresentProc` (keep the export as a thin wrapper so a stage-3 client still links) with:
  - `struct CoaVkSwapchainProcs { PFN_vkCreateSwapchainKHR createSwapchain; PFN_vkDestroySwapchainKHR destroySwapchain; PFN_vkGetSwapchainImagesKHR getSwapchainImages; PFN_vkAcquireNextImageKHR acquireNextImage; PFN_vkQueuePresentKHR queuePresent; PFN_vkDeviceWaitIdle deviceWaitIdle; PFN_vkCreateWin32SurfaceKHR createWin32Surface; PFN_vkDestroySurfaceKHR destroySurface; };`
  - `extern "C" void __stdcall CoaDxvkSetSwapchainProcs(const CoaVkSwapchainProcs* procs)` — null restores the driver's; non-null members override only those calls. Same locking contract as `setPresentProc` (presenter calls under the device queue lock).
  - `CoaVkRequirements` gains `uint32_t opticalFlowQueues;` (appended); `CoaVkDeviceInfo` gains `uint32_t opticalFlowQueueFamily, opticalFlowQueueIndex;` (`0xFFFFFFFF` when none). The adapter picks a family with `VK_QUEUE_OPTICAL_FLOW_BIT_NV` and creates the queue only when `opticalFlowQueues > 0` (needs `VK_NV_optical_flow`, requested by SL's requirements).
  - `extern "C" void __stdcall CoaDxvkSetNextPresentFrame(uint64_t frameIndex)` — stamped on the next `Present` the d3d9 swapchain queues; carried to the queue thread with that present.
  - `typedef void (__stdcall* PFN_CoaPresentMarker)(uint32_t marker, uint64_t frameIndex);` `extern "C" void __stdcall CoaDxvkSetPresentMarkerCallback(PFN_CoaPresentMarker cb)` — called on DXVK's queue thread with `marker` 0 = present start (right before the proxy `vkQueuePresentKHR`), 1 = present end (right after), only for presents that carry a stamped index.

- [ ] **Step 1:** In the DXVK checkout (`out/build/x64-Release-rp/ThirdParty/dxvk/dxvk-prefix/src/dxvk`, already patched), route the presenter's surface / swapchain / acquire / present / waitIdle calls through `coa::` wrappers that use the proc table when set (same pattern as `coa::queuePresent`).
- [ ] **Step 2:** Add the optical-flow queue (adapter queue selection + device create + `CoaVkDeviceInfo`).
- [ ] **Step 3:** Add the frame stamp and marker callback (presenter → queue submission entry → `dxvk_queue.cpp` around the present).
- [ ] **Step 4:** Regenerate the patch; `cmd /c out\claude_rp.bat` expected `BUILD OK` (the patch step re-applies from a clean reset).
- [ ] **Step 5:** Off check: run `D:\WoW\CoA-dev` with all three CVars 0 (static-pixel check off/on/off from stage 3); expected identical, and `Logs` show no Streamline lines.
- [ ] **Step 6:** Commit `DXVK: Streamline swapchain hooks, optical-flow queue, present frame id and markers`.

### Task 4: Streamline client — load FG / Reflex / PCL

**Files:**
- Modify: `CoA/Source/Runtime/Dlss.h` (`Streamline::Deps`, `DeviceDeps`), `CoA/Source/Runtime/Dlss.cpp`
- Modify: `CoA/Source/Runtime/StreamlineClient.h/.cpp`
- Test: `CoA/Tests/StreamlineTest.cpp` (existing stage 3 tests stay green)

**Interfaces:**
- Consumes: `FrameGen::AnyOptionOn`, the Task 3 exports.
- Produces:
  - `Streamline::Deps::mode` → `bool (*anyOption)()`; new `struct Features { bool dlss, frameGen, reflex, pcl; };` and `Features Streamline::Loaded()`, `Features Streamline::Supported()`.
  - `StreamlineClient::Api` gains `reflexSetOptions, reflexSleep, reflexGetState, pclSetMarker, dlssgSetOptions, dlssgGetState` (null when the feature is not loaded/supported; `GetApi()` no longer requires them).
  - Load set: Reflex + PCL always when Streamline loads; DLSS when `coaDlssMode != 0` or `coaFrameGen != 0` at startup (FG reuses DLSS's tags path); DLSS-G when `coaFrameGen != 0` at startup.
  - `CoaVkRequirements` = union of `slGetFeatureRequirements` for every loaded feature (extensions, features12/13, queues, `vkNumOpticalFlowQueuesRequired`); `VulkanInfo.opticalFlowQueueFamily/Index` and `useNativeOpticalFlowMode = true` when DXVK reports a queue.
  - Device callback: after `slSetVulkanInfo`, resolve the eight proxies with sl.interposer's `vkGetDeviceProcAddr` / `vkGetInstanceProcAddr` and call `CoaDxvkSetSwapchainProcs`; `BeforeDeviceRelease` clears them (null) and takes/drops the submission lock before `slShutdown`.
  - `slReflexSetOptions` once after the device is set, with `FrameGen::EffectiveReflex`.

- [ ] **Step 1: Failing tests** in `StreamlineTest.cpp`:
  - `Streamline_AnyOptionLoads`: deps with `anyOption` false → `load` never called; true with DLSS 0, FG 0, Reflex 1 → loaded, `Loaded().reflex && !Loaded().dlss`.
  - `Streamline_FeatureGating`: FG loaded but `supported` reports DLSS-G unsupported → `Supported().frameGen == false`, `Supported().dlss` and `.reflex` unchanged, one log line containing `Frame Generation unsupported`.
  - `Streamline_DeviceCycleClearsProcs`: a fake `setProcs` records calls; `DeviceCreated(dev1)` → set non-null; `DeviceReleasing(dev1)` → set null before `shutdown`; `DeviceCreated(dev2)` → set non-null again.
- [ ] **Step 2:** Run; expected failures.
- [ ] **Step 3:** Implement (deps-based halves in `Dlss.cpp`, Win32/SL halves in `StreamlineClient.cpp`, following the stage 3 split).
- [ ] **Step 4:** `cmd /c out\claude_rp.bat`; expected `0 failed checks`.
- [ ] **Step 5:** In-game smoke (CoA-dev, `coaReflex 1` only): log shows Streamline loaded with Reflex + PCL, `slReflexSetOptions eOk`, game renders. Commit `Streamline: load Frame Generation, Reflex and PCL; swapchain proxies`.

### Task 5: One Streamline frame per game frame (Reflex sleep, PCL markers)

**Files:**
- Create: `CoA/Source/Runtime/SlFrameClient.h/.cpp`
- Modify: `Engine/Source/Event/EvtSched.cpp` (`SynthesizeIdle` 0x47DBC0 / `SynthesizePaint` 0x47DC50 — CoA hook pointers, pattern of `g_cvarSetHook`), `Engine/Source/Gx/CGxDevice/CGxDevice.cpp:181` (`ScenePresent`)
- Modify: `CoA/Source/Runtime/DlssClient.cpp` (`s_slFrame` → `SlFrameClient::Index()`)

**Interfaces:**
- Produces (namespace `CoA::SlFrameClient`):
  - `void BeginFrame()` — at the top of the idle dispatch: index `N = ++counter`, `slGetNewFrameToken(&t, &N)`, `slReflexSleep(*t)`, `PCL eSimulationStart`.
  - `void BeginRender()` — at the top of the paint dispatch: `PCL eSimulationEnd`, `eRenderSubmitStart`.
  - `void EndRender()` — in `CGxDevice::ScenePresent` before the device present: `eRenderSubmitEnd`, `CoaDxvkSetNextPresentFrame(N)`.
  - `uint64_t Index()` — current N (0 before the first frame); `const sl::FrameToken* Token()`.
  - Present marker callback (queue thread): `ePresentStart` / `ePresentEnd` with `slGetNewFrameToken(&t, &frameIndex)`.
  - All no-ops unless `Streamline::Loaded().pcl`.
- Paint without a preceding idle (or idle without paint) must not reuse or skip an index: the counter advances only in `BeginFrame`; `BeginRender` / `EndRender` without a `BeginFrame` since the last `EndRender` do nothing.

- [ ] **Step 1: Failing test** `SlFrame_MarkerOrder` (`CoA/Tests/FrameGenTest.cpp`, via a deps struct of fake `token/sleep/marker/stamp` functions): idle, paint, present → markers `SimStart(1), SimEnd(1), RenderStart(1), RenderEnd(1)`, stamp `1`; a second paint without idle → no markers; next idle → index 2.
- [ ] **Step 2:** Run; expected failure.
- [ ] **Step 3:** Implement (deps-based core testable, Win32/SL glue in the same file behind the deps).
- [ ] **Step 4:** `cmd /c out\claude_rp.bat`; expected `0 failed checks`.
- [ ] **Step 5:** In-game (CoA-dev, `coaReflex 1`): SL log has no "frame index mismatch" / "missing marker" warnings over 60 s of play. Commit `Streamline: one frame token per game frame, Reflex sleep and PCL markers`.

### Task 6: Frame Generation in the frame

**Files:**
- Create: `CoA/Source/Runtime/FrameGenClient.cpp` (declared in `StreamlineClient.h` next to `DlssClient`)
- Modify: `CoA/Source/Runtime/DlssClient.cpp` (tags), `CoA/Source/Runtime/RenderPipelineClient.cpp` (wanted / pipeline forced on)
- Register `coaFrameGen`, `coaReflex` where stage 3 registers `coaDlssMode`
- Test: `CoA/Tests/FrameGenTest.cpp`

**Interfaces:**
- Consumes: `FrameGen::Decide`, `SlFrameClient::Token()/Index()`, `StreamlineClient::Api::dlssgSetOptions`.
- Produces (namespace `CoA::FrameGenClient`): `bool Wanted()` (FG loaded+supported and `coaFrameGen != 0`), `void OnWorldFrame(bool worldRendered)` (called from `RenderPipelineClient` EndWorld / its "no world this frame" path), `void OnResize(bool begin)` (from the device's resize / window-mode path), `void Release()`.
- Behaviour:
  - `DLSSGOptions` set on the main thread only when the decided mode / frame count changes: `mode = eOn` + `numFramesToGenerate = FramesToGenerate(...)`, or `eOff`; flags none (no `eRetainResourcesWhenOff`, so Off frees VRAM).
  - Tags for FG, recorded with the frame's token from `SlFrameClient`: depth and motion vectors (the same images DLSS uses) and `kBufferTypeHUDLessColor` = DLSS output when DLSS evaluates, else the display-size world target; all `eValidUntilPresent` (DLSS_G guide 5.2: start there, tighten only if content is wrong at present). `slSetConstants` every world frame, also when DLSS does not evaluate.
  - UI buffer: not tagged in this stage (hudless only); revisit if Task 7 shows UI artifacts.
  - `RenderPipelineClient`: the pipeline + motion passes are forced on when `DlssClient::Wanted() || FrameGenClient::Wanted()`; with DLSS off the world renders at display size, unjittered.

- [ ] **Step 1: Failing test** `FrameGen_WithoutDlssRunsPipelineNative`: with a fake "DLSS not wanted, FG wanted", the pipeline's wanted predicate is true and the render size equals the display size, jitter off.
- [ ] **Step 2:** Run; expected failure.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** `cmd /c out\claude_rp.bat`; expected `0 failed checks`.
- [ ] **Step 5:** Commit `Frame Generation: options, tags and off-gating in the frame`.

### Task 7: In-game proof and docs

**Files:**
- Modify: `CoA/DIFFERENCES.md` (DLSS section: FG + Reflex, CVars, restart rule), new dated `docs/2026-10-29-frame-generation-reflex.md` (measurements), memory `rendering-groundwork-goal.md`

- [ ] **Step 1:** Ask the user before taking focus of `D:\WoW\CoA-dev` (memory: they may be playing). Then via the `CoATest` addon (SavedVariables-driven, no typing), same spot as stage 3:
  - all CVars 0: static-pixel check off/on/off identical; FPS unchanged vs. before stage 4.
  - `coaReflex 1`, `2`: renders; SL log clean.
  - `coaFrameGen 1/2/3` with `coaDlssMode 2` and with `coaDlssMode 0`: displayed FPS ≈ 2x / 3x / 4x of base (SL's `DLSSGState.numFramesActuallyPresented`), UI text and minimap stable while turning, loading screen (hearthstone) shows no interpolated world, resize/alt-tab survives.
- [ ] **Step 2:** Write the measurements page and the DIFFERENCES entry; update memory.
- [ ] **Step 3:** `node --test "Tools/__tests__/*.test.mjs"` (with `COMMENT_DIFF_BASE` at the stage start) expected `fail 0`; commit `Stage 4: in-game proof and docs`; merge `render-pipeline-stage4` into `render-pipeline` with `--no-ff`.
