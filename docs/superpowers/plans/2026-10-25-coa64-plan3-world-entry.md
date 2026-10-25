# CoA64 Plan 3: World Entry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** CoA64 enters the world on `logon.coa-development.org` with Ascension's FrameXML loaded the way the
32-bit pair loads it, all of Extensions.dll's attach-time client changes in place, and a measured list of what
the world still asks for.

**Architecture:** Phase 4's first plan (spec "Order of work" 4). Every attach-time byte patch
(`AscAttachPatches` / `AscAttachLate` / `AscAttachInline` tables, 127 rows) and attach-time hook
(`AscAttachFixes` 38, `AscAttachLate` 8, `AscAttachLua` 11, `AscAttachMisc` 7) becomes a marked source edit
in the decomp or a CoA module function called from one; logic that does not need client objects lives in
`CoA/Source` and is unit-tested. The Plan 2 deferrals close here. The last task enters the world and turns
what is still missing (unhandled packets, Lua errors, missing natives) into the order of Plans 4+.

**Tech Stack:** C++20, VS 2022 x64, CMake/Ninja (`out\build\x64-Release`), CoATests (`COA_TEST`), Python
3 tools in `Tools/coa`.

**Spec:** `docs/superpowers/specs/2026-10-25-coa64-client-design.md`. Behaviour reference: the reconstruction at
`F:\WorkDir\ascension-extensions-reconstruction` (pin `1b9ebca`). Earlier rulings:
`docs/superpowers/plans/2026-10-25-coa64-rulings.md`.

## Global Constraints

- Reproduce the genuine DLL's behaviour **including its bugs**; known original defects are recorded, not fixed.
- No raw 32-bit address (`0x4xxxxx`–`0xDxxxxx`) or `ptr + 0xNN` field access into client objects in x64 code;
  everything resolves to decomp symbols/members.
- No runtime self-patching (detours, `VirtualProtect`, byte writes): a byte patch is an equivalent source
  change; a detour is a call at the function's start/end; `ReplaceFunction` makes the decomp body dispatch to
  the CoA version.
- Every decomp edit is marked `// CoA: <reconstruction file:function> (orig hook 0xADDR)` or
  `(Extensions.dll patch 0xADDR)`, so `Tools/coa/inventory.py` counts it.
- Credentials are typed into the client only; never written to a file, commit, log, test, memory or
  screenshot name. Screenshots showing account or character names stay in `out/baseline/` (git-ignored).
- Live sessions fit the ~10-minute Warden window; a Warden kick is not a client bug.
- HD content stays gated on `CoA::Hd::Available()` (off until the DXVK plan).
- Build/test command for every task: `cmd //c Tools\coa\check.bat`.

## Review Focus

1. **A patch whose bytes encode a 32-bit address or a code cave** (0x86A62C → 0x9296A2 camera cave, 0x869DB0,
   jmp/call rel32 rows): copying the intent, not the bytes — the x64 edit reproduces what the cave computes.
   Task 1 pins the camera clamp with `AttachPatches_CameraClampFiftyPixels`.
2. **Patches a later module changes again** (AFK logout jmp 0x52B27A ↔ SetAFKLogoutTimer, rune-UI class
   0x728AA3 ↔ AscCAMgr): the attach value is a variable the later module can set, not a constant. Task 1 tests
   `AttachPatches_AfkLogoutDefaultAndOverride`.
3. **AnimationData with more rows than stock 506**: no fixed 506-entry array or `< 0x1FA` check survives at the
   44 sites; Task 6 tests `AnimationCount_FollowsLoadedRows` with 700 rows.
4. **World → glue → world**: every CoA world native is registered on each new world Lua state and the glue
   state again after logout; Task 7 tests `WorldReload_NativesBackAfterGlue`.
5. **A world event name the client does not know while the world UI loads** (0x81B380): added to the custom
   events instead of failing; Task 4 tests `AttachFixes_UnknownEventAddedWhileLoading`.

---

## File Structure

- `CoA/Source/Modules/AttachPatches.h/.cpp` (+`AttachPatchesClient.cpp`): the values the attach tables set
  that are data, not code (uiScale default `"0.85"`, the 8-entry level/distance tables, the AFK logout default,
  the rune-UI class, the camera clamp math); the decomp edits read them.
