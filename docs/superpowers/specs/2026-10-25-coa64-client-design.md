# CoA64 — a native 64-bit Conquest of Azeroth client

Date: 2026-10-25. Status: draft for approval.

## Intent

Players of Conquest of Azeroth (CoA) run Project Ascension's 32-bit `Ascension.exe` (a lightly patched
WoW 3.3.5a build 12340) plus `Extensions.dll`, which adds Ascension's UI systems, Lua API, custom packets,
CVars and client fixes. We want one **native x64 executable** that does everything that pair does, built
from source, so the client stops being limited by 32-bit address space and by a binary DLL we can only patch.

The foundation is `wowdecomp` (`F:\WorkDir\WoWClients\wowdecomp`): a C++ decompilation of 3.3.5a 12340 whose
x64 build is a complete, playable client. Every ported function carries `// OFFSET: 0x...` naming its
original 32-bit address. The Extensions.dll functionality comes from the function-by-function
reconstruction at `F:\WorkDir\ascension-extensions-reconstruction` (32-bit, ~70k lines, 214 files in
`src/Ascension`, full reference in `docs/DLL_REFERENCE.md`).

## Success

1. `CoA64.exe` (x64, Release) builds from the new repo with VS 2022 + CMake.
2. Placed in `D:\WoW\Coa` (the user's CoA install: its MPQs incl. `patch-*.MPQ` set, `Interface\`, `WTF\`),
   it logs in to **`logon.coa-development.org`** (the live CoA dev server, same source as the user's local
   `F:\WorkDir\azerothcore-wotlk-coa`), shows the CoA/Descension login, realm list and character
   select/create as the 32-bit client does, enters the world and plays.
3. Every Extensions.dll feature exists in the x64 client: 1,277 Lua natives, 530 packet handlers,
   400 client hooks, 93 CVars, its byte patches, and the ~11 patch sites Ascension made in `Ascension.exe`
   itself. Coverage is tracked against the reconstruction's own inventory, not estimated.

Testing on the live server is authorised by the user with a test account they supplied in chat.
**The credentials are never written to any file, commit, log or memory.**

## Non-goals

- No changes to the server (`azerothcore-wotlk-coa`) or to the CoA MPQs/`Interface` data.
- Ascension's own logon protocol (the DLL's hooks at `0x8CCE00`, `0x9A83E0`, `0x9A88C0`) is not built; the
  CoA server uses stock SRP6 and the stock world-header cipher, which the decomp already speaks.
- No Win32 build of the new client is required to ship (wowdecomp's Win32 build may stay for layout checks).
- No new gameplay features or UI beyond what the 32-bit pair does.
- Not a fork of wowdecomp; no GitHub publishing unless the user asks.

## Anti-goals (failure even if it "works")

- Behaviour that silently diverges from the 32-bit client: the reconstruction's rule holds — reproduce the
  genuine DLL's behaviour **including its bugs**; known original defects are recorded, not fixed. The only
  deliberate differences are the reconstruction's two (stock SRP6 logon; the `0x726` CA known-entries sync
  order) plus the x64-forced ones listed below.
- Hard-coded 32-bit addresses or 32-bit struct offsets surviving in x64 code (any raw `0x4xxxxx`–`0xDxxxxx`
  address or `ptr + 0xNN` field access into client objects). Everything resolves to decomp symbols/members.
- Runtime self-patching of our own code (detours, `VirtualProtect`, byte writes) where a source edit works.
- Credentials committed anywhere.

## Decisions made

By the user:
- New standalone repo at **`F:\WorkDir\CoA64`**, own history, not a fork.
- Target server: `logon.coa-development.org`.
- **Behaviour reference:** the reconstruction repo, pinned at commit `1b9ebca` (branch
  `parity/dispatch-guard-crash-context-diagnostics`, clean tree); the user confirmed the installed
  `D:\WoW\Coa\Extensions.dll` is built from this source. Later reconstruction commits are ported as diffs,
  recording each new pin in `UPSTREAM.md`, the same way as wowdecomp.
- **Warden:** unchanged from wowdecomp's x64 client (it receives and drops `SMSG_WARDEN_DATA`). Deferred until
  everything else is done. No spoofed answers. Known risk for live tests: if the server's Warden kicks
  non-responding clients (AzerothCore default response delay 600 s), live sessions end after ~10 minutes;
  tests are planned to fit inside that window and the kick is not treated as a client bug.
- **World headers:** stock cipher (the server's plaintext option is gone; the reconstruction's plaintext
  stub at `0x466BF0` is disabled and is not ported).

By me (the user may overrule):
- **Seeding:** one-time copy of wowdecomp at commit `20f9120e` (record it in `UPSTREAM.md`). Later upstream
  fixes are brought over as patches from wowdecomp diffs (`git diff <base>..<new>` applied with `git apply`),
  recording each new base.
