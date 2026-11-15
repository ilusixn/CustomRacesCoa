# Vulkan backend from Esteria's wowdecomp (2026-11-15)

First step of bringing Lumenveil (native) and Azeroth Remastered ("Forever") into CoA64. Both are built on the
Vulkan render device of Esteria's wowdecomp (`STRHercules/wowdecomp`, branch `AFKRealm`, `51b9d02`). The authors
(Zach for Esteria, ilusixn for CoA Custom 2.0) agreed to their use in AFK Realm.

## Where CoA64 meets wowdecomp

CoA64's `Engine` and `WoW` trees match wowdecomp best at `b1c71b8` (2026-10-17): 2264 of 2840 files are the same
blob. That commit is the base of the three-way merges below. From it to `51b9d02` wowdecomp has 168 commits (556 new,
190 changed files); this step takes only the render device.

## What came over

- `Engine/Source/Gx/CGxDeviceVulkan/` (the device, its Vulkan tier and GLSL), `Engine/Source/Gx/CGxDeviceD3d11/`
  (the Vulkan device uses its D3D9 shader translator), `Engine/Source/Gx/GxEffects.h`.
- `ThirdParty/vulkan-headers` and the `VulkanHeaders` target. No Vulkan SDK is needed: `vulkan-1.dll` is loaded at run time.
- Three-way merged with CoA64's own changes: `Gx.cpp`, `Gx.h`, `Types.h`, `CGxDevice*`, `CGxDeviceD3d*`,
  `CGxDeviceOpenGl.cpp`, `Gx/CMakeLists.txt`, `Base/CmdLine.cpp`, `Base/Types.h` (`-vulkan`, `-d3d11`),
  `ConsoleDevice.cpp`, `VideoOptions_Scripts.cpp`, `ShaderEffect/ShadowCache.cpp`. The root and `ThirdParty`
  CMake files were merged by hand: CoA64 keeps its Retail switch and tests, wowdecomp's `Extensions` stay out.
- CoA64 renamed four enumerators that the imported files used under their old names:
  `GxTexGen_EyeLinear` -> `GxTexGen_World`, `GxTexGen_Unk3` -> `GxTexGen_View`, `Unk84` -> `GxRs_BlendFactor`,
  `GxTex_3` -> `GxTex_Recreate`.
- Comments longer than two lines were shortened to the 2-line budget; the full notes are in wowdecomp at `51b9d02`.

## Behaviour

Direct3D 9 stays the default. With retail assets on, wowdecomp's `GxDefaultApi` returns Vulkan; CoA64 does not, so
LuxVelum and the DXVK path keep working. Vulkan runs with `-vulkan` or the `gxApi` cvar set to `Vulkan`. It needs
the `VulkanShaders` folder beside the exe (CoA Custom 2.0's client root carries it).

Next: the `visuals` choice picks the device (Lumenveil and Forever on Vulkan, stock and LuxVelum on D3D9), with a
drop-down on the Effects page.

## 2026-11-15: missing body parts on Vulkan (skin profile)

`CM2Shared::Initialize` chooses the skin profile from the device's vertex-constant cap: `(constants - 64) / 3` bones. D3D9
(and DXVK) report 256, so models load the 64-bone profile. wowdecomp's Vulkan backend reported 1024, which picks the
256-bone profile. CoA's `CM2SceneRender::DrawBatch` uploads at most 75 bones per section (the D3D9 register file), so every
section with more lost its upper bones: missing or misplaced body parts, armour pieces and textures on the custom races,
and broken scene models on the character-creation background. The Vulkan cap is now 256, as on D3D9; the backend's own
register file stays 1024 rows.
