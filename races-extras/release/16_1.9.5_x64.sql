-- CoA Custom 1.9.3 (x64 client): saved Retail looks and forms for Worgen / Dracthyr, Worgen Two Forms switch spell (110690),
-- Chosen Identity removed from Dracthyr. Safe to run more than once.
USE acore_characters;
CREATE TABLE IF NOT EXISTS `character_body_shape` (`guid` int unsigned NOT NULL, `data` binary(49) NOT NULL, PRIMARY KEY (`guid`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
CREATE TABLE IF NOT EXISTS `character_retail_look` (`guid` int unsigned NOT NULL, `data` binary(132) NOT NULL, PRIMARY KEY (`guid`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
CREATE TABLE IF NOT EXISTS `character_form_look` (`guid` int unsigned NOT NULL, `data` binary(132) NOT NULL, PRIMARY KEY (`guid`)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
DELETE FROM `character_spell` WHERE `spell` = 110687;

USE acore_world;
REPLACE INTO `spell_dbc` VALUES (110690,0,0,0,16,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,5000,0,0,0,0,0,101,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-1,0,0,3,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4228,0,5772,0,0,'Two Forms',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,16712190,'Racial',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,16712190,'Switch between your human and worgen forms.',NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,16712190,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,16712188,0,133,1500,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,0,1,0,0,0,1,1,1,0,0);
DELETE FROM `spell_script_names` WHERE `spell_id` IN (14000132, 110690);
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (110690, 'spell_race_two_forms');

USE acore_characters;
INSERT IGNORE INTO `character_spell` (`guid`, `spell`, `specMask`) SELECT `guid`, 110690, 255 FROM `characters` WHERE `race` = 13;

USE acore_world;
-- Gnoll (race 83) active racial: Scavenger, a copy of Cannibalize (spell 14000832, formerly "Bone Gnaw"; script spell_gen_cannibalize).
-- The race's racial list (CoaCustomRacials.h) now includes it; the client Spell.dbc row is renamed in patch-ZZZZD.
UPDATE spell_dbc SET Name_Lang_enUS = 'Scavenger',
  Description_Lang_enUS = 'A gnoll eats what it kills.\n\nWhen activated, regenerates $20578s1% of total health every $20578t1 sec for $20578d.  Only works on Humanoid or Undead corpses within $a1 yds.  Any movement, action, or damage taken while Scavenging will cancel the effect.'
  WHERE ID = 14000832;
INSERT IGNORE INTO spell_script_names (spell_id, ScriptName) VALUES (14000832, 'spell_gen_cannibalize');
-- the Gnoll's old passive "Scavenger" (+5 Skinning) is now "Skinner"
UPDATE spell_dbc SET Name_Lang_enUS = 'Skinner' WHERE ID = 110923;
