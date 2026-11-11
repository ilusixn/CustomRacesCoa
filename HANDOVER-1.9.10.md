# Handover for CoA Custom 1.9.10 (from Stefan / stefanw1337, 2026-11-11)

For ilusixn and the Claude session that builds 1.9.10. Read it before merging. Stefan agrees with all of it; ask him
if something here does not match what you find.

## What 1.9.10 should be
CoA Custom 1.9.10 is the **combined** build: Sven's newest CoA64, ilusixn's races, Stefan's subclassing and
Two-Players, and Stefan's Skinshifter class. Only ilusixn can build it, because it needs his own work (including
anything not pushed yet) and Sven's newest code. Stefan publishes two separate add-ons for people who want only one
part: the Subclassing package (`subclassing-prerelease0.2` on stefanw1337/azerothcore-wotlk-coa, made for exactly
1.9.9) and the Skinshifter pre-release. When 1.9.10 is tagged, tell Stefan: the subclass package is rebuilt against it.

Please release from pushed commits only, and tag the same commit in both repos (`coa-custom-1.9.10` here and in the
server repo). 1.9.9 contained work that was never pushed (Sven's 2026-11-05 CoA64 fixes, the Server zip's
worldserver), which makes it hard for others to build on.

## Client (this repo)
1. **races-port already has Stefan's Two-Players client work** (carried mounts with per-mount fit and the D.R.I.V.E.
   car default, the joined Melee'Ranged name over a seated body, the male-slot item art for the Twins). No extra
   branch is needed for it.
2. **Merge Sven's newest snapshot into races-port.** races-port and Sven's `main` do not share history, so merge
   three-way with Sven's last public commit as the base: base `origin/main` b56a03a9, one side Sven's new snapshot
   on top of it, the other side races-port. Stefan tested this with Sven's 2026-11-11 snapshot (115 files: DLSS,
   frame generation, render pipeline, fixes): the only conflict was the end of `.gitignore` (keep `.superpowers/`
   from Sven; races-port had dropped `!ThirdParty/xvidcore/lib/x64/`). Command that reproduces it:
   `git merge-tree --write-tree --merge-base=origin/main <sven-snapshot> races-port`.
3. **Skinshifter's client patches** (eight, small): stefanw1337/azerothcore-wotlk-coa, branch `skinshifter`,
   `modules/mod-skinshifter/client/coa64/coa64-patches/*.patch`. They were written on a CRLF checkout: convert them
   to LF (or apply with `core.autocrlf=true`), then `git apply` each; all eight apply cleanly on races-port and on
   the Sven + races-port merge. Commit them one per patch so they stay separable.
4. Build note: Sven's new `BuildCoA64.bat` packages DXVK's `x64/d3d9.dll` and the Streamline DLLs listed in
   `Tools/package/files.txt`; DXVK needs meson, ninja and the Vulkan SDK's glslangValidator at configure time. A
   build folder configured before Sven's update has no `dxvk` target until it is configured again.

## Server (stefanw1337/azerothcore-wotlk-coa)
- **Subclassing + Two-Players:** branch `stefanw1337-subclassing-test` (7a3cac3d2b at the time of writing). It
  contains handover-ilusixn-2.0 and the 1.9.5-1.9.9 server work. **Leave out the HotS mount stream**, a separate
  project: take `src/server/coa/AscensionCompat.cpp` as it is at commit cedc342975, and leave out
  `data/sql/updates/pending_db_world/rev_20261111_10_coa_client_vanity_collection_added.sql`. Nothing else in the
  branch belongs to it. `races-extras/subclass` in this repo is the same branch without HotS, as of d69e92c0.
- **Skinshifter:** branch `skinshifter`, one commit (a4e22bce) on CoA main f2a3c0730a: `modules/mod-skinshifter`
  plus its class-33 core changes. Its shipped conf has `Skinshifter.Enable = 1`; the `races-extras/skinshifter`
  copy here has `0`.
- **Switches** (each part can be turned off in its own conf): `Subclass.Enable`, `Subclass.TwoPlayers.Enable`
  (`mod_subclass.conf`), `Skinshifter.Enable`, `Skinshifter.Class.Enable` (`skinshifter.conf`).
- **Bots:** Stefan's builds use Zyth45/mod-playerbots 66221308 (2.3.1). CoA Custom 1.9.9's worldserver used your
  `ilusixn/mod-playerbots` `coa-custom-1.9.1` (CoA Bots 2.2.1 + six commits: ExcludedBotRaces, bot surnames, any
  playable race). Those six commits apply cleanly on 66221308; Stefan's subclass package build applies them.

## Never ship (legal)
- **The male Two-Headed Ogre and the Mutate Me Cho'Gall.** Display 985001 is the Heroes of the Storm Cho'Gall
  body: no CoA Custom MPQ may contain `Creature\TwoHeadedOgre\TwoHeadedOgreMale*` or `Creature\ChoGall\*`. Players
  build both on their own PC from their own Heroes of the Storm install with
  stefanw1337/wow-model-toolkit (branch `chogall`, `scripts/two_headed_ogre/build.py`; the subclass package's
  `files\ogre\build_ogre.py` does it at install time, writing `Data\patch-ZZZZZY-Ogre.MPQ`).
- **Nothing from the HotS mounts project** (its stream code above, its models and tables, ids 986001-986999).
- The 64-bit client exe does not go into this repo.
