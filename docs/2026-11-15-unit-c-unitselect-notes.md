# Unit_C / UnitSelect notes (2026-11-15)

Long-form rationale moved out of inline comments (comment budget, see CLAUDE.md).

## Click-to-move bot report removed

`AscUnitSelect.cpp:FUN_102da340` hooked `0x727400` (`CGPlayer_C::ClickToMove`). Before the original it checked the
return address; a call from past the exe image sent CMSG 0x51F `{"Ascension", "CTM", "Click-to-Move called by bot."}`
(strings XOR-decoded at run time). That report is not ported: it is unnecessary server traffic. Only the after-half is
kept: the tracking turn rate becomes `AutoInteractTurnSpeed` and the click time is recorded for
`AutoInteractCursorTime`. `ClickToMove` no longer needs `__declspec(noinline)` (it was there only so `_ReturnAddress()`
was its own).

## UnitSelect (AscUnitSelect.cpp) hook map

Installer `FUN_102daaf0`. Its seven `FUN_10114540` CVars are checked against the genuine DLL (0x102DAB13 .. 0x102DACF6:
help nullptr, flags 1; `aoeRadiusIndicator*` / `AutoInteract*` category 3, `showQuestUnitCircles` /
`ObjectSelectionCircleSelf` category 4).

| Site | DLL function | Behaviour |
|---|---|---|
| World entry | FUN_102da1c0 | the two cone splats (0x10BE3858 / 0x10BE385C), released first when loaded |
| after 0x7460C0 | FUN_102da2b0 | UnitSelectTexture / TrackingTexture / UnitSelectTexture-Quest (0x10BE384C / 850 / 854) |
| 0x72B7E0 replaced | FUN_102da6c0 | CGUnit_C::Render skipped once AutoInteractCursorTime (ms, when set) has passed since the last click-to-move; else drawn with the tracking texture |
| after 0x727400 | FUN_102da340 | tracking turn rate = AutoInteractTurnSpeed, time kept (bot report not ported, above) |
| after 0x4F6F90 | FUN_102dbf60 | the indicators, the quest circles, the player's own circle |
| every other 0x6DF050 | FUN_102db0d0 | projected-batch z pad (0x82080B): 10.0 outside a dungeon / raid, stock 2.0 inside one while any aoeRadiusIndicator* is 1; the installer writes 10.0 once at start-up |

## Unit_C.cpp

- **0x73E3C9 / 0x73ECD6 (AscAttachLate.cpp, Extensions.dll patches).** The DLL skips `UpdateBaseAnimation(0, -1)`
  (je became jmp at 0x73E3C9; NOPs plus the local's destructor call at 0x73ECD6). The WoWFix335
  npc-turn-attack-anim / missing-pre-cast-animation fixes already drop the same calls, so nothing changes; the
  destructor is implicit in the port.
- **Replace727E70 (AscRuntime.cpp, orig hook 0x727E70).** Only aura types below 0x140 fit the 40-byte set; Ascension's
  higher types would write into the following fields. The DLL's side map of types below 0x16E is read by nothing and is
  not kept.
- **Detour720BF0 (AscInitHooks.cpp, orig hook 0x720BF0).** Before the body: a spell with a class set whose visual has a
  missile, an impact kit and follow-ground flags drops the 0x100 bit of ComputeMissileTrajectory's three intersections
  (the DLL writes the immediates 0x6FD335 / 0x6FD4C7 / 0x6FD59E) until the next targeting update.
- **0x10322B50 (AscSpellVisuals.cpp, orig hook 0x720F80).** From OnSpellGo (0x80ED4E) another player's harmful spell
  has no visual while `hideOtherPlayerHarmfulSpellVisuals` is on; after every return the record takes the server's /
  the appearance's visual (zeroed when hidden). The result is the original's.
