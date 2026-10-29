-- [SUPPORT-V2] mudancas de banco (acore_world) da rodada Paladin Support v2. Reverter: support_v2_db_rollback.sql
-- 1) livro: 5 linhas da aba Support transmitidas ao cliente
DELETE FROM coa_client_skill_line_ability WHERE ID BETWEEN 2650001 AND 2650005;
INSERT INTO coa_client_skill_line_ability (ID,SkillLine) VALUES (2650001,11901),(2650002,11901),(2650003,11901),(2650004,11901),(2650005,11901);
-- 2) nao acumular: Light's Bulwark x Pain Suppression (classicas) | Shared Devotion x Hand of Sacrifice
DELETE FROM spell_group WHERE id IN (26000,26001);
DELETE FROM spell_group_stack_rules WHERE group_id IN (26000,26001);
INSERT INTO spell_group (id,spell_id) VALUES (26000,2600008),(26000,33206),(26000,1133206),(26000,1233206),(26001,2600006),(26001,6940),(26001,1106940);
INSERT INTO spell_group_stack_rules (group_id,stack_rule,description) VALUES (26000,1,"Support: Light's Bulwark x Pain Suppression"),(26001,1,"Support: Shared Devotion x Hand of Sacrifice");
-- 3) Oath of Protection: proc so no BLOQUEIO (HitMask 0x40), 1 vez a cada 6s
DELETE FROM spell_proc WHERE SpellId=2600011;
INSERT INTO spell_proc (SpellId,SchoolMask,SpellFamilyName,SpellFamilyMask0,SpellFamilyMask1,SpellFamilyMask2,ProcFlags,SpellTypeMask,SpellPhaseMask,HitMask,AttributesMask,DisableEffectsMask,ProcsPerMinute,Chance,Cooldown,Charges) VALUES (2600011,0,0,0,0,0,0,0,0,64,0,0,0,100,6000,0);
-- 4) escala com poder magico: Aura of Reprisal 7%; curas copiadas/percentuais sem escala extra
DELETE FROM spell_bonus_data WHERE entry IN (2600007,2600042,1277681);
INSERT INTO spell_bonus_data (entry,direct_bonus,dot_bonus,ap_bonus,ap_dot_bonus,comments) VALUES (2600007,0.07,0,0,0,"Support - Aura of Reprisal"),(2600042,0,0,0,0,"Support - Oath of Protection (1% vida)"),(1277681,0,0,0,0,"Support - Beacon of Virtue copia (sem escala dupla)");
-- 5) script da Beacon of Virtue
DELETE FROM spell_script_names WHERE spell_id=1277680 AND ScriptName="spell_coa_beacon_of_virtue";
INSERT INTO spell_script_names (spell_id,ScriptName) VALUES (1277680,"spell_coa_beacon_of_virtue");
