# Quest log of 64 quests in the client: design

Date: 2026-11-25. Repos: `azerothcore-wotlk-coa` (server), `CoA64` (64-bit client, source),
`ascension-extensions-reconstruction` (32-bit `Extensions.dll` for `Ascension.exe`).

## Intent

The server already lets a character hold up to 64 quests (`Quests.LogSize`, 25–64, commit `30095a9661`).
The 3.3.5a client only knows 25, so quests in server slots 25–63 are shown in a server-sent Lua "Extra
Quests" window (`src/server/coa/AscensionQuestLogOverflow.cpp`) instead of the real quest log. This work
makes both clients treat all 64 slots as ordinary quest log entries: quest log window, tracker, map
markers, quest-item loot, "needed for a quest" checks on items, objects and creatures, abandon, sharing.

## Goals

- Updated clients (CoA64, `Ascension.exe` with the new DLL) show and use every quest the server holds,
  up to `Quests.LogSize`, exactly like a quest in slots 0–24.
- On those clients the Extra Quests window and its info-text progress lines are no longer sent.
- Clients without the update keep working exactly as today (window included).

## Non-goals

- No change to the player update fields (`SMSG_UPDATE_OBJECT`). Slots 0–24 stay where they are.
- No change to the patch MPQs. Interface limits are adjusted from the DLL / CoA64 at runtime.
- No change to the server's slot logic: promotion of overflow quests into a freed native slot and keeping
  timed quests in native slots stay as they are.
- The quest-sort collapse filter keeps its behaviour: its bit tests (`QuestLogCollapseFilter` CVar, guarded
  by `sortId < 25`) are unchanged. Its storage is not: `CGQuestLog::Update` (0x5E6940) appends one sort type
  per distinct zone/sort with no bound, and 64 quests can produce more than 25, so `m_sortTypes` grows to 64
  (amended 2026-11-25 after the site inventory review). Sort types past the 25th are never collapse-filtered.

## Anti-goals

- Anything that moves or resizes the player descriptor (it would break the protocol for every object).
- A client that shows a quest twice (native log plus the window), or loses a quest when slots change.
- An updated client that misbehaves against a server without this feature.

## Approaches considered

1. **Extend `SMSG_UPDATE_OBJECT`** by inserting 39 × 5 fields after slot 25. Shifts every later player
   field index on server and both clients. Rejected.
2. **Append the fields after `PLAYER_END`.** No index shift, but the player field count (~1178) is
   baked into object allocation sizes, the mirror tables (`CGPlayer::s_mirrorFields`, fallback 1178 in
   `CGPlayer::GetSavedIndexFromDataIndex` 0x4F5550) and update-mask bounds; in the 32-bit binary every
   inlined copy would have to be found and patched, and a miss corrupts every player object. It still
   needs all of the quest-log work below. Rejected.
3. **Separate packet for slots 25–63** (chosen by the user). Slots 0–24 keep arriving in the update fields;
   slots 25–63 arrive in a new CoA packet, only after the client announces support. The client merges both
   into one 64-slot log behind a single accessor.

## Protocol (expensive to change once clients ship)

Both opcodes are above `NUM_MSG_TYPES` (0x700) and unused by the genuine DLL, the reconstruction and the
server (checked 0x8F6/0x8F7 against `docs/DLL_REFERENCE.md`, `AscOpcodeNames.generated.inc` and `src/server/coa`).

**CMSG 0x8F6 `CMSG_COA_QUEST_LOG_EXTENDED_HELLO`**: `u8 version (=1)`, `u8 clientMaxSlots (=64)`.
Sent by the client on every world entry (the server forgets the capability on logout, like the hot-patch
capability 0x8F1). Claimed on the server with `AscensionCompatOpcodes::Claim`.

