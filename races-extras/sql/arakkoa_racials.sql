USE acore_characters;
DROP TRIGGER IF EXISTS arakkoa_racials;
CREATE TRIGGER arakkoa_racials AFTER INSERT ON characters FOR EACH ROW
  INSERT IGNORE INTO character_spell (guid, spell, specMask)
  SELECT NEW.guid, s.id, 255 FROM (
    SELECT 13990001 id UNION SELECT 13990002 id UNION SELECT 13990003 id
    UNION SELECT 13990004 id UNION SELECT 13990005 id
  ) s WHERE NEW.race = 79;
INSERT IGNORE INTO character_spell (guid, spell, specMask)
  SELECT c.guid, s.id, 255 FROM characters c JOIN (
    SELECT 13990001 id UNION SELECT 13990002 id UNION SELECT 13990003 id
    UNION SELECT 13990004 id UNION SELECT 13990005 id
  ) s WHERE c.race = 79;
