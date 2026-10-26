# SDD ledger — plan: docs/superpowers/plans/2026-10-26-coa64-plan11-build-wildcard.md
Pre-flight: Task 1 fills the CA Build Creator Deps; Task 2 fills the CA / CAFilter Wildcard Deps; no shared interfaces between Task 1 and Task 2.
Task 1: implemented by a subagent (opus); its rulings:
Task 1: Ruling: 7a count guards on every BuildCreator count loop (spells 17, enchants 17, types 5, page entries 70, spec records 3, ids 1) — cost if wrong: short packets give fewer entries.
Task 1: Ruling: 0x633 / 0x634 / 0x635 clear + push_back instead of resize(n) + fill — identical on well-formed packets — cost if wrong: a truncated list is shorter than the original's zero-padded one.
Task 1: Ruling: 0x632 ignores spec indices >= 0x10000 (still signals) — the original's resize would fail or wrap — cost if wrong: absurd index not stored.
Task 1: Ruling: the filter holds page indices instead of pointers into the page — identical until a reallocation, where the original dangles — cost if wrong: post-reallocation contents differ from undefined.
Task 1: Ruling: link codec via vendored zlib 1.2.2 (level 6, window -15, memLevel 8, output buffer = input size; inflate into 10x with Z_SYNC_FLUSH) for the original's Boost.Beast deflate port; vectors cross-checked with classic zlib 1.3.2 — cost if wrong: exported links differ in bytes but still decode.
Task 1: Ruling: rank->spell uses CA::SpellAtRankOf (byte rank, FUN_101c62e0) instead of the reconstruction's 32-bit version — cost if wrong: only ranks > 255.
Task 1: Ruling: enchant sort / LegendaryEnchant use MysticEnchant's spell index (later row wins, map 0x10BE04C8) instead of the reconstruction's first-row-wins map — cost if wrong: only duplicate enchant spells.
Task 1: Ruling: CA BUILD_CREATOR learn / unlearn rule slots (FUN_10160df0 / FUN_101649b0) implemented from the DLL now that seedOfBuild exists — cost if wrong: named builds gain / lose learnable entries wrongly.
Task 1: Ruling: the editor-container reset moved from CAFilter's glue reset to BuildCreator's (FUN_100fc070), as the DLL does — cost if wrong: none observable.
Task 1: complete (commits 2bcd870..a3fea35, tests: check.bat → 451 tests, 0 failed; mutants 18/18 caught)
Task 2: Ruling: 7a guards on SMSG 0x6EE (20-byte records) and 0x6F0 (cleared, whole 9-byte records) — original overreads — cost: truncated packet leaves later entries unset.
Task 2: Ruling: mode manager +0x00..+0x197 modelled as dwords; out-of-range index reads 0, drops writes — cost: only spec >=20 or <=-2.
Task 2: Ruling: reveal records split between CA Request and a parallel stop-code/timer list — cost: none expected.
Task 2: Ruling: +0x274 record and TemporalContracts +0x04 map not modelled (no readers) — cost: a missed reader.
Task 2: Ruling: TemporalContracts type list empty (VM-protected filler); tests-only SetTypes — cost: none shipped.
Task 2: Ruling: GetQuestSortText null-name row → "Unknown" — cost: negligible.
Task 2: Ruling: TEMPORAL_CONTRACTS_UPDATED not COA_EVENT-declared (VM-only fire) — cost: one inventory entry.
Task 2: Ruling: FUN_10308830 mapped to ClientHooks::suppressLearnMessages — cost: learn messages muted wrongly.
Task 2: complete (commits a3fea35..9c1b938, tests: check.bat → 508 tests, 0 failed; mutants 26/26 caught)
Task 2: Ruling: WildcardMgr glue reset (FUN_10a33b10) zeroes the session id and clears quick-roll configs, unlike the reconstruction — DLL verified by the final reviewer — cost: none.
Task 2: Ruling: unknown stop code (FUN_10a2f5e0) logged and treated as 1; double parse/log on 0x621 is original — cost: none.
Final: Ruling: kept original stuck-cancel bug — cancel then 0x644 OK then 0x621 code 0 with no bonus exits before the drain (0x10a35666), InFlight stays true until world entry/glue — cost: rolling blocked for a session, as on genuine.
Final: Ruling: kept original TemporalContracts timer leak — tick re-arms without storing the id, glue cancel stops only the first; each world entry adds a 50 ms chain — cost: duplicate queued activations, as on genuine.
Final: minor (deferred): link test vectors are Perl-zlib generated, not a genuine client ExportBuild capture — golden vector from genuine pending.
Final review: opus, no Critical/Important.
