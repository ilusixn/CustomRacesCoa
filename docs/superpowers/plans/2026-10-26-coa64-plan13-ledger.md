# SDD ledger — plan: docs/superpowers/plans/2026-10-26-coa64-plan13-collections.md
Task 0: Ruling: Lua-error log gains the active taint and a traceback (CoA64 diagnostic, -extlog only, not in the original) — needed to find the C stack overflow — cost: log volume only.
Task 0: Ruling: Script_LoadAddOn reason locals start at UIADDON_LOADABLE — original reads leftover stack on a gate refusal — cost: refused addon reports UNKNOWN_ERROR vs genuine garbage token.
Task 0: Ruling: loadUnknownAddOns half of the 0x5F7E90 gate sits at its two callers (0x5F8071 / 0x5F80FA) instead of a return-address check — cost: none observable.
Task 0: Ruling: three C++ cleared functions restore taint via scope-exit; Lua ones restore explicitly before each return (a Lua error skips it, as in original) — cost: only if those three raise Lua errors.
Task 0: complete (commits 33d0a67..abede97, tests: check.bat → 591 tests, 0 failed; mutants 20/20 caught)
Task 1: Ruling: count loops (064-bit8/649/653/654/655/624) stop when a record's minimum size isn't left; 0x624/0x625 indices >= 0x10000 dropped; fixed fields past end read 0 — original overreads / failed alloc — malformed packets only.
Task 1: Ruling: ReloadItemDisplayInfoCollections rebuilds from the patched table, not a file reload (FUN_101e7b10 not fully traced) — cost: patched rows may survive a reload.
Task 1: Ruling: collection model paths passed from DLL strings instead of copied into 260-byte client buffers — cost: only if client reads those buffers later.
Task 1: Ruling: collection removal skips a component with no model (original derefs null); quiver slot 11 folder "" (original reads past table) — cost: crash-only paths.
Task 1: Ruling: four DLL hooks are markers only (0x4EF840, 0x4F9F70, 0x826E60, 0x832AB0); crash-report "Last Loaded M2 Name" and Geosets CreateModel call not reproduced — cost: Geosets will need the hook point.
Task 1: Ruling: forced-death callback replaces rather than stacks; CustomStore skill-card quality Dep is a string — cost: none.
Task 1: Ruling: kept bugs — quiver check reads visible item 16 (off-hand); card names lose last 4 chars before race suffix; patch row past max id drops old max row.
Task 1: complete (commits 63f5fe1..d88364c, tests: check.bat → 625 tests, 0 failed; mutants 46/46 caught)
Task 2: Ruling: count loops 0x9B4/0x6FE/0x65F/0x65D/0x688/0x9C4 stop at packet end, allocations capped by bytes left (7a) — cost: malformed packet yields fewer zero entries.
Task 2: Ruling: GetMaxCompletedLevels unresolved token uses guid 0 (original: uninitialised); OnPreAnimate returns with no active player (original: null call) — cost: none/garbage-only paths.
Task 2: Ruling: Manastorm allowed-level set built at first glue screen/first use, not inside CustomDBCMgr — cost: only if 0x68D arrives before first glue screen.
Task 2: Ruling: GroupSize/GroupMin raid guids from CGRaidInfo::m_members pairs, entries past 19 count present (as DLL) — cost: >20 raid slots.
Task 2: Ruling: GetQuestTemplate reads DLL offsets against client 0x28E4 QuestCache, bytes past record read 0 — cost: original returns heap garbage there.
Task 2: Ruling: RacialLeader bool with padding 0; GetAuctionItemRequiredLevel one-past-end read within capacity else 0 — cost: stale/nonzero byte values.
Task 2: Ruling: byte patches 0x80DB37, Aura137 sites, icon quad, SpellRange flags done as source flags/setters — cost: none.
Task 2: Ruling: itemstatcache.wdb load/save deferred to CacheFiles (Plan 15) — cost: stat cache not persisted until then.
Task 2: complete (commits 52422ac..01582c3, tests: check.bat → 658 tests, 0 failed; mutants 51/53 caught, 2 equivalent)
Task 2: Ruling: GetEncounterData for an unknown encounter returns 6 having pushed nothing (DLL-confirmed; the reconstruction pushes 6 nils) — cost if wrong: caller sees its own stack slots instead of nils.
Final review: opus code-reviewer — 0 Critical, 0 Important; 1 Minor (the GetEncounterData ledger gap, now ruled above).
Final: minor (deferred): CustomStore_RealSkillCards test unloads "DBFilesClient/SkillCard.dbc" with a forward slash while code uses backslashes (only matters if Dbc::Get does not normalise).
Task 3: live — C_Manastorm / C_LootLockout / C_SkillCardCollection errors gone; Sound Kit Player no longer opens at world entry. Missing action bars / minimap / chat seen this run reproduces on pre-Plan-13 code (63f5fe1) too, so it is saved-state, not Plan 13.
