# Render pipeline notes (2026-10-29)

Rationale moved out of inline comments (comment budget). Each section names the file and the code line it documents.

## DlssClient.cpp #1

Before `#include "CoA/Source/Runtime/Dlss.h"`:

DLSS Super Resolution in CoA64's render pipeline (stage 3): the world renders at Streamline's render size, then every
world frame the depth remap pass writes the D3D-native depth DLSS is tagged with, and ID3D9VkInteropDevice1::
RecordCallback records Streamline's frame token, constants, tags and slEvaluateFeature into DXVK's own command stream
(the callback runs later on DXVK's CS thread with DXVK's VkCommandBuffer; the per-frame data travels in its user
blob, Dlss::FrameBlob). The display-size output replaces the stretched copy-back (CGxDevice::WorldTargetEndWith).
slDLSSGetOptimalSettings / slDLSSSetOptions / slFreeResources run on the main thread, only after DXVK's command stream
has drained, so no Streamline call ever overlaps another. Not part of the original (CoA64 addition).

## DlssClient.cpp #2

Before `bool EnsureTextures(IDirect3DDevice9* device, uint32_t renderW, uint32...`:

The remap target (render size) and the output (display size, created through DXVK with storage usage). The output
is A8R8G8B8 (VK_FORMAT_B8G8R8A8_UNORM); A16B16G16R16F if the driver refuses that format as a storage image.
fresh: the output was just created (its contents are undefined).

## DlssClient.cpp #3

Before `bool RemapPass(CGxDevice* gx, IDirect3DDevice9* device, IDirect3DBaseT...`:

The depth remap pass (render size): INTZ depth -> saturate(stored * scale + offset) in the R32F target. Raw D3D9 as
the motion passes (RenderMotionClient.cpp): the same plain states, all re-sent afterwards by ExternalStateRestore,
and the world target's surfaces put back. False: nothing drawn.

## DlssClient.cpp #4

Before `bool s_loggedFill;`:

coaRenderDebug 8 / 9: the output filled with magenta (8: before the evaluate, so magenta on screen means
Streamline wrote nothing and black that it wrote black; 9: instead of it, so magenta proves the copy to the back
buffer). The first fill logs its HRESULTs and the output surface's description.

## RenderMotion.h #1

Before `Vec2 StaticMotion(const RenderPipeline::Frame& f, float u, float v, fl...`:

Static pass: motion of a static world point seen at pixel (u, v) (0..1, v down) with the stored depth, as prevUV - curUV.
The stored depth is what the device writes: minZ + ndcD3D * (maxZ - minZ) with ndcD3D = (ndcGL + 1) / 2, where ndcGL
is the clip z / w of Frame.proj (GL-style, -1..1). A stored depth >= depthMax (sky / clear) is a direction at
infinity: only the rotation applies. An empty depth range returns (0, 0). Camera-relative: the unprojected point is
relative to the current eye and is moved into the previous eye's frame with cur.pos - prev.pos.

## RenderMotion.h #2

Before `namespace CoA::RenderMotionClient {`:

The motion passes' engine glue (RenderMotionClient.cpp), driven by RenderPipelineClient while the world target is
begun. Every entry point does nothing unless coaRenderMotion is 1 (or DLSS is wanted) and the device has the motion
targets.

## RenderMotionClient.cpp #1

Before `#include "CoA/Shaders/BiasMaskPS.h"`:

RenderMotion's engine half: the motion passes of CoA64's render pipeline (stage 2), run by RenderPipelineClient while
the world target is begun. OpaqueDone snapshots the opaque world; BeforeCopyBack draws the static (camera) motion,
the skinned-model motion and the translucency mask into the device's motion targets, then records this frame's bone
palettes for the next frame. Raw D3D9; afterwards CGxDevice::ExternalStateRestore re-sends every plain state the
passes touch from the device's own caches and marks the cached bindings dirty for the next engine draw (the D3D9
device is a pure device: no Get* of state), so the engine's state and its caches agree again. Nothing here runs
unless coaRenderMotion is 1 or DLSS is wanted. Not part of the original (CoA64 addition).

