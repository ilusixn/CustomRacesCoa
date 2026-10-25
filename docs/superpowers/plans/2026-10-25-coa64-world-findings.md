# CoA64 world-entry findings (Plan 3, Task 8)

Measured 2026-10-25 on realm Pale Reach (logon.coa-development.org), character Helix Codesign (level 53, Darnassus).

- **32-bit baseline:** `Ascension.exe`, logged in, entered the world, screenshot `out/baseline/7-world.png`, logs
  `out/baseline/{FrameXML,LUA,Fatal}-world.*`.
- **CoA64:** `CoA64.exe -extlog 1` at commit 2ed77d2 (Task 7 complete), same steps, 3 minutes in the world;
  screenshot `out/baseline/x64/7-world.png`, logs `out/baseline/x64/*-world.*` (the Extensions log cut to the run).
- Both runs ended by closing the client, not `/logout`; the server logs the character out on disconnect.

CoA64 entered the world and stayed responsive for the full 3 minutes: no crash, no watchdog disconnect. These items
need work: 305 unhandled packets from 28 opcodes, Lua errors from 7 missing natives, and 4 visible differences from
the baseline.

## Visible differences against the baseline

| # | Seen in CoA64 only | Owner / next step |
|---|---|---|
| 1 | Error dialog "Couldn't load Blizzard_CombatLog: Corrupt" | The client's add-on verification (`AddOns.cpp`) rejects the Blizzard add-on; the original lets it load. Investigate in Plan 4 (stock-code path, not a DLL module). |
| 2 | A "Sound Kit Player" frame is open at world entry | A development / GM frame the baseline keeps hidden; probably gated on a missing native or a realm / GM flag. Investigate in Plan 4. |
| 3 | LootCollector prints "Realm capabilities mapped: WR mode" (baseline: "COA mode") | A realm-type query answers differently; check C_Realm natives and RealmInfo gates against the baseline. Plan 4. |
| 4 | FrameXML.log: `Couldn't open Interface\SharedXML\HDPatch.xml` | The HD patch's FrameXML is not reachable. The HD patches need DXVK; this belongs to the DXVK plan. |
| 5 | Chat: "Details!: error running function for auto run script ..." (5x) and "There are too many errors in your UI" every ~10 s | Follows from the missing natives below. |

FrameXML.log lines present only in CoA64 (from `gluelog.py --base out/baseline/FrameXML-world.log --new
out/baseline/x64/FrameXML-world.log`):

- `Couldn't open Interface\SharedXML\HDPatch.xml`
- `Interface\FrameXML\Util\GlobalFunctions.lua:230: hooksecurefunc(): SuggestInvite is not a function` (AscGroupFinder)
- `Interface\FrameXML\Util\SuperTrackerUtil.lua:9: Usage: hooksecurefunc([table,] "function", hookfunc)` (C_SuperTrack, AscSuperTrack)
- `(null)` (twice; the error text the two lines above leave behind)
- `Loading add-on Ascension_SkillCards`: the add-on loads in CoA64 but not in the baseline run. Check that its load condition matches.

`Logs\LUA.txt` holds only the load markers in both runs. The Lua errors below come from the C_Logger lines in the
Extensions log and from the in-game chat.

## Per-module findings (reconstruction module → what world entry showed)

