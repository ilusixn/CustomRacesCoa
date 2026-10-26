# SDD ledger — plan: docs/superpowers/plans/2026-10-26-coa64-plan12-services.md
Task 1: Ruling: count loops stop when the packet runs out (ticket/message/receipt/player/poll/friends lists) — original overreads — cost: malformed packets only.
Task 1: Ruling: empty or overlong chat-check string dropped — original corrupts a std::string on length 0 — cost: one server edge case.
Task 1: Ruling: GetTicketMessage with no readable id searches id 0 — original uses an uninitialised local — cost: different garbage match.
Task 1: Ruling: absent optional u32 reads 0 — original stores an arg pointer, gated by has-flag — cost: none visible.
Task 1: Ruling: null active-player name treated as "" — cost: none expected.
Task 1: Ruling: PlayerPoll CMSG 0x745 via CoA::Packet instead of 0x406F40 — same packet — cost: send path only.
Task 1: Ruling: GM ticket view sorts by populate key first, SORT_* as tie-breakers (comparator 0x100c1460), unlike the reconstruction — DLL verified — cost: none.
Task 1: Ruling: FriendsMetadata unknown name + empty value neither stored nor signalled (DLL compare with Get) — cost: none.
Task 1: complete (commits 759cb1f..1e91668, tests: check.bat → 543 tests, 0 failed; mutants 34/34 caught, 1 by crash)
Task 2: Ruling: forEachCompletion is a TemporalContracts Dep (brief said Wildcard); Callboard fills it — cost: none.
Task 2: Ruling: count loops stop when no bytes are left (recovery, store, callboard, 0x730) — original overreads — cost: malformed packets only.
Task 2: Ruling: missing/unloadable discord_game_sdk.dll → CMSG 0x52E result 4 (InternalError) — original links statically — cost: server sees 4 instead of e.g. NotRunning.
Task 2: Ruling: SDK loaded via LoadLibraryW/GetProcAddress with x64 C-ABI structs — x86 DLL unloadable — cost: core path untested at runtime.
Task 2: Ruling: 0x90C activity unset fields zeroed (original: uninitialised stack) — cost: none visible.
Task 2: Ruling: Discord callback thread is a std::thread — cost: none.
Task 2: Ruling: kept original recovery-result bug marking the view entry at the all-list index (FUN_103060b0) — cost: as genuine.
Task 2: complete (commits 1e91668..db4adb3, tests: check.bat → 573 tests, 0 failed; mutants 39/39 caught)
Final review: opus — 1 Important, 3 Minor.
Final: fixed filter-table readers not reproducing FrameScript::lua_totable (bad key/value empties the map; map order) — shared CoA::Script::ToBoolMap used in CustomStore, GMTicket and (DLL-verified to reach 0x100BED80 / 0x1011FC10 / 0x10131480) WildcardSets, TrialCreator, ChallengeNatives, AppearanceBrowser — tests CustomStore_Filters, GMTicket_Filters, TrialCreator_Filter, Challenge_Container, WildcardSets_DesiredBrowser, AppearanceBrowser_Filter RED→GREEN, suite 573/573, 2/2 mutants caught.
Final: minor (deferred): DiscordClient targets SDK 2.x x64 ABI; a 3.x SDK needs DISCORD_VERSION 3 / overlay v2 — comment only.
Final: minor (deferred): GMTicket sorts table read per value with StringAt; FUN_1011f710 invalid-value handling not decompiled.
Final: minor (deferred): diagnostics use printf %d where the original uses fmt {} — log text only.
Final: minor (deferred): detour 0x10A46350's once-only FUN_10228d30 registers SMSG 0x904/0x92A/0x92C/0x92D (another module) — check ownership in Plan 15.
