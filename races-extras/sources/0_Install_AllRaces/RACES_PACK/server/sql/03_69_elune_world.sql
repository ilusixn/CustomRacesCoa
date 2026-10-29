-- ELUNE'S CHOSEN (CoA fork): race 85, Nightborne body + ghost skins (patch-ZEZV), Night Elf team/start/stats/racials (base 4).
-- REVERT = PRE_ELUNE dump or the DELETEs below.
START TRANSACTION;
DELETE FROM playercreateinfo WHERE race = 85;
DELETE FROM playercreateinfo_item WHERE race = 85;
DELETE FROM playercreateinfo_action WHERE race = 85;
DELETE FROM player_race_stats WHERE Race = 85;
DELETE FROM ascension_custom_class_race WHERE race = 85;
INSERT INTO playercreateinfo SELECT 85,p.class,u.map,u.zone,u.position_x,u.position_y,u.position_z,u.orientation
  FROM playercreateinfo p JOIN (SELECT * FROM playercreateinfo WHERE race=4 ORDER BY class LIMIT 1) u WHERE p.race=63;
INSERT INTO playercreateinfo_item SELECT 85,class,itemid,amount,Note FROM playercreateinfo_item WHERE race=63;
INSERT INTO playercreateinfo_action SELECT 85,class,button,action,type FROM playercreateinfo_action WHERE race=63 AND type<>0;
INSERT INTO player_race_stats SELECT 85,Strength,Agility,Stamina,Intellect,Spirit FROM player_race_stats WHERE Race=4;
INSERT INTO ascension_custom_class_race SELECT DISTINCT class,85 FROM ascension_custom_class_race WHERE race=63;
COMMIT;
