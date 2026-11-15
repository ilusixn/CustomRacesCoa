# Graphics pack switch (2026-11-15)

One choice decides which graphics pack the client runs and which device draws it:

| Value | Pack | Device |
|---|---|---|
| 0 | Standard | whatever `gxApi` says (D3D9 by default) |
| 1 | Lumenveil (Lumenveil64.dll) | D3D9 (the DLL hooks D3D9; a Vulkan `gxApi` is overridden) |
| 2 | LuxVelum | D3D9 (same reason) |
| 3 | Forever (Azeroth Remastered HD layer from wowdecomp) | Vulkan, if the machine supports it |

## Where the choice lives

- `Visuals.txt` beside the exe (first number in the file), or `-visuals <n>` on the command line, which wins.
- Without either, Lumenveil (1) stays the default, as before.
- The file is read in `CoA::Visuals::Start()` (WinMain) before `main()`, so before Config.wtf exists in memory. That is why
  the choice is not an ordinary saved cvar: the device and the DLL hooks are decided before Config.wtf is read.

## In the game

- Video options → Graphics (the Effects panel): dropdown "Graphics: …" (`CoAVisualsDropDown`,
  WoW/Source/Ui/VisualsOptions.cpp), bottom left under the Global Settings box. wowdecomp's spot (left of Player
  Textures) lands on top of Texture Filtering in CoA's three-column layout.
- Okay sets the unsaved cvar `coaVisuals`, whose callback rewrites `Visuals.txt`. A red note says the change applies after a
  client restart; nothing switches live.
- The launcher may keep writing `Visuals.txt` as before; the values 0–2 mean the same as they always did.

## How the HD layer follows it

- wowdecomp's `visuals` cvar (0 stock, 1 native Lumenveil, 2 Azeroth Remastered) is gone. `Visuals::Current()` in
  WoW/Source/Ui/Visuals.h now returns `AzerothRemastered` only when the run started with pack 3 *and* the device is
  Vulkan; in every other case the HD passes see `Stock` and stay off.
- The device is picked by `Visuals::PickApi()` in ConsoleDevice.cpp (device creation) and VideoOptions_Scripts.cpp
  (multisample formats). Command-line API flags (`-d3d`, `-vulkan`, …) are applied after it and still win.
- wowdecomp's native Lumenveil option pages were not taken over: our Lumenveil64.dll keeps its own hotkeys (Ctrl+F12,
  Ctrl+F7).

## Not here yet

- DLSS / Frame Generation / Reflex: not in any wowdecomp branch. Ilu's `render-pipeline` branch has the groundwork
  (off-screen world, depth, motion vectors) on the DXVK path; the Streamline part is unpublished.
