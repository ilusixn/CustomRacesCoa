# CoA64 Plan 4: World-Entry Errors Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** CoA64 enters the world without the errors Plan 3's findings attribute to small modules: the eight
reconstruction modules behind them are ported, and the three visible differences from the 32-bit baseline are
explained and fixed.

**Architecture:** Each module follows the established split. Logic and packet state go in
`CoA/Source/Modules/X.cpp`, which is pure and unit-tested. Client lookups go in `XClient.cpp`, which is never
linked by the tests. A pure module reaches client code only through a `Deps` struct that the client half installs.
ObjectAddon and GameMode come first, because later modules call their headers. The last task fixes the three
visible differences and measures world entry again against the Plan 3 findings.

**Tech Stack:** C++20, VS 2022 x64, CMake/Ninja (`out\build\x64-Release`), CoATests (`COA_TEST`), Python 3
tools in `Tools/coa`.

**Spec:** `docs/superpowers/specs/2026-10-25-coa64-client-design.md`. Behaviour reference: the reconstruction at
`F:\WorkDir\ascension-extensions-reconstruction` (pin `1b9ebca`), files `src/Ascension/Asc<Module>.cpp/.hpp`.
Input: `docs/superpowers/plans/2026-10-25-coa64-world-findings.md`. Earlier rulings:
`docs/superpowers/plans/2026-10-25-coa64-plan3-ledger.md`.

## Global Constraints

- Reproduce the genuine DLL's behaviour **including its bugs**. Every reproduced bug gets a ledger `Ruling:` line
  and a test (user decision, 2026-10-25).
- An x64-only failure the 32-bit original survives (heap, pointer width) is not "bug included". Deviate there,
  and ledger the deviation.
- No raw 32-bit address or `ptr + 0xNN` field access into client objects in x64 code. Everything resolves to
  decomp symbols and members, and DLL DBCs go through `CoA::Dbc`.
- No runtime self-patching. Every decomp edit is marked `// CoA: <file:function> (orig hook 0xADDR)` or
  `(Extensions.dll patch 0xADDR)`.
- Registration macros only (`COA_NATIVE`, `COA_PACKET`, `COA_EVENT`, `COA_CVAR`), so `Tools/coa/inventory.py`
  counts them.
- Credentials are typed into the client only, never written anywhere. Screenshots stay in `out/baseline/`.
- Live sessions fit the ~10-minute Warden window.
- Build/test command for every task: `cmd //c Tools\coa\check.bat`.

## Review Focus

1. **Logout and relog:** every module's glue-screen reset clears its state, so a second character does not see
   the first one's absorbs, group listing, tracker or game mode. Test pin: each task's `*_GlueResets` test.
2. **A 0x578 field index past the record, or an unknown GUID kind:** ignored with no write, while the remaining
   pairs in the packet still apply. Test pin: `ObjectAddon_OutOfRangeAndKinds`.
3. **SMSG_AURA_UPDATE_ADDON removal (slot 0xFFFFFFFF) for an entry that does not exist:** no crash, but an event
   only when the original fires one. Test pin: `AuraAmounts_RemoveMissing`.
4. **Group finder string lengths running past the packet end:** the reader stops and the record stays as the
   original leaves it. Test pin: `GroupFinder_ShortPacket`.
5. **The SuperTrack visited list at its 20-id cap and the 0xC4604 value bound:** it keeps the last 20 ids, and
   the CVar string matches the original's format. Test pin: `SuperTrack_VisitedListCap`.

---

## Shared conventions (all tasks)

- Module entry: `namespace CoA::X { void Init(); }` (or `CoA::XClient`) declared in `CoA/Source/Modules/Modules.h`.
  Pure modules are listed in `CoA/Source/CoA.cpp` `s_modules` before the client entries; client entries go after
  `"ClientDbcPatchClient"`.
- Glue reset: `CoA::Lifecycle::OnGlueScreen(Reset)`. World entry: `CoA::ClientHooks::enterWorld` (a `Hooks<>`
  list). Timers: `CoA::Timer::Schedule(ms, fn, param)` / `Cancel`.
- Each task ends with an inventory run (`python Tools/coa/inventory.py --recon F:/WorkDir/ascension-extensions-reconstruction --out CoA/inventory-report.md`).
  The module's natives, packets and events must no longer be listed as missing.
- Each task's commit message is `feat(CoA): <Module> world module`. Each task appends a ledger progress line.

### Task 1: ObjectAddon (custom object fields, SMSG 0x578)

