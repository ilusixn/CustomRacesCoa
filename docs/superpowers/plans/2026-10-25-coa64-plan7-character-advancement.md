# CoA64 Plan 7: Character Advancement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Port character advancement: AscCAMgr.cpp (5,578 lines, 126 natives, 14 packets, 5 spell-packet hooks),
AscCARules.cpp (1,484), AscCAFilter.cpp (1,370, 11 natives) and AscSpellApi.cpp (410, 8 natives). They own the CA
errors left in the Plan 6 capture (`GetEntryBySpellID`, `GetTalentsByClass`, `HasRuneUI`, `IsPendingBuildAvailable`,
`IsHelpfulSpell`) and the AE/TE investment MysticEnchant asks for.

**Split:** three sub-plans, each its own branch, review and local merge.
- **7a, engine:** rows, entry index, spell tags, SpellCustomAttr, known-spell sets, Build and its costs and rules,
  the manager, packets 0x725 / 0x726 / 0x728..0x72C / 0x659 / 0x65B / 0x926 / 064-bitA, the once-a-second Update, the
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

Packets 0x725 / 0x726 / 0x728..0x72C / 0x659 / 0x65B / 0x926 / 064-bitA; Update (parameter sync, auto-learn, the pending
signal); the glue reset; the known-spell hooks on the five spell packets and 0x5AAB90; the cast requirement; IsItemAction;
the rune patch. Tests: each packet's events and state, Update's sync and auto-learn, the glue reset. Client half:
`CAClient.cpp` and the decomp edits.

### 7b (branch `plan7b-ca-natives`)

The 126 bindings of AscCAMgr.cpp (lines 1549..5357 of the reconstruction), in four files that share
`CANativesInternal.h` (Usage, entry-table push, id/unit argument readers, the CA_LEARN / CA_UNLEARN name tables).
Natives are registered from `CoA::CA::Init` through one `CoA::Binding` table per file. Unported modules answer
through new `CoA::CA::Deps` members with the original's no-data result (BuildCreator export / link decode / seed,
WildcardRapid active / in flight / reroll pending, WildcardRolls::ArmPoll, Token::AtLeast), each ledgered.
Client half in `CAClient.cpp`: PickupSpell (spellbook 0xBE8D98 / 0xBE6D88, 0x53BAF0), ChrClasses for GetClassInfo,
unit tokens to GUIDs.

- **Task 3, read natives** (`CANatives.cpp`): investment / remaining / expected (active and pending), entry and
  spell type queries, essence costs, class and tab names, entry lists by class, known entries / spells / talent
  ranks, unit queries, qualities, tags, categories, class info, HasRuneUI, CanUseBrowser, credit amount.
  Tests: each native's results on the fixture build, nil / usage paths for bad arguments, sort orders.
- **Task 4, edit natives** (`CAEdit.cpp`): pending-build edits (add / remove / clear / cancel / apply and their Can*
  forms), the pending summary, direct learn / unlearn / lock and their CMSGs, confirmations, unlearn-all, specs and
  loadouts, swap and trade. Tests: sent packet bytes, pending-build state, result names, confirm lists.
- **Task 5, suggestions and inspect** (`CASuggest.cpp`): the suggestion maps and recompute, the tick, SMSG 0x6CE /
  0x6D2, the context overrides, GetSuggestedStats, CMSG 0x6E1 / SMSG 0x6E2, GetInspectInfo, GetInspectedBuild.
  Tests: weights (locked doubled), order, the stat fill-in, the inspect round trip.
- **Task 6, build links** (`CALinks.cpp`): ExportBuild, ImportPendingBuild / ImportPendingBuildID and the URL /
  link parsing. Tests: URL normalising, link round trip, merge of duplicate entries.

### Review Focus (7b)

1. A native given the wrong argument types prints the original's usage text and returns what the original returns.
2. Pending-build natives with no pending build (before 0x725) answer without crashing.
3. Sent CMSGs carry exactly the original's fields and order.
4. Lists come back in the original's sort order (entry order, talent order, locked first).
5. Natives reading the active build before world entry use the default build, not a null.

### 7c (branch `plan7c-ca-filter`)

- **Task 7, spell API** (`SpellApi.h/.cpp`, `SpellApiClient.cpp`): AscSpellApi.cpp's eight C_Spell natives
  (GetMaxLearnableRank, AppliesBuff / AppliesDebuff, IsHarmfulSpell / IsHelpfulSpell, IsImmunitySpell, IsTrainerSpell,
  GetFirstRank) and FUN_10325130 (IsPositiveEffect, public for SpellVisuals later). A spell is a `SpellFields` struct
  the client half fills from Spell.dbc; SkillLineAbility / SkillLine come through Deps, and the 0x934 patch hooks
  (ClientDbcPatch::Deps skillLineAbilityErase / Update) are filled. Tests: positive-effect rules (attribute 0x4000000,
  custom attr bits, family lists, aura cases, trigger-spell nesting), buff / debuff, immunity list, trainer lookup,
  max learnable rank.
- **Task 8, browser filters** (`CAFilter.cpp` replaces the stubs): AscCAFilter.cpp's four containers, the FILTER_*
  enums and groups, the text match, the 256-bit sort keys, '$' tokens, refine vs full filter, category buckets, the
  recent / upcoming sets, the glue reset and its ten natives (C_CharacterAdvancement x7, C_BuildEditor x3). Build
  Creator questions go through CA Deps. Tests: group OR / AND, sort order, tokens empty the results, refine,
  quality words lifted from the query, category buckets, IsFiltered.
- **Task 9, live re-measure**: in-world error capture, before / after table in the findings doc.

### Review Focus (7c)

1. Filters OR within a group and AND across groups, in the original's group numbering per container.
2. Results come back in the 256-bit key order (fields overlapping as in the original).
3. A '$' token empties the results; the refine path is taken only under the original's conditions.
4. IsHelpfulSpell / IsHarmfulSpell agree with FUN_10325130 for nested trigger spells without recursing forever.
5. Natives with bad arguments return what the original returns (C_BuildEditor raises nothing).
