# Live addon list and world-entry tweaks (2026-11-15)

## Live addon list (AddOnWatch, CoA addition)

`AddOnWatchClient.cpp` watches `Interface\AddOns\` (and `-addonspath`) with an overlapped `ReadDirectoryChangesW`
per folder, polled once per frame through `HasOverlappedIoCompleted` (no syscall). `AddOnWatch.cpp` is the
platform-free half: which changes matter and when a burst has settled (`kSettleMs`, since extracting an addon
writes many files).

- A change to a top-level addon folder, or to a `.toc` directly inside one, names that addon. `RescanAddOns`
  (`WoW/Source/UIUtil/AddOns.cpp`) re-reads only those `.toc` files. Then `ADDON_LIST_UPDATE` fires: it is the
  stock glue event, queued as a custom event for the world table, and an open `AddonList` is redrawn.
- A buffer overflow, or the folder disappearing and coming back, rescans everything.
- Any other file change (Lua, XML, textures) leaves the list alone.

### Fix: addons added while the client runs never appeared

Loose disk files resolve only through SFile's file-system hash (`System_SFile2::FindFile`, `s_fileHash`). That
hash is built once and rebuilt only after `SFile::RebuildHash()`. `RescanAddOns` never rebuilt it, so for a newly
extracted addon `SFile::ReadFile` could not find the `.toc`, and `LoadAddOnInfo` silently added nothing. A new
addon's Lua/XML files had the same problem: `/reload` rebuilds the hash only when it creates a new
SavedVariables file.

Every settled burst now calls `SFile::RebuildHash()`, including bursts of plain file changes (`NoteFile`). The
next file lookup rescans the directories once. Only addon changes take the list rescan path.

## AscWorldEntryTweaks (Extensions.dll after-world-entry callbacks)

"Stock class" means classes 1..9 and 11 (FUN_100c6380). The original rewrites client bytes; the port turns each
rewrite into a switch that the decomp site reads.

| Original | Port |
|---|---|
| FUN_10a50e70: `0x809572` `je` -> `jmp` for a non-stock class, never put back | `SkipAmmoCheck()` in `Spell_C_HaveEquippedSpellItems` (latched) |
| FUN_10a5dc40: `0x6DC45F` `jae` for a stock class, `jmp` otherwise (every entry) | `SkipProficiency()` in `CGPlayer_C::CanUseItem` |
| FUN_10a60440: `SpellShapeshiftForm` rows 0x11..0x13 get flag 2 (+0xC) for a non-stock class, lose it for a stock one | the rows' `m_flags` |
| FUN_10a73b70: `CMSG_EXTENSION_INITIALIZED` (0x561) `{u32 0, u32 1}` | sent on every entry |

Not ported, on purpose:
- FUN_10a3abc0: `LoadLibraryA("DivxTac.dll")`, Ascension's anti-cheat.
- FUN_10a4aa40: the server allow-list kill switch. The reconstruction keeps it inert too.

On `CMSG_EXTENSION_INITIALIZED` the server (`AscensionCompat.cpp`, `mod-spellbook`) sends the secure addon list,
spell charge states, the display patch stream, server pet appearances, specialization equipment sets, the LFG
objective, and the spellbook window tags. Before this port, CoA64 clients never received the parts that are sent
only in reply to it.

## AscQuestSpellHide

FUN_10112620: spell 0xE4E2D's effects are removed and its visual is suppressed while quest 0x1D0BB4 is in the
quest log.

## GlobalsJClient native notes (moved out of the source comments)

- **GetSpellData(spell [, a, b])** (FUN_10a4da30): accepts `{number, boolean, boolean}`, then `{number, boolean}`
  (the boolean is read and ignored), then `{number}`. Pushes the Spell.dbc name (+0x220), the description
  (FUN_101ac910(spell, a, 0, b) = SpellText::Description) and the SpellIcon path (+0x214 -> SpellIcon.dbc +4,
  defaulting to the question mark). Pushes nothing for an unknown spell.
- **GetItemTemplate**: the item cache record as FUN_1021c4b0's JSON object. Integer fields are pushed as integers,
  floats as numbers, strings as strings and arrays as tables keyed 1..n. Nothing when the item is not cached.
- **GetEncounterInfo(encounter [, unit])**: returns the name, map, difficulty + 1, the SpellIcon path (defaulting to
  the question mark), the order index, the unit's loot-lockout seconds and whether the current Mythic+ run killed it.
  nil for an unknown encounter.
- **GetReducedChanceToBeCrit**: sums BasePoints + 1 of each SPELL_AURA_MOD_ATTACKER_{SPELL (179), MELEE (187),
  RANGED (188)}_CRIT_CHANCE effect; 197 counts towards all three. It walks the player's auras (every aura, repeats
  included), then the spell book, where each spell counts once and only when no aura already counted it.
- **IsSpellHoldToCast**: returns nothing unless the game window has focus (OsGuiGetWindow(0) == (2)) and the spell has
  a record.
  - Shaman spells (family 11) hold with family flag 0x2 when mystic enchant 81091 is slotted, or with flag[2]
    0x20000000 when CA spell 955831 is known.
  - Otherwise a spell holds when it has no SpellCharges row, lacks AttributesEx2 0x20, has a family, applies no
    shapeshift (aura 36), and targets neither 23 nor 26 with ImplicitTargetA / B.
- **PlayerAuraHidden**: UnitAura's eleven values for the player's first aura of the spell, hidden or not:
  - name, rank, icon, count;
  - the dispel type name (when its immunity flag is set);
  - duration and expiration in seconds (0, 0 when permanent);
  - the caster token;
  - stealable, and AttributesEx7 0x10000000, each as 1 or nil;
  - the spell id.
- **SetItemQualityColor(quality 0..7, r, g, b)**: does nothing when a component is above 1. Each component is scaled
  by 255 and written as two lower-case hex digits; a value above 0xFF ("") skips the update. `|cffRRGGBB` replaces
  the quality's colour string, and its colour becomes 0xFFRRGGBB.
- **SetWorldTime(hour, minute)**: the original writes the 8-byte double 100.0 over the rate (rate 0, next field
  0x40590000), sleeps 50 ms, writes the clock bytes again, then restores the rate to 1/60.
- **GetItemInfoInstant(item)**: the client Item.dbc row and the item addon record, else nil: `{itemID, icon,
  classID, subclassID, name, description, quality, pvePower, pvpPower, miscValue {3}, inventoryType, itemLevel}`.
- **ApplyComponents(slot, texture, model)**: `Item\ObjectComponents\` + each name, with ItemDisplayInfo row 17,
  linked onto the player's character model. The original first copies the paths into the component code's own path
  buffers, and reads the player without a check.

## Why these were missed

`Tools/coa/inventory.py` counted only the attach-time byte-patch tables and the server packets handled. Runtime
code writes made by module callbacks, and client packets sent, were invisible to it.
