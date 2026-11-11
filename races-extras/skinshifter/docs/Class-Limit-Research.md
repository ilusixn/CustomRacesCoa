# More than 32 classes — what has to change

Read-only survey of CoA64 (client, source as of 2026-11-03) and the CoA server fork. Nothing has been changed
in either. Client changes stay local and are never pushed or shipped; see the Trojan agreement.

## Good news
- **Ascension planned for more than 32 classes.** Its own tables already use 64-bit class masks: Mystic
  Enchant (`MysticEnchant.cpp`, `ClassAllowed`), Appearance categories, and Tutorial. Those features already
  handle class 33+.
- Class IDs are 8-bit everywhere (unit field, character list, character creation), so up to 255 is fine.
- CoA64's character creation is already dynamic. It reads the class list from CharBaseInfo, and the
  random-class array has 64 slots.

## What breaks at class 33 — client (stock 3.3.5 code)
These are the `1 << (classId - 1)` tests against 32-bit masks. At 33 the shift wraps around in practice, so
class 33 is read as class 1 (Warrior).

| Where | What |
|---|---|
| `Magic/MagicClient/SpellTable.cpp` (2 places) | SkillLineAbility / SkillRaceClassInfo: which skills and spells the class gets |
| `Ui/PaperDollInfoFrame.cpp` (3) | Item AllowableClass (character window) |
| `UIUtil/Tooltip.cpp` (3) | Item AllowableClass ("Classes: ..." in tooltips) |
| `Ui/ReputationInfo.cpp` (3) | Faction reputationClassMask |
| `Ui/TalentInfo.cpp` / TalentTab | TalentTab classMask: which talent tabs the class sees |
| `WowSvcs/.../FriendList.cpp` | The /who filter: class mask in the packet (32-bit) |
| `Glue/CharacterCreation_Scripts.cpp` | Race/class restriction (trial accounts); harmless, it is 0 |

DBC fields with 32-bit class masks: SkillLineAbility, SkillRaceClassInfo, TalentTab, Faction (4), and Item
AllowableClass (which the server sends in the item query).

## What breaks at class 33 — server (CoA fork)
- `MAX_CLASSES = 33` sizes 11 arrays: stat tables, `_playerClassInfo`, talent pages, item scripts. These
  have to grow.
- `getClassMask()` is `uint32(1) << (class - 1)` and is used in 23 files: items, quests, reputation,
  conditions, spells, learning, mail and the auction house.
- `CLASSMASK_ALL_PLAYABLE = 0xFFFFFFFF` and `ExpandLegacyClassMask`.
- This is a change in the core, not the module, so it needs a patch to the fork, the same way the subclass
  hook did.

## Proposed approach: a "high mask" side table instead of changing stock formats
- No stock DBC layout and no network packet changes. A mask of "all classes" (-1/0) includes the new classes
  automatically.
- For rows that should include or exclude a new class specifically, a small extra table is shared by client
  and server: (table, rowId) → high mask (bit 0 = class 33, ...).
- In the client, the ~12 tests above go through one helper, `ClassAllowed(mask, highMask, classId)`, the
  same pattern Ascension already uses in MysticEnchant.
- On the server, a 64-bit `getClassMask64()` and the same helper are used where the masks are tested.
- The 64-bit client check comes on top: CoA64 sends its own signal at login, and the server refuses clients
  that don't send it when `Skinshifter.Require64BitClient = 1`.

## Distribution
- Server: an AGPL patch plus the module, shared as usual.
- Client data (DBC, Lua, icons): an MPQ patch, as today.
- Client exe: local only. Possibly a binary patcher later, but only if Trojan agrees, since he received the
  C&D.