- `CoA/Source/Modules/AttachLate.cpp` (+`Client`), `AttachFixes.cpp` (+`Client`), `AttachLua.cpp`
  (+`Client`), `AttachMisc.cpp` (+`Client`): one module per reconstruction file; testable logic in the plain
  file, client calls in `*Client.cpp`.
- `CoA/Source/Modules/AnimationCount.h/.cpp`: the runtime AnimationData row count.
- Decomp files edited at the patch/hook sites (located per row with `scratchpad`-style lookup:
  `python Tools/coa/addrmap.py` output `CoA/addrmap.tsv` gives `0xADDR → file:symbol`).
- `CoA/DIFFERENCES.md`: any x64-forced difference found.
- `docs/superpowers/plans/2026-10-25-coa64-world-findings.md`: Task 8's measured list.

## Translating a byte patch (all of Tasks 1–3, 6)

For each row: find the containing decomp function (`CoA/addrmap.tsv`; for mid-function addresses the
nearest preceding `// OFFSET:`), disassemble the original bytes and the patched bytes at that address in
`D:\WoW\Coa\Ascension.exe` (REA, provider `ida`), decide what the change does in C++ terms (constant, branch
taken/removed, buffer size, call target), and make that change in the decomp source with the marker. A row
that has no effect in the decomp (dead code, a check the decomp never compiled in) still gets a marker
comment at the matching source line stating why nothing changed. Ledger every row whose translation is not a
one-to-one constant/branch change.

---

### Task 1: AscAttachPatches (68 rows + race tables, uiScale default, level tables)

**Files:**
- Create: `CoA/Source/Modules/AttachPatches.h`, `AttachPatches.cpp`, `CoA/Tests/AttachPatchesTest.cpp`
- Modify: the decomp sites of `AscAttachPatches.generated.inc` (incl. `CGGameUI::InitializeFrameXML`
  0x52ABD9 / 0x52AD43 / 0x52AD45, `OsWindowProc` 0x86A62C/F, `CenterMouse` 0x869DB0,
  `DispatchMouseButtonChanges` 0x869D83, `CSimpleFrame::SetFrameLevel` 0x4910DB/E2, `MinimapSetZoom`
  0x7F3AFD/03, `CGGameUI::Idle` 0x52B27A, `CGUnit_C::DoPowerRegen` 0x728AA3); `CoA/Source/Modules/Modules.h`,
  `CoA/Source/CoA.cpp`

**Interfaces:**
- Produces (`namespace CoA::AttachPatches`):
  - `extern const char* const kUiScaleDefault;` — `"0.85"`
  - `extern const int32_t kLevels[8];` — `{18, 16, 14, 12, 10, 9, 7, 4}`;
    `extern const float kDistances[8];` — `{210, 180, 150, 120, 90, 60, 40, 25}`
  - `bool AfkLogoutEnabled(); void SetAfkLogoutEnabled(bool);` — the 0x52B27A jmp (attach value: as the table)
  - `uint8_t RuneUiClass(); void SetRuneUiClass(uint8_t);` — 0x728AA3 (attach value 0)
  - `void ClampCursor(int32_t& x, int32_t& y, int32_t width, int32_t height);` — the 0x869DB0 cave's
    50-pixel clamp
  - `void Init();` (registered in `s_modules`)

