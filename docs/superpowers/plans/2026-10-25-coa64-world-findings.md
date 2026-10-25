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

## Plan 4 result (world-entry errors)

Measured 2026-10-25 at commit 7471b11 (`out/baseline/x64/{Extensions,Fatal,FrameXML}-plan4.*`, screenshot
`out/baseline/x64/8-world-plan4.png`). Character in Un'Goro Crater, 3 minutes in the world, client responsive.

- **Gone:**
  - Every unhandled packet of the eight Plan 4 modules: AuraAmounts 0x673 (202 before), ObjectAddon 0x578 (63),
    GameMode 0x90B.
  - The Lua errors from GetCustomGameMode, GetMapIDByZoneID, GetUnitPrimaryStat and C_SpellActivationOverlay.
  - The SuggestInvite / SuperTrackerUtil FrameXML errors.
  - The Blizzard_CombatLog "Corrupt" dialog. Ascension.exe always accepts add-on signatures (0x816625 / 0x81663E /
    0x816694 / 0x81675F); CoA64 now does the same (user-approved, CoA/DIFFERENCES.md).
- **Left, owned by later plans:**
  - Packets: SMSG_CA_AVAILABLE_CREDITS_UPDATE, 0x725 / 0x726 (AscCAMgr, Plan 5); SMSG_TOKEN_UPDATE (AscToken); the
    challenge lists (AscChallenge); SMSG_CALLBOARD_CACHE_CONFIG_UPDATE (AscCallboardCache).
  - Lua: `SpecializationUtil.lua:65` GetActiveSpecID (AscCAMgr, Plan 5).
- **HD archives ruled out:** a 32-bit `Ascension.exe -hd 0` baseline refuses the same eight HD archives as CoA64
  (`out/baseline/7-world-hd0.png`, `FrameXML-world-hd0.log`). It shows no Sound Kit Player frame and prints "COA mode",
  and its FrameXML.log also lacks `HDPatch.xml`. So `HDPatch.xml` is HD-only (the DXVK plan), but the other two
  differences are not.
- **Open, Sound Kit Player frame:** both clients load `patch-B.MPQ`'s `FrameXML.toc` (line 349 includes
  `Ascension_DebugTools`, whose `SoundKitBrowser` frame is not hidden). The 32-bit client must hide it later.
  CoA64's FrameXML.log has a `(null)` load error right after `DevelopmentXML.xml` that the baseline lacks; the code
  that hides the frame probably fails there.
- **Open, realm mode** (LootCollector prints "WR mode"): it decides from `C_GameMode:IsGameModeActive`,
  `C_CharacterCreate.CanCreateArchetype`, `C_Player.IsCustomClass` and `C_Player.IsHero`, since "Pale Reach" is not
  in its name list. One of these still answers differently; probably the same failing FrameXML code, or a CA native
  (Plan 5).
- **Ascension_SkillCards** loads only in CoA64 (with a second `(null)` error). It is probably the unported
  AscAddonTaint hook on 0x5F7E90, which refuses add-ons the server does not list on realms without add-ons.
- **Still printing "too many errors"** about every 10 s: the source is not captured (add-on errors outside C_Logger).
  Next step: capture the Lua error texts (the `(null)` FrameXML errors included), which settles the two open items.

## Plan 5 result (world-entry errors II)

Capture 2026-10-25, `-extlog 1`, about 2 minutes in world (Un'Goro Crater). Gone since the first capture:
CombatFeedback.lua:107 (9,183), SendSystemMessage (2,724), `#Minimap:GetViewRadius` (259), `#Frame:GetAllAttributes`
(83), GlobalsF / GlobalsI / AutoQuest / UnitAddon / ExtraActionButton.

CombatFeedback did not reproduce, so no separate fix was made. The likely cause: an earlier error in a frame's
OnLoad (a missing widget method) stopped `CombatFeedback_Initialize` from running. Watch for it in later captures.

Remaining errors, by count:

| Count | Error | Owner |
|---:|---|---|
| 3684 | GameTooltip.lua:606 `C_MysticEnchant` | MysticEnchant (Plan 6) |
| 8 | TutorialUtil.lua:61 `C_Tutorial` | Tutorial |
| 5 | HealPredictionUtil.lua:599 `IsHelpfulSpell` | SpellApi (Plan 7) |
| 3 | DraftUtil.lua:22 `HasHandOfFateSacrificePick` | DraftMode |
| 3 | AsyncCallbackHandler.lua:42 `C_AssetQueryService` | AssetQueryService |
| 2 | MysticEnchantUtil.lua:37 `C_MysticEnchant` | MysticEnchant (Plan 6) |
| 3 | ChallengeUtil.lua:86 / WorldMapFrame.lua:246 `C_Challenge` | Challenge |
| 3 | Minimap.lua:235 / 346 `C_Manastorm` | Manastorm |
| 2 | LibGroupTalents `GetTalentsByClass`, RuneFrame `HasRuneUI` | CAMgr (Plan 7) |
| 1 | GameTooltip.lua:371 `GetEntryBySpellID`, C_PrimaryStat `IsPendingBuildAvailable` | CA (Plan 7) |
| 2 | UIParent.lua `C_LootLockout` | LootLockout |
| 1 each | `C_SkillCardCollection`, `C_PlayerTicket`, `C_CustomStore`, `C_BuildCreator` | SkillCard, GMTicket, CustomStore, BuildCreator |
| 1 each | missing CVars `ObjectSelectionCircleTexture`, `useUiScale`, `autoAcceptTrades` | CVars still to port |
| 1 | InterfaceOptionsPanels.lua:1105 `InterfaceOptionsSocialPanelProfanityFilterText` | open; check against the 32-bit baseline |

Fatal.txt still lists the unhandled appearance packets (0x69A / 0x69D / 0x6A2), which belong to the Appearance module.

## Plan 6 result (mystic enchant)

Capture 2026-10-25, `-extlog 1`, about 2 minutes in world. The C_MysticEnchant errors (3,686) are gone. GameTooltip.lua
now gets past line 606 and stops at line 633 on `C_Appearance` (3,188, the same per-tooltip-update path), so
Appearance moves up the order. The other remaining errors are as in the Plan 5 table (Tutorial 10, SpellApi 5,
Challenge 5, DraftMode 3, CAMgr `GetEntryBySpellID` 3, Manastorm 3, CAMgr `GetTalentsByClass` 2, LootLockout 2, one
each for the rest). Fatal.txt also lists an unhandled opcode 0x726 (1830) besides the appearance packets.