| Module (`Asc*.cpp`) | Unhandled packets (count in 3 min) | Missing natives seen in errors | Lua errors |
|---|---|---|---|
| AscAuraAmounts | 0x673 SMSG_AURA_UPDATE_ADDON (202) | -- | -- |
| AscObjectAddon | 0x578 SMSG_UPDATE_OBJECT_ADDON (63) | -- | -- |
| AscTutorial | 0x6A5 SMSG_PATCH_TUTORIAL (9), 0x6B0 SMSG_TUTORIAL_REWARDED_LIST (1) | -- | -- |
| AscCAMgr | 0x926 SMSG_CA_AVAILABLE_CREDITS_UPDATE (4), 0x725 / 0x726 (1 each) | C_*.GetActiveSpecID | `SpecializationUtil.lua:65: attempt to call field 'GetActiveSpecID'` (PLAYER_ENTERING_WORLD) |
| AscAppearance | 0x6A2 (2), 0x69D (2), 0x69A (2), 0x699 (1) | -- | -- |
| AscMysticEnchant | 0x5FD, 0x5FC, 0x5F9 (1 each) | -- | -- |
| AscMysticEnchantPreset | 0x5FF, 0x600 (1 each) | -- | -- |
| AscChallenge | 0x597, 0x599, 0x596, 0x5A2 (1 each) | -- | -- |
| AscGameMode | 0x90B SMSG_GAME_MODE_CHANGED (1) | GetCustomGameMode | `C_GameMode.lua:12: attempt to call global 'GetCustomGameMode'` (GAME_MODE_UPDATE) |
| AscBuildCreator | 0x633, 0x634, 0x635 (1 each) | -- | -- |
| AscToken | 0x670 SMSG_TOKEN_UPDATE (1) | -- | -- |
| AscHighRisk | 0x73D SMSG_FEL_COMMUTATION_STATUS_UPDATE (1) | -- | -- |
| AscSpellCharges | 0x9C2 SMSG_CLEAR_ALL_SPELL_CHARGES (1) | -- | -- |
| AscCallboardCache | 0x730 SMSG_CALLBOARD_CACHE_CONFIG_UPDATE (1) | -- | -- |
| AscWorldMap | -- | C_*.GetMapIDByZoneID | `MapPOI.lua:85: attempt to call field 'GetMapIDByZoneID'` (AREA_POI_PAYLOAD) |
| AscGlobalsE | -- | GetUnitPrimaryStat | `UnitUtil.lua:74: attempt to call global 'GetUnitPrimaryStat'` (DragonUI CharacterPanel) |
| AscSpellActivationOverlay | -- | C_SpellActivationOverlay (namespace) | `ActionButton.lua:637: attempt to index global 'C_SpellActivationOverlay'` (DragonUI ModuleRegistry) |
| AscSuperTrack | -- | C_SuperTrack.* | `SuperTrackerUtil.lua:9` hooksecurefunc usage (FrameXML load) |
| AscGroupFinder | -- | SuggestInvite | `GlobalFunctions.lua:230` hooksecurefunc (FrameXML load) |

All client-DBC patch packets (0x92A..0x9CC), PatchData, EventPackets and the Plan 3 modules were handled in this
run. In earlier runs, before Task 7, SMSG_PATCH_SPELL alone logged 28,422 unhandled lines.

## Proposed order of Plans 4+

Modules that world entry showed come first. The rest follow by size, as remaining natives + packets from
`CoA/inventory-report.md` / DLL_REFERENCE.

1. **Plan 4: world-entry errors.** The small modules whose absence breaks UI load and the two packets that flood
   the log:
   - AscGlobalsE (4 natives), AscWorldMap (7), AscSpellActivationOverlay (2), AscSuperTrack (9 + 1),
     AscGroupFinder (16 + 7), AscGameMode (15 + 4).
   - AscAuraAmounts (4 + 2), AscObjectAddon (0 + 1).
   - The investigations from the visible-differences table: CombatLog "Corrupt", the Sound Kit Player frame,
     and the WR vs COA realm mode.
2. **Plan 5: character advancement.** AscCAMgr (129 + 14), AscCAFilter (10), AscSpellApi (8), AscSpellText's
   remainder (2), AscUnitAddon (6). This plan installs the SpellText / AttachFixes / VanityCollection / ClientDbcPatch /
   PatchData dependency hooks left open so far.
3. **Plan 6: collections.** AscAppearance (44 + 10), AscMysticEnchant (60 + 16), AscMysticEnchantPreset (7 + 5),
   AscItemCollections (2 + 1), AscCollectorCache (6). This plan installs the PatchData appearance callbacks.
4. **Plan 7: progression and status.** AscChallenge (35 + 30), AscTutorial (40 + 12), AscToken (3 + 2),
   AscHighRisk (5 + 1), AscSpellCharges (1 + 4), AscCallboardCache (4 + 1), AscCallboard (1 + 2),
   AscLootLockout (20 + 4; GetEncounterInfo's lockout seconds).
5. **Plan 8: builds and game modes.** AscBuildCreator (81 + 12), AscBuildDraft (8 + 7), AscDraftMode (32 + 4),
   AscWildcardRolls / Sets / Rapid (61 + 9), AscSkillCardCollection / Slots (38 + 13), AscTrialCreator (21 + 8),
   AscManastorm (25 + 14), AscSeasonal (6 + 3).
6. **Plan 9: services.** AscGMTicket (43 + 17), AscRecoveryService (5 + 12), AscCustomStore (11 + 3),
   AscBugTracker (13 + 2), AscPlayerPoll (10 + 2), AscScrollOfFortune (9 + 3), AscTemporalContracts (16),
   AscDiscord (3 + 3), AscPacketApi (23).
7. **Plan 10: UI and graphics remainder.** AscClientOptions (18), AscGraphics (19 + 3), AscNamePlates (10),
   AscCamera (6), AscTooltipScaling (5 + 1), AscUnitSelect (6), AscModelMethods / ModelNames (7), the widget methods
   (`#Minimap`, `#MovieFrame`), the software cursor (AscCursor: SetAttackCursor), and the small remainder
   (≤ 5 each).
8. Then the DXVK plan (HD patches, `HDPatch.xml`), then Warden.
