# Quest log 64: rulings made during implementation

Decisions Claude made on the user's behalf while executing the plan (2026-11-25), in order, each with what it costs if wrong.

- T1 Snapshot keeps no logSize param; the test asserts Snapshot returns slot 60 and BuildState(entries, 30, true) still contains it — spec says every occupied slot is sent regardless of logSize — cost if wrong: trivial test edit.
- T2 capability flag on Player, cleared at login and set by each HELLO — client sends HELLO on every world entry, so per-Player equals per-session in practice — cost if wrong: small move to WorldSession.
- T10-13 verified by port fidelity to CoA64 + in-game checks, not unit tests — they detour a closed x86 binary; reviewers compare against the CoA64 bodies — cost if wrong: defects found later in game.
- T8/T14 in-game checks require the user to run the client; when reached, prepare builds and a checklist and ask the user — cost: wait on user.
- server verification uses --settings conf/verify-ql64.json (build_directory var/build/ql64, gitignored) — the user's running authserver/worldserver lock var/build/coa/bin and stopping them is a side effect outside the task — cost: one full extra server build.
- Task 3 also pushes a full 0x8F7 (new logSize) to every online capable player when Quests.LogSize changes on config reload — user asked that the max quest log size be configurable in worldserver.conf; reload is the remaining gap — cost: small extra hook.
- Task 3 swap bound tightened beyond the plan's flat 64 — swap refused if it moves a quest into an empty slot >= capacity, or a pinned (timed) quest into slot >= 25 — spec non-goal keeps server slot rules unchanged and logSize caps quests — cost: a crafted client cannot rearrange into empty high slots (no legitimate UI does that).
- m_sortTypes grows to 64; collapse-filter bit tests keep their < 25 guard (sort types past 25 never filter-collapsed) — Update 0x5E6940 appends unbounded, 64 quests can exceed 25 sort types (overflow); spec non-goal amended — cost if wrong: a 26th+ zone header cannot be collapsed via the saved filter.
- descriptor-range rows (m_playerQuestLog[25] data, Set/UnsetActiveMirrorHandlers, ScriptEventsRegisterUnit, OnUpdateQuest bound) are ignore — anti-goal: descriptor never changes; OnUpdateQuest keeps a refresh-path note.
- client clamps logSize from 0x8F7 to [25,64] (not reject) — server never sends outside, clamp keeps the slot data usable — cost: none.
- Task 6 test = option A — QuestLogExtended::Mirror(slot) (39 mirror entries refreshed on read, zeroed when empty) unit-tested; GetQuestLogEntry/IsOnQuest/64-slot loops verified by build + sites table + Task 8 in-game — CoATests does not link the decomp; linking it is a large build change — cost: decomp loop edits have no unit coverage.
- run Part C while the user does Task 8 — DLL tasks do not depend on in-game results; bugs found get ported — cost: possible rework in ported functions.
- Task 10 also ports ApplyInterfaceLimit (MAX_QUESTLOG_QUESTS); Task 13 ports the refresh (QuestLogSlotUpdated / OnChanged) with a single QUEST_LOG_UPDATE — Task 9 deferred both; Lua API belongs with Task 10, refresh with OnUpdateQuest in Task 13.
- every SlotEntry caller sits under #if ASCENSION_QUEST_LOG_64; detours at an address the DLL already hooks are merged into the existing hook, not stacked.
- remaining functions that only index the relocated list or check list/sort bounds are fixed by IDA-verified operand patches (array address -> DLL list, immediate bounds 50->128 / 25->64 for list/sort bounds only), not full ports; functions whose logic walks descriptor slots are ported — keeps client logic, less code; plan said port — cost if wrong: a missed operand reference reads the stale array (Task 14 xref audit catches it).
- copy-back into the client arrays is removed once no unreplaced reader remains (verified by the xref audit in this task).
- Task 12 ports 0x5E3840 (GetPOIQuestIDByVisibleIndex) instead of operand-patching it — patching would join the watch group and a POI byte mismatch would disable the 64 watches — cost: one more port.
- copy-back kept only as degraded-mode fallback (runs when a patch group fails to apply, logs once) instead of removed — without it an unpatched function reads an empty list — cost: a few dormant lines.
- placement for extended sessions bounded by min(Quests.LogSize, QuestLogExtendedSize()); snapshot unchanged (still sends occupied slots above) — CodeRabbit major — cost: none for 64-slot clients.
- park — native<->overflow CMSG_QUESTLOG_SWAP_QUEST and PROMOTE whisper can still mis-announce "Quest accepted" on an extended client — neither CoA64 nor the DLL sends the swap and the window (only PROMOTE source) is closed for extended clients — cost if wrong: a stray accept message / dropped watch on a crafted client.

User decisions: subagent-driven execution; ASCENSION_QUEST_LOG_64 on by default; Quests.LogSize stays the configurable limit (reload pushes it live); live DLL ships with hot patch + DBC streaming (ship preset); keep promotion and fix its display.
