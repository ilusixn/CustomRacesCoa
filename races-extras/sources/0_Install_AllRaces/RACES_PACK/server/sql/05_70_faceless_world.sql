-- FACELESS ONE (CoA fork): race 86, Faceless body (patch-ZEZW), Orc team/start/stats/racials (base 2), class list = Murloc (30).
-- REVERT = PRE dump or the DELETEs below.
START TRANSACTION;
DELETE FROM playercreateinfo WHERE race = 86;
DELETE FROM playercreateinfo_item WHERE race = 86;
DELETE FROM playercreateinfo_action WHERE race = 86;
DELETE FROM player_race_stats WHERE Race = 86;
DELETE FROM ascension_custom_class_race WHERE race = 86;
INSERT INTO playercreateinfo SELECT 86,p.class,u.map,u.zone,u.position_x,u.position_y,u.position_z,u.orientation
  FROM playercreateinfo p JOIN (SELECT * FROM playercreateinfo WHERE race=2 ORDER BY class LIMIT 1) u WHERE p.race=30;
INSERT INTO playercreateinfo_item SELECT 86,class,itemid,amount,Note FROM playercreateinfo_item WHERE race=30;
INSERT INTO playercreateinfo_action SELECT 86,class,button,action,type FROM playercreateinfo_action WHERE race=30 AND type<>0;
INSERT INTO player_race_stats SELECT 86,Strength,Agility,Stamina,Intellect,Spirit FROM player_race_stats WHERE Race=2;
INSERT INTO ascension_custom_class_race SELECT DISTINCT class,86 FROM ascension_custom_class_race WHERE race=30;
DELETE FROM creature_model_info WHERE DisplayID IN (977620,977621);
INSERT INTO creature_model_info (DisplayID,BoundingRadius,CombatReach,Gender,DisplayID_Other_Gender) SELECT 977620,BoundingRadius,CombatReach,0,0 FROM creature_model_info WHERE DisplayID=49;
INSERT INTO creature_model_info (DisplayID,BoundingRadius,CombatReach,Gender,DisplayID_Other_Gender) SELECT 977621,BoundingRadius,CombatReach,1,0 FROM creature_model_info WHERE DisplayID=50;
COMMIT;
