-- [CONGELA-SUPPORT 12/10] mundo (reverter com REATIVA-banco.sql)
DELETE FROM acore_world.trainer_spell WHERE TrainerId=900100 AND SpellId=2600000;
DELETE FROM acore_world.spell_script_names WHERE ScriptName="spell_coa_beacon_of_virtue";
DELETE FROM acore_world.spell_group WHERE id IN (26000,26001);
DELETE FROM acore_world.spell_proc WHERE SpellId=2600011;
DELETE FROM acore_world.spell_bonus_data WHERE entry IN (2600007,2600042,1277681);
DELETE FROM acore_world.coa_client_skill_line_ability WHERE SkillLine=11901;
