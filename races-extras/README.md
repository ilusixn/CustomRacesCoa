# races-extras

Everything outside the C++ client that the custom race work depends on. Development stage; not stable.

- `glue/`   GlueXML / AddOn Lua (character creation and selection: form tabs, racial lists, preview fit). Packed into a client MPQ.
- `server/` AzerothCore (CoA Custom 1.9 base) source changes: `races-core.patch` against `src/`, and the full changed files under `files/`.
- `sql/`    CoA Custom SQL (racials, races, Worgen Two Forms spell 110690).
- `tools/`  Python/C++ helper scripts used to build the client data patches and merge the pieces.

Not included: models, textures, DBC files, MPQs (large, and many are other authors' work).

## assets (added later)
- `assets/mpq/`  the client patch archives built for these races (patch-ZZZD, ZZZZA, ZZZZC, ZZZZD), as installed in `Data/`.
- `assets/mpq/patch-ZZZZV.MPQ`, `assets/mpq/patch-ZZZZZ-HD.MPQ`  Stefan's Witch Hunter Vault for the 10 original races,
  male and female: ZZZZV holds the standard models (from patch-CHA), ZZZZZ-HD the HD models (from patch-Q). The
  64-bit client opens `patch-*-HD.MPQ` only when HD is on, so HD-off clients keep matching skins and animations.
  Both need the Vault rows in AnimationData.dbc (1792-1795) and SpellVisualKit.dbc (3455900, 3455901). Rebuild them
  on another client with `tools/build_vault_patches.py` in stefanw1337/azerothcore-wotlk-coa.
- `assets/faun_fix_patch/`  the source tree packed into patch-ZZZZA.
Not included: the generated intermediates (about 1.7 GB), third-party race source packs, and the server runtime/database.
