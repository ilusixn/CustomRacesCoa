# Render Pipeline Stage 1 (off-screen world, depth, camera record, jitter) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Behind the console CVar `coaRenderPipeline`, render the 3D world into an off-screen colour target plus a
sampleable INTZ depth texture, copy it back to the screen before chat bubbles / post-processing / UI, record the
world camera per frame (with previous frame), and provide the sub-pixel jitter machinery — pixel-identical to today.

**Architecture:** Pure, unit-tested logic in `CoA/Source/Runtime/RenderPipeline.{h,cpp}` (camera record, jitter,
activation rules). The D3D9 devices (`CGxDeviceD3d`, `CGxDeviceD3d9Ex`) gain a "world target" that temporarily
replaces their default colour/depth surfaces (`m_defColorSurface` / `m_defDepthSurface`), so every engine path that
means "the back buffer" (render target `nullptr`, `TexCopy`, `DeviceCopyTex`) transparently uses the world target
while it is active. `CGWorldFrame::OnWorldRender` and `CGCamera::SetupWorldProjection` call into CoA through engine
hook pointers (the `g_d3d9LibraryHook` pattern). Own shaders are HLSL compiled by `fxc` at build time and embedded.

**Tech Stack:** C++ (MSVC x64), Direct3D 9 (through DXVK), HLSL vs_3_0/ps_3_0 via Windows SDK `fxc.exe`, CMake/Ninja,
CoATests (`COA_TEST`/`CHECK`), Python 3 stdlib for the image diff tool.

**Spec:** `docs/superpowers/specs/2026-10-26-streamline-rendering-design.md` (sections 4, 5, 6.2, 6.3, 7 stage 1).

## Global Constraints

- C++ only; no MPQ / data-file changes; the game's own shaders are not modified.
- With `coaRenderPipeline 0` (default) no new code path runs and output is unchanged.
- With `coaRenderPipeline 1` at MSAA off, frames are pixel-identical to `coaRenderPipeline 0`.
- The pipeline never activates when the back buffer is multisampled (`gxMultisample` > 1), when the device lacks
  INTZ, or with `fixedFunction 1`; it logs once why it stayed off.
- Never write into `D:\WoW\CoA`. In-game testing only in `D:\WoW\CoA-dev`.
- Every engine edit carries a `// CoA64:` comment (repo convention for CoA64 additions, e.g. `CGxDeviceD3d.cpp:523`).
- Both D3D9 device classes get the same change (`CGxDeviceD3d` and `CGxDeviceD3d9Ex` are separate classes).
- CVar names (saved to `Config.wtf`): `coaRenderPipeline` (0/1, default 0), `coaRenderDebug` (0 off, 1 depth view,
  default 0), `coaRenderJitter` (0/1 debug, default 0). Glue CVars, flags 1, category 1 (graphics).

## Review Focus

1. **Window resize / alt-tab / device reset while active** — world targets must be released on device loss and
   recreated at the new size, no crash, no stale-size image. (Task 3, step "reset test" in-game + Task 1 size rule.)
2. **Partial world viewport** (cinematic letterbox, `m_viewportNDC` not full) — only the viewport rect is copied back;
   pixels outside stay as today. (Task 4 in-game check; Task 1 `CopyRect` test.)
3. **Toggling `coaRenderPipeline` at runtime in world, at login screen, during loading screen** — switches cleanly
   next frame, no leaked surface, no black frame. (Task 4.)
4. **Liquid refraction / FFX glow / death effect** read "the back buffer" via `TexCopy` — must still look identical
   with the pipeline on (they now read the world target mid-world, the real back buffer after). (Task 6 scenes.)
5. **Teleports, zone changes, loading screens, first frame** — camera record `reset` must be set so later stages never
   compute motion across a cut. (Task 1 tests.)

---

## File Structure

