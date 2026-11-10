# Handover: subclassing, The Twins and the Two-Headed Ogre for CoA Custom 2.0

From Stefan (stefanw1337), 2026-11-05. This is written for the person or Claude building the next CoA Custom release.
It lists what changed since 1.9.8, what the release must carry, and how to check it before publishing. Follow the
steps in order and run every check; a release that fails a check will break the Twins or the Vault for players.

Nothing here touches Stefan's Skinshifter class. It is a separate, unreleased project: do not take
`patch-ZZZZZZ-SKIN.MPQ`, `mod-skinshifter`, `skinshifter-core.patch` or anything named Skinshifter into a release.

## 1. What 1.9.8 is missing

1.9.8 was built from `handover-ilusixn-2.0` (Nov 2). Stefan's Nov 3–5 work came after it:

| Area | Change | Where it is |
|---|---|---|
| Server | The Thin Twins brother is the player's mirror image (display 977602, model 999402) | `stefanw1337-subclassing-test` |
| Server | The brother can have his own face, hair style and beard (Wardrobe → "Hair & face") | same branch, new characters SQL |
| Server | Hair and beard choices for races above 32 are read from CharSections directly | same branch |
| Server | 1.9.5's server changes from `handover-ilusixn-2.0` are merged into the branch | same branch |
| Client | Vault for the 10 original races, standard and HD models | `races-extras/assets/mpq/patch-ZZZZZY.MPQ`, `patch-ZZZZZ-HD.MPQ` |
| Client | The brother's mirrored model and its two table rows, built from 1.9.8's `custom\thintwins` model | `patch-ZZZZZY.MPQ` |
| Client | `EsteriaAppearanceGeometry.bin` with the Thin Twins and brother entries | `races-extras/assets/client_root/` |
| Client | Character select: the brother is mirrored, stands at the player's right and shows his own look | `races-port` glue (commits 212d7915, 8d547d79, 39be9b61) |
| Addon | Ascension_Subclass: "Hair & face" button in the Wardrobe's twin view | `modules/mod-subclass/addon/Ascension_Subclass` |

Two problems in the 1.9.8 package itself:

- Its `EsteriaAppearanceGeometry.bin` is from Oct 18: 26 entries, no `custom\thintwins\native\male\thintwinsmale.m2`,
  no brother, and an older `thinhumanmale.m2` entry. Ship the one in `races-extras/assets/client_root/` instead
  (1.9.8's 26 entries plus those two; nothing of 1.9.8's is dropped).
- Its server tables (`files/dbc/CreatureDisplayInfo.dbc`, `files/dbc/CreatureModelData.dbc` and
  `files/dbc_clientset/CreatureDisplayInfo.dbc`) have no rows 977602 / 999402. See step 3.

## 2. Server source

1. Merge `stefanw1337/azerothcore-wotlk-coa` branch `stefanw1337-subclassing-test` (at or after `1bb96194`). It already
   contains `handover-ilusixn-2.0`, so it merges on top of 1.9.8's server without redoing that.
2. It is **not** merged with Conquest-of-AzerothCore `main` yet (about 355 commits behind, 19 conflicts in core files).
   Do not mix that merge into the release; it is a separate job.
3. Build the worldserver as usual. The module is `modules/mod-subclass`.

## 3. Server data

1. Characters SQL: copy every file in `modules/mod-subclass/data/sql/db-characters/` into the package's
   `subclass/files/sql/characters/` (and `db-characters/`). The new one is
   `2026_11_03_00_mod_subclass_twin_features.sql`. `apply_subclass.py` runs every file in that folder once.
   **Without this table the worldserver aborts when a player opens the character list** ("Table
   'acore_characters.mod_subclass_twin_features' doesn't exist").
2. Server tables: add the brother's rows to the package's server tables with Stefan's tool (it keeps a
   `.before-mirror` copy and can be run again safely):

   ```
   python modules/mod-subclass/tools/mkmirror.py --server <package>/files/dbc <package>/files/dbc_clientset
   ```

   `dbc_clientset/CreatureDisplayInfo.dbc` must be the client's table with these rows, or the display stream resends
   977602 without textures.
3. Config: `mod_subclass.conf.dist` has `Subclass.ThinTwins.MirrorDisplay = 977602` (0 = the brother uses the player's
   own model). Copy the new `.dist` from `modules/mod-subclass/conf/`.

## 4. Client

