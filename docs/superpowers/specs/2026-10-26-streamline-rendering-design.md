# CoA64 rendering groundwork + NVIDIA Streamline (DLSS, Frame Generation, Reflex) — design

Date: 2026-10-26. Status: draft for approval.

## 1. Intent

CoA64 should be able to grow modern graphics over the long term (better shading and rendering). NVIDIA Streamline
(https://github.com/NVIDIA-RTX/Streamline) is the first consumer of that capability, not the end goal. The main goal of
this project is **solid groundwork**: the 3D world rendered off-screen and separate from the UI, a readable depth
buffer, a clean per-frame camera record, sub-pixel jitter, and **full per-pixel motion vectors** (including skinned /
animated models). On top of that groundwork, the first release ships DLSS Super Resolution (+ DLAA), DLSS Frame
Generation (2x/3x/4x) and NVIDIA Reflex, selectable in the in-game Graphics options, defaulting to Off.

Later work this must enable without rework (not built now): ambient occlusion, better anti-aliasing (TAA), bloom,
better shadows, ray reconstruction, custom shading (replacing shader families one at a time).

## 2. Goals

1. Groundwork: off-screen world target + sampleable depth + camera record + jitter + motion vectors, reusable by any
   future pass.
2. DLSS Super Resolution with modes DLAA, Quality, Balanced, Performance, Ultra Performance.
3. DLSS Frame Generation: 2x, 3x, 4x (3x/4x need RTX 50; FG needs RTX 40/50).
4. NVIDIA Reflex: Off / On / On + Boost; forced On while Frame Generation is on.
5. Three dropdowns on the existing **Graphics** page of the Video options (below Resolution / Multisampling / Vertical
   Sync), added at runtime from C++. Default Off.

## 3. Non-goals

- No MPQ or other client data-file changes. No edits to the game's shader files. C++ only (client code and a C++
  patch to the DXVK build).
- No support on the system D3D9 path (`-nodxvk`, or DXVK missing): Streamline has no D3D9 backend. The options show
  greyed out there.
- No AMD FSR / Intel XeSS in this project.
- No replacement of the game's shaders (that is the later "custom shading" track, see 6.4).
- No deployment into `D:\WoW\CoA` (the live client). Testing happens in `D:\WoW\CoA-dev` (a full copy, already made).

## 4. Acceptance standards ("get nothing wrong")

Every stage must meet all of these before it counts as done:

1. **Off means unchanged:** with all three options Off, frames are pixel-identical to the current build. Verified by
   screenshot comparison (same spot, same camera, old vs new exe) at several scenes (city, outdoors, dungeon, login).
2. **No cost when Off:** no measurable frame-rate difference with the options Off.
3. **Nobody breaks:** AMD, Intel, older NVIDIA, `-nodxvk`, `fixedFunction 1`, low shader profiles all keep working;
   on unsupported setups the dropdowns are greyed out with "Requires an NVIDIA RTX graphics card" (or the more
   specific reason, e.g. Frame Generation "Requires an RTX 40 series or newer").
4. **No data-file changes** and nothing written into `D:\WoW\CoA`.
5. **Proof for every stage:** unit tests in `CoA/Tests` where logic is testable (jitter sequence, mode → render size,
   option gating, Lua injection text), plus an in-game check on the RTX 5080 in `D:\WoW\CoA-dev`.

## 5. Facts about the engine that shape the design

(From a code survey; references are into this repo.)

- Shaders are precompiled `.bls` bytecode read from the MPQs (`CGxDevice::IShaderLoad`,
  `Engine/Source/Gx/CGxDevice/CGxDevice.cpp:1686`). Nothing is embedded. Fixed-function fallbacks exist and are
  hardware/CVar dependent (`fixedFunction`, `CShaderEffect::s_enableShaders`).
- Frame flow: `CGWorldFrame::OnFrameRender` → `RenderWorld` → `OnWorldUpdate` (camera) → `OnWorldRender`
  (`WoW/Source/.../WorldFrame.cpp:1690-1825`). World ends at `RsPop` (`:1819`); then chat bubbles (`:1821`),
  `FFX::Finish` post-processing (`:1823`), then floating world text (`:2136`), then UI strata, then
  `GxScenePresent` → `CGxDeviceD3d::ScenePresent` (`CGxDeviceD3d.cpp:3250`, Present at `:3289`; D3d9Ex equivalent).
- The world draws straight into the back buffer. The main depth is the auto depth-stencil (not sampleable).
  `DeviceReadDepths` is a stub. Render-target textures exist (`ITexCreate`, `DeviceSetRenderTarget`). MSAA applies to
  the back buffer only.