| File | Responsibility |
|---|---|
| `CoA/Source/Runtime/RenderPipeline.h/.cpp` (new) | Pure logic: activation rule, camera record + rollover/reset, Halton jitter, jitter-into-projection, copy-rect from viewport. No Win32/D3D. |
| `CoA/Source/Runtime/RenderPipelineClient.cpp` (new) | Client glue: CVars, installs engine hooks, reads device caps/CVars, drives the device world target, debug view. |
| `CoA/Source/Render/Shaders/*.hlsl` (new) | Own shaders (stage 1: fullscreen VS + depth-view PS). |
| `cmake/EmbedShaders.cmake` (new) + `CoA/Source/CMakeLists.txt` | `fxc` → `${CMAKE_BINARY_DIR}/generated/CoA/Shaders/<name>.h` (`static const unsigned char k<Name>[]`). |
| `Engine/Source/Gx/CGxDevice/CGxDevice.h/.cpp` | Declares the hook struct pointer `g_renderPipelineHook` and world-target virtuals (default no-op). |
| `Engine/Source/Gx/CGxDeviceD3d/CGxDeviceD3d.h/.cpp`, `.../CGxDeviceD3d9Ex/CGxDeviceD3d9Ex.h/.cpp` | World target create/begin/end/release; release on device loss. |
| `WoW/Source/Ui/WorldFrame.cpp` | Begin after `RsPush` (line ~1702), End after `RsPop` (line ~1819). |
| `WoW/Source/UIUtil/Camera.cpp` | `SetupWorldProjection` (line 1190): capture camera, apply jitter. |
| `CoA/Tests/RenderPipelineTest.cpp` (new) | Unit tests for the pure logic and embedded shader bytecode. |
| `Tools/coa/imgdiff.py` (new) | Pixel-exact compare of two uncompressed TGA screenshots. |

---

### Task 1: RenderPipeline pure logic

**Files:**
- Create: `CoA/Source/Runtime/RenderPipeline.h`, `CoA/Source/Runtime/RenderPipeline.cpp`
- Test: `CoA/Tests/RenderPipelineTest.cpp`

**Interfaces:**
- Produces (namespace `CoA::RenderPipeline`):
  - `struct Mat4 { float m[4][4]; };` (row-vector, D3D layout, same memory layout as `C44Matrix`)
  - `struct Camera { Mat4 view; Mat4 proj; float pos[3]; float forward[3]; float up[3]; float right[3]; float nearZ, farZ, fovY, aspect; };`
  - `struct Frame { Camera cur; Camera prev; uint32_t index; bool reset; float jitterX, jitterY; };`
  - `struct ActivationInput { bool cvarOn; uint32_t msaaSamples; bool intzSupported; bool shadersEnabled; };`
  - `enum class Inactive { None, CVarOff, Multisample, NoIntz, NoShaders };`
  - `Inactive WhyInactive(const ActivationInput& in);`
  - `void BeginFrame(const Camera& cam, bool forceReset);` — rolls `cur`→`prev`, increments `index`, sets `reset`
  - `const Frame& Current();`
  - `void Halton23(uint32_t index, float& x, float& y);` — values in [-0.5, 0.5)
  - `uint32_t JitterPhaseCount(uint32_t renderWidth, uint32_t displayWidth);` — `8 * (display/render)^2`, rounded, min 8
  - `Mat4 ApplyJitter(const Mat4& proj, float jx, float jy, uint32_t width, uint32_t height);`
  - `struct Rect { int32_t left, top, right, bottom; };`
  - `Rect CopyRect(float vpMinX, float vpMaxX, float vpMinY, float vpMaxY, uint32_t width, uint32_t height);` — NDC viewport (y up, as `m_viewportNDC`) → pixel rect (y down)
  - `void Reset();` (tests only)
  - constant `kTeleportDistance = 50.0f` (yards; camera moved farther in one frame ⇒ `reset`)

- [ ] **Step 1: Write the failing tests** in `CoA/Tests/RenderPipelineTest.cpp`:

```cpp
COA_TEST(RenderPipeline_InactiveReasonsInOrder) {
    using namespace CoA::RenderPipeline;
    CHECK(WhyInactive({ false, 1, true, true }) == Inactive::CVarOff);
    CHECK(WhyInactive({ true, 4, true, true }) == Inactive::Multisample);
    CHECK(WhyInactive({ true, 1, false, true }) == Inactive::NoIntz);
    CHECK(WhyInactive({ true, 1, true, false }) == Inactive::NoShaders);
    CHECK(WhyInactive({ true, 1, true, true }) == Inactive::None);
    CHECK(WhyInactive({ true, 0, true, true }) == Inactive::None);   // 0 and 1 both mean "no MSAA"
}

COA_TEST(RenderPipeline_FirstFrameIsReset) {
    CoA::RenderPipeline::Reset();
    CoA::RenderPipeline::BeginFrame(CameraAt(0, 0, 0), false);
    CHECK(CoA::RenderPipeline::Current().reset);
    CHECK(CoA::RenderPipeline::Current().index == 1);
}

COA_TEST(RenderPipeline_SecondFrameKeepsPreviousCamera) {
    CoA::RenderPipeline::Reset();
    CoA::RenderPipeline::BeginFrame(CameraAt(0, 0, 0), false);
    CoA::RenderPipeline::BeginFrame(CameraAt(1, 0, 0), false);
    const auto& f = CoA::RenderPipeline::Current();
    CHECK(!f.reset);
    CHECK(f.prev.pos[0] == 0.0f && f.cur.pos[0] == 1.0f);
}

COA_TEST(RenderPipeline_TeleportAndForcedResetSetReset) {
    CoA::RenderPipeline::Reset();
    CoA::RenderPipeline::BeginFrame(CameraAt(0, 0, 0), false);
    CoA::RenderPipeline::BeginFrame(CameraAt(51, 0, 0), false);
    CHECK(CoA::RenderPipeline::Current().reset);
    CoA::RenderPipeline::BeginFrame(CameraAt(52, 0, 0), true);
    CHECK(CoA::RenderPipeline::Current().reset);
    CoA::RenderPipeline::BeginFrame(CameraAt(53, 0, 0), false);
    CHECK(!CoA::RenderPipeline::Current().reset);
}

COA_TEST(RenderPipeline_HaltonFirstValues) {
    float x, y;
    CoA::RenderPipeline::Halton23(1, x, y);
    CHECK(Near(x, 0.0f) && Near(y, -0.5f + 1.0f / 3.0f));          // 1/2 - 0.5, 1/3 - 0.5
    CoA::RenderPipeline::Halton23(2, x, y);
    CHECK(Near(x, -0.25f) && Near(y, -0.5f + 2.0f / 3.0f));
    for (uint32_t i = 1; i < 64; ++i) {
        CoA::RenderPipeline::Halton23(i, x, y);
        CHECK(x >= -0.5f && x < 0.5f && y >= -0.5f && y < 0.5f);
    }
}

COA_TEST(RenderPipeline_PhaseCount) {
    CHECK(CoA::RenderPipeline::JitterPhaseCount(2560, 2560) == 8);
    CHECK(CoA::RenderPipeline::JitterPhaseCount(1280, 2560) == 32);
    CHECK(CoA::RenderPipeline::JitterPhaseCount(1707, 2560) == 18);   // 8 * 1.4997^2 = 17.99
}

COA_TEST(RenderPipeline_ZeroJitterLeavesProjectionBitIdentical) {
    Mat4 p = SampleProjection();
    Mat4 q = CoA::RenderPipeline::ApplyJitter(p, 0.0f, 0.0f, 1920, 1080);
    CHECK(memcmp(&p, &q, sizeof p) == 0);
}

COA_TEST(RenderPipeline_JitterOffsetsClipSpaceByPixels) {
    Mat4 p = SampleProjection();
    Mat4 q = CoA::RenderPipeline::ApplyJitter(p, 0.5f, -0.25f, 1920, 1080);
    CHECK(Near(q.m[2][0], p.m[2][0] + 2.0f * 0.5f / 1920.0f));
    CHECK(Near(q.m[2][1], p.m[2][1] - 2.0f * -0.25f / 1080.0f));      // y flips: +pixel y is down, +clip y is up
    CHECK(q.m[0][0] == p.m[0][0] && q.m[3][2] == p.m[3][2]);
}

COA_TEST(RenderPipeline_CopyRectFullAndLetterbox) {
    auto r = CoA::RenderPipeline::CopyRect(0.0f, 1.0f, 0.0f, 1.0f, 1920, 1080);
    CHECK(r.left == 0 && r.top == 0 && r.right == 1920 && r.bottom == 1080);
    r = CoA::RenderPipeline::CopyRect(0.0f, 1.0f, 0.1f, 0.9f, 1920, 1000);
    CHECK(r.left == 0 && r.top == 100 && r.right == 1920 && r.bottom == 900);
}
```

  Helpers in the test file: `Near(a, b)` (|a-b| < 1e-5), `CameraAt(x, y, z)` (identity view/proj, pos set),
  `SampleProjection()` (any perspective matrix with non-zero `m[2][0]`, `m[2][1]`).

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build out\build\x64-Release --target CoATests && out\build\x64-Release\CoA\Tests\CoATests.exe`
Expected: build FAILS (`RenderPipeline.h` not found).

- [ ] **Step 3: Implement `RenderPipeline.h/.cpp`** with the interfaces above. `ApplyJitter` adds
  `2*jx/width` to `m[2][0]` and `-2*jy/height` to `m[2][1]` (D3D row-vector projection: clip x/y offsets live in the
  third row because `w = z_view`), returning the input unchanged bit-for-bit when both jitters are 0. Teleport test
  uses the squared distance between `cur.pos` and the new `pos`. State is file-static, like `Dxvk.cpp`.

- [ ] **Step 4: Run tests to verify they pass**

Run: same as step 2. Expected: all `RenderPipeline_*` pass, exit code 0.

- [ ] **Step 5: Commit**

```bash
git add CoA/Source/Runtime/RenderPipeline.h CoA/Source/Runtime/RenderPipeline.cpp CoA/Tests/RenderPipelineTest.cpp
git commit -m "RenderPipeline: camera record, jitter and activation rules (pure logic)"
```

---

### Task 2: Embedded shaders (fxc at build time)

**Files:**
- Create: `cmake/EmbedShaders.cmake`, `CoA/Source/Render/Shaders/FullscreenVS.hlsl`, `CoA/Source/Render/Shaders/DepthViewPS.hlsl`
- Modify: `CoA/Source/CMakeLists.txt` (generate + add include dir), `CoA/Tests/CMakeLists.txt` (include dir)
- Test: `CoA/Tests/RenderPipelineTest.cpp`

**Interfaces:**
- Produces: headers `CoA/Shaders/FullscreenVS.h`, `CoA/Shaders/DepthViewPS.h` under the generated include dir, each
  `static const unsigned char kFullscreenVS[] = {...};` / `kDepthViewPS[]` (D3D9 bytecode, `fxc /T vs_3_0` /
  `/T ps_3_0`, `/O3`, `/Vn` name, `/Fh` output).
- `DepthViewPS`: samples INTZ depth at `s0` (point), outputs `pow(saturate(1 - depth), 8)` replicated grey
  (near = bright) — linearisation is not needed for a visual check.
- `FullscreenVS`: input `POSITION` (float4 clip-space) + `TEXCOORD0`, passthrough; uses `c0.xy` as the D3D9
  half-pixel offset.

- [ ] **Step 1: Write the failing test**

```cpp
#include "CoA/Shaders/DepthViewPS.h"
#include "CoA/Shaders/FullscreenVS.h"
COA_TEST(RenderPipeline_EmbeddedShadersAreSm3Bytecode) {
    auto token = [](const unsigned char* b) { return uint32_t(b[0]) | uint32_t(b[1]) << 8 | uint32_t(b[2]) << 16 | uint32_t(b[3]) << 24; };
    CHECK(token(kFullscreenVS) == 0xFFFE0300u);   // vs_3_0
    CHECK(token(kDepthViewPS) == 0xFFFF0300u);    // ps_3_0
}
```

- [ ] **Step 2: Run to verify it fails** — `cmake --build out\build\x64-Release --target CoATests`; Expected: FAIL, header not found.

- [ ] **Step 3: Implement** `cmake/EmbedShaders.cmake`: `find_program(COA_FXC fxc HINTS "$ENV{WindowsSdkVerBinPath}/x64" "C:/Program Files (x86)/Windows Kits/10/bin/*/x64")`
  (FATAL_ERROR if missing — shaders are required, unlike DXVK), and a function
  `coa_embed_shader(TARGET <lib> FILE <hlsl> PROFILE <vs_3_0|ps_3_0> NAME <kName>)` adding an `add_custom_command`
  whose output header is a source of the target. Write the two HLSL files.

- [ ] **Step 4: Run test to verify it passes** — Expected: PASS.

- [ ] **Step 5: Commit** — `git commit -m "Build: embed own HLSL shaders (fxc vs_3_0/ps_3_0) into the client"`

---

### Task 3: Device world target (both D3D9 devices)

**Files:**
- Modify: `Engine/Source/Gx/CGxDevice/CGxDevice.h` / `.cpp` (virtuals + hook pointer), `Engine/Source/Gx/CGxDeviceD3d/CGxDeviceD3d.h/.cpp`,
  `Engine/Source/Gx/CGxDeviceD3d9Ex/CGxDeviceD3d9Ex.h/.cpp`

**Interfaces:**
- Produces on `CGxDevice` (virtual, base returns false / does nothing):
  - `virtual bool WorldTargetSupported();` — INTZ via `CheckDeviceFormat(..., D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, (D3DFORMAT)MAKEFOURCC('I','N','T','Z'))`
  - `virtual bool WorldTargetBegin(uint32_t width, uint32_t height);` — creates (or reuses if size matches) a
    `D3DUSAGE_RENDERTARGET` colour texture in the back-buffer format and an INTZ depth texture, swaps them into
    `m_defColorSurface` / `m_defDepthSurface` (keeping the originals), re-applies the current render target so a
    `nullptr` target now binds the world surfaces. Returns false (and changes nothing) if creation fails.
  - `virtual void WorldTargetEnd(const RECT& copy);` — restores the original default surfaces, re-applies the render
    target, `StretchRect(world colour → back buffer, copy, copy, D3DTEXF_NONE)`.
  - `virtual void* WorldDepthTexture();` / `virtual void* WorldColorTexture();` — `IDirect3DTexture9*` (for the
    debug view now, Streamline/motion pass later).
  - `virtual void WorldTargetRelease();` — also called from the device-lost / destroy paths next to the existing
    `m_defColorSurface->Release()` (`CGxDeviceD3d.cpp:2677`) and the D3d9Ex equivalent.
- Produces in `CGxDevice.h`: `struct RenderPipelineHook { void (*beginWorld)(); void (*endWorld)(const CRect& viewportNDC); void (*setupWorldProjection)(C44Matrix& projection, const C44Matrix& view, const C3Vector& position, float fovY, float aspect, float nearZ, float farZ, const C3Vector& forward, const C3Vector& up); };`
  and `extern RenderPipelineHook* g_renderPipelineHook;` (null by default; defined in `CGxDevice.cpp` next to
  `g_d3d9LibraryHook`).

- [ ] **Step 1: Implement the virtuals in both devices** as above, `// CoA64:` comments on every touched function.
  Begin while already begun, or End without Begin, is a no-op.

