# CoA64 Plan 11: BuildCreator, Wildcard, TemporalContracts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Port AscBuildCreator (C_BuildCreator, the build link codec, the per-spec active builds), AscWildcardRolls /
AscWildcardSets / AscWildcardRapid (C_Wildcard: repurchasable rolls, desired / undesired lists, rapid rolling) and
AscTemporalContracts (C_TemporalContracts). Fill the CA / CAFilter Deps left open in Plan 7 (exportBuild, decodeLink,
seedFromLink, seedOfBuild, specHasCreatorBuild, creatorCanAddSpell / CanRemoveSpell / HasSpell, wildcardRollActive /
InFlight / MarkRerollPending / ArmPoll, wildcardUndesired). Removes the live `C_BuildCreator` error.

**Architecture:** as in Plans 4..10: pure modules with `*Client.cpp` Deps halves; containers via
`CoA::Filter::Container`; timers via `CoA/Source/Runtime/Timer.h`; the link codec's raw inflate / deflate through zlib.
Exports mirror the reconstruction's AscBuildCreator.hpp / AscWildcardRapid.hpp / AscWildcardSets.hpp.

**Spec:** `docs/superpowers/specs/2026-10-25-coa64-client-design.md`. Reference:
`F:\WorkDir\ascension-extensions-reconstruction\src\Ascension\Asc{BuildCreator,WildcardRolls,WildcardSets,WildcardRapid,TemporalContracts}.cpp`
(pin `1b9ebca`).

## Global Constraints

- Reproduce the original exactly, bugs included; every deviation is a ledgered Ruling.
- Reads past a packet's end follow the 7a convention (counts guarded against the bytes left, ledgered).
- Natives register with COA_NATIVE in the original's namespaces; events with COA_EVENT; client reached only through Deps.

## Review Focus

1. The build link round-trips: ExportBuildOf -> DecodeLink -> SeedFromLink reproduces the build (URL decode, base64, raw
   inflate) and matches a link the original produces.
2. CA natives that consult BuildCreator / Wildcard now behave as in the original (the Plan 7 Deps are filled).
3. Rapid rolling's phases, the cancel path and the reroll-pending flag across SMSG 0x61E / 064-bit4 and the glue reset.
4. The roll-ready poll's timing (50 ms for a second, then 1 s) and the edge-triggered WILDCARD_ROLL_READY.
5. TemporalContracts keeps its empty type list (the filler is in the VM-protected part) and the queued activation timer.

---

### Task 1: BuildCreator

Tests: entry container and filters, every packet and event, link encode / decode round trip with a fixed vector, the
per-spec active builds, the pending-entry spell checks, the CA Deps wired (CA natives tested through them), glue reset.

### Task 2: Wildcard (Rolls, Sets, Rapid) and TemporalContracts

Tests: per-spec roll counters and their events, repurchase / reset results, desired / undesired lists and
IsUndesired, rapid-rolling phases and results, the ready poll, TemporalContracts natives / CMSG 0x55B / timer queue,
glue resets, the CA / suggestion Deps wired.

### Task 3: Live re-measure

In-world capture; findings doc "Plan 11 result"; inventory report regenerated.
