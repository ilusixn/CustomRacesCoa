# SDD ledger — plan: docs/superpowers/plans/2026-10-26-coa64-plan10-challenges.md
Pre-flight: TrialCreator (Task 2) consumes Challenge CanActivate / CanDeactivate / ActiveIds; Challenge consumes TrialCreator::HasActiveTrial — built in the same task.
Task 1: Ruling: a tutorial's completion id (+8) is checked against the client's completed-ACHIEVEMENT list (FUN_103087e0 reads [0x10BCC308] = 0xACFDB0 = s_completedAchievements), as the genuine DLL does; the reconstruction calls it a quest — cost if wrong: none, matches the DLL.
Task 1: Ruling: FormatKeywords finds existing |H..|h..|h links in the text as given, not its lower-cased copy (genuine 0x10219363 searches the parameter string); the reconstruction's lower-case search never finds "|H" and so links keywords inside links — deviation from the reconstruction, faithful to the DLL — cost if wrong: keywords inside links stay unlinked.
Task 1: Ruling: count loops of 0x6B0 / 0x6CA / 0x6CC stop at the packet's end (7a convention) — cost if wrong: none for well-formed packets.
Task 1: complete (commits 594a19a..4b7d563, tests: check.bat → 393 tests, 0 failed; mutants 12/12 caught after added checks)
Task 2: implemented by a subagent (opus); its rulings, verbatim in substance:
Task 2: Ruling: 7a count guards on 0x596 / 0x597 / 0x599 / 0x59B / 0x5A2 / 0x5C7 / 0x5BA and TrialCreator 0x5AC / 0x5CA — cost if wrong: a lying count yields fewer records instead of garbage.
Task 2: Ruling: u32-keyed hash maps use 32-bit FNV-1a (the x86 std::hash) so iteration order matches past 8 entries — cost if wrong: order of GetActiveChallenges / GetCompletedChallenges / GetChallengeCriterias / ActiveIds.
Task 2: Ruling: the TrialCreator entry list holds indices (the original holds pointers that dangle when an append reallocates); reset only on the 0x5AC clear byte, per the DLL (the reconstruction resets on every 0x5AC) — cost if wrong: stale list until reset, as in the original.
Task 2: Ruling: PushMergedLevels sorts rows by address; file rows keep file order, patched rows' order is heap-dependent as in the original — cost if wrong: order only with patched rows mixed in.
Task 2: Ruling: unset client Deps answer 0 / false (unknown spell hidden, unknown achievement blocks) as the original's missing-row lookups do — cost if wrong: none in the client.
Task 2: Ruling: Dbc::Table::Upsert drops the old highest row when a patch inserts an id outside the range (loop i < max) — the genuine FUN_10133ac0 has the same loop (cmp ecx,[edi+0xC]; jb), so the bug is kept — cost if wrong: patched tables lose a row, as in the original.
Task 2: complete (commits 4b7d563..73a415b, tests: check.bat → 417 tests, 0 failed; mutants 29/29 caught)
Final review: Opus reviewer — no Critical, 1 Important, 2 Minor.
Final: fixed 0x6CA appending to the mentor spec list (genuine FUN_10a17a70 clears +0x180 first; the reconstruction's "appends" is wrong) — Tutorial_Mentors RED→GREEN, suite 417/417
Final: fixed (test gap M-2) applicability codes BAD_RACE / LEVEL_TOO_HIGH / REQUIRED_GAME_MODES / EXCEPT_GAME_MODES / BAD_EXPANSION and their order now pinned in Tutorial_Applicability.
Final: minor (deferred): Tutorial watch sets (+0x1A0 / +0x1C0) are std::set; the original's are unordered_set (FNV), so TUTORIAL_ENTRY_ADDED / _COMPLETE fire in id order instead of bucket order when several change at once.
