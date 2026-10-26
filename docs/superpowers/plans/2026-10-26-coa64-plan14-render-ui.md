# CoA64 Plan 14: Client Options, Camera, Nameplates, Tooltips, Rendering Hooks

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Port AscClientOptions (incl. the last byte patch 0x52ADCC and the glue-time `useUiScale` CVar), AscCamera,
AscAlphaAmbience, AscTerrainScale, AscSceneTransform, AscLoadingScreen, AscNamePlates (C_NamePlate,
`GetNamePlateForUnit`, NAME_PLATE_UNIT_ADDED / _REMOVED), AscUnitSelect, AscTooltipScaling, AscModelNames
(`GetModelNameCompletions`, `PopulateModelNameCache`), AscFontRender, AscZoneLight, AscSpellVisuals, AscSpellShadow,
AscSpawnVisibility and AscGeosets. Removes the live missing-CVar errors (`useUiScale`, `ObjectSelectionCircleTexture`,
`autoAcceptTrades` ...) and most of the missing CVars / client hooks in `CoA/inventory-report.md`.

**Not in this plan:** SpellMods, MovementForce, SpellTokens / UnitTokens, PacketApi (CDataStore natives), CacheFiles,
GossipQuest, QuestSpellHide, SmallHooks, WorldEntryTweaks, the remaining events / packets (Plan 15); DXVK; Warden.

**Architecture:** as in Plans 4..13: pure modules with `*Client.cpp` Deps halves; CVars registered where the original
registers them (glue vs world state); client detours only through ClientHooks lists with an `orig hook 0x...` marker
in the WoW / Engine source; byte patches as source flags / setters (Plan 13 ruling).

**Spec:** `docs/superpowers/specs/2026-10-25-coa64-client-design.md`. Reference:
`F:\WorkDir\ascension-extensions-reconstruction\src\Ascension\Asc{ClientOptions,Camera,AlphaAmbience,TerrainScale,
SceneTransform,LoadingScreen,NamePlates,UnitSelect,TooltipScaling,ModelNames,FontRender,ZoneLight,SpellVisuals,
SpellShadow,SpawnVisibility,Geosets}.cpp` (pin `1b9ebca`); genuine DLL `F:\WorkDir\genuine\Extensions.genuine.dll`
where the reconstruction is unsure.

## Global Constraints

- Reproduce the original exactly, bugs included; every deviation is a ledgered Ruling.
- CVar names, defaults, flags and help strings exactly as the original registers them; the inventory's `<name>.<default>`
  native-table entries must match.
- Natives register with COA_NATIVE in the original's namespaces; events with COA_EVENT; client reached only through Deps.
- Rendering hooks must be no-ops at the CVar defaults unless the original changes default rendering.

## Review Focus

1. `useUiScale` / `uiScale` exist on the glue state before GlueXML OptionsPanelTemplates.lua:391 runs (live error).
2. Every CVar callback the original installs runs on change and at load (saved config-cache.wtf values applied).
3. NamePlates: unit tokens `nameplateN` resolve, ADDED / REMOVED fire once per plate, glue reset clears.
4. Camera / UnitSelect / TooltipScaling hooks keep stock behavior at the default CVar values.
5. Rendering hooks (ZoneLight, SpellVisuals, SpellShadow, TerrainScale, Geosets) do not crash with missing DBC rows.

---

### Task 1: ClientOptions, Camera, AlphaAmbience, TerrainScale, SceneTransform, LoadingScreen

Tests: every CVar (name / default / flags / callback), the 0x52ADCC patch as a flag, useUiScale on the glue state,
camera CVar effects through Deps, loading-screen selection, terrain scale / scene transform values.

### Task 2: NamePlates, UnitSelect, TooltipScaling, ModelNames

Tests: nameplate natives / unit tokens / events, unit-select circle CVars and modes, tooltip scaling state machine,
model-name cache and completions, glue resets.

### Task 3: FontRender, ZoneLight, SpellVisuals, SpellShadow, SpawnVisibility, Geosets

Tests: each module's hooks through Deps at default and non-default CVars, DBC-missing paths, Geosets CreateModel hook
point (Plan 13 Task 1 ruling).

### Task 4: Live re-measure

In-world capture; findings doc "Plan 14 result"; inventory report regenerated.
