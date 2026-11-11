# subclass

Stefan's (stefanw1337) server work for the subclass races: mod-subclass (subclassing, The Twins 96, Two-Headed Ogre 85,
Two-Players), mod-livemon, and the core changes they need. Snapshot of `stefanw1337/azerothcore-wotlk-coa`, branch
`stefanw1337-subclassing-test` at `d69e92c0` (2026-11-11), without the HotS mount stream commits (a separate project). It carries the 1.9.5-1.9.7 server files from `../server`
and everything since; building the worldserver from that branch is the simplest route.

- `modules/mod-subclass/`, `modules/mod-livemon/`: copy into the server's `modules/`.
- `server/subclass-core.patch`: our core changes against CoA base `ee7e5d26`, without the module folders. Files that
  are unchanged from `../server/files` are left out.
- `server/files/`: the same changed files in full (they already include the race changes where both touch a file).

New since the 8ff12d95 snapshot (2026-11-11), not tested properly yet:
- Two-Players: two players share one body. The male Two-Headed Ogre (race 85) is the body of the Melee Head with the
  Ranged Head riding as a hidden passenger; The Twins (race 96) pair two players as the twins. Pair with `.pair invite <name>`;
  each head has its own spells, the Melee Head lends abilities that land on the Ranged Head's own action bars and cost
  only when cast, buffs are shared, the Ranged Head is hidden whenever it is not seated, and Dungeon Finder takes the
  rider along with its body. Switch it off with `Subclass.TwoPlayers.Enable = 0`: no new Two-Players characters, no
  pairing, existing pairs play as single characters.
- A bot can be the other head (SquidBotsLite): the real player leads the party, the bot body keeps grinding, questing,
  looting and resting unless told to stay, and its bot menu is on the player frame (addon `SubclassPairs.lua`).
- `addon/CoAC_BotPlan`: `/botplan` shows what the group's bots plan to do, with "Do now". The quest route supplement
  (`src/SubclassBotQuestRoute.cpp`) steers bots between quest givers; it only adds to SquidBotsLite and is untested.
- New world SQL under `server/files/data/sql/updates/pending_db_world/` (Two-Players racials, shared blood, bonk heads).
- The client side of Two-Players (carrying a mount, the joined name, item art) is in the CoA64 source of this branch.

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
- `modules/mod-subclass/conf/mod_subclass.conf.dist` has new settings (`Subclass.TwoPlayers.Enable` among them): copy the new ones into the server's
  `mod_subclass.conf`, or the server warns and uses the defaults.

Earlier (1.9.7):
- Witch Hunter Vault is a handspring for the original races, a butt bounce for the Two-Headed Ogre, and a leapfrog
  over the brother for The Twins (client: mod-subclass `tools/mkvault.py`, CoA `tools/patch_handspring_client.py`).
- Thin Twins: hair under hats (`tools/mkhatcuts.py`), brother forms follow spell rules, mana bar flicker, accursed
  brother; Ogre fist-before-face cast; subclass line only on the character tab; single/dual wield from any source.

Not included: client MPQs and DBCs (built from the tools; see `modules/mod-subclass/HANDOVER.md`).