## RenderMotionClient.cpp #2

Before `constexpr float kDepthBias = -0.00002f;`:

The model pass's depth test against the world's INTZ (read-only, LESSEQUAL): the re-drawn batches land on the
depth the world wrote for them, pulled slightly towards the eye so equal depths pass. D3DRS_DEPTHBIAS and
D3DRS_SLOPESCALEDEPTHBIAS take the float's bits.

## RenderMotionClient.cpp #3

Before `Mat4 NativeProjection(const Mat4& matrix) {`:

The device's native projection for a GL-style one, exactly as CGxDeviceD3d::XformSetProjection derives
m_projNative (c3 normalised to 1, z remapped to D3D's [0, 1]). The 0.2 scale it applies without
GxMasterEnable_NormalProjection is not reproduced: the passes do not run in that state.

## RenderMotionClient.cpp #4

Before `void PointClamp(IDirect3DDevice9* device, DWORD sampler) {`:

Point / clamp sampling. Only the sampler states the engine itself manages are set (ExternalStateRestore re-sends
them); ADDRESSW is CLAMP on every sampler (IStateSetD3DDefaults) and SRGBTEXTURE / MAXMIPLEVEL are never changed
from their D3D9 defaults (0) by the engine, so they already hold what the passes need. MIPMAPLODBIAS may hold the
world's LOD bias (CGxDevice::WorldLodBiasSet, while the world renders below display size), which changes nothing
here: every texture the passes sample has a single level and is point-sampled.

## RenderMotionClient.cpp #5

Before `void CommonStates(IDirect3DDevice9* device) {`:

The render states every pass shares; exactly the set ExternalStateRestore re-sends. Stencil, sRGB write and
WRAP0 are never changed from their D3D9 defaults by the engine, fixed-function fog does not apply to ps_3_0, and
FILLMODE only differs in the wireframe debug mode, so none of those are touched.

## RenderMotionClient.cpp #6

Before `constexpr uint32_t kVsConstantsUsed = 13;`:

The highest shader constant registers the passes write (+1): VS c0..c12 (SkinnedMotionVS; FullscreenVS uses c0),
PS c0..c9 (StaticMotionPS; MotionPS / MotionViewPS use c0). ExternalStateRestore re-sends that many from the
engine's shadow constants.

## RenderMotionClient.cpp #7

Before `bool PackRow(float* row, const M2Data* data, const M2SkinSection* sect...`:

Packs one palette row: slot i = model bone m_boneCombos[boneComboIndex + i], 3 texels holding the engine's c31+
registers for it (texel k = column k of the matrix: a_k, b_k, c_k, d_k; CM2SceneRender::DrawBatch). False when a
slot names a bone outside the array.

## RenderMotionClient.cpp #8

Before `bool PackBatches(CGxDevice* gx, IDirect3DTexture9* palette) {`:

Phase 2: one D3DLOCK_DISCARD lock of the whole palette level (DXVK ignores DISCARD on a sub-rect lock and may stall
on last frame's upload), only the rows in use (2 per found batch) written, both rows of every batch packed, then
s_batches built for the draws. False when the lock fails.

## RenderMotionClient.cpp #9

Before `void ModelPass(CGxDevice* gx, IDirect3DDevice9* device, IDirect3DTextu...`:

2. Model pass: the opaque batches of every model that moved on its own (skinned, or a static model whose world
transform changed) re-drawn with SkinnedMotionVS / MotionPS, depth-tested against the world's INTZ. One palette
upload per frame (FindBatches / PackBatches), then one draw per batch with its palette row base in c12.x; stream
0 and the index buffer are only re-bound when they change.

## RenderMotionClient.cpp #10

Before `const Mat4 renderProj = NativeProjection(ApplyJitter(frame.cur.proj, f...`:

c0..c3: the projection the world rasterised with (the jittered cur.proj in the device's native form), so the
depth test lines up; c4..c7 / c8..c11: the unjittered GL-style cur.proj / prev.proj (the motion uses .xyw,
which the native form leaves the same up to a common factor of w). c12.x (per batch): palette row base.

## RenderMotionClient.cpp #11

Before `void StoreBones(const Frame& frame) {`:

4. This frame's bones and world transform of every model of this frame's opaque elements (s_frameModels) that was
animated this frame and is not culled (drawn in the model pass or not), for next frame's model pass; models above
MotionCache::kMaxModelBones are not stored.

## RenderPipelineClient.cpp #1

Before `#include "CoA/Shaders/DepthViewPS.h"`:

RenderPipeline's engine half: the g_renderPipelineHook callbacks (CGWorldFrame::OnWorldRender begin/end,
CGCamera::SetupWorldProjection), the activation rules' inputs and the device's world target. The hook is installed
only while coaRenderPipeline is 1 or DLSS is wanted (coaDlssMode not 0 with Streamline ready, which forces the pipeline
and the motion passes on without rewriting their CVars), so otherwise the engine runs its original paths. With DLSS
the world renders at Streamline's render size and its output replaces the copy-back (DlssClient.cpp). Not part of the
original (CoA64 addition).

## RenderPipelineClient.cpp #2

Before `int32_t s_lastFrame;`:

Device frame (CGxDevice::m_frameCount) and size of the last active world render: s_width x s_height is the
display (window) size, s_renderWidth x s_renderHeight the world target's (the display size unless coaRenderScale
makes it smaller).

## RenderPipelineClient.cpp #3

Before `const Frame& frame = Current();`:

c0.x: 1 = depth view, 2 = diagnostic (uv gradient, blue where the depth is still at the clear value).
c1 = (viewport minZ, 1 / (maxZ - minZ)): the Z buffer holds minZ + ndc * (maxZ - minZ). c2 = (near * far, far,
far - near, 1 / 16) linearises the NDC depth to yards for the greyscale 1 / (1 + yards / 16).

## RenderPipelineClient.cpp #4

Before `void OnWorldTargetRelease() {`:

The device is releasing its D3DPOOL_DEFAULT resources (reset / destroy): DLSS's Streamline feature and textures
(first: slFreeResources once DXVK's stream has drained), the debug objects go, and a failed world target creation
may be tried again on the new device.

## RenderPipelineClient.cpp #5

Before `if (!(dlss || CVarOn(CoA::RenderPipelineClient::JitterSlot())) || memc...`:

Jitter only the world camera of a frame that is rendering into the world target: the device must still hold
the projection SetupWorldProjection just set for it (unchanged, so not a hit test's or another camera's).
CShaderEffect::UpdateProjMatrix and the world draws run after this, so they all see the jittered one. DLSS
always jitters (coaRenderJitter only gates it without DLSS).

## RenderPipelineClient.cpp #6

Before `void* dlssOutput = s_dlss ? CoA::DlssClient::Evaluate(device, Current(...`:

DLSS: the depth remap pass and Streamline's evaluate recorded into DXVK's command stream; its display-size
output is copied to the back buffer instead of the world. Nullptr (any failure) falls back to the stretched
copy-back for this frame. coaRenderDebug 7 still evaluates (DLSS's history stays continuous) but shows the
raw render-size world stretched, for A/B; 8 shows the output as it is after a magenta fill before the
evaluate (magenta: Streamline wrote nothing), 9 the magenta fill alone, without evaluating (the copy path).

## RenderPipelineClient.cpp #7

Before `copied = device->WorldTargetEnd(0, 0, static_cast<int32_t>(s_width), s...`:

The whole target, not only the viewport: it started as a copy of the back buffer and the
partial-viewport GxSceneClear cleared all of it, so a full copy reproduces the original back buffer
exactly. A world target smaller than the display (coaRenderScale, DLSS's render size) is stretched over
the whole window (linear).

## RenderPipelineClient.cpp #8

Before `if (debug >= 3 && debug <= 6) {`:

coaRenderDebug 1 / 2: the depth view is drawn over the copied world (the same full rect), so a failure
leaves the normal image; 3 / 4: the motion / translucency mask view, 5: the motion view with the model pass
writing a constant motion (its coverage), 6: the motion view 16x brighter (nothing without coaRenderMotion
1); 7 / 8 / 9: DLSS's A/B and probe views (above). The UI draws on top afterwards as usual.

## RenderPipelineClient.cpp #9

Before `bool OnPipeline(CVar* /* var */, const char* /* oldValue */, const cha...`:

coaRenderPipeline's callback; also runs at registration with the value loaded from Config.wtf, so a 1 there
installs the hook at startup. 1 installs; 0 releases the world target and uninstalls at once outside a world
render, else (begun) at that render's EndWorld.

## RenderPipelineClient.cpp #10

Before `bool OnDlssMode(CVar* /* var */, const char* /* oldValue */, const cha...`:

coaDlssMode's callback (also at registration, after the device exists, so Streamline's readiness is known): a
non-zero mode with Streamline ready installs the hook like coaRenderPipeline 1; 0 uninstalls unless
coaRenderPipeline is 1 (outside a world render; else at that render's EndWorld). With the pipeline staying on,
DlssClient::Begin frees Streamline's feature at the next world render.

## Dlss.h #1

Before `namespace CoA::Dlss {`:

Pure logic for CoA64's DLSS integration (stage 3, via NVIDIA Streamline): modes, render sizes, texture LOD bias, the camera
matrices Streamline asks for, and the depth remap. Not part of the original (CoA64 addition).

Matrix layout: RenderPipeline::Mat4 is row-vector (D3D) layout, clip = (p, 1) * M, 16 contiguous floats row by row.
sl::float4x4 is four sl::float4 rows with the same row-vector convention in the SDK's own formulas
(clipToPrevClip = clipToView * viewToViewPrev * viewToClipPrev), so a Mat4 copies into an sl::float4x4 as-is, without
a transpose (memcpy of 16 floats).

## Dlss.h #2

Before `struct SlMatrices {`:

The matrices of sl::Constants (row-major, no jitter). The world is camera-relative (the view has no translation).
Clip z is D3D-native [0, 1] (near 0, far 1), matching the depth tagged to DLSS (see DepthRemapFor), as the SDK requires
the depth to suit clipToPrevClip; x, y and w are the recorded GL-style projection's.

## Dlss.h #3

Before `SlMatrices BuildMatrices(const RenderPipeline::Frame& f);`:

clipToPrevClip = Z^-1 * inverse(cur.view * cur.proj) * T(cur.pos - prev.pos) * (prev.view * prev.proj) * Z: the same math
as RenderMotion::StaticMotion, in D3D clip depth. Equals Streamline's clipToView * viewToViewPrev * viewToClipPrev with
viewToViewPrev = inverse(cur.view) * T(cur.pos - prev.pos) * prev.view.

## Dlss.h #4

Before `struct DeviceDeps {`:

Streamline's Vulkan device across D3D device creation and release (DXVK's device callback, the engine's
g_d3dDeviceReleaseHook). Streamline is handed one Vulkan device at a time and shut down only when exactly that
device is released; the next device then initialises it again first.

## Dlss.h #5

Before `void DeviceCreated(const DeviceDeps& deps, uint64_t device);`:

A device callback (VkDevice as an integer). Ignored unless PreDevice succeeded. After a DeviceReleasing shutdown:
init first. While Streamline still holds another device: DLSS off (no second slSetVulkanInfo). A device that cannot
be set or does not support DLSS shuts Streamline down for the rest of the process.

## Dlss.h #6

Before `void BeforeDeviceRelease(void* d3dDevice);`:

g_d3dDeviceReleaseHook: the D3D device (IDirect3DDevice9*) is about to be released (device re-creation, client
shutdown). Releases DLSS's resources, drains DXVK, then slShutdown while the Vulkan device still exists; the next
device callback initialises Streamline again.

## Dlss.h #7

Before `bool Begin(CGxDevice* device, uint32_t displayW, uint32_t displayH, ui...`:

BeginWorld, once the pipeline is active, before the world target begins: decides whether DLSS evaluates this
world render. Applies a new mode or display size first (on the main thread, with DXVK's command stream drained:
slFreeResources, slDLSSGetOptimalSettings, slDLSSSetOptions). True: renderW x renderH is the world's size and the
world camera is always jittered.

## Dlss.h #8

Before `void* Evaluate(CGxDevice* device, const RenderPipeline::Frame& frame, ...`:

EndWorld, after the motion passes, before the copy-back: the depth remap pass, then Streamline's tags, constants
and evaluate recorded into DXVK's command stream. Returns the display-size DLSS output texture for
CGxDevice::WorldTargetEndWith, or nullptr when this frame must use the stretched copy-back (logged once per streak).
debug (coaRenderDebug): 8 fills the output with magenta before the evaluate and returns it whatever the CS thread
reported (magenta on screen: Streamline wrote nothing; black: it wrote black); 9 fills it with magenta and returns
it without any evaluate (magenta proves the copy to the back buffer).

## StreamlineClient.h #1

Before `#include <vulkan/vulkan.h>`:

The Streamline functions CoA64 calls (sl.interposer.dll, NVIDIA Streamline SDK). Needs the SDK and Vulkan headers
(CoA's streamline_headers / Vulkan include paths), so only glue files include it. Not part of the original (CoA64
addition).

## CGxDevice.h WorldRenderSize

Before `virtual void WorldRenderSizeBegin(uint32_t width, uint32_t height);`:

CoA64: render pipeline world render size -- while begun, the default window rect is {0, 0, width, height} (the
world target's size) instead of the display's, so everything sized from the window during the world render
(DeviceCurWindow / CapsWindowSize, viewport, scissor rect, point scale) works in render pixels, also after a
world-internal SetRenderTarget(nullptr). End restores the display rect. Both mark the viewport, scissor rect and
point scale for re-sending. A Begin while begun only changes the size; an End while not begun does nothing.

## CGxDevice.h (b) #1

Before `virtual bool WorldTargetSupported();`:

CoA64: render pipeline world target -- an off-screen colour target + sampleable depth texture swapped in as the
default surfaces while the 3D world renders, then copied back to the back buffer. The base device has none
(Supported/Begin return false, the rest do nothing).

## CGxDevice.h (b) #2

Before `virtual bool WorldMotionSupported();`:

CoA64: render pipeline motion targets beside the world target -- motion vectors (G16R16F), translucency mask
(A8R8G8B8) and an opaque snapshot (colour format), all at the world target size and freed with it. The base
device has none (Supported/Ensure/Snapshot return false, the textures are nullptr).

## CGxDevice.h (b) #3

Before `virtual void WorldLodBiasSet(float bias);`:

CoA64: render pipeline texture LOD bias -- while non-zero, every texture stage the device binds also gets
D3DSAMP_MIPMAPLODBIAS = bias (the stages bound now at once); 0 puts every stage back to the D3D9 default (no
bias). The base device has no bias.

## CGxDevice.h (b) #4

Before `virtual void ExternalStateRestore(uint32_t vsConstants, uint32_t psCon...`:

CoA64: puts the D3D9 states the render pipeline's raw D3D9 motion passes change (render targets excepted) back in
step with the device's own state caches: plain values and the first vsConstants / psConstants shader constants are
re-sent, cached bindings (textures, shaders, geometry) are marked dirty for the next engine draw. The base device
has nothing to restore.

## CGxDevice.h (b) #5

Before `virtual bool WorldTargetEndWith(void* texture);`:

CoA64: as WorldTargetEnd, but the back buffer gets the given display-size texture (IDirect3DTexture9*, DLSS's output)
instead of the world colour: the original default surfaces are restored, then the texture is copied 1:1 over the
back buffer (both clamped to the smaller, D3DTEXF_NONE). Not begun: no-op (true). False when the copy failed or
the texture is null (the originals are restored either way). The base device has no world target.

## CGxDevice.h (b) #6

Before `extern void (*g_d3dDeviceReleaseHook)(void* d3dDevice);`:

CoA64: called by both D3D9 devices' IDestroyD3d (device re-creation and client shutdown) right before the
IDirect3DDevice9 is released, with that device, so Streamline can drain DXVK, free its resources and shut down while the
Vulkan device still exists.

## DlssFrame.h (b) #1

Before `#include "CoA/Source/Runtime/Dlss.h"`:

The per-frame data DlssClient hands to DXVK's RecordCallback as its user blob (copied by DXVK at the call, read by the
callback later on DXVK's CS thread): Streamline's sl::Constants followed by the frame index, viewport, DLSS mode and
sizes. Header-only and Vulkan-free (sl_consts.h only) so the layout test can include it. Not part of the original
(CoA64 addition).

## DlssFrame.h (b) #2

Before `struct FrameBlob {`:

Byte layout (x64, SDK 2.14.16): sl::Constants at 0 (its BaseStructure `next` must stay nullptr), then eight
uint32_t: frameIndex, viewport, mode (sl::DLSSMode value), renderWidth, renderHeight, displayWidth,
displayHeight, generation (DlssClient's release counter; a callback of an older generation does nothing).

## DlssFrame.h (b) #3

Before `inline void FillFrameBlob(FrameBlob& b, const RenderPipeline::Frame& f...`:

Global Constraints: row-major matrices without jitter (BuildMatrices), jitterOffset in pixels times the sign,
mvecScale = (sign, sign) for the UV-unit motion (prevUV - curUV), camera motion included, depth not inverted,
motion not jittered, 2D motion, camera vectors and planes from Frame::cur.

## Frame Generation hang (2026-10-29)

Symptom: with `coaFrameGen` on, about 1-5 s after Frame Generation turns on in the world Streamline logs
"Pacer flush has timed out" and the client stops responding (GPU idle).

Evidence:
- User dumps (cdb): `nv.sl.dlss_g.thread.pacer` in `NtDxgkSubmitPresentToHwQueue` holding an nvoglv64 lock;
  `nv.sl.dlss_g.thread.dlssg` and the main thread (DXVK `createImageResource`) wait on that lock.
- Kernel stacks (LiveKD): the pacer's present waits in `dxgkrnl!DXGDEVICE::FlushScheduler` ->
  `dxgmms2!VidSchFlushDevice`, i.e. for all GPU work of the device.
- PresentMon: every present of CoA64 is "Composed: Copy with GPU GDI", also with Streamline off, in fullscreen,
  without `CS_OWNDC`, and with the NVIDIA profile setting "Vulkan/OpenGL present method = Prefer layered on DXGI
  Swapchain" (0x20D690F8 = 1) written for CoA64.exe.
- `sl_dlss_g.h`: the default `DLSSGQueueParallelismMode::eBlockPresentingClientQueue` blocks the client's presenting
  queue until the DLSS-G workload (submitted by the dlssg thread) completes.

Cycle: copy present flushes the device -> DXVK's queue waits on DLSS-G work -> the dlssg thread waits for the
driver lock held by the flushing present.

Ruled out (each run still hung): overlay layers (OBS/Wallpaper Engine, Cheat Happens), `slDLSSGGetState` off the
present thread (now on DXVK's present thread anyway), a >100 ms hitch, DLSS (FG alone hangs), DXVK semaphore reuse,
DXVK acquire-ahead and GPU waits on Streamline's acquire, VSync, fullscreen.

`eBlockNoClientQueues` avoids the hang but no frames are generated (54 presents/s, all real). DLSS-G needs a flip
present; until the client gets one on NVIDIA's Vulkan path (or runs through D3D12), `FlipPresent()` reports false and
Frame Generation stays off.
