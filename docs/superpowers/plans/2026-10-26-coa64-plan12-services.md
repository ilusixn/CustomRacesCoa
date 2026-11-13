# CoA64 Plan 12: Player Services (Tickets, Bugs, Polls, Recovery, Store, Callboard, Friends, Chat, Discord)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Port AscGMTicket (C_GMTicket), AscBugTracker, AscPlayerPoll, AscRecoveryService, AscCustomStore, AscCallboard /
AscCallboardCache, AscFriendsMetadata, AscChatCheck and AscDiscord. Removes the live `C_CustomStore` /
`C_PlayerTicket` errors and fills the Wildcard `forEachCompletion` Dep (Callboard).

**Architecture:** as in Plans 4..11: pure modules with `*Client.cpp` Deps halves; containers via
`CoA::Filter::Container`; timers via `CoA/Source/Runtime/Timer.h`. Discord loads `discord_game_sdk.dll` at run time
through `LoadLibrary` / `GetProcAddress("DiscordCreate")` (the install ships the x86 SDK, which an 64-bit process cannot
load, so the "no core" path runs: CMSG 0x52E) -- a ledgered ruling; the SDK callback thread is kept.

**Spec:** `docs/superpowers/specs/2026-10-25-coa64-client-design.md`. Reference:
`F:\WorkDir\ascension-extensions-reconstruction\src\Ascension\Asc{GMTicket,BugTracker,PlayerPoll,RecoveryService,
CustomStore,Callboard,CallboardCache,FriendsMetadata,ChatCheck,Discord}.cpp` (pin `1b9ebca`).

## Global Constraints

- Reproduce the original exactly, bugs included; every deviation is a ledgered Ruling.
- Reads past a packet's end follow the 7a convention (counts guarded against the bytes left, ledgered).
- Natives register with COA_NATIVE in the original's namespaces; events with COA_EVENT; client reached only through Deps.

## Review Focus

1. GMTicket's record wire order (0x110-byte record) and the snapshot container's filters / sort; every binding gated on
   client 0x5191C0(0x11).
2. CustomStore / RecoveryService purchase and restore flows: CMSG payloads byte-exact, result codes mapped as the
   original maps them.
3. Callboard completions feed Wildcard's `forEachCompletion` and the cache survives / clears where the original does.
4. Discord with no loadable SDK: Disabled, CMSG 0x52E sent once, no thread, no crash; SMSG 0x90C..0x90E harmless.
5. Glue resets clear exactly what the originals clear.

---

### Task 1: GMTicket, BugTracker, PlayerPoll, ChatCheck, FriendsMetadata

Tests: ticket record round trip, SMSG 0x709 append / clear / reset, container filters and sort, the 0x5191C0 gate,
every CMSG; bug report CMSG payloads; poll packets / natives / events; chat-check rules; friends metadata packets.

### Task 2: RecoveryService, CustomStore, Callboard (+Cache), Discord

Tests: recovery lists and restore CMSGs, store catalogue / purchase / results and events, callboard packets / cache /
`forEachCompletion` wired into Wildcard, Discord no-SDK path and the activity packet parsing (pure part), glue resets.

### Task 3: Live re-measure

In-world capture; findings doc "Plan 12 result"; inventory report regenerated.
