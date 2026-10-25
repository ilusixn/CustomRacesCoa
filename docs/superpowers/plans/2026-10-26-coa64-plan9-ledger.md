# SDD ledger — plan: docs/superpowers/plans/2026-10-26-coa64-plan9-game-modes.md
Pre-flight: Token (Task 1) produces TypeIndex/AtLeast consumed by CA tokenAtLeast; Inflate (Task 1) consumed by Task 3 — consistent.
Task 1: Ruling: Inflate passes the source length by value — the DLL pushes &srcLen where 0x778180 reads a value (client disasm: [ebp+0x14] -> avail_in), so the original's stream gets a stack address as input size; well-formed streams are identical — cost if wrong: a truncated stream fails here where the original reads past its buffer.
Task 1: Ruling: Token 0x66F / Seasonal 0x9BE counts stop at the packet's end (7a convention) — cost if wrong: none for well-formed packets.
Task 1: Ruling: mutation "total < 3" in Inflate survives as equivalent (3 bytes fail inflate either way; the original's memcpy over-reads) — cost if wrong: none.
Task 1: Ruling: CA tokenAtLeast now filled from Token::AtLeast(token, count, 0) in CAClient — cost if wrong: CA token gates misjudge.
Task 1: complete (commits 01f0b0e..f8c1033, tests: check.bat → 377 tests, 0 failed; mutants 10/11 caught, 1 equivalent)
Task 2: Ruling: CanToggleBuildDraftMode's "Hero" check reads the player's class through Deps (the active player object is always a player, so the original's type 3 / 4 test is folded into hasPlayer) — cost if wrong: none while 0x4038F0 yields players only.
Task 2: Ruling: BuildDraft 0x637 indices >= 0x10000 are dropped (the original grows the list to any index, i.e. a multi-GB allocation); list counts stop at the packet's end — cost if wrong: a slot that high is never shown.
Task 2: complete (commits f8c1033..b64d0ae, tests: check.bat → 381 tests, 0 failed; mutants 11/11 caught)
Task 3: Ruling: DraftMode and ScrollOfFortune share one Deps (DraftMode::Dependencies: hasPlayer, playerLevel, itemCount = CA::UnitItemCount of the active player, now) — the originals share FUN_10308570 and read the same player fields — cost if wrong: none.
Task 3: Ruling: the 0x753 trailing skip and the 0x6B7 / 0x6B8 record counts stop at the packet's end (7a convention) — cost if wrong: none for well-formed packets.
Task 3: complete (commits b64d0ae..957b220, tests: check.bat → 385 tests, 0 failed; mutants 13/13 caught after 4 added checks)
Final review: Opus reviewer — no Critical, no Important, 5 Minor.
Final: fixed truncated 0x753 adding zero picks (re-graded from M1: violates the plan's count-guard constraint) — DraftMode_ShortPacket RED→GREEN, suite 386/386
Final: Ruling: 0x6AA / 0x6AB inflated record loops stop at the inflated data's end (reference guard; the genuine loop reads n records unchecked) — cost if wrong: none for well-formed data.
Final: minor (deferred): Read::Inflate resizes to the declared u32 size before inflating; a lying size can allocate up to 4 GB on x64 (malformed packets only).
Final: minor (deferred): tests do not pin 0x6AA firing nothing, the <4-byte 0x6AA case, or Seasonal custom points / achievements surviving the glue screen.
Final: fixed plan Review Focus #1 wording (failed inflate clears first) — doc only.
