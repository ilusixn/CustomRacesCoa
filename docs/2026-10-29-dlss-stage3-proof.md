# DLSS stage 3 in-game proof (2026-10-29)

RTX 5080, driver 621.01, `D:\WoW\CoA-dev`, 2560x1440 (maximised), vsync off, `maxFPS 0`, Goldshire (busy, players
passing). Driven by the dev-only `CoATest` addon. Build: `render-pipeline` after the main merge (6900a49) plus the
device-destroy fix.

## Results

| Check (plan Task 7) | Result |
|---|---|
| `coaDlssMode 0` from startup loads nothing | Pass: only `x64\d3d9.dll` in the process, no `sl.*` / `nvngx_*` |
| DLAA / Quality / Performance / Ultra Performance render | Pass: correct framing, sharp UI, nameplates in place |
| Live mode switches 2 → 0 → 1 → 4 → 5 → 2 and 2 → 4 → 1 → 0 → 2 | Pass, no crash |
| Resolution changes (x3) and `RestartGx` (x4) | Pass: each device cycle logs slShutdown, slInit, DLSS active at the new size |
| Sign defaults (`coaDlssMvecSign`, `coaDlssJitterSign`) | Kept at 1, see below |
| FPS higher than native | Not met in this scene: CPU-bound, see below |
| Streamline log clean | Pass, see below |

## Signs

- **Motion vectors:** `prevUV - curUV` with v pointing down (`RenderMotion.cpp`) is DLSS's convention (vector from
  the current pixel to its previous position, UV units, `mvecScale` 1). While turning, `mvec -1` with `jitter +1`
  smeared the ground visibly; `+1/+1` did not.
- **Jitter:** a still camera gives near-identical images for +1 and -1 (frame-to-frame difference 0.26-0.33 for +1,
  0.38-0.39 for -1, 8-bit grey mean). The SDK only says "pixel space". +1 stays.

## Performance

GPU utilisation was about 44% throughout (nvidia-smi, 500 ms samples), so this scene is CPU-bound and DLSS Super
Resolution cannot raise the frame rate here. FPS also swings with the number of players nearby (runs differed by up
to 30% at the same settings), so only back-to-back samples compare.

- A/B, 3 rounds, 6 s each: off 256-289 FPS, DLAA 227-233, Quality 208-238.
- One foreground round of the cost breakdown: off 268, pipeline only 258, pipeline + motion pass 208. The motion pass
  is the dominant cost (about 1 ms per frame, CPU side: the extra model draws), which DLSS forces on.
- A gain is expected only where the GPU is the bottleneck (higher resolution, weaker GPU); not shown yet.

## Streamline log

Nothing from our integration. Remaining lines:

- NGX lists models for unrelated features (vsr, truehdr, ...) as not cached.
- `VK_EXT_debug_utils` is not enabled, so there are no debug names.
- `sl.common` reports that the `CmdBindPipeline` / `CmdBindDescriptorSets` / `BeginCommandBuffer` hooks are not
  supported. This is harmless: the evaluate runs through DXVK's `beginExternalRendering()`, which resets DXVK's state
  tracking, so DXVK re-binds everything itself.

## Fixed during the proof

A DXVK device created for a D3D9 device whose `InitialReset` then failed was destroyed without the engine's release
hook, so Streamline kept its hold. The next device was then refused ("still holds the old one") and DLSS stayed off for
the process. `streamline.patch` now reports `~DxvkDevice` (idle, before the VkDevice goes) through
`CoaDxvkSetDeviceDestroyCallback`, which frees the hold the same way as the release hook.
