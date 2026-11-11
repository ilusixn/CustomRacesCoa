# skinshifter

Stefan's (stefanw1337) **Skinshifter**: a new CoA class (class 33), a thief who steals creatures' abilities and
looks and shifts between them, with the **Possessor** as its second identity. It is an unfinished prototype,
published to play with. It is its own module, separate from mod-subclass and the races work, and it lives only on
this branch.

**It is off by default.** `mod-skinshifter/conf/skinshifter.conf.dist` ships with `Skinshifter.Enable = 0`. Then
the server refuses the `.skin` commands, removes the class 33 rows and the Skinshifter spells at start-up, and
nothing of it reaches players. Set it to 1 in the server's `skinshifter.conf` to try it. Parts can be switched off
on their own: `Skinshifter.Class.Enable`, `Skinshifter.Advancement.Enable` (Character Advancement trees) and
`Skinshifter.Takeover.Enable` (going into a creature and playing it).

## Contents
- `mod-skinshifter/`: the server module (`src/`, `conf/`). Copy it into the server's `modules/`.
- `skinshifter-core.patch`: class 33 in the server core (`CLASS_SKINSHIFTER`, `MAX_CLASSES` 34, class masks,
  items, quests, skills). Apply it to the server source with `git apply` before building. Without the patch the
  module does not build.
- `addon/SkinCollection/`: the client addon (Skin Collection, tokens, slots, spellbook search, tooltips).
- `tools/`: the client patch builder. `mkskinclient.py` builds `patch-ZZZZZZ-SKIN.MPQ` (class 33 in the class list,
  its class table, colours, icons and spells) from the client's own newest files; `Build-Skinshifter-Client.bat`
  runs it. Edit `<repack folder>` and `<game folder>` in the bat and at the top of `mkskinclient.py` first.
  `icons/` and `art/` are the class's own icons and their source pictures; `creature-map/` builds the creature
  table the design was made from.
- `docs/`: the design (`Skinshifter-Design.md`, `Skinshifter-Kit-and-Talents.md`, `Skinshifter-Progression.md`,
  `Ideas-Later.md`) and the research notes on the class limit and class 33.

## Try it
1. Server: apply `skinshifter-core.patch`, copy `mod-skinshifter` into `modules/`, build. Copy
   `skinshifter.conf.dist` to `skinshifter.conf` and set `Skinshifter.Enable = 1`.
2. Client: run `tools/Build-Skinshifter-Client.bat` with the game closed and copy `addon/SkinCollection` into
   `Interface/AddOns`. Remove `Data/patch-ZZZZZZ-SKIN.MPQ` to take it out again.
3. In game: `.skin steal` (also `.skin lift`) on a target, `.skin list`, `.skin wear <number>`, `.skin shed`,
   `.skin grimoire`, `.skin forget <spellId>` or `.skin forget all`.

## Status
Prototype. Stolen abilities keep the creature versions' fixed damage, the specializations (Beastskin, Courtier,
Felstriker, Elementalist) and their talents are being built, and balance is untested. Expect rough edges.