**SMSG 0x8F7 `SMSG_COA_QUEST_LOG_EXTENDED`**:
```
u8  version        (=1)
u8  flags          bit0 = full snapshot (client clears slots 25..63 first)
u8  logSize        server's effective Quests.LogSize, min(config, clientMaxSlots), 25..64
u8  count
count x {
    u8  slot       25..63
    u32 questId    0 = slot empty
    u32 state      QuestSlotStateMask, as PLAYER_QUEST_LOG_x_2
    u16 counters[4]
    u32 time       as PLAYER_QUEST_LOG_x_4
    u32 level      scaled quest level, as 0x578 field 61+slot for slots 0-24
    u32 rewardXP   display reward XP, as 0x578 field 36+slot for slots 0-24
}
```
- `level` and `rewardXP` are computed by the server exactly as `AscensionQuestLog::SendSlot` does for slots
  0–24 (`GetQuestLevel`, `RewardXPForDisplay`: XP rate, `OnPlayerQuestComputeXP`). The 0x578 range cannot
  hold slots 25–63: field 36+25 is slot 0's level field, and the client's per-unit addon record has only
  0x58 fields (86 and 87 are taken). Chosen by the user over computing them on the client.
- Every trigger that resends 0x578 for slots 0–24 (`AscensionQuestLogSync.cpp`: login, level change, map
  change; promotion and swap via `SendSlot`) also sends the affected slots 25–63 as a 0x8F7 delta. A quest
  promoted into slots 0–24 gets its 0x578 fields as today.
- The server answers a valid HELLO with a full snapshot, then sends deltas (only the slots that changed)
  at most once per world update tick per player, batched. A slot moving from overflow into a native slot
  appears as `questId 0` here and as the usual update-field change.
- `version` mismatch: the server ignores a HELLO it does not understand (client keeps the window
  behaviour); the client ignores an SMSG with an unknown version.
- `logSize` drives the interface's "Quests: n/N" and the client's own "log full" view. Every occupied slot
  25–63 is sent even when it is at or above `logSize` (`Quests.LogSize` is reloadable, and lowering it leaves
  quests the server still holds); `logSize` only caps new quests and the display.

## Server (`azerothcore-wotlk-coa`)

- Per-session capability flag set by the HELLO, cleared on logout.
- Snapshot on HELLO; deltas from the places that already change overflow slots (`SetQuestSlot*`,
  `SwapQuestSlot`, promotion in `QuestLogSlots::NextPromotion`), collected and flushed once per tick.
- Capable sessions: no Extra Quests window code, no `COAQLOG` messages, no info-text progress lines from
  `AscensionQuestLogOverflow.cpp`.
- Capable sessions only: `CMSG_QUESTLOG_REMOVE_QUEST` and `CMSG_QUESTLOG_SWAP_QUEST` accept slots below
  `MAX_QUEST_LOG_SIZE` (64) (today `< NATIVE_QUEST_LOG_SIZE`, `QuestHandler.cpp:396/408`), and
  `CMSG_QUEST_POI_QUERY` accepts up to 64 quest ids (today `> NATIVE_QUEST_LOG_SIZE` is rejected, `QueryHandler.cpp:423`).
  Non-capable sessions keep the 25 limits.
- Tests: unit tests next to `src/test/server/game/Modules/QuestLogSlotsTest.cpp` for the snapshot/delta
  builder; `apps/coa-gameplay-test` scenarios for a capable client (no window, packet contents, abandon of
  slot 40, promotion) next to the existing `quest-log-overflow-*.json`.

## CoA64 (reference client implementation)

- One accessor for a slot (0–63): slots 0–24 read `m_player->m_playerQuestLog`, 25–63 read a new
  `PlayerQuestLogEntry s_extendedQuestLog[39]` held by the quest log module and filled by the 0x8F7
  handler. `CGPlayer::GetQuestLogEntry` (0x5E0870) becomes that accessor; every direct
  `m_playerQuestLog[...]` use goes through it.
- A shared constant for the slot count (64) and a runtime `logSize` from the packet (25 until a packet arrives).
- About 70 functions that assume 25 (list: grep of `WoW/Source` for slot loops; `QuestLog.cpp`,
  `QuestLog_Scripts.cpp`, `Player_C.cpp`, `Player.cpp`, plus `CoA/Source/Modules` `AttachFixesClient`,
  `GlobalsBClient`, `GlobalsDClient`, `SmallApisClient`, `QuestSpellHideClient`).
