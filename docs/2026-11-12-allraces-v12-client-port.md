# AllRaces v12 in the client (2026-11-12)

CoA Custom 1.9.9 ships 14 more playable races (AllRaces v12). Twelve of them are Esteria "pack" races whose models and
skins live only in the client's `Esteria<Name>.bin` banks: Sureki, Dreadlord, Titan, Wretched, Fungarian, Sporeling,
Stormforged, Mo'arg, Nerubian, Lich, Mogu and Lost One. `races-port` and the `coa-custom-1.9.9` tag never carried the
client half of them (the pack's tables and banks were pushed, the code was not), so a client built from this repo left
those races on the Haranir bank: bodies with no textures. Elune's Chosen (118) and Qiraji (88) already worked.

## What was recovered

ilusixn's 1.9.9 `CoA64.exe` (built 2026-11-05 in `C:\CoA-Build\coa64`) was read with a small PE/x64 scanner: MSVC's
indexed switches (byte index table + RVA jump table, `sub ecx, base`) were decoded, and each arm's `lea` target resolved
to its string or its `Option` triple array. That gave, for the races above and nothing else:

- `RaceOf()`: client race id -> Esteria id, ids 80..91 in bank order (86->80 Sureki, 89->81 Dreadlord, 91->82 Titan,
  92->83 Wretched, 93->84 Fungarian, 111->85 Sporeling, 114->86 Stormforged, 119->87 Mo'arg, 120->88 Nerubian,
  122->89 Lich, 123->90 Mogu, 125->91 Lost One).
- `RetailSelections()`, `RetailTextureBank()`, `RetailCacheFamily()`: the bank, texture bank and cache folder per id.
- The field counts per race and sex, read from his `.rdata` `Option` arrays and cross-checked against each bank's own
  selection records (the two agree, e.g. Fungarian `0:11 1:8 2:3 3:5 4:4`, Stormforged `0:13 1:3`, Mogu `0:12`).
  Sureki's female and Titan's male bank additionally carry a second field (hair / armour colour).

`CreatureAppearance::Uses`, `GenderAllowed`, `Options` and `Validate` were extended to 80..91, the `EA_*` cycle chain
got their controls and labels, and `SetHaranirMaterials()` lets them through the same way it already did Qiraji: their
skins come from the selection bank, so there is no per-race armour fit to port.

## Verified and still open

Verified: the rebuilt exe's `RaceOf` switch and its two string switches now read the same 38 race mappings as
ilusixn's exe, byte for byte as far as the switch tables go.

Open, none of it needed for the bodies to be textured: the labels are ours (his exact control labels for the new races
were not attributed to an id, only their tables), `EsteriaSkillRace()` leaves these ids unmapped so the stock race
masks apply, and the character-creation screen lists races from the server's tables rather than from this module.
