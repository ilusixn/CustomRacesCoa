# CoA64 Plan 8: Appearance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Port AscAppearance.cpp (2,050 lines): C_Appearance (24), C_AppearanceCollection (12), C_AppearanceOutfit (6),
C_ItemSet GetAppearances / GetItemSetNumCollected, SMSG 0x698..0x6A2 / 0x6EB, the nine appearance events, the glue
reset, the 0x724820 aura predicate and the patch refreshes (0x692 / 0x693 / 0x6EC). This removes the GameTooltip.lua:633
`C_Appearance` errors (3,243 per two minutes in world).

**Architecture:** as in Plans 4..7. `Appearance.cpp` (pure) holds the manager, packets, validator and natives;
`AppearanceBrowser.cpp` (pure) the two klib containers (CoA::Filter::Container) and their natives;
`AppearanceClient.cpp` fills `CoA::Appearance::Deps`: the active player (class, spellbook), Spell.dbc presence,
Item.dbc (class, subclass, inventory type), ItemSet.dbc (name, 17 items, id range), the creature-template cache
(BridgeStore::CreatureByEntry), item text (BridgeStore::FetchInfo / GetInfo), the item camera (C_UICamera's
FUN_1021a020), a GUID string to an item's GUID, and the in-world flag. DLL DBCs (AppearanceCategories, Appearances,
AppearanceDetails, ItemAppearances, ItemSetAppearances, SeasonalAppearances, GlobalStrings, UICameras) are read
through CoA::Dbc. PatchData's three appearance hooks and the SpellVisuals query (`PlayerSpellVisual`) are exported.

**Spec:** `docs/superpowers/specs/2026-10-25-coa64-client-design.md`. Reference:
`F:\WorkDir\ascension-extensions-reconstruction\src\Ascension\AscAppearance.cpp` (pin `1b9ebca`).

## Global Constraints

- Reproduce the original exactly, bugs included; every deviation is a ledgered Ruling.
- Reads past a packet's end follow the 7a convention (CoA::Read, zero on failure; counts guarded against the bytes left
  and ledgered).
- Natives register with COA_NATIVE in the original's namespaces; events with COA_EVENT.

## Review Focus

1. A pending change reaches the server only after every changed category validates; the APPLY result names match.
2. Item sets expand to their per-category parts (robe counts as chest, only the tracked set slots).
3. The collection browser's filter groups, text match (needs a selected category) and sort keys match the original.
4. Outfit save / delete keep the pending name until the server's answer, and the NEW_OUTFIT name is refused.
5. The glue screen drops pending, current and the browser state; the collected list and outfits survive it.

---

### Task 1: Manager, packets, C_Appearance, C_ItemSet

`Appearance.h` / `Appearance.cpp`, `AppearanceClient.cpp`, registration. Tests (`AppearanceTest.cpp`): each packet's
state and events (0x698 OK copies pending to current; 0x699 / 0x69A / 0x69B / 0x69C / 0x69D / 0x69F / 0x6A1 / 0x6A2 /
0x6EB), pending get / set / clear and PENDING_APPEARANCE_CHANGED, the validator's codes in order (OUT_OF_RANGE,
NOT_IN_WORLD, BAD_APPEARANCE, NOT_REALM_APPROPRIATE, BAD_CATEGORY, CATEGORY_NOT_FOUND, NOT_AVAILABLE_FOR_CLASS,
NOT_COLLECTED, item class / thrown, pet spells, item set, confirmation), ApplyPendingAppearances' CMSG 0x697,
SetCanSeeAppearances' CMSG 0x6A3, item-set expansion, store URLs, details, display info, alternates, creature display
items, the aura predicate, the glue reset.

### Task 2: Collection browser, outfits, seasonal

`AppearanceBrowser.cpp`. Tests (`AppearanceBrowserTest.cpp`): ApplyCategoryFilter with filters / sorts / text, page
count and pages of 18, outfit pages for category -2 with the NEW_OUTFIT entry first, collected counts, appearance types
and categories for a class, seasons / chapters / seasonal items, outfit natives (save / delete CMSGs, refused
NEW_OUTFIT, current outfit name), CollectItemAppearance's CMSG 0x6EA.

### Task 3: Live re-measure

In-world capture; findings doc "Plan 8 result"; inventory report regenerated.
