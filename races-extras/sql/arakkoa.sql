-- Arakkoa (Alliance race 79). Pack used 45 (Leper Troll) and displays 977475/6 (Furbolg).
-- 79 shares Tuskarr's 32-bit race-mask bit; racials are granted by exact race id, not racemask.
USE acore_world;
START TRANSACTION;
DELETE FROM playercreateinfo WHERE race=79;
DELETE FROM playercreateinfo_item WHERE race=79;
DELETE FROM playercreateinfo_action WHERE race=79;
DELETE FROM player_race_stats WHERE Race=79;
DELETE FROM ascension_custom_class_race WHERE race=79;
INSERT INTO playercreateinfo SELECT 79,class,map,zone,position_x,position_y,position_z,orientation FROM playercreateinfo WHERE race=1;
INSERT INTO playercreateinfo_item SELECT 79,class,itemid,amount,Note FROM playercreateinfo_item WHERE race=1;
INSERT INTO playercreateinfo_action SELECT 79,class,button,action,type FROM playercreateinfo_action WHERE race=1;
INSERT INTO player_race_stats SELECT 79,Strength,Agility,Stamina,Intellect,Spirit FROM player_race_stats WHERE Race=1;
INSERT INTO ascension_custom_class_race SELECT DISTINCT class,79 FROM ascension_custom_class_race;
DELETE FROM creature_model_info WHERE DisplayID IN (977507,977508);
INSERT INTO creature_model_info (DisplayID,BoundingRadius,CombatReach,Gender,DisplayID_Other_Gender)
  SELECT 977507,BoundingRadius,CombatReach,0,977508 FROM creature_model_info WHERE DisplayID=49 LIMIT 1;
INSERT INTO creature_model_info (DisplayID,BoundingRadius,CombatReach,Gender,DisplayID_Other_Gender)
  SELECT 977508,BoundingRadius,CombatReach,1,977507 FROM creature_model_info WHERE DisplayID=50 LIMIT 1;
COMMIT;
