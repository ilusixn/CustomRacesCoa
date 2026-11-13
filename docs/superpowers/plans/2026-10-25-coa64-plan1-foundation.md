# CoA64 Plan 1 — Foundation (build, exe patches, address map, CoA runtime) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `CoA64.exe` (64-bit) builds from this repo, loads the CoA install's data from `D:\WoW\Coa` to the CoA
login screen, carries Ascension's exe patches, and has a `CoA` module with the runtime plumbing every later
Extensions.dll port uses (attach, logging, custom opcode routing, custom events, Lua registration, CVar lists),
plus the tools that measure porting coverage.

**Architecture:** The decomp tree stays as seeded. A new static library `CoA` (`CoA/Source/...`) is linked into
the `Wow` executable target. Hooks from the reconstruction become calls from the decomp function at the hook
point into `CoA`, marked `// CoA: <recon file:function> (orig hook 0xADDR)`. Python tools under `Tools/coa/`
(stdlib only, `unittest`) map 32-bit addresses to decomp symbols and diff the CoA registrations against the
reconstruction's inventory.

**Tech Stack:** C++ (MSVC v143, VS 2022), CMake ≥ 3.10 with Ninja or the VS generator, 64-bit; Python 3 stdlib.

**Spec:** `docs/superpowers/specs/2026-10-25-coa64-client-design.md`

**Scope of this plan:** design phases 1–2 only. Plans 2 (glue: login, realm list, character select/create —
the 167 glue natives) and 3..n (world modules in batches) are written after this plan
lands, from the coverage reports Task 3/Task 9 produce. They are not optional; they are just not plannable at
task granularity before the address map exists.

## Global Constraints

- Behaviour reference: `F:\WorkDir\ascension-extensions-reconstruction` at commit `1b9ebca`. Reproduce its
  behaviour including bugs. Do not modify that repo or `F:\WorkDir\WoWClients\wowdecomp`.
- No raw 32-bit client addresses or 32-bit struct offsets in 64-bit code; resolve to decomp symbols/members.
- No runtime detours, `VirtualProtect` or byte writes into our own image; edit the source.
- Every edit to decomp code (outside `CoA/`) carries `// CoA:` with the reconstruction file:function and the
  original hook/patch address.
- Executable name `CoA64.exe`, output to `bin/Win/`. It runs with working directory `D:\WoW\Coa`; nothing in
  `D:\WoW\Coa` is modified except by copying `CoA64.exe` (+ `.pdb`) in.
- Not loaded: `MMgr64.exe`/MemoryBridge, `DivxTac.dll`, the account kill switch, Ascension's logon hooks
  (`0x8CCE00`, `0x9A83E0`, `0x9A88C0`), the plaintext-header stub (`0x466BF0`). Warden stays as wowdecomp has it.
- The live test account credentials are typed into the client only — never written to a file, commit, log,
  test, or memory.
- Commit messages end with `Claude-Session: https://claude.ai/code/session_01R4uvBjQUXZDyfVjuqrqs5p`.

## Review Focus

1. **Archive count above 64.** `D:\WoW\Coa\Data` has ~70 `patch-*.MPQ` (multi-letter names like `patch-CA.MPQ`,
   `patch-WB1.MPQ`). Expected: all open; none silently dropped. Task 2 verifies the opened count in the log.
2. **Patch priority order with multi-letter names.** `patch-*.MPQ` sorted descending (stock `CompareStrings`)
   must give the same precedence Ascension.exe gives, so `patch-Z`/`patch-ZG` override `patch-B`. Task 2's
   check compares a file present in several archives (`Interface\GlueXML\RealmData.lua`) against the 32-bit
   client's loaded copy hash.
3. **Opcodes above 0x55E reaching stock tables.** A custom opcode must never index the stock handler array.
   Task 6 tests an opcode `0x55F` and the highest Ascension opcode.
4. **Lua state rebuilt on every glue/world transition.** Natives must be re-registered each time; Task 7 tests
   two consecutive state creations.
5. **Per-character CVars after logout** (flags `0x21`): world-list CVars deleted on logout and re-created; slot
   pointers must not dangle. Task 8 tests register → delete → re-register.

---

## File Structure

