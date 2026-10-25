# SDD ledger — plan: docs/superpowers/plans/2026-10-25-coa64-plan6-mystic-enchant.md
Pre-flight: Task 2 consumes Task 1's CoA::Filter::Container (ArgMatch/ArgGroup/TextMatch/FilterKey) -- consistent; Task 3/4 extend Task 2's file and Deps -- consistent.
Task 1: Ruling: FilterContainer.h keeps the reconstruction header's brace style (ported verbatim, namespace renamed) -- cost if wrong: a style pass.
Task 1: complete (commits 6fa124b..8731cca, tests: cmd //c Tools\coa\check.bat → OK)
Task 2: Ruling: Tasks 2 and 3 landed in one commit (one source file, both test files written first and seen failing to compile) -- cost if wrong: none.
Task 2: Ruling: SMSG 0x56B inserts the patched row into both indexes (the original's two FUN_100BD860 inserts) instead of the reconstruction's rebuild-from-table, because CoA::Dbc keeps new ids out of RowAt -- cost if wrong: a stale key from an in-place patch stays mapped, as in the original.
Task 2: Ruling: Inspect without a cached result pushes (true|false, nil) and returns 1, so Lua sees nil (the original's bug, kept; test MysticEnchant_Inspect) -- cost if wrong: none, matches the original.
Task 2: Ruling: SaveCollectionReforge pushes true and returns 0 (the original's bug, kept) -- cost if wrong: none.
Task 2: Ruling: list counts in 0x5F9 / 0x5FD / 0x614 are refused when larger than the bytes left (deviation: the original allocates whatever the count says) -- cost if wrong: a malformed packet leaves the old list instead of a huge allocation.
Task 2: Ruling: the AE/TE investment comes from Deps::investment, which answers 0 until Plan 7 ports character advancement -- cost if wrong: enchants with an AE/TE requirement report REQUIRED_*_INVESTMENT until then.
Task 2: complete (commits 8731cca..198fd6d, tests: cmd //c Tools\coa\check.bat → OK)
Task 3: complete (commits 8731cca..198fd6d, tests: cmd //c Tools\coa\check.bat → OK)
Task 4: Ruling: the preset checks read the player through MysticEnchant's exports (PlayerCasting / PlayerIsCoAClass / BagItemCount) instead of a second Deps -- cost if wrong: none, same reads.
Task 4: Ruling: the world-entry scroll prefetch goes to the item cache directly (GetRecord with a no-op callback) because AscAssetQuery is not ported yet; no ITEM_CACHE_REQUEST_* events -- cost if wrong: those two events are missing until AssetQuery is ported.
Task 4: Ruling: the progress-aura multiplier reads Spell +0x144, which is EffectBasePoints[1] (not [0]) -- kept as the original reads it -- cost if wrong: none, matches the original.
Task 4: complete (commits 198fd6d..5d6514b, tests: cmd //c Tools\coa\check.bat → OK)
Task 5: complete (commits 5d6514b..f5ba1b7, tests: cmd //c Tools\coa\check.bat → OK)
Final review: opus subagent -- 0 Critical, 4 Important (all test strength), 4 Minor.
Final: fixed patch test did not prove the container reset -- MysticEnchant_PatchResetsContainerOnlyForNewIds, RED with container.Reset() removed, GREEN restored; suite 284/284.
Final: fixed no tests for no-player / short packets / unexpected enum -- MysticEnchant_NoPlayer, MysticEnchant_ShortPackets, MysticEnchant_UnexpectedEnumValue; suite 284/284.
Final: fixed glue tests checked part of the reset -- MysticEnchant_GlueDropsPendingAndInspect, MysticEnchantPreset_GlueResetsPending; suite 284/284.
Final: fixed progress level 0 / 250+ untested -- MysticEnchant_ProgressEdges; suite 284/284.
Final: Ruling: SMSG 0x5FF refuses an outer count larger than the bytes left and stops at an oversized inner count (same deviation as the Task 2 ruling for 0x5F9 / 0x5FD / 0x614) -- cost if wrong: a malformed packet leaves presets short instead of a huge allocation.
Final: Ruling: short scalar packets (0x5FA / 0x5FC / 0x5FE / 0x619 / 0x600) apply zeroed values (CoA::Read returns 0 past the end) where the original reads past the buffer -- cost if wrong: a malformed server packet sets a zero instead of garbage.
Final: minor (deferred): GetAppliedEnchant without a player reads the inspect cache where the original's null == null takes the slot branch (unobservable: tokens do not resolve without a player).
Final: minor (deferred): the Task 2 commit message says 61 natives / 16 packets; the module has 67 natives / 21 handlers with the presets.
