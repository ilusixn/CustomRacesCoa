# Living Azeroth from Esteria's wowdecomp (2026-11-16)

Living Azeroth is Zach's native port of `wxl-treewind`, `wxl-grasswind` and `seyris-living-azeroth` (all GPL-3.0) in
`STRHercules/wowdecomp`, commits `96aaaa2b`, `bbaf9232`, `deea741f`, `666d04be` (branch `Esteria`). Zach agreed to its use
in AFK Realm. The GPL needs the source of a shipped exe that contains it to be available to its players.

## What was there before

The Vulkan/HD step (2026-11-15, from `51b9d02`) brought `LivingWind`, `CustomTable`, `HdGrassWind`, `HdDetailDoodad` and
`HdSurfaceCover`, but none of their hooks: the world wind was never registered nor updated (grass swayed with fixed
defaults), and tree wind, kept grass, grass density and snow were missing.

## What came over now

- New files, verbatim apart from comments squashed to the 2-line budget (full text in wowdecomp `666d04be`):
  `Engine/Source/Model2/M2TreeWind.{h,cpp}`, `WoW/Source/WorldClient/GrassDensity.{h,cpp}`, `SurfaceCover.{h,cpp}`.
- Hooks, three-way merged (base `96aaaa2b^`, theirs `666d04be`):
  - `M2Model.cpp`: `TreeWind::Apply` after the model matrix (kept with CoA's body sliders, `M2BodyWorld`).
  - `Client_CVar.cpp`: `TreeWind`, `LivingWind`, `CDetailDoodad`, `GrassDensity`, `SurfaceCover` cvars.
  - `WorldFrame.cpp`: `LivingWind::Update` and `SurfaceCover::Update` each world update.
  - `DetailDoodad.{h,cpp}`, `Map.cpp`, `MapChunk.cpp`: kept grass pools (`grassBufferMemory`), keep frames, density
    copies and far thinning.
  - `MapRenderChunk.cpp`, `World.cpp`: snow drawn after each chunk and the near patch after the terrain.
  - `Camera.cpp`: the camera stays above lying snow. `GameUI_Scripts.cpp`: `LivingAzeroth_Get*` Lua.
  - `WorldParam_CVar.cpp`: `groundEffectDist` up to 10000 (CoA had raised 140 to 1400).
- Left out: Esteria's option pages (`UIUtil/DistanceOptions.cpp`, not in CoA64); every feature is a cvar, on by default.

## Data

Read beside the exe (loose files win over archives for the tables):

- `DBFilesClient\{WindProfile,AreaClimate,GroundEffectDoodadDensity,GroundEffectDoodadWind,GroundMaterial,
  GroundMaterialSelector,SurfaceCover}.cdbc` (from the Esteria client).
- `forever_data\` (WoW Forever's light, fog, ambient, liquid and FFT tables and the grading LUTs) for Azeroth
  Remastered. Without it HdWater, HdForever and HdGrade used hand-made fallbacks.

Both ship in the voices release as `exe/GraphicsData.zip`, unpacked by AFK Realm into the game folder.

Snow draws on the vertex-shader terrain path (`CMap::enableTerrainShaderVertex`), which the Vulkan device uses.