- `CMakeLists.txt` (modify): `add_subdirectory(CoA/Source)`.
- `WoW/Source/CMakeLists.txt` (modify): output name `CoA64`, link `CoA`.
- `CoA/Source/CMakeLists.txt`: static lib `CoA`, globs `CoA/Source/**/*.cpp`.
- `CoA/Source/CoA.h/.cpp`: `CoA::Attach()`; module init list.
- `CoA/Source/Runtime/Log.h/.cpp`: `-extlog` developer log + `C_Logger` channels (port of `AscLog`/`C_Logger`).
- `CoA/Source/Runtime/Packets.h/.cpp`: custom opcode table (port of the scaffold `CustomPackets` + `AscRuntime`
  packet routing).
- `CoA/Source/Runtime/Events.h/.cpp`: custom FrameXML event names + `Signal` (port of `AscRuntime` events,
  `AscEvents.generated.inc`).
- `CoA/Source/Runtime/LuaReg.h/.cpp`: native registration per state (port of `AscBindings` registration and
  `AscBindingStates.generated.inc`).
- `CoA/Source/Runtime/CVars.h/.cpp`: glue/world CVar lists (port of `FUN_101145c0`/`FUN_10114540` lists).
- `Tools/coa/addrmap.py`, `Tools/coa/inventory.py`, `Tools/coa/tests/test_*.py`.
- `CoA/addrmap.tsv`, `CoA/inventory-report.md`: generated, committed.
- `CoA/DIFFERENCES.md`: deliberate differences list (from the spec).

---

### Task 1: Build the seeded tree as `CoA64.exe` and run it on CoA data

**Files:**
- Modify: `WoW/Source/CMakeLists.txt` (`WOW_OUTPUT_NAME`), `GameVersion.h` (`CLIENT_NAME`), `README.md` (replace
  intro with CoA64 purpose + build/run, keep wowdecomp credit), `BuildWaystone.bat` → rename `BuildCoA64.bat`
  (target `Wow`, package name `CoA64`).

**Interfaces:**
- Produces: `bin/Win/CoA64.exe`, `bin/Win/CoA64.pdb`; build dir `out/build/x64-Release` (Ninja,
  `RelWithDebInfo`).

- [ ] **Step 1:** Configure + build unchanged tree: from a vcvars64 shell
  `cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -S . -B out/build/x64-Release` then
  `cmake --build out/build/x64-Release --target Wow`. Expected: `bin/Win/Wow64.exe` exists. If it fails, fix only
  what the copy broke (paths) — record anything else and stop.
- [ ] **Step 2:** Set `WOW_OUTPUT_NAME` to `CoA64` for 64-bit, `CLIENT_NAME` to `"CoA64"`; rebuild. Expected:
  `bin/Win/CoA64.exe`.
- [ ] **Step 3:** Copy `CoA64.exe`/`.pdb` to `D:\WoW\Coa`, launch with cwd `D:\WoW\Coa`. Expected: window opens and
  reaches a login screen or a recorded failure. Screenshot it; note archive-related errors. This is the baseline
  for Task 2.
- [ ] **Step 4:** Commit `build: produce CoA64.exe from the seeded tree`.

### Task 2: Port Ascension.exe's own patches

The 32-bit diff (Ascension.exe vs `D:\WoW\world of warcraft 3.3.5a hd\wow.exe`) found these sites. That reference
exe is itself modified, so **each site is first checked against the decomp source**: port only where
Ascension.exe differs from the original logic the decomp implements. Record each site's verdict in
`CoA/DIFFERENCES.md` § Exe patches.

| Site | Ascension.exe | Decomp location | Port as |
|---|---|---|---|
| `.rdata 0x9E2716` | pattern `patch-*.MPQ` (was `patch-?.MPQ`) | `PatchFiles.cpp` `PatchFiles::PatchFiles` spec 0 | pattern literal |
| `0x405E2E`, `0x5D0046/0x5D0069` | archive limit `0x40`→`0x7F` | `Client.cpp` `OpenArchives` (`priority = 64`, `SetCount(64)`, `basePriority = 63`) and the untagged function at `0x5D0040` | constants; port `0x5D0040` with OFFSET tag if absent |
| `.rdata 0x9E2A18` | `"Ascension.gg"` replaces `"World of WarCraft (build 12340)"` | find xrefs in decomp (error/crash caption) | string |
| `.rdata 0x9FC672` | `"|Htuest:"` replaces `"|Hquest:"` | find the user of that literal | string |
| `0x4648B3` | `push 0x3038` (was `0x3034`) | find containing function (untagged) | constant; realm-data `buildinfo` later rewrites it (Plan 3) — expose as a variable |
| `0x4DA7FC/809/816` | branches retargeted `0x4DA816`→`0x4DA835` | containing function (untagged) | control flow |
| `0x4DCCF0` | body `return 0` | `GlueScriptEvents_Scripts.cpp:275` | body |
| `0x4E0F53…0x4E0FC0` | local array `0x28`→`0x78` bytes (10→30 entries) | `CharacterCreation.cpp:395` | array size |
| `0x40B7D0` + cave `0x4E5CB0` | sets dword `0xB6B474 = 1`, `LoadLibraryA("Extensions.dll")`, then original | `Win64/WinMain.cpp:37` | set the decomp global for `0xB6B474` to 1 and call `CoA::Attach()` (Task 5) at that point |
| `0x510089`, `0x68EDD7`, `0x727430/43E` | differ from the HD exe | decomp source | expected to be HD-exe-only changes: verify, record, do not port unless Ascension differs from the original |

