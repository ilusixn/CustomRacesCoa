# SDD ledger — plan: docs/superpowers/plans/2026-10-26-coa64-plan14-render-ui.md
Pre-flight: tasks share no interfaces except ClientHooks lists and CVar registration (existing runtime)
Task 1: Ruling: 0x52ADCC as ClientHooks::options.frameXmlRegistersUiScale cleared by ClientOptions::Init; CGGameUI::Initialize (uiScale/useUiScale) runs from the glue-list callback (DLL 0x10A67D5E) — cost: none.
Task 1: Ruling: 24 runtime callback byte patches as ClientHooks::options flags at decomp sites (Plan 13 pattern) — cost: none.
Task 1: Ruling: 0x731720 skips only the store; the original's x87 stack leak/shifted compare not reproduced (no x87 on x64) — cost: flags2&8 tracking-turn compare differs.
Task 1: Ruling: BuildSelectionRotMatrix made a const member (object in ecx) — cost: none.
Task 1: Ruling: null CombatLogMelee/autoAcceptTrades/cameraM2Collision slots treated as stock (DLL derefs null; pre-world only) — cost: none in practice.
Task 1: Ruling: TerrainScale null MCLY skips, layer count clamped to 4 (original reads past) — cost: malformed ADTs only.
Task 1: Ruling: Camera/ClientOptions first in s_modules for world CVar order; SceneTransform 0x681F60 at its call site; Engine hooks via function pointers — cost: none.
Task 1: Ruling: chat-bubble hook reads nameplate CVars via CVar::Lookup until Task 2 slots; HitTestDebug scratch zeroed; AfterTargetChange evaluates reaction eagerly — cost: none/minor.
Task 1: DLL-over-reconstruction: LoadingScreen gate starts open (0x10BCB8C8=1); WorldTextScale exact stock floats; selection radius maxss(5,sum).
Task 1: complete (commits e5fff6c..580dbce, tests: check.bat → 691 tests, 0 failed; mutants 29/29)
Task 2: Ruling: crash-only paths of the original made safe (unparsable nameplateN token, null focused plate, AutoInteractTurnSpeed slot pre-world, null GetGlobalString, "%d %s" scan bounded 1023, DPS rewrite w/o cache record, non-string auction type, inbox/attachment bounds, ClearLine bounds, nil self[0] in Has/Get/SetScript, DurabilityCosts/Quality bounds, inspect slot 19 reads 0) — cost: original crashes/garbage become stock/no-op.
Task 2: Ruling: plate registry entries by value (original leaks); unordered_map order may differ — cost: only with duplicate entries.
Task 2: Ruling: bot-report return-address check uses the running exe image end instead of 0xDFCBFF — cost: reports may differ for code below the exe.
Task 2: Ruling: install patches as ClientHooks::options flags; 0x82080B as Engine global g_m2ProjectedBatchZPad; FrameScript Has/Get/SetScript hook as Engine fn pointer — cost: none.
Task 2: Ruling: chat-bubble keeps CVar::Lookup by name (DLL sub_10a429a0 reads by name) — cost: none.
Task 2: Ruling: RequiredFormLine (FUN_10a10af0) unported pending untranscribed SetSpell detour 0x10A11130 (reconstruction no-op) — cost: one tooltip "Requires form" line rewrite missing.
Task 2: kept bugs — plate low-byte compare, queued ADDED ids survive leave-world, PopulateModelNameCache returns 1 pushing nothing, trade context reads target slot, auction index count+1 accepted, cone radius reads effect 0 twice, z pad stale with indicators off, trade flag left set; non-stock defaults reproduced (nameplateDistance 60 yd clamp, vertical offset 0, z pad 10).
Task 2: complete (commits 580dbce..d4ae32f, tests: check.bat → 723 tests, 0 failed; mutants 39/39)
Task 3: Ruling: Geosets null model / out-of-range selection index / null stats skipped (original crashes or reads past array) — crash paths only — cost if wrong: none / garbage pet model touched
Task 3: Ruling: SpellVisuals HitTarget spell==nullptr keeps visual (original derefs) — crash path — cost if wrong: none
Task 3: Ruling: SkipDynamicObject unknown spell reads zero fields (original reads uninitialised stack) — cost if wrong: garbage harmful/helpful verdict for missing Spell.dbc row
Task 3: Ruling: ZoneLight iterates zones in 64-bit std::hash order — cost if wrong: different 5 zone lights picked when >5 overlap
Task 3: Ruling: second moon glare is a static DNMoonGlare, vtable set at construction — cost if wrong: none
Task 3: Ruling: whole client on FreeType 2.14.1 (can't link two) incl. FT_Select_Charmap / stock fallback — cost if wrong: charmap selection difference for odd fonts
Task 3: Ruling: 0x6BE290 NOP tied to create-library hook presence; 0x80ED4E site recognised by RAII SpellGoScope flag instead of return address; tooltip green as constant 0xFF00FF00; SpellShadow textures CGxTexFlags() for 0x201 — cost if wrong: none / texture filter flags
Task 3: Ruling: kept original bugs — SpellShadow reads PendingSpellCast+0x10 (always 0) as spell id; cone persists through unflagged CAST_ERROR_CURSOR; shadow box half-height/selection circle z ±2.5 at defaults; Geosets display zeroed not restored, SetUpModel applies to last CreateModel; ZoneLight re-creates glare/occlusion per LoadMap; 0x680 past-max row drops last row; FontRender SDF at defaults
Task 3: Ruling: Geosets CreateModel capture via shared afterCreateModel list (DLL has its own 0x81F8F0 detour 0x101A14F0) — same behaviour — cost if wrong: hook ordering vs ItemCollections
Task 3: complete (commits d4ae32f..9f055bb, tests: check.bat → 770 tests, 0 failed; mutants 63/63)
Task 4: complete (live: 2 Lua errors, was 5; findings 'Plan 14 result')
Final: fixed LoadingScreen DBC path "DBFilesClient\L..." (invalid escape, variation never applied) — LoadingScreen_PicksANonZeroVariation (fake reader now exact-path) RED→GREEN, suite 771/771
Final: Ruling: NamePlates — Unit_C teardown releases the plate inline (bypasses 0x725840), leaving the original's registry pointing at the freed unit; CoA64 calls NamePlates::ForgetUnit there (drops entries, no REMOVED event, as the original fires none) — x64 freed-memory read would crash — cost if wrong: a token that in the original returned a garbage GUID now returns no unit. Test NamePlates_ForgetUnitDropsDanglingEntries RED→GREEN, suite 771/771
Final: Ruling: NamePlateClearAll (plates toggled off) also bypasses 0x725840 — units stay alive, so kept as the original: re-enabling plates gives a new id and a second ADDED without REMOVED — cost if wrong: addon sees duplicate ADDED
Final: Ruling (declined-to-judge items): low-byte-0 plate re-added every frame, FreeType shutdown with cached faces, SpellShadow style index past 2 textures when no player in state 2/3, raw CM2Model* across frames, tooltip s_compare after luaL_error, sscanf EOF match, Spell_C_BeginCast always-true — all faithful to the DLL — cost if wrong: as in the original
