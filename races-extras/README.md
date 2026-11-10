# races-extras

Everything outside the C++ client that the custom race work depends on. Development stage; not stable.

- `glue/`   GlueXML / AddOn Lua (character creation and selection: form tabs, racial lists, preview fit). Packed into a client MPQ.
- `server/` AzerothCore (CoA Custom 1.9 base) source changes: `races-core.patch` against `src/`, and the full changed files under `files/`.
- `sql/`    CoA Custom SQL (racials, races, Worgen Two Forms spell 110690).
- `tools/`  Python/C++ helper scripts used to build the client data patches and merge the pieces.

Not included: models, textures, DBC files, MPQs (large, and many are other authors' work).

## assets (added later)
- `assets/mpq/`  the client patch archives built for these races (patch-ZZZD, ZZZZA, ZZZZC, ZZZZD), as installed in `Data/`.
- `assets/mpq/patch-ZZZZY.MPQ`  Stefan's client patch for CoA Custom 1.9.8 and later. It sorts after patch-ZZZZX and
  holds, built from 1.9.8's own files:
  - the Witch Hunter Vault for the 10 original races, male and female (standard models from patch-CHA), with the
    Vault rows in AnimationData.dbc (1792-1795) and SpellVisualKit.dbc (3455900, 3455901);
  - the Thin Twins brother's mirror image `custom\thintwins\native\male\thintwinsmirrormale.m2`, display 977602 /
    model 999402 in CreatureDisplayInfo.dbc and CreatureModelData.dbc (1.9.8's patch-ZZZZS tables plus these rows;
    999402, since 1.9.9 has the Qiraji male at 901402).
  It replaces patch-ZZZZV.MPQ (delete that one). The four tables are whole copies: if a release changes one of them
  in an earlier archive, rebuild this patch on that client so it does not hide the change.
- `assets/mpq/patch-ZZZZZ-HD.MPQ`  the Vault's HD models (from patch-Q). The 64-bit client opens `patch-*-HD.MPQ`
  only when HD is on, so HD-off clients keep matching skins and animations.
- `assets/client_root/EsteriaAppearanceGeometry.bin`  ship this one: 1.9.8's copy (Oct 18) has no entry for
  `custom\thintwins\native\male\thintwinsmale.m2` and none for the brother, and an older `thinhumanmale.m2` entry.
  It holds 1.9.8's 26 entries plus those two; nothing of 1.9.8's is missing.
- Rebuilding on a client: in stefanw1337/azerothcore-wotlk-coa, `modules/mod-subclass/tools/Build-Vault-Patches.bat
  --fresh` and then `Build-Mirror-Twin.bat` (run next to Data). They read the Thin Twins model path from
  CreatureModelData row 901401 and take their tables only from archives that load before patch-ZZZZY.
- Server, for 1.9.9: take `stefanw1337-subclassing-test` (it contains handover-ilusixn-2.0) and add
  `modules/mod-subclass/data/sql/db-characters/2026_11_03_00_mod_subclass_twin_features.sql` to Apply-Subclass
  (the brother's own face, hair and beard). Without that table the worldserver stops at the character list.
- `assets/faun_fix_patch/`  the source tree packed into patch-ZZZZA.
Not included: the generated intermediates (about 1.7 GB), third-party race source packs, and the server runtime/database.