- [ ] **Step 1:** For each row, read the decomp code and disassemble both exes at the site (capstone script in
  `Tools/coa/exediff.py`, committed, prints both sides) to determine the verdict.
- [ ] **Step 2:** Apply the source edits with `// CoA: Ascension.exe patch 0xADDR` markers.
- [ ] **Step 3:** Rebuild; run in `D:\WoW\Coa`. Expected: login screen is the CoA/Descension one (Descension logo,
  GitHub/Discord/database buttons — compare against a screenshot of 32-bit `Ascension.exe` at the same screen).
  Glue Lua errors about missing natives are expected until Plan 2; archive/load errors are not.
- [ ] **Step 4:** Log the archive count opened (temporary `CoA` developer log line in `OpenArchives`, kept behind
  `-extlog`) and confirm it equals the number of `patch*.MPQ` + base archives in `D:\WoW\Coa\Data` (+ locale).
- [ ] **Step 5:** Commit `feat: port Ascension.exe exe patches`.

### Task 3: Address map tool

**Files:** Create `Tools/coa/addrmap.py`, `Tools/coa/tests/test_addrmap.py`, `CoA/addrmap.tsv`.

**Interfaces:**
- Produces: `python Tools/coa/addrmap.py --decomp . --recon <recon> --out CoA/addrmap.tsv` →
  TSV columns `addr  kind  recon_refs  decomp_symbol  decomp_file:line  status`;
  `kind ∈ {func, data, unknown}`, `status ∈ {mapped, unported, manual, n/a}`. A `manual` row is filled by hand
  in `CoA/addrmap.manual.tsv` (same columns) and merged; re-running never drops manual entries.
- `addrmap.index_offsets(root) -> dict[int, tuple[str, str, int]]` (addr → symbol, file, line).
- `addrmap.scan_recon(root) -> dict[int, list[str]]` (addr → `file:line` refs), addresses in
  `0x00401000..0x00DFFFFF` only (exclude DLL `0x10xxxxxx` and small constants).

- [ ] **Step 1: tests** — `test_index_offsets` on a temp file `// OFFSET: 0x00405DD0\nstatic void OpenArchives() {`
  → `{0x405DD0: ("OpenArchives", ..., 2)}`; `test_scan_recon` finds `0x81B530` in
  `reinterpret_cast<int(__cdecl*)(int)>(0x81B530)` and ignores `0x10A3CF90` and `0x7F`; `test_merge_manual`
  keeps a manual row over an `unported` one.
- [ ] **Step 2:** run `python -m unittest discover Tools/coa/tests` → FAIL.
- [ ] **Step 3:** implement (symbol = first identifier before `(` on the next non-comment line; classify `func` when
  in the decomp's OFFSET index or when the recon casts it to a function pointer/`Detour`/`ReplaceFunction`,
  `data` when dereferenced as a pointer).
- [ ] **Step 4:** tests PASS; generate `CoA/addrmap.tsv` against the pinned recon; print summary counts.
- [ ] **Step 5:** Commit `tools: address map from OFFSET tags`.

### Task 4: Inventory checker

**Files:** Create `Tools/coa/inventory.py`, `Tools/coa/tests/test_inventory.py`, `CoA/inventory-report.md`.

**Interfaces:**
- `inventory.parse_reference(md_path) -> Inventory` with sets `natives: set[(ns, name, state)]`,
  `packets: set[int]` (opcode), `cvars: set[str]`, `events: set[str]`, `hooks: set[int]` — parsed from the tables
  in `docs/DLL_REFERENCE.md` (Lua API index rows `| \`Name\` | state | ... |` under `### <namespace>`, plus the
  packet/CVar/event/hook tables).