- [ ] **Step 2: Build** — `cmake --build out\build\x64-Release --target Wow`; Expected: builds, no warnings in touched files.

- [ ] **Step 3: Commit** — `git commit -m "Gx: D3D9 world target (colour RT + INTZ depth) swapped in as the default surfaces"`

  (Behaviour is verified in Task 4/6; nothing calls it yet.)

---

### Task 4: Wire the pipeline into the frame (CVars, hooks, glue)

**Files:**
- Create: `CoA/Source/Runtime/RenderPipelineClient.cpp` (+ `CoA::RenderPipelineClient::Install()` declared in `RenderPipeline.h`)
- Modify: `WoW/Source/Ui/WorldFrame.cpp:1702` and `:1819`, `WoW/Source/UIUtil/Camera.cpp:1190`, the CoA init that calls `CoA::DxvkClient::Preload()` (add `Install()` beside it)
- Test: `CoA/Tests/RenderPipelineTest.cpp`

**Interfaces:**
- Consumes: Task 1 API, Task 3 device virtuals and `g_renderPipelineHook`.
- Produces: CVars `coaRenderPipeline`, `coaRenderDebug`, `coaRenderJitter` (Global Constraints values), queued with
  `COA_CVAR(..., world=false)` so they exist at the login screen.
- `WorldFrame.cpp`: after `GxRsSet(GxRs_Multisample, 1)` → `if (g_renderPipelineHook) g_renderPipelineHook->beginWorld();`;
  immediately after `g_theGxDevicePtr->RsPop();` → `if (g_renderPipelineHook) g_renderPipelineHook->endWorld(this->m_viewportNDC);`.
  Both on the early-`return` path too (the `if (!scene) return;` at ~1770 returns after Begin — End must still run:
  restructure so End runs on every exit after Begin).
