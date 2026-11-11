# HD without DXVK (2026-11-11)

The official CoA64 opens its eight HD archives and the HD model tables only after `x64\d3d9.dll` (DXVK) has loaded.
Where DXVK does not load (no Vulkan, an old driver, an antivirus, a 32-bit DLL in the x64 folder) the client
silently falls back to system D3D9 and drops HD: the races are back on their old models.

AFK Realm has patched that check out of the official exe for some time (`Coa64HdPatch`), and HD runs on system
D3D9 there. `CoA::Hd::Available()` now does the same in the source: HD is on unless `-hd 0`. `SetVulkanRenderer`
stays, for code that wants to know which renderer runs.