- `inventory.scan_coa(root) -> Inventory`, reading registrations from `CoA/Source` via the macros defined in Tasks
  6–8: `COA_NATIVE(ns, name, state, fn)`, `COA_PACKET(opcode, fn)`, `COA_CVAR(name, ...)`, `COA_EVENT(name)`, and
  `// CoA:` markers containing `orig hook 0xADDR` for hooks.
- CLI writes `CoA/inventory-report.md` with per-category `have/total` and the missing lists; exit 0 always
  (Plan n flips it to fail on missing).

- [ ] **Step 1: tests** — reference parser on a 6-line fixture copied from the real table (`AbandonSkillID` world)
  yields `("", "AbandonSkillID", "world")`; total natives parsed from the real `DLL_REFERENCE.md` == 1277,
  packets == 530, hooks == 400, cvars == 93 (if the doc's own counts differ, the test asserts the README's numbers
  and the mismatch is reported to the controller, not adjusted); `scan_coa` finds a `COA_NATIVE("C_Chat",
  "Foo", world, ...)` line.
- [ ] **Step 2–4:** fail → implement → pass.
- [ ] **Step 5:** generate the report (expect ~0 have); commit `tools: Extensions.dll inventory checker`.

### Task 5: `CoA` library, attach point, logging

**Files:** Create `CoA/Source/CMakeLists.txt`, `CoA/Source/CoA.h/.cpp`, `CoA/Source/Runtime/Log.h/.cpp`,
`CoA/DIFFERENCES.md`. Modify root `CMakeLists.txt`, `WoW/Source/CMakeLists.txt`, `WoW/Source/Win64/WinMain.cpp`.

**Interfaces:**
- `namespace CoA { void Attach(); }` — called once at the `0x40B7D0` cave point (Task 2 row), on the main thread.
  Runs, in order: runtime init (Tasks 6–8), then each module's `Init` from a static list
  `CoA::Module { const char* name; void (*init)(); }` in `CoA.cpp` (empty now).
- `namespace CoA::Log { void Printf(const char* fmt, ...); bool Enabled(); }` — writes `Logs\Extensions.log`
  only when the command line has `-extlog 1` (port `AscLog`, `AscArgs`).