- Camera-relative rendering: view matrix has no translation; world geometry is offset by `-camPos`
  (`World.cpp:2806`). Projection set via `CGxDeviceD3d::XformSetProjection` (`:780`); shaders get it at VS c2..c5
  (`ShaderEffect.cpp:161`). The projection is swapped mid-frame (horizon, shadows, FFX ortho, UI models).
- M2 skinning is on the GPU when shaders are on: bone matrices at VS c31+, 3 registers per bone
  (`CM2SceneRender.cpp:435-455`, instanced `:635-660`), computed in `CM2Model::AnimateMT` already in view space
  (`m_matrixF4 = world*view`). CPU skinning when shaders are off.
- No previous-frame transform cache exists anywhere.
- DXVK (Raz0r1337 fork, v3.1.1, pinned `4165a8ec`) exposes `ID3D9VkInteropDevice` (Vulkan handles, submission queue
  lock, image info, layout transitions, flush) and loads `vulkan-1.dll` in `src/vulkan/vulkan_loader.cpp`.
- Streamline SDK latest: v2.14.16 (2026-10-16), prebuilt signed binaries in `streamline-sdk-v2.14.16.zip`.
  Streamline supports D3D11/D3D12/Vulkan only. DLSS-G requires Reflex, and tagged depth, motion vectors, HUD-less
  color and UI color/alpha; `sl::Constants` per frame (matrices, jitter, `cameraMotionIncluded`, `reset`, ...).

## 6. Design

### 6.1 Architecture overview

```
 CoA64.exe (C++)                                       x64\d3d9.dll (DXVK, patched)       x64\sl.*.dll, nvngx_*.dll
 ┌──────────────────────────────────────────────┐     ┌──────────────────────────────┐    ┌───────────────────────┐
 │ RenderPipeline (new, Engine/Source/Gx + CoA) │     │ loads sl.interposer.dll as   │───▶│ Streamline + DLSS /   │
 │  - world RT (render res) + INTZ depth tex    │     │ its Vulkan loader when asked │    │ DLSS-G / Reflex       │
 │  - camera record, jitter                     │ D3D9│ (otherwise vulkan-1.dll)     │    └───────────────────────┘
 │  - motion pass (own shaders, in exe)         │────▶│ ID3D9VkInteropDevice used by │
 │  - DLSS eval (via interop) → output RT       │     │ the client to reach VkImages │
 │  - UI at output res on back buffer           │     └──────────────────────────────┘
 │ Streamline glue (CoA/Source/Runtime)         │
 │  - slInit, feature support, tags, constants, │
 │    Reflex markers, FG options                │
 │ Options UI (Lua injected at runtime)         │
 └──────────────────────────────────────────────┘
```

When every option is Off, none of the new code paths run: the world draws to the back buffer exactly as today.

### 6.2 Off-screen world target and depth (stage 1)

- When the pipeline is active, the world (everything from `FFX::Start` through `RsPop` in `OnWorldRender`) renders into
  a **world color RT** at render resolution and a **sampleable depth texture** (D3D9 `INTZ`, supported by DXVK) bound
  as depth-stencil. Stencil must keep working (INTZ has 8 stencil bits).
- After `RsPop`: DLSS (or, in stage 1, a plain copy) resolves the world RT into a full-resolution **output RT**; chat
  bubbles, `FFX::Finish`, world text and UI then draw at output resolution as today. Post-processing (`FFX` glow etc.)
  therefore runs on the upscaled image. *Builder may revisit the exact position of `FFX::Finish` relative to DLSS if
  it causes artifacts; requirement is that glow/fog look the same as today at DLAA.*
- Passes that use their own projection/RTs mid-frame (horizon, shadow maps, minimap, portraits, liquid back-buffer
  copy) must be audited so they read/write the world RT instead of the back buffer where they assume the back buffer.
- MSAA is not combined with the pipeline: whenever the pipeline is active, multisampling is forced off (DLSS
  replaces it; INTZ depth cannot be multisampled). With the pipeline inactive, MSAA works exactly as today. The stage 1
  pixel-identical proof compares at MSAA off (my call, builder check Q4).
