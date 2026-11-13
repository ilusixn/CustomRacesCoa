# DXVK from Vulkan\ (2026-11-13)

CoA Custom 2.0 moves DXVK to `<game>\Vulkan\d3d9.dll` (next to `VulkanShaders\`), where 1.9.x kept it in
`<game>\x64\d3d9.dll`. `CoA::Dxvk::Preload` now tries `Vulkan\d3d9.dll` first and falls back to `x64\d3d9.dll`, so a
2.0 install and a 1.9.x install both get their Vulkan renderer, and a stale `x64` copy no longer wins over the
release's own. A missing pair still falls back to the system d3d9, as before.
