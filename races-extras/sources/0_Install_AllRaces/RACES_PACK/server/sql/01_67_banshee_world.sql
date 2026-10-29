-- BANSHEE (CoA fork): race 84, Blood Elf model + banshee skins (patch-ZEZU), Undead team/start/stats/racials (base race 5).
-- Classes = the Blood Elf class list; start = Deathknell (Undead warrior start).
-- REVERT = PRE_BANSHEE dump (deploy script) or the DELETEs below.
START TRANSACTION;
DELETE FROM playercreateinfo WHERE race = 84;
DELETE FROM playercreateinfo_item WHERE race = 84;
DELETE FROM playercreateinfo_action WHERE race = 84;
DELETE FROM player_race_stats WHERE Race = 84;
DELETE FROM ascension_custom_class_race WHERE race = 84;
INSERT INTO playercreateinfo SELECT 84,p.class,u.map,u.zone,u.position_x,u.position_y,u.position_z,u.orientation
  FROM playercreateinfo p JOIN (SELECT * FROM playercreateinfo WHERE race=5 ORDER BY class LIMIT 1) u WHERE p.race=10;
INSERT INTO playercreateinfo_item SELECT 84,class,itemid,amount,Note FROM playercreateinfo_item WHERE race=10;
INSERT INTO playercreateinfo_action SELECT 84,class,button,action,type FROM playercreateinfo_action WHERE race=10 AND NOT (type=0 AND action IN (28730,25046,50613,28734,28877,822));
INSERT INTO player_race_stats SELECT 84,Strength,Agility,Stamina,Intellect,Spirit FROM player_race_stats WHERE Race=5;
INSERT INTO ascension_custom_class_race SELECT DISTINCT class,84 FROM ascension_custom_class_race WHERE race=10;
COMMIT;
