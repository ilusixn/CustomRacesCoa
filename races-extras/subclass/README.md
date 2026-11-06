# subclass

Stefan's (stefanw1337) server work for the subclass races: mod-subclass (subclassing, The Twins 96, Two-Headed Ogre 85),
mod-livemon, and the core changes they need. Snapshot of `stefanw1337/azerothcore-wotlk-coa`, branch
`stefanw1337-subclassing-test` at `8ff12d95` (2026-11-07). It carries the 1.9.5-1.9.7 server files from `../server`
and everything since; building the worldserver from that branch is the simplest route.

- `modules/mod-subclass/`, `modules/mod-livemon/`: copy into the server's `modules/`.
- `server/subclass-core.patch`: our core changes against CoA base `ee7e5d26`, without the module folders. Files that
  are unchanged from `../server/files` are left out.
- `server/files/`: the same changed files in full (they already include the race changes where both touch a file).

New since the 1.9.7 snapshot (for 2.0):
- `.subclass clear` and changing a subclass: only that class's own mounts go (not every mount), its summoned pet is
  dismissed, and its spellbook copies (Demon Armor, a Marksmanship tab, ...) are removed, also at login.
- A borrowed mount pressed again dismounts; a subclass pet gets its own movable pet bar.
- The Thin Twins brother is the player's mirror image (`Subclass.ThinTwins.MirrorDisplay`, client `tools/mkmirror.py`)
  and can have his own face, hair and beard (new characters SQL `2026_11_03_00_mod_subclass_twin_features.sql`).
- Addon: the Ogre stands centred in the character window with the Orc background, Starcaller's Lunar Phase uses CoA's
  moon, and the subclass trees use CoA's own grid spacing.
- Client patch builders merge into `patch-ZZZZZY.MPQ`, which loads after the release's `patch-ZZZZZ.MPQ` and builds on
  its tables (see `../../HANDOVER.md`).
- `modules/mod-subclass/conf/mod_subclass.conf.dist` has five new settings: copy the new ones into the server's
  `mod_subclass.conf`, or the server warns and uses the defaults.

Earlier (1.9.7):
- Witch Hunter Vault is a handspring for the original races, a butt bounce for the Two-Headed Ogre, and a leapfrog
  over the brother for The Twins (client: mod-subclass `tools/mkvault.py`, CoA `tools/patch_handspring_client.py`).
- Thin Twins: hair under hats (`tools/mkhatcuts.py`), brother forms follow spell rules, mana bar flicker, accursed
  brother; Ogre fist-before-face cast; subclass line only on the character tab; single/dual wield from any source.

Not included: client MPQs and DBCs (built from the tools; see `modules/mod-subclass/HANDOVER.md`).
