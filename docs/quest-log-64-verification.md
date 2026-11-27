# Quest log 64: in-game verification of CoA64

Builds: server `F:\WorkDir\azerothcore-wotlk-coa\var\build\ql64\bin\worldserver.exe` (branch `feat/quest-log-64`),
client `F:\WorkDir\CoA64\bin\Win\CoA64.exe` (branch `feat/quest-log-64`, 43e1488).
Server config: `Quests.LogSize = 64` in the worldserver.conf used. Start the client with `-extlog 1`.
Character: about 40 quests in the log (`.quest add <id>` as GM).

Mark each line PASS / FAIL with a note.

| # | Check | Result |
|---|---|---|
| 1 | Quest log lists all 40 quests with zone headers; counter reads "Quests: 40/64" | PASS (27 quests listed, counter n/64) |
| 2 | Quests 26–40 can be tracked (watch frame) | PASS after fix 4be37d2 (32 of 32 tracked) |
| 3 | Quests 26–40 show map POIs on the world map | PASS (27 quests) |
| 4 | Quests 26–40 show the scaled level colour and reward XP in the log and in quest links | PASS (all quests scaled; XP/links not separately reported) |
| 5 | Killing a mob for an overflow quest (e.g. quest 30) updates the tracker and the log | PASS |
| 6 | Quest items for an overflow quest (e.g. quest 35) drop / show as quest loot; quest objects sparkle | PASS |
| 7 | Abandon an overflow quest (e.g. quest 38): it leaves the log and the server | PASS |
| 8 | Share an overflow quest (e.g. quest 33) with a party member | PASS |
| 9 | Turn in a quest in slots 1–25: an overflow quest moves up, shown once, no duplicate | PASS |
| 10 | `/reload`: all quests still listed | PASS |
| 11 | Log out to another character: that character's own log, no leftovers | PASS |
| 12 | The Extra Quests window never appears | PASS |
| 13 | With `Quests.LogSize = 30` and `reload config` while logged in: counter reads n/30 without relog | PASS (instant) |
| 14 | Against the old server build (`var\build\coa`), the client behaves as a 25-quest client | PASS (25 in log, 3 in Extra Quests window) |
| 15 | Track a quest in slot 26+, log out and back in: it is still tracked. With 30 quests and auto quest watch off, track quest #30 and turn in quest #1: no "Quest accepted", #30 still tracked | |
