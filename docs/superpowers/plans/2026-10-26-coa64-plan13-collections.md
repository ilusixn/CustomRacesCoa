# CoA64 Plan 13: Manastorm, Skill Cards, Collections, Item / Quest Data

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Port AscManastorm (C_Manastorm), AscSkillCardCollection / AscSkillCardSlots (C_SkillCardCollection ...),
AscLootLockout (C_LootLockout), AscItemCollections, AscCollectorCache, AscItemStats, AscPOI, AscSpellCharges,
AscAura137, AscQuestAddon, AscAssetQuery (C_AssetQueryService) and AscAlternatePower. Removes the live `C_Manastorm`,
`C_LootLockout`, `C_SkillCardCollection`, `C_AssetQueryService` errors and fills the Deps left open earlier:
SkillCard (`skillCardUnblocked`, CustomStore `skillCardByItem` / `skillCardCollectedAtLeast` / `skillCardQualityIndex`),
QuestAddon (`questAddonPoints` in Wildcard, TemporalContracts, CallboardCache) and AlternatePower (subscriber of
`ClientHooks::afterAddKnownSpell`).

**Architecture:** as in Plans 4..12: pure modules with `*Client.cpp` Deps halves; containers via
`CoA::Filter::Container`; timers via `CoA/Source/Runtime/Timer.h`; client detours only through ClientHooks lists with an
`orig hook 0x...` marker in the WoW source.

**Spec:** `docs/superpowers/specs/2026-10-25-coa64-client-design.md`. Reference:
`F:\WorkDir\ascension-extensions-reconstruction\src\Ascension\Asc{Manastorm,SkillCardCollection,SkillCardSlots,
LootLockout,ItemCollections,CollectorCache,ItemStats,POI,SpellCharges,Aura137,QuestAddon,AssetQuery,AlternatePower}.cpp`
(pin `1b9ebca`).

## Global Constraints

- Reproduce the original exactly, bugs included; every deviation is a ledgered Ruling.
- Reads past a packet's end follow the 7a convention (counts guarded against the bytes left, ledgered).
- Natives register with COA_NATIVE in the original's namespaces; events with COA_EVENT; client reached only through Deps.

## Review Focus

1. Every Dep that earlier plans left answering "no data" is now filled, and the consumers' tests exercise the real
   answer (CustomStore filters 13..30, Wildcard / TemporalContracts / CallboardCache quest points, AlternatePower spell
   watch).
2. Manastorm's state machine and its Minimap.lua readers (Minimap.lua:235 / :346) see the values the original returns
   outside a Manastorm.
3. Skill card collection / slot packets and their containers; collected-at-least and quality index semantics.
4. LootLockout readers at UIParent.lua:1150 / :1748 outside any lockout.
5. Glue resets clear exactly what the originals clear.

---

### Task 0: AddonTaint (live "C stack overflow" root cause)

Found live: SharedXML C_Hook.lua recurses (SendEvent -> SecureSetValues -> SetAttribute -> OnAttributeChanged ->
SendEvent) until "C stack overflow" because issecure() is nil while a server-listed secure addon (Ascension_WildCard)
runs. Port AscAddonTaint (installer FUN_10312150): issecure (0x816DA0) / 0x5191C0 / 0x488540 answer "secure" for a
listed secure addon; the twelve "cleared" functions run with the taint cleared and restored; 0x525FC0 inside a
protected call; the target-change clear; the 0x5F7E90 addon-load gate if not already ported. Tests: each hook's
secure / not-secure answer through Deps, the taint save / restore, the addon-load refusals.

### Task 1: SkillCardCollection, SkillCardSlots, ItemCollections, CollectorCache, QuestAddon, AlternatePower

Tests: every packet / native / event, containers and filters, glue resets, and the earlier Deps wired (CustomStore
skill-card filters, quest points in Wildcard / TemporalContracts / CallboardCache, AlternatePower on
afterAddKnownSpell, Wildcard `skillCardUnblocked`).

### Task 2: Manastorm, LootLockout, ItemStats, POI, SpellCharges, Aura137, AssetQuery

Tests: every packet / native / event, Manastorm phases and timers, lockout lists, item stat / spell charge / aura
readers, POI, the asset query service callbacks, glue resets.

### Task 3: Live re-measure

In-world capture; findings doc "Plan 13 result"; inventory report regenerated.
