# subclass

Stefan's (stefanw1337) server work for the subclass races: mod-subclass (subclassing, The Twins 96, Two-Headed Ogre 85,
Two-Players), mod-livemon, and the core changes they need. Modules: snapshot of `stefanw1337/azerothcore-wotlk-coa`, branch
`stefanw1337-subclassing-test` at `47538fce5a` (2026-11-23). `server/` is still the `d69e92c0` (2026-11-11) core
snapshot; the core changes since then are only in that branch, which Corfirean's public server build pins
(`races-source.json`). Building the worldserver from that branch is the simplest route.

- `modules/mod-subclass/`, `modules/mod-livemon/`: copy into the server's `modules/`.
- `server/subclass-core.patch`: our core changes against CoA base `ee7e5d26`, without the module folders. Files that
  are unchanged from `../server/files` are left out.
- `server/files/`: the same changed files in full (they already include the race changes where both touch a file).

New since the af6f850d02 snapshot (2026-11-23):
- The Twins' racial One Lazy Thin (spell 9530135): each use turns the brother into a Kul Tiran, then the player, then
  both thin again. It works for one-player Twins and Two-Players Twins, and the choice is kept across logins (new
  characters table `mod_subclass_lazy_thin`). With a Kul Tiran, the brother walks further out, and piggyback and saddle
  use their own seats (vehicles 9720-9769, Two-Players copies 9820-9869).
- The character screen shows the Kul Tiran look on the player and on the brother, in their own gear (server payload
  `TWNS` key `<name>#kultiran`, client commits on this branch). An unpaired Two-Players Twin stands alone there.
- Core fix (`server/client-stream-new-spells.patch`, against the `stefanw1337-subclassing-test` core): a spell the
  server streams to the client that is not in the client's Spell.dbc now shows in the spellbook. Before, it arrived
  after the login spell list and the client dropped it. Not applied to `server/files`, whose older core has no
  new-spell stream; build the worldserver from the branch.
- `.subclass twin info` also reports the Two-Players carry state and writes it to the server log.
- New SQL: world `rev_20261123_02` to `rev_20261123_05`, characters `rev_20261123_00`.
- Client: the spellbook ignores a second learn of a spell it already lists. Two-Players lending could send one and
  show a spell twice (Boon of the Bear and Boon of the Turtle on a Wildwalker subclass).
- Known: the Kul Tiran male's hanging set pieces (a knife and a medallion on one leather set) float on any Kul Tiran
  wearing that set; it is the set together with the Kul Tiran model, not the Twins.

New since the fb05f2e477 snapshot (2026-11-23):
- Bonk Heads! speaks of brothers for the Two-Players Twins (messages and tooltip).
- Core fix (`server/native-model-gender.patch`, also applied to `server/files/.../Unit.cpp`): a player who leaves a
  shapeshift form (Ghost Wolf and others) keeps their own gender. Before, a custom race character took the form
  model's "no gender" and turned green and bald until the next login.
- The Twins' brother rides on the back of every ridden form look: Ghost Wolf, Wargstride, Travel Form (the tiger) and
  the collection looks (Draenor wolf, Zandalari raptor, primal dog, Kul Tiran stag, travel cat, doe, marmoset). Each
  look has its own seat (vehicles 9710-9719, Two-Players copies 9810-9819, table `mod_subclass_form_seat`).
- `.subclass twin seat <seat> <x> <y> <z> [attachment]` moves a twin seat live and stores it (GM).
- Two-Players Twins: the pair shares one body like the ogre (piggyback seat 9630, carry vehicles 9630-9652), the head
  chosen at creation leads, whispers reach both heads, the character screen shows the partner's gear on the brother,
  the seated Twin dresses his own stand-in, and same-class pairs share class resources (Insanity and the like).
- The subclass races start in their own start areas.
- New world SQL under `server/files/data/sql/updates/pending_db_world/` (`rev_20261116_00` to `rev_20261123_01`).
- Server owners who replay the module's old SQL files on every update must replay the current ones from
  `modules/mod-subclass/data/sql/`: older copies of `2026_10_18_02_mod_subclass_twin_seats.sql` and the beast and car
  seat files put old seat values back and delete vehicles 9543-9599.

New since the d69e92c0 snapshot (2026-11-16):
- Server tables: the Two-Players displays (985001-985004, 999402) and ChrRaces 85/96 are not in 2.0.1's `files/dbc`,
  so the Two-Headed Ogre body cannot show. Stefan sends 2.0.1's tables with only these rows added.
- Client tables: 18 tables (ChrRaces, CharSections, CreatureDisplayInfo, CreatureModelData, Spell, SkillLineAbility,
  Vehicle, VehicleSeat, ...) with our rows carried onto the newest copies in the 2.0.1 client (`tools/carrydbc.py`,
  no clashes). Stefan sends them as a zip; they belong in the release's own table patch so only one copy ships.
- Client program: the 1.9.9 pack races (Esteria ids 80-91) and the Qiraji read their skins from
  `EsteriaPackSkins.bin` (they were white). Commit on this branch.
- Addons for the client zips (the launcher only installs addons from client zips): `modules/mod-subclass/addon/`
  `Ascension_Subclass` and `CoAC_BotPlan`.
- Courier bag (sell by item, by quality or at once), bots open quest objects and feed quest NPCs, a bot body mounts
  up for long trips, Agree/Disagree, Bonk Heads!, LiveMon fight log. `Subclass.TwoPlayers.Bots` needs SquidBots on.
- Not for release: the male Two-Headed Ogre and Cho'Gall models stay local (built on the player's PC); the four
  Two-Players displays must point at the female Two-Headed Ogre model in release tables.

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