Add these to the release next to 1.9.8's files:

| File | Goes to | Notes |
|---|---|---|
| `races-extras/assets/mpq/patch-ZZZZZY.MPQ` | `<game>\Data\` | **Replaces `patch-ZZZZY.MPQ` and `patch-ZZZZV.MPQ`; remove both from the release and the installer** |
| `races-extras/assets/mpq/patch-ZZZZZ-HD.MPQ` | `<game>\Data\` | Opened only when HD is on (the `-HD` suffix rule) |
| `races-extras/assets/client_root/EsteriaAppearanceGeometry.bin` | `<game>\` | Replaces 1.9.8's |
| `modules/mod-subclass/addon/Ascension_Subclass/*` | `<game>\Interface\AddOns\Ascension_Subclass\` | From the server branch |
| `CoA64.exe` built from `races-port` | `<game>\` | Needs the three glue commits above |

`patch-ZZZZZY.MPQ` holds: the 20 standard Vault models of the original races, `AnimationData.dbc` (rows 1792–1795),
`SpellVisualKit.dbc` (rows 3455900, 3455901), `CreatureDisplayInfo.dbc` (row 977602), `CreatureModelData.dbc` (row
999402), `custom\thintwins\native\male\thintwinsmirrormale.m2` with its skin, and the subclass's client tables:

- `Spell.dbc`: 1.9.8's patch-ZZZZS table plus **24 103 subclass client spells it lacks** (the other head's casts and
  glows, among them) and our field changes on 308 spells. Without them a borrowed spell such as Create Soulstone
  plays on the main head's arm, and cast glows stick in her hand.
- `SkillLineAbility.dbc` (13 931 rows) and `SkillRaceClassInfo.dbc` (135 rows): our class masks, so a subclass can
  learn its spells.
- `SpellVisualKitModelAttach.dbc`: the Ogre's quest mark at Y −0.218 (the visual's Y offset is mirrored from the
  model's axes), so the "!" floats over her second head.

These were in Stefan's own client before and are missing from 1.9.8, so they affect every 1.9.8 player. They are
client files only: they can go into a release without rebuilding the worldserver.

### Archive order matters

The client uses the copy of a file from the archive that sorts **last**. `patch-ZZZZZY` sorts after 1.9.9's
`patch-ZZZZZ`, `patch-ZZZZS` and `patch-ZZZZX`, and its tables are **whole copies** of 1.9.9's tables plus our rows.
The one in `races-extras` was built on a 1.9.9 client on Nov 5 and keeps every 1.9.9 row. So:

- If you change Spell, AnimationData, SpellVisualKit, CreatureDisplayInfo or CreatureModelData in any archive for 2.0, or
  move the Thin Twins model again, rebuild `patch-ZZZZZY` (below). Otherwise it hides your change.
- Never give the release an archive named after `patch-ZZZZZY` that carries those four tables without our rows, or the
  Vault and the brother disappear. That is exactly how the Vault broke locally on Nov 5.

### Rebuilding patch-ZZZZZY on a release client

Put Stefan's `modules/mod-subclass/tools/` files (plus `tools/build_vault_patches.py` and
`tools/patch_handspring_client.py` from the server branch) in a folder next to the client's `Data`, close the game,
and run in this order:

```
Rebuild-Our-Client-Patches.bat
```

It runs `Build-Vault-Patches.bat --fresh`, `carrytables.py` (our rows from `subclass-tables.MPQ`, which Stefan can send,
onto the client's newest tables) and `Build-Mirror-Twin.bat`.

`--fresh` starts `patch-ZZZZZY` over. Both read their tables only from archives that load before `patch-ZZZZZY`, and the
mirror tool reads the Twins model path from `CreatureModelData` row 901401, so it follows the model wherever it lives.
Then copy the new `Data\patch-ZZZZZY.MPQ`, `Data\patch-ZZZZZ-HD.MPQ` and `EsteriaAppearanceGeometry.bin` into the
release.

## 5. Checks before publishing

Run these with Python 3 from a clone of `stefanw1337/azerothcore-wotlk-coa` (they only read files). Set `GAME` to the
client folder you are about to package and `SRV` to the server package folder.

```python
import struct, sys
sys.path[:0] = ["modules/mod-subclass/tools", "tools"]
from mpqpull import Archive
import patch_handspring_client as h

GAME, SRV = r"<client folder>", r"<server package folder>"

def ids(table):
    count, _, size, _ = struct.unpack_from("<4I", table, 4)
    return {struct.unpack_from("<I", table, 20 + i * size)[0] for i in range(count)}

y = Archive(GAME + r"\Data\patch-ZZZZZY.MPQ")
need = {"AnimationData": set(h.ANIMATION_NAMES), "SpellVisualKit": set(h.VAULT_KITS),
        "CreatureDisplayInfo": {977602}, "CreatureModelData": {999402}}
for name, rows in need.items():
    table = y.extract(f"DBFilesClient\\{name}.dbc")
    print(name, "OK" if table and rows <= ids(table) else "MISSING")
print("mirror model", "OK" if y.extract(r"custom\thintwins\native\male\thintwinsmirrormale.m2") else "MISSING")

geometry = open(GAME + r"\EsteriaAppearanceGeometry.bin", "rb").read()
for path in (b"custom\\thintwins\\native\\male\\thintwinsmale.m2", b"custom\\thintwins\\native\\male\\thintwinsmirrormale.m2"):
    print(path.decode(), "OK" if path in geometry else "MISSING")

for table, row in (("dbc\\CreatureDisplayInfo.dbc", 977602), ("dbc\\CreatureModelData.dbc", 999402),
                   ("dbc_clientset\\CreatureDisplayInfo.dbc", 977602)):
    print(table, "OK" if row in ids(open(SRV + "\\files\\" + table, "rb").read()) else "MISSING")
```

Every line must say OK. Also check by hand:

- `Data\patch-ZZZZV.MPQ` and `Data\patch-ZZZZY.MPQ` are gone from the client and the installer (1.9.9 ships a
  stale one-table `patch-ZZZZY.MPQ`; `patch-ZZZZZY.MPQ` replaces it).
- No archive sorting after `patch-ZZZZZY` in the release carries `AnimationData.dbc` or `SpellVisualKit.dbc`. If one
  must, rebuild it on a client that already has `patch-ZZZZZY`, so it starts from our rows.
- The characters SQL folder in the package contains `2026_11_03_00_mod_subclass_twin_features.sql`.

## 6. In-game test

On a test realm with the new server and client:

1. Create a male Thin Twins character (race 96). At creation and at character select the brother is a mirror image,
   standing at the player's right (the viewer's left), and the server does not stop at the character list.
2. As a Two-Headed Ogre with a Warlock subclass, Create Soulstone animates the second head's arm, and the quest "!"
   floats over the second head.
3. In game, Wardrobe → "Dressing: Your twin" → "Hair & face": Hair style and Facial hair list choices; picking one
   respawns the brother with it, and character select shows it.
4. Witch Hunter Vault works for a Two-Headed Ogre, a Night Elf male and one other original race (HD on and off).
5. From behind, the brother's sheathed and held shield faces outward and leans the opposite way from the player's.
   The shield's picture is not mirrored; that is expected (the game can only turn an item, not flip it).

## 7. Since 1.9.9 (for 2.0)

- Client (`races-port`, `WoW/Source/UIUtil/PortraitButton.cpp`): the Two-Headed Ogre's portrait showed her whole body
  small, because the portrait camera's position came out wrong after the model was animated (the target was right).
  The portrait now takes the position and target from the model's own camera data, which is what working models such
  as the Thin Twins already got. The imp's portrait had the same wrong position.
- Addon (`stefanw1337-subclassing-test`): the character window no longer pushes the Ogre out of view, she stands
  centred, and her background is the Orc one (the character window gives races without a background Human's; both
  Ogre races are "Ogre" in ChrRaces). `/subclass sheet <zoom> <right> <up> <turn>` adjusts her by hand.
- Addon: a pet summoned through the subclass gets its own movable pet bar, like the subclass stance bar.
- Server (`stefanw1337-subclassing-test`): dropping a Warlock or Paladin subclass unlearned every mount (now only the
  class's own mounts go); its summoned pet stayed out (now dismissed whenever the subclass is cleared or changed).
- `races-extras/subclass/` is refreshed to the server branch as of 2026-11-07 (`8ff12d95`): module, SQL, addon, conf
  template and core changes. Its README lists what changed since 1.9.7.

## 8. Known issues (not fixed)

- The creator does not yet have "You / Brother / Both" tabs; the brother's look is chosen in the Wardrobe.
- Female Two-Headed Ogre: a helmet shows on one head only (1.9.9's list).