**Files:**
- Create `CoA/Source/Modules/ObjectAddon.h/.cpp`, `CoA/Source/Modules/ObjectAddonClient.cpp`.
- Test: `CoA/Tests/ObjectAddonTest.cpp`.
- Modify `AttachFixesClient.cpp` (install `unitAddonField`), `SmallApisClient.cpp` (`C_Item.GetScalingLevel` reads
  item field 0).

**Interfaces (Produces, used by Plans 5–8):**
```cpp
namespace CoA::ObjectAddon {
    struct Field { uint32_t value = 0; void (*callback)(uint64_t guid, uint32_t index, void* arg, Field* f) = nullptr; void* arg = nullptr; };
    std::vector<Field>& Unit(uint64_t guid);   // FUN_102cdcf0: created with 0x58 fields
    std::vector<Field>& Item(uint64_t guid);   // FUN_102cdbb0: created with 1 field
    float AsFloat(const Field& f);
    using TypeCallback = void (*)(uint64_t guid, uint32_t index, void* arg, Field* f);
    void RegisterTypeCallback(uint32_t kind, uint32_t index, TypeCallback fn, void* arg);   // FUN_102ce900
    uint32_t Kind(uint64_t guid);              // FUN_102ce0c0's classification, 0..7; 8 = ignored
    void DropUnit(uint64_t guid);              // FUN_102ccfc0 (object freed)
    void DropAllUnitsExcept(uint64_t keep);    // FUN_102cd780 (every second SMSG_INITIAL_SPELLS)
    void Reset();                              // tests only
}
```
The client half connects `ClientHooks::objectFree` to `DropUnit` and `ClientHooks::initialSpellsAlternate` to
`DropAllUnitsExcept(active player)`.

- [ ] **Step 1: Failing tests** in `ObjectAddonTest.cpp`:
  - `ObjectAddon_UnitFieldsAndCallbacks`: feeding `{guid 0x0000000000001234, (3, 7), (86, 9)}` to 0x578 gives
    `Unit(0x1234)[3].value == 7` and `[86].value == 9`. A field callback and a kind-0 type callback registered on
    index 3 both run once, field callback first.
  - `ObjectAddon_OutOfRangeAndKinds`:
    - Index 0x58 on a unit is ignored and the next pair still applies.
    - An item GUID (high word 0x4000) writes `Item(guid)[0]`.
    - A 0x1F50 GUID changes nothing.
    - `Kind()` returns the original's kind for 0xF130 / 0xF140 / 0xF150 / 0xF100 / 0xF101 / 0xF110 / 0x1FC0.
  - `ObjectAddon_Lifecycle`: `DropAllUnitsExcept(a)` keeps `a` and drops `b`; `DropUnit(a)` drops `a`.
- [ ] **Step 2: Run CHECK.** Expected: FAIL (link errors or failed checks).
- [ ] **Step 3: Implement** from `AscObjectAddon.cpp`. The kinds and store selection follow `handler_0x0578`
  exactly, including the empty stores for 0xF100 / 0xF101 / 0xF110 / 0x1FC0.
- [ ] **Step 4: Run CHECK.** Expected: PASS. Commit.

### Task 2: AuraAmounts (absorb amounts, SMSG 0x673 / 0x674)

**Files:**
- Create `AuraAmounts.h/.cpp`, `AuraAmountsClient.cpp`.
- Test: `CoA/Tests/AuraAmountsTest.cpp`.

**Interfaces:** `CoA::AuraAmounts::Deps { bool (*acceptsSpell)(uint64_t unit, uint32_t spell); std::vector<std::string> (*tokens)(uint64_t unit); uint64_t (*guidOf)(const char* token); uint64_t (*activePlayer)(); }`.
The client half installs these over `CGSpellBook::CanShowAura` (0x53D580), `Script_GetTokenIDsFromGUID` and
`Script_GetGUIDFromToken`. The natives `UnitGetTotalAbsorbs`, `UnitGetTotalHealAbsorbs`,
`UnitGetTotalSchoolAbsorbs` and `C_Aura.GetArmorPenetrationModifier` are pure, over the map plus `guidOf`.

- [ ] **Step 1: Failing tests:**
  - `AuraAmounts_UpdateAndTotals`: a 0x673 record for the player gives `UnitGetTotalAbsorbs("player")` equal to the
    original's sum.
  - `AuraAmounts_RemoveMissing`: removing a missing entry signals nothing.
  - `AuraAmounts_OtherUnitFiltered`: another unit's entry with `acceptsSpell` false is not kept.
  - `AuraAmounts_UpdateAll`: 0x674 handles several records up to the packet end.
  - `AuraAmounts_GlueResets`.
  - Event names and arguments follow `FUN_10a13440`: `UNIT_ABSORB_AMOUNT_CHANGED` /
    `UNIT_HEAL_ABSORB_AMOUNT_CHANGED("%s", token)`, once per token.
