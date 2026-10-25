# CoA64 Plan 6: Mystic Enchant Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Port C_MysticEnchant and C_MysticEnchantPreset (AscMysticEnchant.cpp, AscMysticEnchantPreset.cpp). They are
the source of 3,686 of the ~3,730 Lua errors in the Plan 5 capture (GameTooltip.lua:606, MysticEnchantUtil.lua:37).

**Architecture:** as in Plans 4 and 5. Pure code (`MysticEnchant.cpp`, `MysticEnchantPreset.cpp`) holds the manager
state, DBC indexes, validators, costs, the 67 natives and the 21 packet handlers; unit tests cover it. Every client
read (player class / level / money / casting, items and bags, the item cache, spell records, auras, the nearby altar,
spell text, the character-advancement investments) goes through one `Deps` struct that `MysticEnchantClient.cpp`
fills. The shared FilterableContainer becomes `CoA/Source/Runtime/FilterContainer.h` (nine reconstruction modules
use it), and the CA enum tables are copied to `CoA/Source/Modules/CAEnums.generated.inc`.

**Spec:** `docs/superpowers/specs/2026-10-25-coa64-client-design.md`. Reference:
`F:\WorkDir\ascension-extensions-reconstruction` (pin `1b9ebca`).

**Not in this plan:** character advancement itself (Plan 7). Until then the `investment` dependency answers 0, so an
enchant with an AE / TE requirement reports RE_*_REQUIRED_AE/TE_INVESTMENT. Plan 7 installs the real answer.

## Review Focus

1. A logout / glue screen clears every manager field the original clears (OnGlueScreen 0x102E32D0, presets
   0x102F2230), and the filter container repopulates afterwards.
2. SMSG 0x56B (patch) on a new id resets the container and rebuilds both indexes; on an existing id it keeps the
   container (the original's behaviour).
3. Natives called before world entry or with no player: no crash. QueryEnchants returns an empty table out of world.
4. Packet readers never read past a short packet (CoA::Read reports Failed).
5. Enum names past a table's end read "UNEXPECTED_ENUM_VALUE_<n>".

---

### Task 1: FilterContainer and CA enums

Port `AscFilterContainer.hpp` to `CoA/Source/Runtime/FilterContainer.h` (namespace `CoA::Filter`, same API:
`Container<T>`, `ParseTokens`, `AppendU32` / `AppendU64` / `AppendByte`, `Lower`). Copy `AscCAEnums.generated.inc`.
Tests: token parsing (digits, overflow, non-digit word dropped, space collapse), text + token filtering, argument
groups (group 0 fails on first miss, other groups need one match), sort by PopulateKey / SortKey / FilterKey, Reapply
drops tokens.

### Task 2: MysticEnchant core

`MysticEnchant.h/.cpp`: the manager, the two DBC indexes, the packets (0x5F9..0x619, 0x56B, 0x734), the lifecycle
(glue reset, world-entry prefetch, altar open / close / scroll use), progress, inspect, enchant info, QueryEnchants,
and the exported `SlotEnchant` / `Slots` / `Slotted` / `Collected` / `Worldforged`. Tests for each packet, the glue
reset, progress maths (level 0 / 1 / 250+), enchant info fields, and QueryEnchants paging and filters.

### Task 3: Validators, costs, scrolls, the remaining natives

Costs, quality caps (normal / reborn / fusion), the projected slot maps, every Check* validator and the natives that
report them, the pending apply / reforge lists and their save packets, the scroll list and filter. Tests per
validator error order and per save packet.

### Task 4: Client half and presets

`MysticEnchantClient.cpp` fills `Deps` and installs the SpellText mystic dependencies and the altar / item-use hooks.
`MysticEnchantPreset.cpp` (pure, tests for its packets and checks).

### Task 5: Re-measure

Live capture; record the remaining errors in the findings doc ("Plan 6 result").