- `Camera.cpp` `SetupWorldProjection`: build the projection as `CSimpleCamera::SetGxProjectionAndView` does, call
  `setupWorldProjection(projection, view, m_position, ...)` before `XformSetProjection`; the hook may modify
  `projection` (jitter). With no hook the code path is byte-for-byte the old one.
- Glue rules: `beginWorld` evaluates `WhyInactive` (CVar, `gxMultisample`, `WorldTargetSupported()`,
  `CShaderEffect::s_enableShaders`), logs the reason once per change via `CoA::Log::Printf`, and when active calls
  `WorldTargetBegin(window w, h)`. `setupWorldProjection` calls `BeginFrame(camera, forceReset)` with
  `forceReset` = first world frame after a loading screen / world enter / size change; jitter applied only when
  `coaRenderJitter 1` and active (phase count `JitterPhaseCount(w, w)` in stage 1). `endWorld` computes
  `CopyRect(viewportNDC, w, h)` and calls `WorldTargetEnd`, or draws the depth view (Task 5) when `coaRenderDebug 1`.
  Turning the CVar off releases the world target on the next frame.

- [ ] **Step 1: Write the failing test** — CVar registration:

```cpp
COA_TEST(RenderPipeline_CVarsQueuedAsGlueWithDefaultsOff) {
    CoA::CVars::Reset();
    CoA::RenderPipelineClient::QueueCVars();
    size_t n = 0;
    const CoA::CVarEntry* e = CoA::CVars::Glue(n);
    auto find = [&](const char* name) -> const CoA::CVarEntry* { for (size_t i = 0; i < n; ++i) if (!strcmp(e[i].name, name)) return &e[i]; return nullptr; };
    for (const char* name : { "coaRenderPipeline", "coaRenderDebug", "coaRenderJitter" }) {
        const CoA::CVarEntry* v = find(name);
        CHECK(v && !strcmp(v->defaultValue, "0") && v->flags == 1 && v->category == 1);
    }
    CoA::CVars::Reset();
}
```
  (`QueueCVars()` lives in `RenderPipeline.cpp` so the test does not link client glue.)

