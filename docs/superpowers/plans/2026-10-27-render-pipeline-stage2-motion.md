# Render Pipeline Stage 2 (motion vectors, translucency mask) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** While `coaRenderPipeline 1` and `coaRenderMotion 1`, produce per frame a full-resolution screen-space motion
texture (camera motion for static geometry + exact object motion for moving/animated models) and a translucency
("bias current colour") mask — the inputs DLSS needs in stage 3 — with debug views, and change nothing else.

**Architecture:** Three passes at the end of the world render (before the copy-back), all with our own embedded
shaders and the game's shaders untouched: (1) a full-screen *static pass* reprojects every pixel from INTZ depth with
the current/previous camera record; (2) a *model pass* redraws every moving M2 batch with a skinning VS that takes
this frame's bones and last frame's bones (both in the view space they were drawn in) from a vertex texture and
writes object+camera motion, depth-tested against INTZ; (3) a *mask pass* marks pixels changed by translucent work,
by comparing the final world colour with a snapshot taken right after the opaque geometry. Pure math and the
per-model previous-bone cache are unit-tested C++; the engine-facing passes live in a new client file.

**Tech Stack:** C++ (MSVC x64), Direct3D 9 via DXVK (vs_3_0 vertex texture fetch, RG16F/A32B32G32R32F textures),
HLSL compiled by `fxc` (existing `coa_embed_shader`), CoATests, in-game checks in `D:\WoW\CoA-dev` via the `CoATest` addon.

**Spec:** `docs/superpowers/specs/2026-10-26-streamline-rendering-design.md` (sections 4, 6.3, 6.4, 7 stage 2).
Stage 1 plan for context: `docs/superpowers/plans/2026-10-26-render-pipeline-stage1.md`.

## Global Constraints

- C++ only; no MPQ / data-file changes; the game's own shaders are not modified (spec 6.4, decided option A).
- With `coaRenderMotion 0` (default) or `coaRenderPipeline 0`, no new code path runs and stage 1 behaviour is unchanged
  (the stage 1 pixel-identity standard still holds for the colour image with motion ON: the passes only write their
  own targets).
- New CVar: `coaRenderMotion` (0/1, default 0), glue CVar, flags 1, category 1 — same queueing as stage 1's CVars.
- `coaRenderDebug` gains values 3 (motion view) and 4 (translucency mask view); 1 and 2 keep their meaning.
- Motion convention: RG16F, `(prevUV - curUV)` in UV units of the world target (u right, v down), computed with the
  UNJITTERED current and previous projections (jitter never appears in motion). (0,0) = no motion.
