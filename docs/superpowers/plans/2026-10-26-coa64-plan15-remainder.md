# CoA64 Plan 15: The Remainder (inventory total / total)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** close every remaining row of `CoA/inventory-report.md` (after Task 0's accounting fix: 40 natives, 8 packets,
106 hooks, 16 CVars, 3 events), so the report reads total / total in every category. Removes the live
`SpellQueueWindow` missing-CVar error.

**Not in this plan:** Warden (next plan), Helios (never), the Prat / NewCharacterSetupUtil taint popup and LootCollector
"WR mode" (findings doc follow-ups), DXVK (done).

**Architecture:** as in Plans 4..14: pure modules with `*Client.cpp` Deps halves; CVars registered where the original
registers them (glue vs world list, exact name / default / flags / category); natives with COA_NATIVE, events with
COA_EVENT, packets with COA_PACKET; client detours only through ClientHooks lists or Engine function pointers with an
`// CoA: <Module>.cpp:<fn> (orig hook 0x...)` marker at the WoW / Engine site; byte patches as source flags.

**Spec:** `docs/superpowers/specs/2026-10-25-coa64-client-design.md`. Reference:
`F:\WorkDir\ascension-extensions-reconstruction\src\Ascension\Asc<Module>.cpp` (pin `1b9ebca`) and its
`docs/DLL_REFERENCE.md`; genuine DLL `F:\WorkDir\genuine\Extensions.genuine.dll` where the reconstruction is unsure.

## Global Constraints

- Reproduce the original exactly, bugs included; every deviation is a ledgered Ruling with its cost if wrong.
- A hook whose whole purpose cannot exist in the 64-bit single-image client (32-bit address-space workarounds, crash
  reporter plumbing that has no 64-bit counterpart) gets its marker only with a Ruling that names what the original does
  and why the 64-bit site needs nothing / what it does instead. No marker without either a port or such a ruling.
- 64-bit: no pointer truncation; packet / CDataStore layouts keep the 32-bit wire sizes.
- `cmd //c "Tools\\coa\\check.bat"` green after every task; `python Tools/coa/inventory.py --recon
  F:/WorkDir/ascension-extensions-reconstruction` regenerated and committed per task.

## Review Focus

1. Graphics CVars at their defaults leave rendering stock (no culling / fog / light change until a value is set).
2. PacketApi: CDataStore natives never read past the packet end; Send only builds client opcodes the original allows.
3. SpellMods / MovementForce packets with short or malformed payloads do not crash (7a guard convention).
4. SpellQueueWindow 0x805F60 replacement: no player / no cast end -> false; elapsed -> true; else window compare.
5. CrashContext / InitHooks / LargeAlloc rulings: nothing that changes allocation or init order on 64-bit without proof.

---

### Task 0: Inventory accounting (done, 36bfe87)

`Tools/coa/inventory.py` counts DLL_REFERENCE's CVar-table rows (`name.default`), string-pair table rows
(`DAMAGE_SCHOOL3.RESISTANCE2_NAME`) and events fired through `CoA::Events::Signal` without a table entry.

### Task 1: Graphics, Cursor

AscGraphics: CVars ambientGlow, ambientShade, antiportal, entityCulling, fogDensity, lightCulling, lightLimitFix,
lightLinearAttenuation, lightQuadraticAttenuation, mistDensity, overrideDayProgress, portalCulling, terrainCulling,
wmoCulling; native SetRenderOcclusion; packets 0x650 / 0x9D2 / 0x9D3; hooks 0x681580 0x6815C0 0x780770 0x7EB180
0x7EB210 0x7F1010 0x7F1CD0 0x834A40 0x834AB0. AscCursor: hooks 0x6162C0 0x616830 0x683640 0x7695E0; AscGlobalsJ's
SetAttackCursor. Tests: every CVar row, callbacks, packets, each hook at default and non-default values.

### Task 2: SpellMods, MovementForce

AscSpellMods: packet 064-bitF; hooks 0x71AF90 0x71B820 0x7FD970 0x7FDC60 0x7FF100 0x7FF180 0x8012F0 0x802C30 0x806200
0x80CCE0. AscMovementForce: packets 0x761 0x762 0x765 0x766; hooks 0x403B70 0x6EB730 0x6F09F0 0x6F1490 0x762E00
0x987D00. Tests: packet parsing incl. short payloads, mod application, force lifecycle and glue reset.

### Task 3: PacketApi, SmallHooks, GossipQuest, SpellTokens, UnitTokens

AscPacketApi: CreatePacket / ClearPacket / RegisterPacket and the 20 `@CDataStore` methods. AscSmallHooks: CVars
SpellQueueWindow ("400", flags 1, category 3, world) and bankEquipmentManager; hooks 0x454070 0x5ADA20 0x5C2030
0x805F60. AscGossipQuest: hooks 0x53BA40 0x58A9E0 0x58AF10 0x6D1110 0x6D4F80; events GOSSIP_OPTION_SELECTED,
QUEST_REMOVED, QUEST_TURNED_IN. AscSpellTokens: 0x5782D0 0x57AAF0 0x6238A0. AscUnitTokens: 0x60BB70 0x84E350.

### Task 4: CrashContext, InitHooks, LargeAlloc, ClientGuards, MemoryBridge, Registry

AscCrashContext (23 hooks: 0x4D7750 0x57ABC0 0x65C290 0x682200 0x6AFFD0 0x717A20 0x7B31E0 0x7BD480 0x7F6A10 0x810ED0
0x812410 0x819210 0x81F700 0x823ED0 0x823F10 0x824FC0 0x82CED0 0x82EC30 0x82F0F0 0x8309C0 0x830DC0 0x832260 0x838490);
AscInitHooks (23: 0x401A80 0x52E4D0 0x54D010 0x59D0B0 0x5E0420 0x5E0D20 0x61E5C0 0x6DE330 0x6E4950 0x6E9600 0x6FDA20
0x710390 0x717910 0x720BF0 0x7234D0 0x740450 0x744030 0x7441D0 0x7512B0 0x755130 0x756240 0x7FF070 0x8D2400);
AscLargeAlloc 0x4B9760 0x6348B0 0x83CF00; AscClientGuards 0x68E4AD 0x8540D0; AscMemoryBridge 0x4067F0; AscRegistry
0x855060. Port behavior wherever it is observable on 64-bit (crash context strings, guards, init side effects);
64-bit-inapplicable hooks per the Global Constraint.

### Task 5: Single leftovers

AscAttachPatches hooks 0x4CDA43 0x4E157D 0x5238AE 0x7F3BA5 0x98EE8B; AscCharacterCreate 0x4E0BE0 0x4E0C10;
AscWildcardRolls 0x542030; AscZoneLight 0x63200D 0x632A40.

### Task 6: Live re-measure

In-world capture; findings doc "Plan 15 result"; inventory report total / total.