- [ ] **Step 2: Run to verify it fails** — Expected: FAIL (`QueueCVars` undefined).
- [ ] **Step 3: Implement** `QueueCVars`, `RenderPipelineClient.cpp`, and the two engine edits above.
- [ ] **Step 4: Run tests** — Expected: all CoATests pass. Build `Wow` target: succeeds.
- [ ] **Step 5: In-game check in `D:\WoW\CoA-dev`** — copy `bin\Win\CoA64.exe` and `bin\Win\x64\d3d9.dll` into
  `D:\WoW\CoA-dev` (never `D:\WoW\CoA`), start with `-extlog 1`, `gxMultisample 1`:
  - `/console coaRenderPipeline 1` in Dalaran and in open world: image unchanged, `Logs\Extensions.log` shows activation.
  - Resize window, alt-tab out of fullscreen and back, `/reload`, log out and in: no crash, still unchanged.
  - Toggle the CVar at the login screen, during a loading screen and in world: no black frame, no errors.
  - Set `gxMultisample 4` + `RestartGx`: log says "inactive: multisample", image as today.
- [ ] **Step 6: Commit** — `git commit -m "RenderPipeline: world renders off-screen behind coaRenderPipeline (stage 1)"`

---

### Task 5: Depth debug view

**Files:**
- Modify: `CoA/Source/Runtime/RenderPipelineClient.cpp`