- [ ] **Steps 2–4** as Task 1.

### Task 3: GameMode (game mode and per-spec links, SMSG 0x90B / 0x5CC / 0x737 / 0x738)

**Files:**
- Create `GameMode.h/.cpp`.
- Test: `CoA/Tests/GameModeTest.cpp`.
- Modify `SpellTextClient.cpp` or the module that installs `SpellText::Dependencies().gameMode`.

**Interfaces (Produces):**
```cpp
namespace CoA::GameMode {
    uint32_t Mode();  uint32_t ActiveMask();  bool SpecBuildDraft(int32_t spec);  int32_t ActiveSpecIndex();
    void OnModeChanged(void (*cb)());
    struct Deps { int32_t (*activeSpecIndex)() = nullptr; };   // AscCA::ActiveSpecIndex; -1 until Plan 5
    Deps& Dependencies();
}
```
Natives (16): `GetCustomGameMode`, `IsDraftMode`, `IsBuildDraftModeEnabled`, the `Toggle*Mode`s (CMSG 0x5A4), and
`Get/SetEquipmentSetIDForSpecialization` / `Get/SetMysticEnchantPresetIDForSpecialization` (CMSG 0x73A / 0x739).
Each one follows its reconstruction handler.

- [ ] **Step 1: Failing tests:**
  - `GameMode_ModeChanged`: 0x90B sets `Mode()` and `ActiveMask()` and fires `ASCENSION_CUSTOM_GAME_MODE_CHANGED`
    after the `OnModeChanged` callbacks. `GetCustomGameMode` reports the original's value, including the 0x800
    draft bit rule.
  - `GameMode_SpecLinks`: 0x5CC, 0x737 and 0x738 update the per-spec records, fire their events, and answer the
    getters.
  - `GameMode_Toggles`: the exact CMSG bytes.
  - `GameMode_GlueResets`.
- [ ] **Steps 2–4** as Task 1. Then install `SpellText::Dependencies().gameMode = CoA::GameMode::Mode`.

### Task 4: GlobalsE and WorldMap (client natives)

**Files:**
- Create `GlobalsEClient.cpp`, `WorldMapClient.cpp`.
- Test: `CoA/Tests/WorldMapTest.cpp` for the pure part, if any is split out.

GlobalsE adds `GetInboxInvoiceQuantity`, `GetSkillInfo`, `GetUnitPrimaryStat` and `GetWorldMapAreaID`; the
already-ported `GetClassInfoByName` / `GetRaceInfo` stay where they are. WorldMap is `C_WorldMap`'s 7 natives over
`g_worldMapAreaDB` / `g_areaTableDB`. Its "AreaID" arguments are WorldMapArea ids + 1, as the original's.

- [ ] **Step 1: Failing test** `WorldMap_AreaIdOffsets`, over a pure helper
  `CoA::WorldMap::FindArea(const std::vector<Area>&, ...)` if the lookup logic is factored out. Otherwise the
  natives are client-only and verified live (Task 8). Ledger which.
- [ ] **Steps 2–4** as Task 1.

### Task 5: SpellActivationOverlay (C_SpellActivationOverlay, 100 ms tick)

**Files:**
- Create `SpellActivationOverlay.h/.cpp`, `SpellActivationOverlayClient.cpp`.
- Test: `CoA/Tests/SpellActivationOverlayTest.cpp`.

**Interfaces:** `Deps` for the row evaluation's client questions: aura stacks, spell cooldown, charges, and the
trigger types `0x1031f640` reads. `Tick()` is pure. It walks `SpellActivationOverlays.dbc` (`CoA::Dbc`), keeps the
shown list in walk order (unsorted, as the original) and fires `SHOW_` / `HIDE_SPELL_ACTIVATION_OVERLAY("%u")`.
The client half arms the 100 ms timer on world entry and cancels it at the glue screen.

- [ ] **Step 1: Failing tests:**
  - `SpellActivationOverlay_ShowHide`, over a fake DBC and fake deps: rows becoming true or false fire the events
    in walk order. `IsSpellOverlayed` and `GetSpellActivationOverlayInfo` answer as the original.
  - `SpellActivationOverlay_GlueResets`.
- [ ] **Steps 2–4** as Task 1.

