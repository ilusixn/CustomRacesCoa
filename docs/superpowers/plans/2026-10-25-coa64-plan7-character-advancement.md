# CoA64 Plan 7: Character Advancement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Port character advancement: AscCAMgr.cpp (5,578 lines, 126 natives, 14 packets, 5 spell-packet hooks),
AscCARules.cpp (1,484), AscCAFilter.cpp (1,370, 11 natives) and AscSpellApi.cpp (410, 8 natives). They own the CA
errors left in the Plan 6 capture (`GetEntryBySpellID`, `GetTalentsByClass`, `HasRuneUI`, `IsPendingBuildAvailable`,
`IsHelpfulSpell`) and the AE/TE investment MysticEnchant asks for.

**Split:** three sub-plans, each its own branch, review and local merge.
- **7a, engine:** rows, entry index, spell tags, SpellCustomAttr, known-spell sets, Build and its costs and rules,
  the manager, packets 0x725 / 0x726 / 0x728..0x72C / 0x659 / 0x65B / 0x926 / 0x64A, the once-a-second Update, the
  glue reset, the five spell-packet hooks, the 0x5AAB90 hook, the cast-requirement check after 0x80B5D0, the rune
  regeneration patch, the global IsItemAction, and wiring into GameMode (active spec), MysticEnchant (investment) and
  SpellText (CA spell questions).
- **7b, natives:** the 126 C_CharacterAdvancement / C_Spell / global natives, suggestions (0x6CE / 0x6D2) and
  inspect (0x6E2).
- **7c, browser and spell API:** AscCAFilter.cpp (until then its reset entry points are no-ops) and AscSpellApi.cpp,
  then a live re-measure.

**Architecture:** as in Plans 4..6. Pure code (`CA.cpp`, `CARules.cpp`, later the natives) holds the logic and is
unit-tested; every client read goes through `CoA::CA::Deps`, filled by `CAClient.cpp`. A unit is passed by GUID
(0 = the build's owner). Modules not ported yet (BuildCreator, Wildcard*, Token, Challenge, AssetQuery) answer
through Deps with the original's no-data result; each is ledgered.

**Spec:** `docs/superpowers/specs/2026-10-25-coa64-client-design.md`. Reference:
`F:\WorkDir\ascension-extensions-reconstruction` (pin `1b9ebca`).

## Review Focus (7a)

1. The glue screen drops the player record, the inspect records, the known-spell / rune / tag sets and turns rune
   regeneration back to the stock compare.
2. SMSG 0x726 diffs into the original's event order (KNOWN_ENTRY_UPDATED, ENTRY_UPDATED, KNOWN_ENTRY_REMOVED, the
   three callback lists, KNOWN_ENTRIES_UPDATED) and always resets the pending build.
3. Learn validation returns the first failing slot in slot order, honours include / exclude, and the cache only
   returns a remembered failure that still fails.
4. A build without rules (a non-full copy) validates everything as learnable.
5. Packets shorter than they claim never read past the buffer.

---

### 7a Task 1: Rows, index, builds, rules

`CA.h` (public API), `CAInternal.h` (what the CA files share), `CA.cpp` (rows, index, spell tags, SpellCustomAttr,
known spells, Build, the manager, ActiveBuild / PendingBuild), `CARules.cpp` (the rule slots, Prepare,
ValidateLearn / Unlearn / Purge / All, Optimize / Reorder / TraverseOrFix, mastery removal, ValidateApply, costs).
Tests: index and RowBySpell order, Visible / ClassTypeAdmits, budgets and investment sums (wildcard group charged
once), each build-rule slot, slot order and include / exclude, the learn cache, Reorder of an out-of-order build,
ValidateApply's codes, unlearn / purge costs.

### 7a Task 2: Packets, Update, glue, hooks

Packets 0x725 / 0x726 / 0x728..0x72C / 0x659 / 0x65B / 0x926 / 0x64A; Update (parameter sync, auto-learn, the pending
signal); the glue reset; the known-spell hooks on the five spell packets and 0x5AAB90; the cast requirement; IsItemAction;
the rune patch. Tests: each packet's events and state, Update's sync and auto-learn, the glue reset. Client half:
`CAClient.cpp` and the decomp edits.

### 7b, 7c

Planned after 7a merges, from the same reconstruction files.
