# Render pipeline stage 5 — in-game proof (2026-10-29)

RTX 5080 (NVAPI arch 0x1B0), driver 621.01, `D:\WoW\CoA-dev`, driven by the CoATest addon (`eval` / `shot`).

| Check | Result |
|---|---|
| Dropdowns built on `VideoOptionsResolutionPanel` (titled "Display" in this client) | DLSS at (199,-154) next to Multisampling, Frame Generation (399,-98), NVIDIA Reflex (399,-154); no overlap |
| State, all options Off at start | DLSS / Reflex enabled, Frame Generation greyed "Not available yet", `restart = true` |
| Choosing DLSS Quality (real menu click) | pending 2, red "Takes effect after restarting the game" under DLSS, Multisampling greyed |
| Cancel | `coaDlssMode` stays 0, the dropdown returns to the saved value |
| Okay | `coaDlssMode` 3 saved, `gxMultisample` 1 |
| Default | all three CVars 0 |
| Started with `coaDlssMode 2` (Streamline loaded), Balanced + Okay | no restart note; log `DLSS: active (Quality ...)` then `DLSS: active (Balanced, 2560x1440 from 1485x835)` 9 s later |
| `-nodxvk` | all three greyed "Requires the Vulkan renderer" |

Not from this change: the "Prat-3.0 has been blocked" popup (taint.log: Prat-3.0 in `NewCharacterSetupUtil.lua`)
also appears in a run before the dropdowns existed.

Off means unchanged: stage 5 touches no render path (option state, the Lua native, the injected panel code, and
`StreamlineClient::FlipPresent` moved into `Dlss.cpp`), so no screenshot comparison was run.
