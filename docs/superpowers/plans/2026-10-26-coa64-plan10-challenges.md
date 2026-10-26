# CoA64 Plan 10: Tutorial, Challenge, TrialCreator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Port AscTutorial (C_Tutorial, 40 natives, 0x6AF..0x6B2 / 0x6C4..0x6CC, the 0x6A5 / 0x6A7 / 0x6AC table
patches), AscChallenge (C_Challenge, ~30 packets incl. 0x591 / 0x5B2..0x5BE / 0x663 / 0x664 / 0x677, the challenge
tick and data-store rebuild) and AscTrialCreator (C_TrialCreator, 8 packets). Removes the live `C_Tutorial` /
`C_Challenge` world errors.

**Architecture:** as in Plans 4..9. Pure `Tutorial.cpp`, `Challenge.cpp` (split into data stores / validators /
natives files as size requires), `TrialCreator.cpp`, each with a `*Client.cpp` filling its `Deps` (player race / class /
level, known spells, completed quests and achievements, party size and leader, the Achievement.dbc client table,
showTutorials CVar). Containers use `CoA::Filter::Container`; table patches use `CoA::DbcPatch::Register`; the enter-world,
idle-tick and stock-event points use `ClientHooks::afterEnterWorld`, `Lifecycle::OnAfterIdle` and
`ClientHooks::stockEvents`. Challenge exports the AscChallenge.hpp pieces (CanActivate / CanDeactivate / ActiveIds /
ActiveHasRule / PushMergedLevels ...); TrialCreator exports HasActiveTrial.

**Spec:** `docs/superpowers/specs/2026-10-25-coa64-client-design.md`. Reference:
`F:\WorkDir\ascension-extensions-reconstruction\src\Ascension\Asc{Tutorial,Challenge,TrialCreator}.cpp` (pin `1b9ebca`).

## Global Constraints

- Reproduce the original exactly, bugs included; every deviation is a ledgered Ruling.
- Reads past a packet's end follow the 7a convention (CoA::Read, zero on failure; counts guarded against the bytes left
  and ledgered).
- Natives register with COA_NATIVE in the original's namespaces; events with COA_EVENT.
- Time and the client are reached only through Deps, so every check is testable.

## Review Focus

1. Tutorial applicability codes in the original order (realm, race, class, expansion, then level / previous / spells /
   game modes), including the class mask past 32 bits.
2. FormatKeywords: whole-word, case-insensitive, never inside an existing link, longest match wins, and a keyword at
   position 0 is not linked (kept bug).
3. Challenge data-store rebuild: group-keyed rows attach to every challenge of the group; rewards filtered by realm
   expansion, player class and achievement gate; featured multiplier.
4. Challenge start / stop validation order and its codes; the trial lock (codes 0xF / 4).
5. Glue resets clear exactly what the originals clear; mentor whois / spec lists append where the original appends.

---

### Task 1: Tutorial

Tests: applicability codes, FormatKeywords cases, containers and filters, packets 0x6AF..0x6B2 / 0x6C4..0x6CC and their
events, the three table patches, watches (enter world, level up, stock events 0x258 / 0x1C0), mentor natives and CMSGs
0x6A8 / 0x6C3 / 0x6C6 / 0x6C8 / 0x6CB / 0x189, glue reset.

### Task 2: TrialCreator and Challenge

Tests: data-store rebuild, condition / requirement validators, start / stop codes, every SMSG's state and events, the
tick, the container and filter, CMSGs, the trial manager's packets and natives, the exported pieces, glue resets.

### Task 3: Live re-measure

In-world capture (and glue screens); findings doc "Plan 10 result"; inventory report regenerated.
