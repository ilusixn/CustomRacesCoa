# Installing mod-barde

This guide installs the module on an AzerothCore (3.3.5a) server and its addon on the client. Allow half an hour, plus the build time. *Français : [INSTALL.fr.md](INSTALL.fr.md).*

You work in three places: the **server** (module, database, DBC, configuration), the **client patch** and the **addon**. The order below avoids the usual traps.

## 0. Before you start

- **Back up** your databases (`acore_world`, `acore_characters`) and the server's `Data/dbc` folder. The DBC script keeps its own `*.pre-barde` copy of every file it changes, but a full backup is still wise.
- Install **Python 3** (no library to add).
- Check that the bard's ids are free on your server: spells and items 1950001 to 1950999, item enchantments 1950501 to 1950504, titles 226, 227 and 231 to 235 (the "Reuse" rows of `CharTitles.dbc`), game objects 1950401 to 1950403.

```sql
SELECT entry, name FROM item_template WHERE entry BETWEEN 1950001 AND 1950999 AND name NOT LIKE '%cuivre%';
```

## 1. The module

1. Put the module in the AzerothCore sources' `modules/`, in a folder named **`mod-barde`** (the repository is called `Prof-bard`, but the module must carry this name):

```
git clone https://github.com/romaindnd89-cmd/Prof-bard.git modules/mod-barde
```
2. Build the server with static modules (`-DMODULES=static`), as for any AzerothCore module. On Windows, `scripts/windows/BUILD_BARDE.bat` shows a complete build (Visual Studio, CMake, Ninja, vcpkg): **adapt the paths**, it was written for one particular machine.

## 2. The database

Apply the SQL files in this order (or let AzerothCore's updater do it if `Updates.EnableDatabases` is on):

- **world** database: `data/sql/db-world/*.sql` (the five files: world, camp, instruments, songs, trainer);
- **characters** database: `data/sql/db-characters/mod_barde_characters.sql`.

The files can be replayed safely.

**The trainer.** The profession is learned from the Crafting book through the spell "Apprenti barde" (1950200). `mod_barde_world_trainer.sql` adds it to trainers 200001 and 200002 of the original server: if those ids do not exist on yours, replace them with yours, or teach the profession by hand in game with `.barde apprendre`.

## 3. The configuration

Copy `conf/mod_barde.conf.dist` to `mod_barde.conf` in the server's modules configuration folder. Every setting is commented. The most useful:

| Setting | Purpose |
|---|---|
| `Barde.Enable` | turns the module on |
| `Barde.RequireProfession` | requires the profession to have been learned |
| `Barde.SkillMax` | maximum skill (300) |
| `Barde.Buff.*` | durations, caps and crescendo of the chants |
| `Barde.Camp.*`, `Barde.Concert.*` | camp, purse, tips |
| `Barde.Spec.*`, `Barde.Ovation.*` | specialties, ovations |

## 4. The DBC files and the client patch

The client must know the bard's spells, items, enchantments, animations and titles. The tool adds them to the server's DBC files and builds the client patch:

```
python tools/barde_dbc.py <server Data/dbc folder> <output path>/patch-ZO.MPQ
```

- It copies each DBC it changes to `*.pre-barde` (once) and can be replayed without duplicates.
- If an id is already taken it stops and says so: change it in `tools/barde_dbc.py` (and in the SQL files).
- The patch is named `patch-ZO.MPQ` so that it loads **after** the client's other patches. If you have patches later in alphabetical order, rename it further (`patch-ZP.MPQ`...).

**Restart the server** after changing its DBC files.

## 5. The client

**Close the game first**: it keeps the patch open.

1. Copy `patch-ZO.MPQ` into the client's `Data` folder.
2. Copy `addon/Barde` into the client's `Interface\AddOns`.
3. Start the game. A new addon file or a changed `.toc` requires **fully quitting the game** and starting it again: a `/reload` does not re-read the file list.

The sounds (`addon/Barde/Sounds`) and textures are already provided. To regenerate them: `python tools/gen_instrument_sounds.py addon/Barde/Sounds` and `python tools/gen_textures.py addon/Barde/Textures`.

## 6. Check that everything works

In game, with a GM account:

1. `.barde luth`: a Copper Lute arrives in your bag, with its icon.
2. Learn the profession: from the trainer (Crafting book, "Apprenti barde") or with `.barde apprendre`, then click "Barde" in the spellbook (or type `/barde`).
3. Pick a song, "Play": the notes fall, play with D F J K, the chant arrives at the end of the song.
4. `.barde competence 300` then `.barde instruments 4 3` give maximum skill and epic arcanite instruments to try the workshop, the specialties and the legendary quest.

## GM commands

| Command | Access | Effect |
|---|---|---|
| `.barde airs` | player | lists the songs |
| `.barde jouer <n> [entrainement]` | player | starts a song |
| `.barde luth` | GM | gives the Copper Lute |
| `.barde instruments [tier] [quality]` | GM | gives the instruments (tier 0 to 4, quality 0 to 4) |
| `.barde competence <value>` | GM | sets the bard skill |
| `.barde apprendre` | GM | teaches the bard profession to the target (or yourself) |
| `.barde duo <song> [accuracy %]` | GM | test duo with a virtual partner |
| `.barde legende tout` / `reset` | GM | validates or clears the legendary quest steps (testing) |
| `.barde aura <1-7>`, `.barde pose <1-3>` | GM | tests the aura and the dances |
| `.barde recharger` | admin | reloads songs and instruments from the database |

## Troubleshooting

- **An "Interface\AddOns\Barde\..." error window**: fully quit the game and start it again (new addon file), then send the error text.
- **Missing items or icons ("?")**: the client patch is not loaded. Check it is in `Data`, that its name sorts after the other patches, and that the game was restarted.
- **The server refuses to start on an id already taken**: change the id in `tools/barde_dbc.py` and in the SQL, and replay.
- **An item you just received does not show in your bag**: go back to the character screen and log in again.
- **"patch-ZO.MPQ" cannot be copied**: the game is open.
- **A MySQL version error at build time**: see `scripts/windows/mysql-version.patch`, a workaround specific to one server.