- [ ] **Step 1: Write the failing tests** in `AttachPatchesTest.cpp`:
  `AttachPatches_LevelTablesAreEightEntries` (exact values above), `AttachPatches_UiScaleDefault`
  (`strcmp(kUiScaleDefault, "0.85") == 0`), `AttachPatches_CameraClampFiftyPixels` (a point 10 px from each
  edge of an 800×600 rect comes back 50 px inside, one 60 px inside is unchanged — derive the exact rule from
  the cave bytes at 0x869DB0/0x869D83 and assert it), `AttachPatches_AfkLogoutDefaultAndOverride` (default
  equals the table's bytes at 0x52B27A; `SetAfkLogoutEnabled` flips it).
- [ ] **Step 2: Run CHECK.** Expected: FAIL (link errors, `AttachPatches.h` symbols missing).
- [ ] **Step 3: Implement** `AttachPatches.cpp` and translate every row of `AscAttachPatches.generated.inc`,
  plus `RaceTables` (FUN_10a5e020), the uiScale default (0x5238AD) and useUiScale `"0"`→`"1"` (0x5238D1), and
  the level tables (0xA41E04 / 0xA41E1C, loop count 5→7) per "Translating a byte patch". The 0x744A55 fade
  flag and the vtable slot belong to Task 4 (AscAttachFixes).
- [ ] **Step 4: Run CHECK.** Expected: PASS. `python Tools/coa/inventory.py --recon
  F:/WorkDir/ascension-extensions-reconstruction --out CoA/inventory-report.md`: none of the 68 table
  addresses is listed under "Missing: Client byte patches".
- [ ] **Step 5: Live check.** Deploy, log in, Enter World with the test character. Expected: no "Your game
  interface files are corrupt" dialog; the world loads (screenshot `out/baseline/x64/6-world.png`). Log out.
- [ ] **Step 6: Commit** `feat(CoA): attach-time client patches`.

### Task 2: AscAttachLate (32 rows, 8 hooks)

**Files:**
- Create: `CoA/Source/Modules/AttachLate.h/.cpp`, `AttachLateClient.cpp`, `CoA/Tests/AttachLateTest.cpp`
- Modify: the 32 table sites; hook sites 0x4DE450, 0x52ED60, 0x5DEA60, 0x6DAC10 (call from 0x587450),
  0x726160, 0x54A370, 0x5E4B70, 0x86B5A0

**Interfaces:**
- Consumes: `CoA::RealmInfo::Get()` (`rate1`, `rate2`, `rateXpQuest`).
- Produces (`namespace CoA::AttachLate`): `float ScaleByRealmRate(float value);` (0x5E4B70: the argument times
  the realm's first rate); `void ScaledExperience(uint32_t in[3], uint32_t out[3]);` (0x54A370's three values
  by realm object +0x10 / +0x14); `bool KnowsHigherRank(uint32_t spell, bool (*known)(uint32_t), uint32_t
  (*supercededBy)(uint32_t));` (0x726160's chain walk, fatal on a self-superseding spell).

- [ ] **Step 1: Failing tests:** `AttachLate_ScaleByRealmRate` (rate 2.0 → 10 becomes 20; before
  SMSG_REALM_INFO the rate is the struct default and the result is what the original computes with it),
  `AttachLate_ScaledExperience`, `AttachLate_KnowsHigherRankWalksChain` (chain 1→2→3, knows 3 → true for 1),
  `AttachLate_SelfSupersedingSpellIsFatal` (assert through the fatal sink the module uses).
- [ ] **Step 2: Run CHECK.** Expected: FAIL.
- [ ] **Step 3: Implement** the module; translate the 32 rows; place the 8 hook calls (REPLACED ones make the
  decomp body dispatch to CoA; 0x86B5A0 becomes an empty body).
- [ ] **Step 4: Run CHECK.** Expected: PASS; inventory lists none of AscAttachLate's 32 addresses or 8 hooks.
- [ ] **Step 5: Commit** `feat(CoA): attach-late patches and hooks`.

### Task 3: AscAttachInline (27 rows)

**Files:** Modify the 27 sites of `AscAttachInline.generated.inc`; `CoA/DIFFERENCES.md` if any is x64-forced.

- [ ] **Step 1: Coverage test:** run inventory. Expected: the 27 addresses listed as missing (RED).
- [ ] **Step 2: Translate** every row per "Translating a byte patch".
- [ ] **Step 3: Run CHECK** and inventory. Expected: PASS; "Missing: Client byte patches" lists only rows
  owned by Task 6 (AnimationCount) and later plans' modules (record that list in the ledger).
- [ ] **Step 4: Commit** `feat(CoA): attach-inline patches`.

### Task 4: AscAttachFixes (38 hooks; 0x6B0F90 already done)

**Files:**
- Create: `CoA/Source/Modules/AttachFixes.h/.cpp`, `AttachFixesClient.cpp`, `CoA/Tests/AttachFixesTest.cpp`
- Modify: the hook sites listed in `AscAttachFixes.cpp` batches 1–6 (0x4D5F70 … 0x805D70), incl. the 0x744A55
  fade flag and the vtable slot from Task 1's exclusions; `Engine/` sites get a hook pointer like
  `g_dataMgrGetCoordHook` (Engine does not link CoA).

**Interfaces:**
- Produces (`namespace CoA::AttachFixes`):
  - `void LogMissingFileOnce(const char* name);` (0x424B50: a failed SFile open logged once to C_Logger
    MissingFiles, channel 6)
  - `bool ReplaceNameToken(std::string& name, const char* guidName);` (0x70CDF0 "$CN")
  - `bool AddUnknownEventWhileLoading(const char* name);` (0x81B380; true when added)
  - `uint8_t LfgRoleClass(uint8_t playerClass);` (0x6B1080 for the five LFG call sites, via
    ChrClassesRoles.dbc)

- [ ] **Step 1: Failing tests:** `AttachFixes_MissingFileLoggedOnce` (two opens of the same missing name → one
  MissingFiles line), `AttachFixes_NameTokenReplaced` ("$CN's Pet" with "Thrall" → "Thrall's Pet"; no token →
  unchanged, false), `AttachFixes_UnknownEventAddedWhileLoading` (unknown name while loading → added; when not
  loading → refused), `AttachFixes_LfgRoleClassFromDbc` (fixture ChrClassesRoles.dbc row maps class 12 to a
  stock role class).
- [ ] **Step 2: Run CHECK.** Expected: FAIL.
- [ ] **Step 3: Implement** and place all 37 remaining hooks, batch by batch, each marked. The tamper reports
  (0x4181B0 / 0x84E400, batch 5) are ported with their mute flag never set, as the reconstruction notes.
- [ ] **Step 4: Run CHECK.** Expected: PASS; inventory lists none of the 38 addresses as missing hooks.
- [ ] **Step 5: Commit** `feat(CoA): attach fixes`.

### Task 5: AscAttachLua (11) and AscAttachMisc (7)

**Files:**
- Create: `CoA/Source/Modules/AttachLua.cpp` (+`Client`), `AttachMisc.cpp` (+`Client`),
  `CoA/Tests/AttachLuaTest.cpp`
- Modify: hook sites 0x48FC30, 0x48FCE0, 0x4FBD20, 0x510AC0, 0x557BE0, 0x567450, 0x5846E0, 0x584E10, 0x5854C0,
  0x5DB2A0, 0x5DB810, 0x635710, 0x801C90 (AttachLua) and 0x81AC90, 0x7668C0, 0x403DE0, 0x5B3020, 0x5B32F0,
  0x5B3610, 0x808200 (AttachMisc)

**Interfaces:**
- Produces: `std::string CoA::AttachLua::CombatLogFileName(const SYSTEMTIME&);` ("Logs\<date-time>
  WoWCombatLog.txt", exact format from 0x4FBD20); `bool CoA::AttachMisc::CVarSetAllowed(const char* name, const
  char* value);` (scriptProfile only "0").

- [ ] **Step 1: Failing tests:** `AttachLua_CombatLogFileName`, `AttachMisc_ScriptProfileOnlyZero`,
  `AttachMisc_SignalEventPastTableRefused` (an id past the event table → refused, Fatal channel line).
- [ ] **Step 2: Run CHECK.** Expected: FAIL.
- [ ] **Step 3: Implement** and place the 18 hooks (GLOBAL_MOUSE_DOWN/UP signals are dropped as in the
  original — the events are never registered).
- [ ] **Step 4: Run CHECK.** Expected: PASS.
- [ ] **Step 5: Commit** `feat(CoA): attach Lua and misc hooks`.

### Task 6: Plan 2 deferrals

**Files:**
- Create: `CoA/Source/Modules/AnimationCount.h/.cpp`, `CoA/Tests/AnimationCountTest.cpp`
- Modify: the 44 AnimationCount sites; `CoA/Source/Modules/BridgeStoreClient.cpp` (run it from
  `AfterClientDBInitialize`, before the bridge tables, as the original's set does); `CoA/Source/Modules/Profiler.cpp`
  (+`Client`: the 30-second packet watchdog thread); SaveSavedVariables (`AscAddonTaint` / `AscLibrary` call
  sites); the raid-video / graphics / cursor CVar callbacks left null in Plan 2 (`GlueCVars.cpp`)

**Interfaces:**
- Produces: `uint32_t CoA::AnimationCount::Rows();` (stock 506 until set);
  `void CoA::AnimationCount::Set(uint32_t rows);` — every one of the 44 sites reads `Rows()`; fixed arrays sized
  506 become sized from `Rows()` at load.

- [ ] **Step 1: Failing tests:** `AnimationCount_DefaultIsStock506`, `AnimationCount_FollowsLoadedRows` (Set(700)
  → Rows() 700), `Profiler_WatchdogDisconnectsAfter30sSilence` (the watchdog's decision function with an
  injected clock: 29 s → no, 30 s → yes, as the original's comparison), one test per CVar callback whose effect
  is observable without the client (value clamping / echo as the reconstruction does).
- [ ] **Step 2: Run CHECK.** Expected: FAIL.
- [ ] **Step 3: Implement**; convert the 44 sites (ledger any site whose array cannot be resized in place).
- [ ] **Step 4: Run CHECK.** Expected: PASS; inventory lists no AnimationCount address as missing.
- [ ] **Step 5: Commit** `feat(CoA): animation count, packet watchdog, saved variables, CVar callbacks`.

### Task 7: World halves of the already-ported modules

The modules ported in Plans 1–2 (`Misc`, `GlobalsB/C/D/G/H/J`, `SmallApis`, `PatchData`, `EventPackets`,
`Config`, `Account`, `RealmInfo`, `Bindings`-owned natives, `ClientDbcPatch` packets) registered their glue
parts only. Their remaining natives, packets and events (≈220 natives, ≈60 packets per the inventory) belong to
the world.

**Files:** Modify the existing module files and their `*Client.cpp`; tests in the existing `*Test.cpp` files;
create `CoA/Source/Modules/ClientDbcPatch.cpp` and `CoA/Tests/ClientDbcPatchTest.cpp` if not present.

- [ ] **Step 1: Failing tests:** for each module, its world natives against an empty store (original defaults,
  never an error) and with a fed packet; `WorldReload_NativesBackAfterGlue` (register world, then glue, then
  world again: every world native present on the last state); `PatchData_WorldRegistrationsRebuild` for each
  PatchData opcode with a callback.
- [ ] **Step 2: Run CHECK.** Expected: FAIL.
- [ ] **Step 3: Implement** the missing natives / packets / events of these files, porting from their Asc*.cpp
  counterparts.
- [ ] **Step 4: Run CHECK and inventory.** Expected: PASS; none of these files' natives or packets listed as
  missing.
- [ ] **Step 5: Commit** `feat(CoA): world halves of the ported modules`.

### Task 8: World-entry acceptance and findings

**Files:** Create `docs/superpowers/plans/2026-10-25-coa64-world-findings.md`; modify
`CoA/inventory-report.md`, `CoA/addrmap.tsv` (regenerated).

- [ ] **Step 1: 32-bit baseline.** Run `Ascension.exe` (user-approved for baselines), log in, enter the world
  with the same character, screenshot `out/baseline/7-world.png`, copy `Logs\FrameXML.log` to
  `out/baseline/FrameXML-world.log`, log out. Within the Warden window.
- [ ] **Step 2: CoA64 run.** Same steps with `CoA64.exe -extlog 1`; screenshot `out/baseline/x64/7-world.png`;
  wait 3 minutes in the world; log out.
- [ ] **Step 3: Measure.** `gluelog.py --base out/baseline/FrameXML-world.log --new D:\WoW\Coa\Logs\FrameXML.log`;
  every `Unhandled packet` line in `Logs\Fatal.txt` with its opcode name; every Lua error (`Logs\LUA.txt`,
  error dialog); every missing native named in those errors.
- [ ] **Step 4: Write findings**: one table per reconstruction module (`Asc*.cpp`) — opcodes, natives, Lua errors
  seen at world entry — and the proposed order of Plans 4+ (modules seen at world entry first, then by size from
  the inventory).
- [ ] **Step 5: Run CHECK.** Expected: PASS.
- [ ] **Step 6: Commit** `docs: world-entry findings`.