- Depth: the INTZ value is `depthMin + ndc * (depthMax - depthMin)` (`Frame::depthMin/depthMax`, world MaxZ 0.94).
- A model seen for the first time (no cached previous bones) or after a reset gets no model-pass draw (its pixels
  keep the static pass's camera motion). On `Frame::reset` the whole motion texture is cleared to (0,0).
- Both D3D9 device classes (`CGxDeviceD3d`, `CGxDeviceD3d9Ex`) get identical device changes; engine edits carry
  `// CoA64:` comments.
- Never write into `D:\WoW\CoA`. Builds: `.superpowers/sdd/<plan>/build.bat <target>` (vcvars64 wrapper, as stage 1);
  tests: `out\build\x64-Release\bin\CoATests.exe`.

## Review Focus

1. **Characters turning/walking while the camera also moves** — object motion must include camera motion exactly once
   (prev view-space bones already contain last frame's view). (Task 2 test `ModelMotion_*`, Task 6 in-game.)
2. **Models whose bone count exceeds the palette texture width** (>85 bones in one skin section) — skip with a one-time
   log, never read out of bounds. (Task 6 step "palette bounds"; Task 3 test `MotionCache_RejectsOversizedPalette`.)
3. **A `CM2Model*` freed and a new model allocated at the same address** — must not inherit the old model's previous
   bones. (Task 3 test `MotionCache_HandleChangeDropsHistory`.)
4. **Camera cuts** (vehicle enter/exit, cinematic, `/reload` mid-world, big rotation snaps) — `reset` set, motion
   cleared. (Task 1 test `RenderPipeline_CameraCutResets`.)
5. **Device reset / resize with motion on** — motion, mask and snapshot textures released with the world target and
   recreated at the new size. (Task 4; in-game resize in Task 7.)

---

## File Structure

| File | Responsibility |
|---|---|
| `CoA/Source/Runtime/RenderPipeline.h/.cpp` (modify) | Camera-cut reset; `Frame::jitterX/Y` filled; `QueueCVars` adds `coaRenderMotion`. |
| `CoA/Source/Runtime/RenderMotion.h/.cpp` (new) | Pure reference math: static reprojection, skinned motion of a point. Mirrors the shaders; tests pin the conventions. |
| `CoA/Source/Runtime/MotionCache.h/.cpp` (new) | Per-model previous-frame bone palettes (keyed by model pointer + handle), eviction. |
| `CoA/Source/Render/Shaders/StaticMotionPS.hlsl`, `SkinnedMotionVS.hlsl`, `MotionPS.hlsl`, `BiasMaskPS.hlsl`, `MotionViewPS.hlsl` (new) | Embedded shaders (vs/ps_3_0). |
| `Engine/Source/Gx/CGxDevice/CGxDevice.h/.cpp`, both D3D9 device classes (modify) | Motion (RG16F), mask (A8R8G8B8) and opaque-snapshot targets; create/release with the world target. |
| `WoW/Source/Ui/WorldFrame.cpp` (modify) | New hook call `opaqueDone()` right after `CWorldScene::RenderDetailDoodads()`. |
| `CoA/Source/Runtime/RenderMotionClient.cpp` (new) | The three passes, palette texture upload, debug views 3/4; called from `RenderPipelineClient` endWorld / opaqueDone. |
| `CoA/Tests/RenderMotionTest.cpp` (new) | Tests for RenderMotion + MotionCache + camera cut. |

---

### Task 1: Stage 1 carry-overs and `coaRenderMotion`

**Files:**
- Modify: `CoA/Source/Runtime/RenderPipeline.h/.cpp`, `CoA/Source/Runtime/RenderPipelineClient.cpp`
- Test: `CoA/Tests/RenderMotionTest.cpp` (new file; later tasks add to it)

**Interfaces:**
- Produces: `constexpr float kCameraCutDegrees = 60.0f;` — `BeginFrame` also sets `reset` when the angle between
  `prev.forward` and `cam.forward` exceeds it. `void SetJitter(float jx, float jy);` stores the applied jitter (pixels)
  in `Current().jitterX/Y` (0,0 when jitter is not applied this frame). `CVar** MotionSlot();` and `coaRenderMotion`
  queued by `QueueCVars()`.

- [ ] **Step 1: Write the failing tests**

```cpp
COA_TEST(RenderPipeline_CameraCutResets) {
    CoA::RenderPipeline::Reset();
    CoA::RenderPipeline::BeginFrame(CameraFacing(1, 0, 0), false);
    CoA::RenderPipeline::BeginFrame(CameraFacing(1, 0, 0), false);
    CHECK(!CoA::RenderPipeline::Current().reset);
    CoA::RenderPipeline::BeginFrame(CameraFacing(0.6f, 0.8f, 0), false);    // ~53 degrees: no cut
    CHECK(!CoA::RenderPipeline::Current().reset);
    CoA::RenderPipeline::BeginFrame(CameraFacing(-0.6f, 0.8f, 0), false);   // ~74 degrees: cut
    CHECK(CoA::RenderPipeline::Current().reset);
}

COA_TEST(RenderPipeline_SetJitterStoresPixels) {
    CoA::RenderPipeline::Reset();
    CoA::RenderPipeline::BeginFrame(CameraFacing(1, 0, 0), false);
    CoA::RenderPipeline::SetJitter(0.25f, -0.125f);
    CHECK(CoA::RenderPipeline::Current().jitterX == 0.25f && CoA::RenderPipeline::Current().jitterY == -0.125f);
    CoA::RenderPipeline::BeginFrame(CameraFacing(1, 0, 0), false);
    CHECK(CoA::RenderPipeline::Current().jitterX == 0.0f && CoA::RenderPipeline::Current().jitterY == 0.0f);
}

COA_TEST(RenderPipeline_MotionCVarQueuedOff) {
    CoA::CVars::Reset();
    CoA::RenderPipelineClient::QueueCVars();
    size_t n = 0;
    const CoA::CVarEntry* e = CoA::CVars::Glue(n);
    const CoA::CVarEntry* v = nullptr;
    for (size_t i = 0; i < n; ++i) if (!strcmp(e[i].name, "coaRenderMotion")) v = &e[i];
    CHECK(v && !strcmp(v->defaultValue, "0") && v->flags == 1 && v->category == 1);
    CoA::CVars::Reset();
}
```
  (`CameraFacing(x, y, z)`: identity matrices, pos 0, `forward` = normalised (x, y, z).)

- [ ] **Step 2: Run to verify they fail** (build `CoATests`; expected: compile errors for `SetJitter` / missing CVar).
- [ ] **Step 3: Implement** the three items; `BeginFrame` zeroes `jitterX/Y` each frame; RenderPipelineClient calls
  `SetJitter` where it applies jitter. Also fix the parked stage 1 finding: in BeginWorld set `s_beginFailed = true`
  *after* `Deactivate(device)` (one extra retry today).
- [ ] **Step 4: Run tests** — all pass.
- [ ] **Step 5: Commit** — `RenderPipeline: camera-cut reset, jitter in the frame record, coaRenderMotion`

---

### Task 2: Motion reference math (pure)

**Files:**
- Create: `CoA/Source/Runtime/RenderMotion.h/.cpp`
- Test: `CoA/Tests/RenderMotionTest.cpp`

**Interfaces:**
- Produces (namespace `CoA::RenderMotion`, uses `RenderPipeline::Mat4/Frame`):
  - `struct Vec2 { float x, y; };`
  - `Vec2 StaticMotion(const RenderPipeline::Frame& f, float u, float v, float storedDepth);` — the static pass:
    storedDepth → ndc via `depthMin/depthMax`; unproject with `inverse(cur.view * cur.proj)` (camera-relative,
    eye at origin) to a camera-relative point; add `cur.pos - prev.pos`; project with `prev.view * prev.proj`;
    returns `prevUV - curUV`. UV: u = (ndcX + 1) / 2, v = (1 - ndcY) / 2.
  - `Vec2 ModelMotion(const Mat4& curBoneView, const Mat4& prevBoneView, const float pos[3], const Mat4& curProj, const Mat4& prevProj);`
    — single-bone reference of the model pass: `clip = (pos,1) * bone * proj` (row vectors), returns
    `prevUV - curUV`.
  - `Mat4 Inverse(const Mat4& m);` (general 4x4).

- [ ] **Step 1: Write the failing tests**

```cpp
COA_TEST(StaticMotion_StillCameraIsZero) {          // cur == prev camera, any pixel/depth
    auto f = FrameWith(Cam(0, 0, 0, Yaw(0)), Cam(0, 0, 0, Yaw(0)));
    auto m = CoA::RenderMotion::StaticMotion(f, 0.3f, 0.7f, 0.9f);
    CHECK(Near(m.x, 0) && Near(m.y, 0));
}
COA_TEST(StaticMotion_SidestepMovesPointsOpposite) { // camera moved +1 yd along its right axis since last frame
    auto f = FrameWith(/*prev*/ Cam(0, 0, 0, Yaw(0)), /*cur*/ Cam(1, 0, 0, Yaw(0)));   // right axis = +x
    auto m = CoA::RenderMotion::StaticMotion(f, 0.5f, 0.5f, DepthAt(f, 10.0f));        // centre pixel, 10 yd away
    CHECK(m.x > 0 && Near(m.y, 0));   // camera moved right: the point was further right on screen last frame
}
COA_TEST(StaticMotion_DepthRangeHonoured) {         // same point, depth range 0..0.94 vs 0..1: same motion
    auto a = FrameWith(Cam(0, 0, 0, Yaw(0)), Cam(1, 0, 0, Yaw(0)), 0.0f, 0.94f);
    auto b = FrameWith(Cam(0, 0, 0, Yaw(0)), Cam(1, 0, 0, Yaw(0)), 0.0f, 1.0f);
    auto ma = CoA::RenderMotion::StaticMotion(a, 0.4f, 0.5f, DepthAt(a, 10.0f));
    auto mb = CoA::RenderMotion::StaticMotion(b, 0.4f, 0.5f, DepthAt(b, 10.0f));
    CHECK(Near(ma.x, mb.x) && Near(ma.y, mb.y) && ma.x > 0);
}
COA_TEST(ModelMotion_StaticModelUnderStillCameraIsZero) {
    Mat4 bone = Mul(Translate(0, 0, 10), CamView(Yaw(0)));                          // world * view
    float p[3] = { 0.5f, 0.2f, 0.0f };
    auto m = CoA::RenderMotion::ModelMotion(bone, bone, p, CamProj(), CamProj());
    CHECK(Near(m.x, 0) && Near(m.y, 0));
}
COA_TEST(ModelMotion_CameraMotionIncludedOnce) {     // model still in the world, camera sidestepped +1 yd:
    auto f = FrameWith(Cam(0, 0, 0, Yaw(0)), Cam(1, 0, 0, Yaw(0)));
    Mat4 world = Translate(0, 0, 10);                                                // camera-relative to the prev eye
    Mat4 prevBone = Mul(world, f.prev.view);
    Mat4 curBone = Mul(Mul(world, Translate(-1, 0, 0)), f.cur.view);                  // same point, re-centred on new eye
    float p[3] = { 0, 0, 0 };
    auto mm = CoA::RenderMotion::ModelMotion(curBone, prevBone, p, f.cur.proj, f.prev.proj);
    auto ms = CoA::RenderMotion::StaticMotion(f, CurU(f, curBone, p), CurV(f, curBone, p), CurDepth(f, curBone, p));
    CHECK(Near(mm.x, ms.x, 1e-4f) && Near(mm.y, ms.y, 1e-4f));
}
COA_TEST(RenderMotion_InverseRoundTrip) {
    Mat4 m = Mul(CamView(Yaw(0.7f)), CamProj());
    Mat4 i = Mul(m, CoA::RenderMotion::Inverse(m));
    for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) CHECK(Near(i.m[r][c], r == c ? 1.0f : 0.0f, 1e-4f));
}
```
  Extra helpers: `FrameWith(prev, cur, depthMin = 0, depthMax = 0.94f)`, `Translate`, `Mul` (row-vector order),
  `CamView(yaw)`, `CamProj()`, and `CurU/CurV/CurDepth` (project `p` through `curBone * cur.proj` to the current UV and
  stored depth).
  Helpers: `Cam(x, y, z, yaw)` builds a camera like `CSimpleCamera::SetGxProjectionAndView` (view rotation-only via
  look-at from the origin, `Projection_Exact` FOV 0.6*1.57, aspect 1.6, near 0.2, far 777), `DepthAt(f, yards)` the
  stored INTZ value for a point straight ahead at that distance. **Sign rule:** a camera moving right makes a static
  point move left on screen, so its *previous* UV is to the right of the current one (`m.x > 0`).

- [ ] **Step 2: Run to verify they fail.**
- [ ] **Step 3: Implement** `RenderMotion.cpp` (no globals; pure functions).
- [ ] **Step 4: Run tests** — pass.
- [ ] **Step 5: Commit** — `RenderMotion: reference math for static and model motion`

---

### Task 3: Previous-bone cache (pure)

**Files:**
- Create: `CoA/Source/Runtime/MotionCache.h/.cpp`
- Test: `CoA/Tests/RenderMotionTest.cpp`

**Interfaces:**
- Produces (namespace `CoA::MotionCache`):
  - `constexpr uint32_t kMaxBones = 85;` (palette texture 256 texels wide / 3 texels per bone)
  - `struct Key { const void* model; uint32_t handle; };`
  - `bool Store(Key key, uint32_t frameIndex, const RenderPipeline::Mat4& world, const RenderPipeline::Mat4* bones, uint32_t count);`
    — false (and no store) when `count > kMaxBones` or `count == 0`. `world` = the model's `m_worldTransform`.
  - `const RenderPipeline::Mat4* PreviousWorld(Key key, uint32_t frameIndex);` — the world transform stored at
    `frameIndex - 1` (same rules as `Previous`); the client compares it with the current one for `worldChanged`.
  - `const RenderPipeline::Mat4* Previous(Key key, uint32_t frameIndex, uint32_t& count);` — the bones stored at
    `frameIndex - 1` for this key; nullptr when absent, stored at another frame, or the handle differs.
  - `void EndFrame(uint32_t frameIndex);` — drops entries not stored at this frame (models not drawn this frame).
  - `void Clear();` (reset / device loss) and `size_t Size();`
  - Storage: one map from `model` pointer to `{handle, frameIndex, bones[kMaxBones], count}`; double-buffered per
    frame: `Store` for frame N writes the "current" slot, `Previous` for frame N reads the slot written at N-1.

- [ ] **Step 1: Write the failing tests**

```cpp
COA_TEST(MotionCache_PreviousIsLastFrame) {
    CoA::MotionCache::Clear();
    Mat4 a[2] = { Mat(1), Mat(2) }, b[2] = { Mat(3), Mat(4) };
    CHECK(CoA::MotionCache::Store({ &s_m1, 7 }, 10, Mat(0), a, 2));
    uint32_t n = 0;
    CHECK(CoA::MotionCache::Previous({ &s_m1, 7 }, 10, n) == nullptr);           // nothing from frame 9
    CoA::MotionCache::EndFrame(10);
    CHECK(CoA::MotionCache::Store({ &s_m1, 7 }, 11, Mat(0), b, 2));
    const Mat4* p = CoA::MotionCache::Previous({ &s_m1, 7 }, 11, n);
    CHECK(p && n == 2 && p[0].m[0][0] == 1.0f && p[1].m[0][0] == 2.0f);            // frame 10's bones, not 11's
}
COA_TEST(MotionCache_GapDropsHistory) { /* stored at 10, asked at 12 -> nullptr */ }
COA_TEST(MotionCache_HandleChangeDropsHistory) { /* stored {m1,7} at 10, asked {m1,8} at 11 -> nullptr */ }
COA_TEST(MotionCache_RejectsOversizedPalette) { /* Store(..., 86) == false; Store(..., 0) == false; Size unchanged */ }
COA_TEST(MotionCache_EndFrameEvictsUnseen) { /* m1,m2 stored at 10; only m1 at 11; EndFrame(11) -> Size()==1 */ }
```
  (`Mat(k)`: a matrix with `m[0][0] = k`.)

- [ ] **Step 2: Run to verify they fail.**  - [ ] **Step 3: Implement.**  - [ ] **Step 4: Tests pass.**
- [ ] **Step 5: Commit** — `MotionCache: previous-frame bone palettes per model`

---

### Task 4: Device targets for motion, mask and opaque snapshot

**Files:**
- Modify: `Engine/Source/Gx/CGxDevice/CGxDevice.h/.cpp`, `Engine/Source/Gx/CGxDeviceD3d/CGxDeviceD3d.h/.cpp`,
  `Engine/Source/Gx/CGxDeviceD3d9Ex/CGxDeviceD3d9Ex.h/.cpp`

**Interfaces:**
- Produces on `CGxDevice` (virtual, base: false / nullptr / no-op):
  - `virtual bool WorldMotionTargetsEnsure();` — while the world target is held, creates (or reuses at the same
    size) a `D3DFMT_G16R16F` render-target texture (motion), a `D3DFMT_A8R8G8B8` render-target texture (mask) and a
    colour-format render-target texture (opaque snapshot), all at the world target size, `D3DPOOL_DEFAULT`. False if
    any creation fails (then none are kept).
  - `virtual void* WorldMotionTexture(); virtual void* WorldMaskTexture(); virtual void* WorldSnapshotTexture();`
  - `virtual bool WorldSnapshotOpaque();` — `StretchRect(world colour → snapshot, D3DTEXF_NONE)` while begun.
  - Release: freed inside `WorldTargetRelease` (so device reset / destroy / pipeline off cover them).
  - `G16R16F` render-target support is checked once in `WorldTargetSupported`-style caching
    (`virtual bool WorldMotionSupported();`, cached like R11); unsupported → motion stays off with a log.

- [ ] **Step 1: Implement** in both device classes (`// CoA64:` comments; same ownership pattern as the world target).
- [ ] **Step 2: Build** `Wow` + `CoATests`; full suite passes.
- [ ] **Step 3: Commit** — `Gx: motion, mask and opaque-snapshot targets beside the world target`

---

### Task 5: Motion shaders

**Files:**
- Create: `CoA/Source/Render/Shaders/StaticMotionPS.hlsl`, `SkinnedMotionVS.hlsl`, `MotionPS.hlsl`,
  `BiasMaskPS.hlsl`, `MotionViewPS.hlsl`; modify the CMake embed list
- Test: `CoA/Tests/RenderMotionTest.cpp`

**Interfaces (constant/sampler contract the client uses — fixed here):**
- `StaticMotionPS` (with `FullscreenVS`): s0 = INTZ depth (point); c0..c3 = `inverse(cur.view*cur.proj)` (rows),
  c4..c7 = `prev.view*prev.proj`, c8.xyz = `cur.pos - prev.pos`, c9.xy = (depthMin, 1/(depthMax-depthMin)).
  Output `float4(prevUV - curUV, 0, 1)`. Must match `RenderMotion::StaticMotion`.
- `SkinnedMotionVS`: input = M2Vertex layout (POSITION float3 @0, BLENDWEIGHT ubyte4n @0x0C, BLENDINDICES ubyte4 @0x10;
  normal/texcoords unused). Vertex sampler 0 (`D3DVERTEXTEXTURESAMPLER0`) = palette texture
  `A32B32G32R32F` 256×2: row 0 = this frame's bones, row 1 = last frame's, bone *i* = texels 3i..3i+2 holding the
  same transposed columns the engine puts in c31+ (`CM2SceneRender.cpp:435-455`). c0..c3 = render projection (the
  device's current, possibly jittered, `m_projNative`, for `POSITION`), c4..c7 = `cur.proj` unjittered,
  c8..c11 = `prev.proj`. Outputs POSITION (render projection), TEXCOORD0 = current clip `.xyw` (unjittered
  `cur.proj`, this frame's bones) and TEXCOORD1 = previous clip `.xyw` (`prev.proj`, last frame's bones).
- `MotionPS`: divides, converts to UV, outputs `float4(prevUV - curUV, 0, 1)`.
- `BiasMaskPS` (with `FullscreenVS`): s0 = final world colour, s1 = opaque snapshot; output white where any channel
  differs by more than 2/255, else black.
- `MotionViewPS` (debug 3): s0 = motion; colour = hue from direction, brightness = `saturate(length(m) * 64)`.
  Debug 4 reuses a grey blit of the mask (may reuse `MotionViewPS` with c0.x mode switch — implementer's call).

- [ ] **Step 1: Write the failing test** — every new header's first DWORD is the right version token (vs 0xFFFE0300,
  ps 0xFFFF0300), as `RenderPipeline_EmbeddedShadersAreSm3Bytecode`.
- [ ] **Step 2–4:** fail, write the five shaders, pass.
- [ ] **Step 5: Commit** — `Shaders: static/skinned motion, translucency mask, motion view`

---

### Task 6: Motion passes (engine glue)

**Files:**
- Create: `CoA/Source/Runtime/RenderMotionClient.cpp` (+ `namespace CoA::RenderMotionClient { void OpaqueDone(); void BeforeCopyBack(); void Release(); }` declared in `RenderMotion.h`)
- Modify: `CoA/Source/Runtime/RenderPipelineClient.cpp` (calls), `Engine/Source/Gx/CGxDevice/CGxDevice.h`
  (`RenderPipelineHook` gains `void (*opaqueDone)();`), `WoW/Source/Ui/WorldFrame.cpp` (call it right after
  `CWorldScene::RenderDetailDoodads();`, null-checked like the other hook calls)

**Interfaces:**
- Consumes: Tasks 1–5; `Frame` (`cur/prev` cameras, `depthMin/Max`, `reset`, `index`); stage 1's
  `WorldDepthTexture()` / world target; M2 facts below.
- Runs only when the pipeline is active and `coaRenderMotion` is 1 and `WorldMotionSupported()`.
- `OpaqueDone()`: `WorldMotionTargetsEnsure()` then `WorldSnapshotOpaque()`.
- `BeforeCopyBack()` (called by EndWorld before `WorldTargetEnd`), saving/restoring D3D state with a `D3DSBT_ALL`
  state block as the stage 1 depth view does:
  1. **Static pass** → motion target, no depth bound, INTZ sampled; on `Frame::reset` instead clear motion to 0.
  2. **Model pass** (skipped on reset) → motion target with INTZ bound as depth-stencil (read-only: ZWRITE off,
     ZFUNC LESSEQUAL, `D3DRS_DEPTHBIAS` = -0.00002f as DWORD bits, `SLOPESCALEDEPTHBIAS` = -1.0f; tune in-game if gaps
     appear and record the final values in the report). Walk `World::s_m2Scene->m_modelList` via `m_sceneNext`;
     a model is drawn when `m_frameStamp == scene->m_frameStamp` (animated this frame), not `m_fFlags & 0x2000`
     (culled), not flag 0x1000 static (`AnimateMTSimple`) unless its world matrix changed since last frame (compare
     the first bone of the cached palette is not enough — cache `m_worldTransform` alongside, in MotionCache or a
     local map), and `MotionCache::Previous` returns bones. For each opaque batch element of that model in
     `scene->m_passElements[0]` (type 0 only; skip `m_flags & 4` optimized geometry with a one-time log): upload the
     batch's palette (`m_boneCombos[boneComboIndex + i]` → `m_boneMatrices[...]`, this frame) and the same indices
     from the cached previous palette into the 256×2 palette texture (dynamic, `D3DLOCK_DISCARD`), re-issue the
     batch geometry with `shared->SetIndices()` / `SetVertices(0)` and the batch range from `CM2SceneRender::DrawBatch`
     (`CM2SceneRender.cpp:470-478`) but with our VS/PS and our vertex declaration. **Palette bounds:** a batch whose
     bone count exceeds `MotionCache::kMaxBones` is skipped with a one-time log.
  3. **Mask pass** → mask target from final world colour vs snapshot (`BiasMaskPS`).
  4. After the passes: for every model drawn or eligible this frame, `MotionCache::Store` this frame's `m_worldTransform` and full
     `m_boneMatrices` (count = the model's bone count; models above `kMaxBones` not stored), then
     `MotionCache::EndFrame(frame.index)`. `Release()` (hooked into the stage 1 release path) clears the cache and
     frees the palette texture, vertex declaration and shaders.
  5. Debug: with `coaRenderDebug 3` / `4`, after `WorldTargetEnd` draw `MotionViewPS` over the frame (as the stage 1
     depth view does) from the motion / mask texture.
- The model key handle: `CM2Model::m_handle` (confirm the member; fall back to the model's `m_shared` pointer if no
  handle exists, and note it in the report).

- [ ] **Step 1: Write the failing test** for the one pure piece here: `RenderMotion_ModelEligibility` — factor the
  "is this model drawn in the model pass" rule into `bool RenderMotion::ModelNeedsPass(bool animatedThisFrame, bool culled, bool staticSimple, bool worldChanged, bool havePrevious)` in RenderMotion.cpp and test its truth table
  (drawn only when animated && !culled && havePrevious && (!staticSimple || worldChanged)).
- [ ] **Step 2–4:** fail; implement `ModelNeedsPass`, `RenderMotionClient.cpp`, the hook and the WorldFrame call;
  build `Wow` + `CoATests`; suite passes.
- [ ] **Step 5: Commit** — `RenderMotion: static, model and translucency-mask passes behind coaRenderMotion`

---

### Task 7: In-game proof (controller) and docs

**Files:**
- Modify: `CoA/DIFFERENCES.md` (extend the stage 1 entry: `coaRenderMotion`, debug 3/4)

- [ ] **Step 1 (controller, CoA-dev):** with `coaRenderPipeline 1`, `coaRenderMotion 1`:
  - standing still, camera still: motion view (debug 3) black everywhere except animated NPCs/idle animations.
  - camera turning (`MoveViewLeftStart`): uniform smooth pattern over static geometry, no seams at model edges.
  - nearby NPCs walking and the player turning in place: models show their own motion distinct from the background.
  - translucency mask (debug 4): white on particles/spell effects/weather/glow cards, black on opaque geometry.
  - colour image with motion on vs off: static-pixel check (`static_check.py` off/on/off) shows no change on static
    pixels (the passes must not touch the world colour).
  - resize ×3 and `RestartGx` with motion on: no crash; cost check (motion on vs off) recorded.
- [ ] **Step 2:** DIFFERENCES entry; commit `RenderPipeline stage 2: DIFFERENCES entry`.
- [ ] **Step 3:** Merge only with the user's consent (finishing-a-development-branch).