- `namespace CoA::CLogger { void Write(const char* channel, const char* fmt, ...); }` — always on,
  `Logs\<Channel>.txt` (port of the original's `C_Logger`).

- [ ] **Step 1:** add the library + link; `Attach` logs `"CoA attach"`. Build.
- [ ] **Step 2:** run with `-extlog 1` in `D:\WoW\Coa` → `Logs\Extensions.log` contains `CoA attach`; without the
  flag → file not created/appended.
- [ ] **Step 3:** write `CoA/DIFFERENCES.md` from the spec's deliberate/x64-forced differences.
- [ ] **Step 4:** Commit `feat(CoA): library, attach point, logging`.

### Task 6: Custom opcode routing

Port the routing in `AscRuntime` / scaffold `CustomPackets`: opcodes `> 0x55E` go to the CoA table; `≤ 0x55E` to
the stock path; unknown custom opcode → `CLogger::Write("Fatal", "Unhandled packet: %s (%u)", name, op)` and
dropped unread. Opcode names come from the original's table (`FUN_102c4a90`, ported into `Packets.cpp`).

**Files:** `CoA/Source/Runtime/Packets.h/.cpp`; modify `WoW/Source/Net/NetClient/NetClient.cpp`
(`SetMessageHandler` `0x631FA0`, `ProcessMessage` `0x631FE0`) with `// CoA:` markers; test
`CoA/Tests/PacketsTest.cpp` (+ `CoA/Tests/CMakeLists.txt`, console exe `CoATests`, plain asserts, registered
with `ctest`).

**Interfaces:**
- `typedef int32_t (*CoA::PacketFn)(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);`
- `void CoA::Packets::Set(uint32_t opcode, PacketFn fn, void* param);`
- `bool CoA::Packets::Dispatch(uint32_t opcode, uint32_t time, CDataStore* msg); // true if opcode > 0x55E (handled or dropped)`
- `#define COA_PACKET(opcode, fn)` — static registration consumed by `inventory.py`.
- Client→server: `class CoA::Packet` with `U8/U16/U32/U64/F32/Str/Data` and `Send()`, little-endian, same layout as
  `AscScript::Packet`.

- [ ] **Step 1: tests** — `Dispatch(0x55E, ...)` returns false and touches no CoA handler; `Set(0x55F, fn)` then
  `Dispatch(0x55F)` calls `fn` once; `Dispatch` of an unset custom opcode returns true, does not read `msg`
  (`m_read` unchanged); highest opcode in the ported name table dispatches without out-of-range access.
- [ ] **Step 2–4:** fail → implement → pass (`ctest --test-dir out/build/x64-Release -R CoA`).
- [ ] **Step 5:** Commit `feat(CoA): custom opcode routing`.

### Task 7: Lua native registration and custom events

Port registration: the detour on Lua library setup `0x855060` (decomp `Engine/Source/lua-5.1/src/lbaselib.c:639`)
registers CoA natives for each new state *before* the client's own; glue vs world from
`AscBindingStates.generated.inc`. Custom event names appended to the client's event table at registration
(`0x81B5F0`, `Engine/Source/FrameScript/FrameScript.cpp:1608`); `Signal` fires via `0x81B530` (same file :1599).

**Files:** `CoA/Source/Runtime/LuaReg.h/.cpp`, `CoA/Source/Runtime/Events.h/.cpp`; modify `lbaselib.c` (C file:
call an `extern "C"` hook), `FrameScript.cpp`; tests `CoA/Tests/LuaRegTest.cpp`, `EventsTest.cpp`.

**Interfaces:**
- `enum class CoA::LuaState { Glue, World, Both };`
- `#define COA_NATIVE(ns, name, state, fn)` — `ns` `""` for globals; registers into a static table.
- `extern "C" void CoA_OnLuaLibSetup(lua_State* L);` — decides glue/world from the client's current state and
  registers matching natives (namespace tables created as needed).
- `void CoA::Events::RegisterNames(); int CoA::Events::Id(const char* name); template<class...A> void CoA::Events::Signal(const char* name, const char* fmt, A...);`
- `#define COA_EVENT(name)`.

- [ ] **Step 1: tests** — two consecutive fresh `lua_State`s (`luaL_newstate` + `CoA_OnLuaLibSetup` as glue) both
  have a test native `C_Test.Ping` callable returning 1; a `World`-only native is absent from a glue state;
  `Events::Id("COA_TEST_EVENT")` ≥ stock count after `RegisterNames`, `-1` for an unknown name.
- [ ] **Step 2–4:** fail → implement → pass.
- [ ] **Step 5:** in-client check: launch, `/run print(C_Test and C_Test.Ping())` is not available at glue, so use
  the glue console `-console` + `/script` equivalent or the `Logs\Extensions.log` registration count line. Remove
  `C_Test` after the check.
- [ ] **Step 6:** Commit `feat(CoA): Lua native registration and custom events`.

### Task 8: CVar lists

Port the glue list (`FUN_101145c0`, 19 CVars, registered with the client's at startup) and world list
(`FUN_10114540`, 74, registered after each world load; flags `0x21` deleted on logout). Each registration
writes its `CVar*` into the entry's slot; readers always read the slot.

**Files:** `CoA/Source/Runtime/CVars.h/.cpp`; modify the decomp's CVar startup registration site and
world-load/logout sites (find via the reconstruction's hook addresses in `addrmap.tsv`); test `CoA/Tests/CVarsTest.cpp`.

**Interfaces:**
- `struct CoA::CVarEntry { const char* name; const char* help; uint32_t flags; const char* def; CVar** slot; bool world; };`
- `#define COA_CVAR(name, help, flags, def, slot, world)`
- `void CoA::CVars::RegisterGlue(); void CoA::CVars::RegisterWorld(); void CoA::CVars::OnLogout();`

- [ ] **Step 1: tests** — register world list → slot non-null; `OnLogout` → slots of `0x21` entries are null and
  the CVar no longer looks up; `RegisterWorld` again → new non-null slot, lookup returns it.
- [ ] **Step 2–4:** fail → implement → pass.
- [ ] **Step 5:** Commit `feat(CoA): glue/world CVar lists`.

### Task 9: Foundation check and Plan 2 inputs

- [ ] **Step 1:** Release build clean; `ctest -R CoA` passes; `python -m unittest discover Tools/coa/tests` passes.
- [ ] **Step 2:** Regenerate `CoA/addrmap.tsv` and `CoA/inventory-report.md`; commit them.
- [ ] **Step 3:** Launch in `D:\WoW\Coa`, log in to `logon.coa-development.org` with the user-supplied test account
  (typed into the client, never stored). Expected at this stage: login succeeds (stock SRP6) and the realm list /
  character screens appear, possibly with Lua errors for missing natives. Record what is seen in
  `CoA/inventory-report.md` § Live check (no credentials).
- [ ] **Step 4:** Commit `chore: foundation coverage reports`. Hand back to the controller to write Plan 2 (glue)
  from the report.
