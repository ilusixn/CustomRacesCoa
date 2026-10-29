-- desfaz support_v2_db.sql + magias
DELETE FROM coa_client_skill_line_ability WHERE ID BETWEEN 2650001 AND 2650005;
DELETE FROM spell_group WHERE id IN (26000,26001);
DELETE FROM spell_group_stack_rules WHERE group_id IN (26000,26001);
DELETE FROM spell_proc WHERE SpellId=2600011;
DELETE FROM spell_bonus_data WHERE entry IN (2600007,2600042,1277681);
DELETE FROM spell_script_names WHERE spell_id=1277680 AND ScriptName="spell_coa_beacon_of_virtue";
DELETE FROM spell_dbc WHERE ID BETWEEN 2600006 AND 2600043;
DELETE FROM coa_client_spell_description WHERE ID BETWEEN 2600006 AND 2600043;
