# Instrument chant values

*English translation of [`../valeurs-des-buffs.md`](../valeurs-des-buffs.md) (French).*

Values for a level-60 server (CoA), set in `tools/instruments.json` (columns `min` and `max`), which feeds the `mod_barde_instrument` table
(`min_solo`, `max_solo`) and the client's spells. They can also be changed directly in the database, without recompiling.
A song gives `min` for the first one of the book and `max` for the hardest, in between in proportion to its rank.
The duo multiplies by 1.6, the trio by 2 (v0.3). The values are integers: the game does not accept decimals for these effects.

Design choice (17 October 2026): for the profession to be worth playing, solo reaches **the level of a class buff**
on the hardest song. The chant is a separate spell: it adds to the class buffs (no `spell_group` links them for now).
One chant at a time per instrument; a bard can carry up to three different ones.

| Instrument | Effect | Solo (first song → hardest) | Duo | Trio | Reference points on the server |
|---|---|---|---|---|---|
| Lute | mana / 5 s | 6 → 33 | 53 | 66 | Blessing of Wisdom rank 6 and two other mana-regeneration auras of rank 6: 33. A mana-regeneration elixir: 30. Flask of Pure Mojo: 45. |
| Harp | HP / 5 s | 10 → 45 | 72 | 90 | Few references (Troll's Blood Potion: 20). |
| Flute | speed % | 5 → 12 | 19 | 24 | No lasting speed buff among the classes. |
| Crumhorn | damage dealt % | 2 → 5 | 8 | 10 | Class group damage buffs: 3 %. **To watch**: solo exceeds the class. |
| Fiddle | armor | 150 → 1,200 | 1,920 | 2,400 | Inner Fire ranks 5 and 6: 1,170 to 1,395. Elixirs: 450. |
| Piano | magic resistance | 10 → 60 | 96 | 120 | Class resistance auras: 60 to 70. |
| Portable organ | haste % (melee, ranged and spells) | 2 → 5 | 8 | 10 | Wrath of Air, Swift Retribution: 3 to 5 %. No experience (too strong). |
| Ocarina | critical strike % | 1 → 4 | 6 | 8 | Leader of the Pack, Moonkin aura: 3 %. **To watch.** |
| Timpani | stamina | 10 → 54 | 86 | 108 | Power Word: Fortitude rank 6: 54 to 100. |
| Drum | attack power | 40 → 250 | 400 | 500 | Rhythm part of the ensemble. |

The reference points come from the server's `Spell.dbc`. The harp, crumhorn and ocarina figures rest mostly on the Classic scale,
for lack of a comparable lasting buff in the database.

Difference with the design brief: the brief plans that the chant does not stack with a class buff of the same effect (strongest-wins rule,
`spell_group_stack_rules`). That is not done: the chant adds on top of the class buff, so that the profession keeps its appeal.
