# subclass

Stefan's (stefanw1337) server work for the subclass races: mod-subclass (subclassing, The Twins 96, Two-Headed Ogre 85),
mod-livemon, and the core changes they need. Snapshot of `stefanw1337/azerothcore-wotlk-coa`, branch
`ilusixn-195-server` at `c1d22382` (the same commit as `handover-ilusixn-2.0`), which already carries the 1.9.5-1.9.7
server files from `../server`. Building the worldserver from that branch is the simplest route.

- `modules/mod-subclass/`, `modules/mod-livemon/`: copy into the server's `modules/`.
- `server/subclass-core.patch`: our core changes against CoA base `ee7e5d26`, without the module folders. Files that
  are unchanged from `../server/files` are left out.
- `server/files/`: the same changed files in full (they already include the race changes where both touch a file).

New since the 1.9.6 server:
- Witch Hunter Vault is a handspring for the original races, a butt bounce for the Two-Headed Ogre, and a leapfrog
  over the brother for The Twins (client: mod-subclass `tools/mkvault.py`, CoA `tools/patch_handspring_client.py`).
- Thin Twins: hair under hats (`tools/mkhatcuts.py`), brother forms follow spell rules, mana bar flicker, accursed
  brother; Ogre fist-before-face cast; subclass line only on the character tab; single/dual wield from any source.
- `tools/mkmergepatch.py` merges our client patches into one `patch-ZZZZY.MPQ`.

Not included: client MPQs and DBCs (built from the tools; see `modules/mod-subclass/HANDOVER.md`).