- **Layout:** decomp tree kept as is (`Engine/`, `WoW/`, `ThirdParty/`, `Tools/`, `cmake/`). The CoA code lives
  in a new module **`CoA/Source/...`** mirroring the reconstruction's module split (one `.cpp` per `Asc*` module,
  same names). Decomp files are edited only at the call sites where the DLL had a hook/patch, each edit
  marked `// CoA: <reconstruction file:function> (orig hook 0xADDR)` so all touch points are greppable.
- **Hook translation:** detour → call into the CoA module at the decomp function's start/end (or the exact
  point the detour captured); `ReplaceFunction` → the decomp function body dispatches to the CoA version;
  byte patch → equivalent source change (constant, branch, buffer size). No runtime hooking library.
- **Address translation:** build a tool (`Tools/coa-addrmap`) that indexes every `// OFFSET:` tag in the copy
  into a `0xADDR → file:symbol` map, and lists every address literal in the reconstruction with its
  resolution status. Unmapped functions (≈10% of the binary is unported) must be ported into the decomp
  properly (with OFFSET tags) when the DLL needs them. Globals and data addresses (not tagged in the decomp)
  are mapped by hand to the decomp's globals; struct offsets are mapped via the decomp headers' `/* 0xNN */`
  32-bit layout comments. The map is kept as data (`CoA/addrmap.tsv`) so coverage is measurable.
- **x64-forced differences:**
  - `MMgr64.exe` / MemoryBridge dropped: the six bridged DBCs (Creature, Quest, ItemAddon, Vanity,
    SpellAffect, Mythic) load in-process. Same data, same API to the rest of the CoA code.
  - `DivxTac.dll` (32-bit anti-cheat) and the account kill switch are not loaded — a 64-bit process cannot load
    it and the CoA server does not check for it. Recorded as a deliberate difference.
  - Discord rich presence needs the x64 `discord_game_sdk.dll`; optional: absent DLL = feature off.
  - The realm-data hot-swap's `LoadLibraryA` of `*.dll` under `Data\<path>` would load 32-bit DLLs; keep the
    behaviour but it will only load x64 DLLs (32-bit ones fail to load and are logged).
  - The build-number patching (`buildinfo`, `0x9F5200`/`0x8CA5F0`/`0x4648B3`) becomes a runtime variable.
- **Exe patches** (Ascension.exe vs a 3.3.5a exe; `.text` differs in ~101 bytes at 0x405E2F, 0x40B7D0,
  0x4648B3, 0x4DA7FD/80A/816, 0x4DCCF0, 0x4E0F55–0x4E0FC3, 0x4E5CB0 cave, 0x510089, 0x5D0048/06C, 0x68EDD7,
  0x727430/43E; `.rdata` 4 runs, `.rsrc` icons/version). Examples: MPQ archive limit 0x40→0x7F (CoA ships ~70
  archives), character-create array 0x28→0x78, `0x4DCCF0` forced to `return 0`, the `LoadLibrary` cave (not
  needed — CoA code is linked in). The reference exe used for the diff (`D:\WoW\world of warcraft 3.3.5a hd\
  wow.exe`) is itself modified, so every site is confirmed against the decomp source (which is the original
  12340 logic) before porting; HD-exe-only changes are not ported.
- **Logging/CLI:** keep the reconstruction's `-extlog 1` developer log and the original `C_Logger` channels.
- **Executable name:** `CoA64.exe`; it must also accept running from `D:\WoW\Coa` beside `Ascension.exe`
  without touching it.
- **Order of work** (each phase ends runnable):
  1. Repo seeded; decomp x64 builds unchanged as `CoA64.exe`; launches against `D:\WoW\Coa` data
     (exe patches that affect loading, e.g. MPQ limit, applied first).
  2. Address map tool + CoA module skeleton: runtime plumbing (packet routing for opcodes > `0x55E`, event
     table extension, CVar lists, Lua registration on both states, logging).
  3. Glue: the 167 glue-state natives and the login/realm/char select/create modules → log in to
     `coa-development.org` and reach character select with the CoA UI identical to the 32-bit client.
  4. World: remaining modules one at a time (character advancement, collections/vanity, store, mythic, etc.)
     until the inventory is 100%; world entry and play on the live server.

## Verification

- Build: Release x64 builds clean after every phase.
- Coverage: `coa-addrmap` reports 0 unresolved addresses; a checker diffs the CoA module's registered Lua
  natives / packet handlers / CVars / events against the reconstruction's inventory (`DLL_REFERENCE.md`
  tables) — must be equal.
- Port tests: the reconstruction's portable tests (`tests/`, e.g. `native_ca_packets.py`) are re-pointed at the
  x64 code where they compile against API doubles.
- Behaviour: side-by-side runs of 32-bit `Ascension.exe` and `CoA64.exe` against `coa-development.org` —
  login, realm list, character select/create, world entry, and each module's UI — with screenshots compared
  and `Logs\` checked for `Unhandled packet` / Lua errors. Live tests use the user's test account only.

## Left to the builder

Exact CMake target structure; how the address map tool is written; which reconstruction tests are worth
porting; module order within Phase 4; how to stage decomp functions that must be newly ported; the CMake test
target layout.
