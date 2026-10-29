-- Vanilla warlocks start with their Imp, as in stock WotLK (the Bronzebeard copy, like the rest of their spells).
-- It also unlocks the Imp slot in Wardrobe > Incarnations, which the client shows only for a known summon spell.
DELETE FROM acore_world.playercreateinfo_spell_custom WHERE Note = 'vanilla start: Summon Imp';
INSERT INTO acore_world.playercreateinfo_spell_custom (racemask, classmask, Spell, Note) VALUES (0, 256, 1100688, 'vanilla start: Summon Imp');

-- Vanilla classes (1-9, 11) never received Shoot. CoA classes get it from playercreateinfo_spell_custom;
-- SkillLineAbility.dbc classmask 512 limits Shoot (3018) to Hero. Hunters also need Auto Shot (75).
-- classmask 1535 = bits for classes 1-9 and 11. classmask 4 = Hunter.
DELETE FROM acore_world.playercreateinfo_spell_custom WHERE Note LIKE 'vanilla start: %'
  AND Note <> 'vanilla start: Summon Imp';
INSERT INTO acore_world.playercreateinfo_spell_custom (racemask, classmask, Spell, Note) VALUES
(0, 1535, 3018, 'vanilla start: Shoot'),
(0, 4, 75, 'vanilla start: Auto Shot'),
(0, 1535, 264, 'vanilla start: Bows'),
(0, 1535, 266, 'vanilla start: Guns'),
(0, 1535, 5011, 'vanilla start: Crossbows'),
(0, 13, 2764, 'vanilla start: Throw');

-- Existing vanilla characters created before this grant.
INSERT IGNORE INTO acore_characters.character_spell (guid, spell, specMask)
SELECT c.guid, 3018, 255 FROM acore_characters.characters c
WHERE c.class IN (1, 2, 3, 4, 5, 6, 7, 8, 9, 11);
INSERT IGNORE INTO acore_characters.character_spell (guid, spell, specMask)
SELECT c.guid, 75, 255 FROM acore_characters.characters c WHERE c.class = 3;
INSERT IGNORE INTO acore_characters.character_spell (guid, spell, specMask)
SELECT c.guid, 264, 255 FROM acore_characters.characters c
WHERE c.class IN (1, 2, 3, 4, 5, 6, 7, 8, 9, 11);
INSERT IGNORE INTO acore_characters.character_spell (guid, spell, specMask)
SELECT c.guid, 266, 255 FROM acore_characters.characters c
WHERE c.class IN (1, 2, 3, 4, 5, 6, 7, 8, 9, 11);
INSERT IGNORE INTO acore_characters.character_spell (guid, spell, specMask)
SELECT c.guid, 5011, 255 FROM acore_characters.characters c
WHERE c.class IN (1, 2, 3, 4, 5, 6, 7, 8, 9, 11);
INSERT IGNORE INTO acore_characters.character_spell (guid, spell, specMask)
SELECT c.guid, 2764, 255 FROM acore_characters.characters c
WHERE c.class IN (1, 3, 4);
