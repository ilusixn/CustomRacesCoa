# Skinshifter — Creature Map (WotLK / CoA world data)

Separate project from mod-subclass and the Vault handspring. Read-only source data:
- `coa-build/src/data/sql/base/db_world` — creature_template, creature (spawns), creature_template_spell, smart_scripts
- `CoA Server-Ogre-edits/Data/dbc` — Spell.dbc (209,510 spells incl. CoA's), CreatureFamily.dbc

The CoA server's world DB is stock AzerothCore 3.3.5 plus the mod-subclass SQL. So the world creatures are WotLK's; CoA's own content is in the spell side, not in new mobs.

## What counts as "liftable"
Creatures that:
- spawn in the world;
- are not bosses (rank 0/1/2/4 only);
- are not triggers, bunnies or unselectable;
- have no NPC flags (no vendors, trainers or quest givers);
- are of a combat type.

**8,788 creatures** qualify.

| Type | Liftable | With spells | EK | Kalimdor | Outland | Northrend | Instance-only |
|---|---|---|---|---|---|---|---|
| Humanoid | 4,716 | 2,808 | 1,151 | 780 | 867 | 944 | 1,004 |
| Beast | 1,469 | 977 | 309 | 464 | 284 | 271 | 166 |
| Undead | 1,014 | 730 | 193 | 36 | 103 | 356 | 331 |
| Elemental | 449 | 357 | 56 | 96 | 92 | 84 | 121 |
| Demon | 388 | 328 | 26 | 90 | 170 | 9 | 93 |
| Not specified | 283 | 161 | — | — | — | — | — (oozes, silithid, bugs) |
| Dragonkin | 222 | 184 | 45 | 51 | 19 | 50 | 57 |
| **Mechanical** | **158** | **78** | 39 | 12 | 24 | 36 | 54 |
| Giant | 89 | 65 | 7 | 20 | 18 | 18 | 26 |

## Findings that change the design

1. **Mechanical is too thin for a spec.** There are only 158 liftable mechanicals, half have no spells, and most of the spells they do have are quest scripts (kill credit, "Eject Sneed"). Demons are almost absent from Northrend (9) and the Eastern Kingdoms (26) as well, so a Felbound player would have a dry levelling path there.
2. **Undead is the second-richest pool (1,014),** and it's strong in Northrend and in instances. It deserves a spec.
3. **`unit_class` is useless as an archetype source.** Beasts are 97% "warrior"; "paladin" just means the mob has mana; rogue appears 16 times in total. **The archetype has to come from the mob's spells.** Spell-derived split, Humanoid example: 833 caster, 784 melee, 296 healer, 126 hybrid, and 2,677 with no or utility-only spells.
4. **About 40% of liftable mobs have no combat spells** (they only auto-attack). The kit must be built from templates (type/family + archetype), with the creature's own spells as optional flavour. A real stolen spell can't be the backbone.
5. **550 beasts have family 0** (no hunter family). They need a family assigned by model or name for templates to work.
6. **Spell lists contain lots of junk:** quest credit, "(DND)" visuals, summons of named NPCs, phase and vehicle spells. A curated whitelist is required. Nothing from the raw list can be handed out automatically.

## Spec roster revisited (proposal)

| Spec | Types | Role | Pool | Why |
|---|---|---|---|---|
| Beastskin | Beast | Melee DPS | 1,469 | 33 families, every zone |
| Courtier | Humanoid | Healer (+ disguise thief) | 4,716 | 296 healer mobs to steal from |
| Felbound | Demon + Undead ("the Damned") | Ranged caster | 1,402 | Demons in Outland/Kalimdor, undead in Northrend; shadow, fire and frost casters |
| Constructskin | Mechanical + Elemental + Giant | Tank | 696 | Golems, elementals and giants all read as "heavy body"; Stone Skin, Ground Tremor, Trample, War Stomp, Lightning Shield |

Dragonkin (222) and "Not specified" oozes/silithid are best kept as rare "exotic skins" that any spec can lift: one ledger slot, cosmetic plus utility only.

## Ability vocabulary per type (top spells by number of creatures using them)

**Beast**
- Damage: Poison, Rend, Gore, Venom Sting, Swipe, Ravage, Maul
- Debuff: Tendon Rip, Infected Wound, Rabies, Pierce Armor
- Buff: Rushing Charge, Thrash, Enrage, Furious Howl, Feign Death
- CC: Swoop, Web, Sonic Burst, Crystalline Slumber
- Stealth: Sneak, Burrow

**Humanoid**
- Damage: Shoot, Lightning Bolt, Shadow Bolt, Fireball, Frostbolt, Cleave, Backstab
- Heal: Healing Wave, Heal, Renew, Healing Ward, Power Word: Shield
- Utility: Net, Frost Nova, Shield Bash, Kick, Disarm, Bloodlust, Battle Shout, Demoralizing Shout
- Mobility: Intercept, Shield Charge, Blink

**Demon**
- Damage: Shadow Bolt, Fireball, Fel Fireball, Immolate, Rain of Fire, Mortal Strike, Cleave
- Debuff: Curse of Weakness, Curse of Tongues, Cripple, Mana Burn, Lash of Pain
- CC: Psychic Scream, Fear, Sleep, Rain of Chaos
- Heal: Gift of the Xavian, Drain Life

**Undead**
- Damage: Frostbolt, Shadow Bolt, Shadow Word: Pain, Icy Touch, Plague Strike, Scourge Hook, Mind Flay, Shadow Bolt Volley
- Debuff: Curse of Thule, Banshee Curse, Curse of Blood, Grave Coil, Disease Cloud
- CC: Fear, Frost Nova, Deafening Screech, Web Wrap
- Defence: Shroud of Death, Bone Armor, Frost Armor, Dark Mending
- Summon: Birth (ghoul spawn)

**Elemental**
- Damage: Lightning Bolt, Chain Lightning, Lava Burst, Earth Shock, Water Bolt, Fire Nova
- CC: Entangling Roots, Ground Tremor, Frost Nova
- Defence: Lightning Shield, Thorns, Stone Skin, Wild Regeneration

**Mechanical**
- Machine Gun, Cutting Laser, Fel Cannon Blast, Electrified Net, Short Circuit, Mechano Kick, Repair, Reflection Field, Energy Sap

**Giant**
- Trample, War Stomp, Stomp, Knock Away, Boulder, Ground Slam, Growth

**Dragonkin**
- Flame, Frost, Acid and Sand Breath; Wing Buffet/Flap; Sleep; Fire Shield; Intangible Presence

## Expanding it — kit-template system

The kit is made of 4 slots. Each slot comes from a template keyed by (spec, family/sub-type, archetype):

| Slot | Source | Example (Beastskin, Cat) | Example (Courtier, healer mob) |
|---|---|---|---|
| Builder | Template | Claw | Healing Wave–style direct heal |
| Spender | Template | Rake/Ravage finisher | Renew-style HoT |
| Cooldown | Template, by family | Prowl-pounce (Cat), Furious Howl (Wolf), Web (Spider) | Healing Ward / Bloodlust |
| Signature | Mob's own whitelisted spell, re-scaled | Rabies, Tendon Rip, etc. | Power Word: Shield, Lightning Shield, etc. |

- **Family templates for Beastskin:** about 12 groups instead of 33 families. Cat/raptor/ravager = bleed; wolf/hyena = pack; spider/scorpid/wasp/silithid = poison+web; bear/boar/rhino/gorilla = brawler; bird/bat/wind serpent/dragonhawk = aerial; crab/turtle/crocolisk = shell; and so on.
- **Mobs with no spells** still get a full kit from the template, plus a generic signature for their family.
- **Rares and elites** get a cosmetic upgrade (glow, a better signature visual), not more power.
- **CoA's own Spell.dbc** (209k entries) is a good source of existing scaled player-side versions to borrow for template spells. The next step is to search it for player-usable copies of the top signature spells.

## Technical notes found while mapping
- Humanoid NPCs use baked `CreatureDisplayInfoExtra` displays, so the "exact look" works with gear and all.
- Giants and dragonkin need a scale clamp on the morph (some models are 3–5× player size).
- `Shoot`/`Throw` signatures need a ranged-weapon check or a converted version.

## Files
- `creature_pool.csv` — every liftable creature: type, family, level, rank, spawns per continent, spell-derived archetype, and spells (id:name:category).
- `spells_by_type.csv` — every spell used by liftable creatures per type: category, number of creatures using it, level range and example creatures. Use it as the starting point for the whitelist.
