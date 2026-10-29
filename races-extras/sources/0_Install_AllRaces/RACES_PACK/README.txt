CUSTOM RACES PACK  -  Banshee / Elune's Chosen / Faceless One / Qiraji
=====================================================================
For: AzerothCore "CoA" fork server + Ascension (CoA) game client (custom race slots up to 128).
A plain 3.3.5a client/server can NOT show new races without client exe changes - this pack needs the CoA client + CoA server source.

RACES
 ID  Race            Faction   Plays like (base)   Body                                   Extra racial
 84  Banshee         Horde     Undead              Banshee (hand-painted, 9 skins, 8 hair) Spectral Glide (fast travel, slow fall, water walk)
 85  Elune's Chosen  Alliance  Night Elf           Ghostly Nightborne (6 skins, 8 hair)    Elune's Veil (ghost glow, applied at login)
 86  Faceless One    Horde     Orc                 Faceless One / N'Zoth (9 skins)         -
 87  Qiraji          Horde     Troll               AQ Swarmguard, own animated wings       Wings of the Swarm (slow fall, at login)
                                                   male 7 skins (blue-led), female 8 skins
 Base race gives racials, starting zone, reputation, quests, voice. Display ids 977610/11, 977620/21, 977630/31.
 Spells 13990131 / 13990141 / 13990151.

WHAT'S DONE TO THE BODIES (so they don't look like morphs)
 - every missing player animation filled in (walk back, jump, swim, emotes, mounted, casts...).
   Bodies with no spell-cast animation use their ATTACK animations for casts.
 - weapon / shield / back attachment points added -> equipped weapons show in hand.
 - scaled to about human height (males a touch taller). Hitbox = human.
 - emote/voice sounds from the base race.

FOLDERS
 client\Data\         patch-ZEZU/ZEZV/ZEZW/ZEZX.MPQ (models, skins, DBCs), patch-ZI.MPQ (character-create screen), dbc\Spell.dbc
                      -> EVERY PLAYER needs these in their client Data folder. patch-ZI.MPQ REPLACES the existing one.
 server\dbc\          ChrRaces, CreatureModelData, CreatureDisplayInfo, Spell.dbc -> worldserver Data\dbc
 server\sql\          01..07, run in order on the WORLD database
 server\src\          apply_races_src.py -> edits the server source; then rebuild worldserver
 install.ps1          does all of the above (edit the paths at the top first). Makes .PRE_RACES backups + a DB backup.
 previews\            screenshots / art
 assets\, data\, tools\   raw models, skins (new + old), voice lines, animation lists, DBC rows - see ASSETS_README.txt

MANUAL INSTALL (if not using install.ps1)
 1. close server + client  2. copy client\Data into the client  3. copy server\dbc into the server dbc folder
 4. run server\sql\*.sql in order on the world DB  5. py -3 server\src\apply_races_src.py "<azerothcore source>"
 6. rebuild worldserver, start, delete client Cache folder, create a character.

IMPORTANT
 - The pack's patch-ZI.MPQ and DBCs are built on top of a specific CoA client/server version. If the target server
   has its OWN custom changes in patch-ZI / ChrRaces / Spell.dbc / CreatureDisplayInfo, these files would overwrite them -
   merge instead (ask for a re-build against their files).
 - Race ids 84-87 must be free on the target server.
 - Faction is set per race (table above). Changing a faction = rebuild (base race + ChrRaces team).

REVERT: put back the *.PRE_RACES files, delete patch-ZEZU..ZEZX.MPQ, restore PRE_RACES_world_backup.sql, undo the source edit (git checkout).