- Stage 1 is enabled by a console CVar only (e.g. `coaRenderPipeline 1`, name is builder's call) and must be visually
  identical to Off at 100% scale — this proves the split is correct before any NVIDIA code is involved.

### 6.3 Camera record and jitter (stage 1)

- A per-frame record captured once per frame for the world camera only: view (camera-relative) and projection,
  camera world position, previous frame's values, near/far, FOV, aspect, right/up/forward vectors, frame index, and a
  `reset` flag (teleport, loading screen, zone change, camera cut, resolution change).
- Jitter: Halton(2,3) sub-pixel offsets, phase count per NVIDIA guidance for the chosen scale, applied only to the
  world camera projection (not horizon/shadow/UI/portrait projections). Jitter off when DLSS is off.
- Because rendering is camera-relative, motion vectors are computed with the camera translation delta folded in
  explicitly; the record stores both frames' camera positions.

### 6.4 Motion vectors — separate motion pass (stage 2) — decided: option A

- The game's shaders are **not** modified. A new **motion pass** runs after the opaque world, writing a 2D motion
  vector RT (RG16F, render resolution, pixels in screen space, current → previous).
- Static geometry (terrain, WMOs, static doodads): motion from depth + current/previous camera (a full-screen pass);
  this is also what Streamline would compute with `cameraMotionIncluded=false`, but we produce it ourselves so the
  buffer is complete and reusable.
- Moving geometry: everything whose transform or pose changes between frames — characters, creatures, mounts, moving
  game objects (boats, elevators, doors), animated doodads — is drawn again by the motion pass with **our own shaders
  compiled into the exe** (HLSL compiled at build time with `fxc` to vs_3_0/ps_3_0 bytecode and embedded), using this
  frame's and the previous frame's world transform and bone palette. Depth test equal/less against the world depth so
  only visible pixels are written.
- Previous-frame state: each `CM2Model` keeps last frame's world matrix and bone palette in **world space** (not view
  space, because the view changes), keyed by the existing `m_frameStamp`; a model seen for the first time, or after a
  teleport/LOD switch, uses current = previous (zero object motion).
- Translucent effects (particles, ribbons, spell visuals, weather, glares): no per-pixel motion; they are written to a
  **bias-current-color mask** (Streamline `kBufferTypeBiasCurrentColorHint`) so DLSS favours the current frame there.
  Liquids: treated as static surface motion (camera only) plus the bias mask for animated surfaces — builder may refine.
- A debug view (console CVar) visualises the motion RT (colour = direction, brightness = speed) and the depth texture,
  used to verify correctness in-game. Optionally also the Streamline `sl.imgui` validation overlay in dev builds.
- This design keeps the later "custom shading" track open: when a shader family is later replaced with our own, it can
  write motion directly and drop out of the motion pass.

### 6.5 Streamline integration (stages 3–4)

- SDK: the **prebuilt, NVIDIA-signed** Streamline v2.14.16 release (pinned), fetched by CMake like DXVK and staged into
  `bin/Win/x64` with the DLSS / DLSS-G / Reflex plugins and `nvngx_*.dll`. Production builds verify the signature on
  `sl.interposer.dll` as the SDK requires.
- DXVK patch (new `ThirdParty/dxvk/streamline.patch`, applied like `hqx-msvc.patch`). Everything in it is gated on a
  flag the client sets before the d3d9 device is created (environment variable, name is builder's call); without the
  flag DXVK is exactly as today. With the flag:
  - DXVK loads `sl.interposer.dll` from its own directory as the Vulkan loader instead of `vulkan-1.dll`
    (`src/vulkan/vulkan_loader.cpp` currently tries `winevulkan.dll`, then `vulkan-1.dll`), so Streamline sees
    instance/device creation, swapchain and present.
  - **Command recording hook (my call, builder check Q1):** a new interop method lets the client record Vulkan
    commands into DXVK's own command stream (executed on DXVK's CS thread with DXVK's current command buffer and the
    images in known layouts). `slEvaluateFeature` and tag copies use it, so there is no per-frame flush/wait sync
    point. Using only `LockSubmissionQueue` + own command buffers was rejected for the per-frame CPU/GPU sync it forces.
  - **Reflex / present (my call, builder check Q2):** DXVK's own latency tracker (`dxvk.latencySleep`,
    `dxvk_latency_reflex.cpp`) is disabled so it does not fight Streamline over `VK_NV_low_latency2`; the present ID
    DXVK attaches (`dxvk_presenter.cpp:181-216`) is the Streamline frame token; present start/end Reflex markers are
    emitted from DXVK's presenter (queue thread, `dxvk_queue.cpp`) via a client callback, so they time the real
    present, not the queueing of it. Simulation and render-submit markers stay in the game loop.
- Client glue (new module under `CoA/Source/Runtime`, Win32 deps pattern like `Dxvk`/`DxvkClient`): `slInit`
  (application ID / engine type set), `slIsFeatureSupported` per feature for gating the UI, per-frame tokens,
  `slSetConstants` once per frame from the camera record, resource tagging (depth, motion vectors, HUD-less color =
  the output RT before UI, UI color/alpha if cheap, bias mask, scaling input/output) obtained through
  `ID3D9VkInteropDevice` (`GetVulkanImageInfo`, `LockSubmissionQueue`, `TransitionTextureLayout`, `FlushRenderingCommands`),
  `slEvaluateFeature(kFeatureDLSS)` after the world pass, DLSS-G options, Reflex options + PCL markers
  (simulation start/end, render submit start/end, present start/end) placed in the game loop.