### Task 6: SuperTrack (C_SuperTrack, SMSG 0x740, VisitedSuperTracks CVar)

**Files:**
- Create `SuperTrack.h/.cpp`, `SuperTrackClient.cpp`.
- Test: `CoA/Tests/SuperTrackTest.cpp`.

The pure module holds:
- the tracker: quest, SuperTrack row, cleared values `map -1`, position 0, radius 5.5;
- SMSG 0x740 and the setters (`SUPER_TRACKING_CHANGED`);
- the visited list: at most the last 20 ids, values < 0xC4604, header 0x0176, CVar `VisitedSuperTracks` flags
  0x21 default "".

The client half holds the three world-entry timers (1000 ms, 100 ms, 1000 ms), the quest-target resolution, the
player position and `PositionFrame`.

- [ ] **Step 1: Failing tests:**
  - `SuperTrack_SetAndClear`.
  - `SuperTrack_Packet740`.
  - `SuperTrack_VisitedListCap`.
  - `SuperTrack_GlueResets`: the world Lua init clears the tracker and sets the fresh flag.
- [ ] **Steps 2–4** as Task 1.

### Task 7: GroupFinder (C_GroupFinder, SMSG 0x580–0x587 / 0x59D, CMSG 0x57C–0x59E)

**Files:**
- Create `GroupFinder.h/.cpp`, with a client half only if a native needs client objects (player name, class).
- Test: `CoA/Tests/GroupFinderTest.cpp`.

The manager, its own listing and the listed-groups map follow the reconstruction's record layout. The LFG tables
come through `CoA::Dbc` (patched by 0x579–0x57B, Plan 3). `SuggestInvite` and `RequestInvite` are globals as
well as `C_GroupFinder` members.

- [ ] **Step 1: Failing tests:**
  - `GroupFinder_CreateUpdateRemove`: the CMSG bytes and the result events.
  - `GroupFinder_ListedGroups`: 0x583 fills the map and `GetListedGroups` / `GetGroupInfo` answer.
  - `GroupFinder_Invites`: 0x585 / 0x587 / 0x59D events.
  - `GroupFinder_ShortPacket`.
  - `GroupFinder_GlueResets`.
- [ ] **Steps 2–4** as Task 1.

### Task 8: The visible differences, then acceptance

**Files:** Decomp or CoA edits as the causes dictate. Update `docs/superpowers/plans/2026-10-25-coa64-world-findings.md`
with a "Plan 4 result" section.

- [ ] **Step 1: CombatLog "Corrupt".** `IsAddOnLoadable` returns `UIADDON_CORRUPT` when `addon->m_signed` is
  set, which happens when `FrameXML_CheckSignature(toc, bindings, publicKey, digest)` returns 2
  (`WoW/Source/UIUtil/AddOns.cpp:720`).
  - Find why the x64 check fails for `Blizzard_CombatLog` but the 32-bit one passes: digest, key, file read, or
    pointer width. Use superpowers:systematic-debugging.
  - Write a pure test of the failing computation where possible, fix it, and ledger the cause.
- [ ] **Step 2: Sound Kit Player frame.**
  - Extract the FrameXML that creates it from the client MPQs (StormLib / mpyq).
  - Find its show condition, and which native or state answers differently from the 32-bit client.
  - Fix it, or ledger it as belonging to a later module.
- [ ] **Step 3: Realm mode "WR" vs "COA".**
  - LootCollector (`Interface/AddOns/LootCollector/Modules/Constants.lua:293`) decides from these calls, since
    "Pale Reach" is not one of its known realm names:
    - `C_GameMode:IsGameModeActive`;
    - `C_CharacterCreate.CanCreateArchetype`;
    - `C_Player.IsCustomClass`;
    - `C_Player.IsHero`.
  - With Task 3 in place, find which one still answers differently and fix it, or ledger its owning module.
- [ ] **Step 4: Live acceptance.**
  - CoA64 enters the world on the same character and stays 3 minutes.
  - Measure as Plan 3 Task 8 did: unhandled packets with names, C_Logger and chat Lua errors, FrameXML.log diff
    against `out/baseline/FrameXML-world.log`.
  - Expected:
    - none of the eight modules' opcodes and natives remain in the measurement;
    - the C_GameMode, MapPOI, UnitUtil, ActionButton, SuperTrackerUtil and GlobalFunctions errors are gone;
    - `GetActiveSpecID` (AscCAMgr, Plan 5) may remain.
- [ ] **Step 5:** Write the "Plan 4 result" section, run CHECK, commit `docs: plan 4 world-entry result`.
