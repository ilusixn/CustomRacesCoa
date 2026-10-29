# Install

**Read first:** this is a study project. Back up your database, your server tree and your client
`Data` folder before you start. Never install client patches while the game is running.

## Requirements
- Server source: the AzerothCore CoA fork by ilusixn, branch `coa-custom-1.5` at
  `e69c0bf748` ("CoA Custom 1.5.1"), merged with jealous-sound `main` at `6985abb527` (2026-10-10).
  Our patches were produced on top of that merge; on other revisions they may need manual rebasing.
- Playerbots module 1.9.x was present on the test realm (not required by these patches).
- Docker-based AzerothCore setup (the tools call `docker exec <db container> mysql ...`).
- Client: CoA Custom 1.5.1 client (3.3.5a 12340 based) with its own `patch-B/M/T` MPQs.
- Python 3.10+, `patch`, gcc + StormLib (to build `client/build/mkpatch`).

Environment variables used by the tools: `COA_ROOT` (server tree, default `.`), `DB_CONTAINER`
(default `ac-database`), `DB_USER` (default `root`), `DB_PASS`, `WORLD_CONTAINER` (default
`ac-worldserver`), `MPQ_TOOLS` (folder with `mkpatch` + `libstorm.so`).

## 1. Server source
```bash
cd <your azerothcore-wotlk-coa checkout>
git am /path/to/repo/core/patches/000[1-4]-*.patch
git apply /path/to/repo/core/patches/0005-working-tree-changes.patch
```
Then build and recreate the worldserver container (a plain `restart` keeps the old image):
```bash
docker compose build ac-worldserver && docker compose up -d --force-recreate ac-worldserver
```
Set in `coa.conf`: `CoA.ClassModel = "coa wcr"`. Optional (mod-spellbook): `Spellbook.Classic.MaxLevel`.

## 2. Generate the talent DBCs from YOUR server data
The generators read your installed `CharacterAdvancement.dbc` / `ChrSpecs.dbc` / `CharacterAdvancementEssence.dbc`
and write new ones; nothing is shipped pre-generated.
```bash
export COA_ROOT=<server tree>
tools/aplica_todas.sh            # generates the 9 classic trees (see script header)
tools/fecha_regen.sh <in_dir> <out_dir>   # stabilization + Support freeze + duplicate-audit gate
```
`fecha_regen.sh` runs `congela_support.py` while a file named `SUPPORT_CONGELADA` exists in `COA_ROOT`
(create it: `touch $COA_ROOT/SUPPORT_CONGELADA`). The audit gate must print `TRAVA DUPLICADAS: OK`.
Copy the output DBCs to `env/dist/data/dbc/` and restart the worldserver.

## 3. World database
```bash
for f in sql/world/*.sql; do docker exec -i $DB_CONTAINER mysql -u$DB_USER -p$DB_PASS acore_world < "$f"; done
```
Do **not** apply `sql/support-frozen/support_v2_db.sql` (that would re-enable Support rules).
`sql/support-frozen/freeze_support_world.sql` is safe to apply (disables Support rows if present).

## 4. Client patches
```bash
python3 client/build/extract_originals.py orig <client>/Data/patch-T.MPQ <client>/Data/patch-M.MPQ <client>/Data/patch-B.MPQ
ORIG=orig GEN=<generated dbc dir> OUT=out MPQ_TOOLS=<dir with mkpatch+libstorm.so> client/build/build_patches.sh
```
Bases used to build the deltas/diffs: SkillLineAbility, ChrRaces, CharBaseInfo, SkillRaceClassInfo,
CharacterCreate.lua, UIParent.lua, MainMenuBarMicroButtons.lua = `patch-T`; SkillLine = `patch-M`;
the other Lua files = `patch-B`. Copy `out/patch-ZZ*.MPQ` into the client `Data` folder **with the game closed**.

## 5. Verify
- worldserver log: `Loaded N CoA talent entries ... 97 specializations`, no new errors.
- `python3 tools/audita_duplicadas.py env/dist/data/dbc/ --trava` → `TRAVA DUPLICADAS: OK`.
- In game: create a classic character, open the talent window (3 specs), learn a talent, switch spec, relog.

## Uninstall / rollback
- Server: `git revert`/reset the applied patches, restore your backed-up DBCs, restore the DB tables you backed up
  (`playercreateinfo_action`, `trainer_spell`, and the Support tables listed in `sql/support-frozen/`).
- Client: delete `patch-ZZS/ZZV/ZZW/ZZX/ZZY/ZZZ.MPQ` (or restore your previous copies) with the game closed.
- Support un-freeze: remove `SUPPORT_CONGELADA`, restore the pre-freeze DBCs, apply
  `sql/support-frozen/reactivate_support_world.sql`, rebuild client patches.
