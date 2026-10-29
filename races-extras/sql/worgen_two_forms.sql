-- Worgen form switch: its own spell (a Visage clone) so spell 14000132 keeps its fear removal.
USE acore_world;
DELETE FROM acore_world.spell_dbc WHERE ID = 110690;
CREATE TEMPORARY TABLE tmp_two_forms SELECT * FROM acore_world.spell_dbc WHERE ID = 110685;
UPDATE tmp_two_forms SET ID = 110690, Name_Lang_enUS = 'Two Forms', Description_Lang_enUS = 'Switch between your human and worgen forms.', SpellIconID = 5772;
INSERT INTO acore_world.spell_dbc SELECT * FROM tmp_two_forms;
DROP TEMPORARY TABLE tmp_two_forms;
DELETE FROM acore_world.spell_script_names WHERE spell_id IN (14000132, 110690);
INSERT INTO acore_world.spell_script_names (spell_id, ScriptName) VALUES (110690, 'spell_race_two_forms');
INSERT IGNORE INTO acore_characters.character_spell (guid, spell, specMask)
SELECT guid, 110690, 255 FROM acore_characters.characters WHERE race = 13;
