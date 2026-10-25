# CoA64 Plan 9: Game-mode modules Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Port AscToken, AscHighRisk, AscSeasonal, AscCanToggle, AscBuildDraft, AscDraftMode and AscScrollOfFortune
(~2.1k reconstruction lines): their natives, SMSGs / CMSGs, events and glue resets. This removes the
`HasHandOfFateSacrificePick` world errors and the mode-toggle gaps.

**Architecture:** as in Plans 4..8. Each module is a pure `X.cpp` (state, packets, natives) with an `XClient.cpp` that
fills its `Deps` (player GUID, item counts, player auras, unit fields). A shared pure helper
`CoA::Read::Inflate(CDataStore*, std::vector<uint8_t>&)` reproduces the reconstruction's InflatePacket through Storm's
`zlib_uncompress` (0x778180). Token exports TypeIndex / AtLeast / Amount for CA's `tokenAtLeast` Dep.

**Spec:** `docs/superpowers/specs/2026-10-25-coa64-client-design.md`. Reference:
`F:\WorkDir\ascension-extensions-reconstruction\src\Ascension\Asc{Token,HighRisk,Seasonal,CanToggle,BuildDraft,DraftMode,ScrollOfFortune}.cpp`
(pin `1b9ebca`).

## Global Constraints

- Reproduce the original exactly, bugs included; every deviation is a ledgered Ruling.
- Reads past a packet's end follow the 7a convention (CoA::Read, zero on failure; counts guarded against the bytes left
  and ledgered).
- Natives register with COA_NATIVE in the original's namespaces; events with COA_EVENT.

## Review Focus

1. A compressed packet that fails to inflate (short, bad zlib, size mismatch) clears the lists first; DraftMode 0x6AA then fires
   nothing, ScrollOfFortune 0x6AB always fires its event (as the originals).
2. Token 0x66F skips unknown names without consuming their amount (kept bug) and later names then misalign.
3. CanToggle reasons are returned in the original's order, with the CONFIG_SURVIALIST_ENABLE typo kept.
4. The glue screen resets exactly what each original resets (DraftMode keeps the Hand of Fate rewards).
5. Seasonal 0x905 initial packets insert silently; later ones fire the VALUE_CHANGED events with old and new.

---

### Task 1: Inflate, Token, HighRisk, Seasonal

`Runtime/Read` Inflate; `Token.cpp`, `HighRisk.cpp`, `Seasonal.cpp` and client halves. Tests: Inflate (short, bad,
good); 0x66F list incl. unknown names, 0x670 TOKEN_UPDATED, C_Token natives, glue reset, CA tokenAtLeast; 0x73D / 0x73B,
insurance cost tiers and the max-level surcharge; 0x9BE / 0x9BF / 0x523 / 0x905 and the six globals.

### Task 2: CanToggle, BuildDraft

Tests: each CanToggle*Mode's reason list in order (config, GM level, event, auras, level, class, spec);
C_BuildDraft natives, SMSGs 0x636..0x63D, CMSG 0x63A, glue reset.

### Task 3: DraftMode, ScrollOfFortune

Tests: 0x753 state and the change events, 0x6AA compressed rewards into maps[flagA][flagB], 0x6B4 / 0x6B7, the
draft globals incl. HasHandOfFateSacrificePick, C_DraftRewards CheckClaim codes and CMSG 0x6B3, 0x750..0x752;
0x6AB / 0x6B8 / 0x6B6 and C_WildcardRewards, CMSG 0x6B5, glue resets.

### Task 4: Live re-measure

In-world capture plus realm list, character creation and options screens; findings doc "Plan 9 result"; inventory
report regenerated.