**Interfaces:**
- Consumes: `kFullscreenVS`, `kDepthViewPS` (Task 2), `WorldDepthTexture()` (Task 3).
- With `coaRenderDebug 1` and the pipeline active, `endWorld` restores the default surfaces (as `WorldTargetEnd`
  does) and then draws a full-screen quad over the copy rect with the two shaders and the INTZ texture at sampler 0
  instead of the colour copy, saving and restoring every D3D9 state it touches (use an `IDirect3DStateBlock9`
  `D3DSBT_ALL` captured before, applied after). Shaders are created once per device and released with the world
  target.

- [ ] **Step 1: Implement** as above.
- [ ] **Step 2: In-game check (CoA-dev):** `coaRenderDebug 1` shows a greyscale depth image (near bright, sky
  black) that lines up with the scene; terrain, buildings, characters present; `coaRenderDebug 0` returns to the
  normal image with UI unaffected in both modes.
- [ ] **Step 3: Commit** — `git commit -m "RenderPipeline: depth debug view (coaRenderDebug 1)"`

---

### Task 6: Pixel-identical proof, cost check, merge

**Files:**
- Create: `Tools/coa/imgdiff.py`
- Modify: `CoA/DIFFERENCES.md` (one entry: CoA64 addition, console-only render pipeline, off by default)

**Interfaces:**
- `python Tools/coa/imgdiff.py A.tga B.tga [--out diff.tga]` → prints `identical` and exits 0, or prints
  `N pixels differ (max channel delta D)` and exits 1; supports uncompressed and RLE TGA, 24/32-bit (stdlib only).

- [ ] **Step 1: Write the tool and self-test it**: `python Tools/coa/imgdiff.py X.tga X.tga` → `identical`;
  a copy with one byte changed → `1 pixels differ`.
- [ ] **Step 2: Capture pairs in `D:\WoW\CoA-dev`** with `screenshotFormat tga`, `gxMultisample 1`, UI hidden
  (Alt+Z), camera not moving, `coaRenderJitter 0`: for each scene (Dalaran, an outdoor zone with water, a dungeon,
  character select / login screen), take a screenshot with `coaRenderPipeline 0`, then `1`.
- [ ] **Step 3: Compare** each pair with `imgdiff.py`. Expected: `identical` for all. Any difference is a bug
  (fix, return to Task 3/4) — do not accept "close enough".
- [ ] **Step 4: Cost check** — same spot, 60 s each with `coaRenderPipeline 0` and `1`, read FPS (`Ctrl+R`).
  Off must match the pre-change build; On may cost at most the one full-screen copy (report the numbers).
- [ ] **Step 5: Commit, merge to `main`**

```bash
git add Tools/coa/imgdiff.py CoA/DIFFERENCES.md
git commit -m "RenderPipeline stage 1: pixel-identical check tool, DIFFERENCES entry"
git checkout main && git merge --no-ff <stage-1 branch> -m "Merge render pipeline stage 1 (off-screen world, depth, camera record)"
```

---

## Later stages (separate plans, written after this one lands)

2. Motion pass + previous-frame model state + bias mask + motion debug view.
3. Streamline + DLSS SR/DLAA (DXVK `streamline.patch`, SDK staging, signature checks, glue).
4. Frame Generation 2x/3x/4x + Reflex (DXVK present/Reflex handover).
5. Graphics-page dropdowns, NVAPI gating, restart prompt, greyed-out states.