- Tags are set to null and `renderingGameFrames=false` on glue screens, loading screens, cinematics/movies and while
  the world is not rendered. Frame Generation is switched off before window resize / mode changes and back on after.
- DLSS output size changes (resolution, mode) recreate the world/motion/depth RTs at the new render size.

### 6.6 Options UI (stage 5) — decided: Graphics page

- New CVars registered from C++ (names builder's call, e.g. `coaDlssMode`, `coaFrameGen`, `coaReflex`), saved in
  `Config.wtf` like other CVars, default `0` (Off).
- Three dropdowns injected at runtime with `FrameScript_Execute` into the existing Graphics (Resolution) panel of the
  Video options, below Resolution / Multisampling / Vertical Sync, following the precedent of `AddOnWatchClient.cpp`.
  They use the panel's own Okay / Cancel / Default behaviour.
- Contents (my call, shown to the user in a mock-up, not objected to):
  - DLSS Mode: Off · DLAA · Quality · Balanced · Performance · Ultra Performance
  - Frame Generation: Off · 2x · 3x · 4x (3x/4x shown only where supported)
  - NVIDIA Reflex: Off · On · On + Boost (locked to On while Frame Generation is on)
- Turning DLSS on greys out and disables Multisampling.
- Unsupported hardware / path: dropdowns shown **greyed out** with the reason text (user's choice).

## 7. Stages and delivery

Each stage is merged into `main` when done (feature branch + merge commit, as the repo already does), hidden behind a
console CVar until stage 5 exposes the options. Each stage meets section 4.

1. Off-screen world RT + INTZ depth + camera record + jitter plumbing (jitter unused). Proof: pixel-identical with the
   CVar on at 100% scale; depth debug view.
2. Motion pass + previous-frame state + bias mask + motion debug view. Proof: debug view correct while running,
   turning, riding, in combat with animated creatures; zero motion when standing still.
3. Streamline + DLSS SR/DLAA (DXVK patch, SDK staging, glue). Proof: DLAA/Quality in-game on the 5080, no ghosting on
   moving characters, sl validation clean.
4. Frame Generation 2x/3x/4x + Reflex. Proof: in-game FPS doubling/tripling/quadrupling, UI stable (no UI
   interpolation artifacts), latency with Reflex.
5. Graphics-page dropdowns + gating + greyed-out states. Proof: AMD/`-nodxvk` checks (via forcing unsupported path),
   Okay/Cancel/Default behaviour.

## 8. Decisions that are expensive to change later

- Separate motion pass instead of rewriting shaders (6.4).
- Previous-frame state stored in world space per model (6.4).
- Streamline enters through DXVK loading `sl.interposer.dll` (6.5).
- CVar names (saved into players' `Config.wtf`).

## 9. Open questions (need the user)

- **Q-A (enabling and greying) — my call (user handed it over):** Streamline is loaded only when at least one of
  the three options is on at startup, so Off stays exactly as today at zero cost. Support for greying is detected
  without Streamline, through NVAPI (`ThirdParty/nvapi`: NVIDIA GPU + architecture — Turing+ for DLSS/Reflex, Ada+
  for Frame Generation, Blackwell+ for 3x/4x) plus "DXVK active". Switching from all-Off to any option shows
  "Requires restart" (as stock restart-only settings do) and takes effect on the next client start; once Streamline
  is loaded, changing modes (DLSS mode, FG 2x/3x/4x, Reflex, back to Off) applies live. If Streamline then reports a
  feature unsupported (e.g. driver too old), the option falls back to Off and shows the reason.
- **Q-B (application ID / distribution) — answered:** the user plans to obtain an NVIDIA developer agreement /
  application ID. Until then: `slInit` uses a CoA64 project GUID + engine type custom, kept in one constant so the
  issued ID drops in later. Production builds ship only NVIDIA's signed production DLLs and verify
  `sl.interposer.dll` at startup (`WinVerifyTrust` + `sl::security::verifyEmbeddedSignature`, full paths); the
  development (unsigned) DLLs are for local testing only and never packaged. Licence obligations (attribution,
  redistribution terms) are the user's to settle before a public release of stages 3–4.

## 10. Left to the builder

CVar names; exact HLSL of the motion shaders; RT formats beyond those stated; exact Reflex marker placement; the
screenshot-comparison tooling; how the Streamline SDK is fetched and cached; unit test shape.