- Bigger static lists: `CGQuestLog::m_quests[50]` (quests plus zone headers) to 128,
  `m_questWatches[25]` and `m_questPOIs[25]` to 64, and the fixed local arrays in the POI and quest
  ordering code (`chain[25]`, `completeList[25]`, `forces[25][2]`, ...).
- 0x8F7 handler: apply snapshot/delta, then run what a quest update-field change runs today
  (`OnUpdateQuest` 0x6DF370 path: re-sort, POI refresh, `QUEST_LOG_UPDATE`, quest-item loot state).
- HELLO sent on world entry.
- Scaled level and reward XP: the readers in `CoA/Source/Modules/InitHooksClient.cpp` (ports of the DLL's
  detours of 0x5E0420, 0x61E5C0, 0x5E0D20) loop over 25 descriptor slots and read the 0x578 fields. They walk
  all 64 slots: 0–24 from the 0x578 fields as today, 25–63 from `level` / `rewardXP` stored with the
  extended entry.
- Interface: after the UI loads, set `MAX_QUESTLOG_QUESTS` to `logSize` (and again when a packet changes it).
- Entry in `CoA/DIFFERENCES.md`. Tests in `CoA/Tests` for the accessor and packet parsing.

## DLL (`ascension-extensions-reconstruction`)

Same behaviour as CoA64, by replacing functions in `Ascension.exe`:
- Each affected function is detoured to a port of its CoA64 implementation, at the address in its
  `// OFFSET:` tag. The ported code uses DLL-owned lists (128-entry quest list, 64 watches, 64 POIs) and the
  slot accessor.
- The client's own lists (`m_quests` at 0xC237B0, count 0xC23AD0, watches, POIs) must end up unused:
  every remaining xref in `Ascension.exe` to them is found in IDA (`F:\WorkDir\genuine\`) and its function
  replaced too. Leftover readers of 0xC237B0 in this repo (`AscAttachFixes.cpp` `QuestRelevant`) switch to the
  DLL list.
- The existing hooks that walk 25 slots (`AscGlobalsB`, `AscGlobalsD`, `AscSmallApis`, `AscAttachFixes`,
  `AscQuestSpellHide`) use the accessor. The level/XP detours in `AscInitHooks.cpp` (0x5E0420, 0x61E5C0,
  0x5E0D20) do the same as in CoA64: 0x578 fields for slots 0–24, the 0x8F7 values for 25–63.
- 0x8F7 handler registered like the other hash-dispatched CoA SMSGs; HELLO sent on world entry.
- A deliberate difference from the genuine DLL: listed in `README.md` under "Deliberate differences" and
  behind CMake option `ASCENSION_QUEST_LOG_64`, default ON.
- Tests: a portable test in `tests/` (style of `native_ca_packets.py`) for packet parsing and the accessor;
  in-game check of each replaced function family.

## Order of work

1. Server: capability, packet, gating, raised limits, tests.
2. CoA64: full implementation, tested in game against the server with `Quests.LogSize = 64`.
3. DLL: port of the CoA64 changes, function family by function family (quest log data and Lua API, then
   tracker/watches, then POIs, then loot and "needed for quest" checks), each tested in game.

Each repo gets a `feat/quest-log-64` branch.

## Calls made by Claude (check these)

- Opcode numbers 0x8F6 / 0x8F7 and the packet layout above, with a version byte for later changes.
- Field order within an entry (level and reward XP last).
- Server sends `logSize`; the client limit follows the server instead of a fixed 64.
- HELLO on every world entry rather than once per login.
- List sizes: 128 for the quest list (64 quests plus up to 64 headers), 64 for watches and POIs.
- DLL feature behind `ASCENSION_QUEST_LOG_64`, default ON.

## Left to the builder

- Exact delta-batching point on the server (per-tick flush hook).
- The final list of `Ascension.exe` functions to replace (CoA64 grep plus IDA xrefs to the lists).
- Whether any Ascension interface addon (patch-B) hard-codes 25 besides `MAX_QUESTLOG_QUESTS`; if one does,
  override it the same way.
