# SDD ledger — plan: docs/superpowers/plans/2026-10-26-coa64-plan8-appearance.md
Pre-flight: Task 2 consumes Task 1 state (Appearance internal header) -- same author.
Task 1: Ruling: the counted lists of 0x699 / 0x69A / 0x69D (and each outfit's appearances) read nothing when the count exceeds the bytes left (the 7a 0x726 guard); the original reads past the buffer -- cost if wrong: a malformed packet leaves those lists empty instead of reading garbage.
Task 1: Ruling: client records come through Deps as small structs (ItemRecord class / subclass / inventory type, ItemSetRecord id / name / 17 items) -- the original reads the 32-bit client rows (Item.dbc +4 / +8 / +0x18, ItemSet.dbc +4 / +8..+0x48) directly -- cost if wrong: none if the field mapping is right.
Task 1: Ruling: added a test-only CoA::Dbc::Table::Unload so the appearance tests serve their own GlobalStrings / UICameras / AppearanceCategories / ItemAppearances rows without colliding with other tests that serve the same files (tables are process-wide) -- cost if wrong: none in the client (unused there).
Task 1: Ruling: BindingsClient exports ItemCamera (FUN_1021a020) for the display info's camera; no behaviour change -- cost if wrong: none.
Task 1: note: kept the original's MaxId-long item-set list (the highest category id never gets a set part) and CanApplyPendingAppearances' OUT_OF_RANGE when the applied list is shorter than the category ids.
Task 1: note: transcribed before tests; mutations of the robe -> chest rule, the realm gate, the thrown check, the pet confirmation and 0x69B's already-collected check each CAUGHT.
Task 1: complete (commits 7ccb96b..38fc13f, tests: cmd //c Tools\coa\check.bat -> 359 tests, 0 failed)
Task 2: note: kept GetCollectedCount's quirk -- category 0 matches every empty category slot in the collected count while the total is counted only above 0.
Task 2: note: transcribed before tests; mutations of the browser's category gate, the thrown-weapon skip, the NEW_OUTFIT refusal, the seasonal order and FILTER_COLLECTED each CAUGHT.
Task 2: complete (commits 38fc13f..ce760ea, tests: cmd //c Tools\coa\check.bat -> 365 tests, 0 failed)
Final review (Opus): no Critical, no Important. Confirmed against the genuine DLL: the item-set id from +4 (0x100CB529) and the aura predicate (0x100C6410).
Final: test hardening (the user asked for complete tests): the aura blocking path (SpellCustomAttr row served), the untracked set slot (neck), filter groups OR / AND, every SORT_* direction, a failed outfit save keeping the pending name, the glue keeping outfits / dropping the browser category, and oversized counts; mutations of the set-slot filter, the custom-attr test, the glue category reset, the save-OK gate, SORT_ID_DESC and SORT_RECENTLY_COLLECTED_DESC each CAUGHT; suite 368/368.
Final: Ruling: AppearanceClient's ItemGuid skips the object lookup for a zero GUID where the original calls ObjectPtr(0, 2) -- the object manager has no object with GUID 0, so both answer "no item" -- cost if wrong: none.
Final: Ruling: Dbc::Table::Unload stays a public test-only method (documented as such); nothing in the client calls it -- cost if wrong: a future caller would leave modules holding row pointers dangling.
Final: open item: PlayerSpellVisual (the SpellVisuals query) has no caller until AscSpellVisuals is ported; applied SPELL_VISUAL appearances do not swap the player's spell visual until then.
Final: fixed the header comment's event argument order (ok, result) inherited from the reference.
Task 3: complete (live: Lua errors 3,274 -> 31, no C_Appearance errors, no crash; findings 'Plan 8 result')
