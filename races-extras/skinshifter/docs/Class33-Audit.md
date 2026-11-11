# Class 33 audit (2026-11-04)

Every place the server decides something per class, and what class 33 gets. Fixed in the module unless noted.

| # | Finding | Status |
|---|---|---|
| 1 | Racial abilities of the stock races never learned (racial rows all list classes; class 33's mask is 0) | Fixed: learned at login as class 14 would |
| 2 | Ranged attack power used the generic Agility - 10 | Fixed: + level, as a Rogue |
| - | Melee attack power was 0 (no case for class 33) | Fixed: 2 x level + Str + Agi - 20 |
| 3 | No Parry (3127) | Fixed: learned at login |
| 4 | No Dual Wield (674) | Fixed: learned at login |
| 5 | Reborn armor not applied (`AscensionItemLadder.cpp:131` stops at Spirit Mage) | Open: core patch |
| 6 | Armour/shield rules (group loot Need, heirlooms) treated class 33 as a cloth wearer | Fixed: counts as Druid there |
| 7 | Dodge mixes Rogue base dodge and Druid's agility factor (about 3.5% less base dodge than a Druid) | Open: minor |
| 8 | Talent tabs use `1 << (class - 1)` (undefined at 33, acts as Warrior): N showed the Warrior trees | Fixed: core patch uses `ClassMaskBit` for the talent tab pages (server); with Character Advancement N is CoA's window (client patch: `IsCustomClass`) |
| 9 | Stock talent points given but cannot be spent | Fixed: none given |
| 10 | CoA Character Advancement / Book of Ascension not usable (classes 12-32 only) | Fixed 2026-11-04: core patch widens the CoA class checks to 33; the module writes our rows of the CA tables (server overlay `Data/dbc_skinshifter/`, client patch from `skinshifter_advancement.txt`) |
| 11 | Pets of class 33 got no pet type | Fixed: summon pet |
| 12 | ~~Server treats a Skinshifter as a Warrior (class mask 1)~~ **Wrong, corrected 2026-11-04**: the core patch already gives class 33 no class bit (`ClassMaskBit`, `getClassMask` = 0), so Warrior-only items, quests, conditions, mails and reputations do not apply. Only the client read it as the Warrior bit (talent tabs, `UnitClassMask`) | Server: nothing to do. Client: `IsCustomClass`/`IsDefaultClass` answered by class file (client patch) |
| 13 | Mystic Enchants treat class 33 as a stock class | Open: policy question |
| 14 | Empty Rogue skill lines (38, 39, 253) from the Rogue create skills | Fixed: removed at login |
| 15 | Account info class name blank, no name colour | Open: cosmetic |
| 16 | As a CoA class (`IsAscensionClass` up to 33) the CoA login sync would have stripped every weapon/armour proficiency (no live baseline rows) and refused character creation (no live starter kit) | Fixed in the core patch: a CoA class without those rows keeps the stock path |
| 17 | CoA's client keeps spell modifiers for spell families 0-38 only; family 39 (class + 6) would write past the table | Avoided: the Skinshifter's spells use family 16 (unused in 3.3.5) |
| 18 | Not widened on purpose (client DLL keeps 12-32 there too): Freepick class admission, Mystic Enchant class rules, `OnPlayerIsClass` legacy class, Reborn armour (#5). CA unlearn/purge costs in the client use the stock-class formula for class 33 | Open: by choice |

Verified correct: copied gt tables (crit, regen, rating scalar), health/mana/armor/spell power from stats,
mana as power, character creation rows, weapon and armor skills, client stat tables.
