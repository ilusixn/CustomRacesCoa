USE acore_characters;
-- Custom races keep only the racial skill lines of the race they borrow from (gen_races.py RACIAL). Characters
-- made while a race borrowed another race kept that race's line too: the Murlocs had the Night Elf racials
-- (Shadowmeld twice with their bonus racial) next to the Undead ones. (2026-10-04)
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 9 AND cs.skill IN (101,124,126,220,733,753,754,756,760,11760);
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 12 AND cs.skill IN (101,124,125,126,220,753,754,756,760,11125,11760);
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 13 AND cs.skill IN (101,124,125,220,733,753,754,756,760,11125,11760);
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 14 AND cs.skill IN (101,124,125,126,220,733,753,754,760,11125,11760);
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 15 AND cs.skill IN (124,125,126,220,733,753,754,756,760,11125,11760);
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 16 AND cs.skill IN (101,124,125,126,220,733,753,756,760,11125,11760);
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 17 AND cs.skill IN (101,125,126,220,733,753,754,756,760,11125,11760);
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 18 AND cs.skill IN (101,124,125,126,220,733,753,756,760,11125,11760);
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 19 AND cs.skill IN (101,124,125,126,220,753,754,756,760,11125,11760);
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 20 AND cs.skill IN (101,124,125,126,220,733,753,754,756,11125);
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 21 AND cs.skill IN (101,124,125,126,733,753,754,756,760,11125,11760);
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 22 AND cs.skill IN (101,124,125,126,220,733,753,754,756,11125);
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 23 AND cs.skill IN (101,124,126,220,733,753,754,756,760,11760);
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 24 AND cs.skill IN (101,124,125,126,220,753,754,756,760,11125,11760);
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 25 AND cs.skill IN (101,124,125,126,220,753,754,756,760,11125,11760);
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 26 AND cs.skill IN (101,124,125,126,733,753,754,756,760,11125,11760);
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 27 AND cs.skill IN (124,125,126,220,733,753,754,756,760,11125,11760);
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 28 AND cs.skill IN (101,124,125,126,220,753,754,756,760,11125,11760);
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 29 AND cs.skill IN (101,125,126,220,733,753,754,756,760,11125,11760);
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 30 AND cs.skill IN (101,124,125,126,733,753,754,756,760,11125,11760);
DELETE cs FROM acore_characters.character_skills cs JOIN acore_characters.characters c ON c.guid = cs.guid WHERE c.race = 31 AND cs.skill IN (101,124,125,126,733,753,754,756,760,11125,11760);
