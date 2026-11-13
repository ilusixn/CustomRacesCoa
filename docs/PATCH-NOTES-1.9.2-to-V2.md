# CoA Custom: patch notes from 1.9.2 to V2

Every change since 1.9.2, oldest first. Back up your characters before updating the server.

## 1.9.3
- New client with the **Retail race adapter**: Worgen and Dracthyr get their retail looks and both forms (saved look at character select).
- New world server.
- Install: all zips into one folder, run `Install-Custom.bat`, start the servers.

## 1.9.4
- Complete package in one release (Server + Client-1 to Client-9).
- All the Retail data Worgen and Dracthyr need is included, so nothing is downloaded from Blizzard at play time.

## 1.9.5
- Slime Giant chest-only armor.
- Gnoll Scavenger (active) and Skinner.
- Kyrian / Venthyr hover fix.
- Racial animations, saved form look at character select.

## 1.9.6
- **Stefan's subclassing**: The Twins (race 96, Alliance) and the Two-Headed Ogre (race 85, Horde) in their own Subclass Races group; the second twin / head learns a second class tree. The Twins show a copy of your character behind you in the creator.
- Ogre face / chest skin fix, spellbook tabs fix, Hide Races saved to file, Both button, Sylvari robe.
- One extra step: run `Apply-Subclass.bat` once with the servers stopped.

## 1.9.7
- **Body slider presets and sharing** (Kalibros): PRESETS tab, share codes, saved looks per race and sex; sliders survive teleports and `.char custom`.
- Mag'har Orc uses the retail model with its Retail data cached.
- Sethrak head and hood follow the skin colour; Sylvari back to the older model; Kyrian shoulders (Shotboom's fit), Faceless shoulders, Venthyr icons.
- Retail gear fixes: knee / hip / buckle seating, floating foot pieces removed (Void Elf, Nightborne, Dracthyr, The Twins).
- Aspollon's client fixes merged (startup archive crash, inventory types, Two Forms sync).

## 1.9.8
- Shapeshift fix: coming back from druid form no longer breaks the face / texture of the custom races.
- Spell animations: projectiles and impacts of custom server spells show again.
- Stefan's subclass update: Thin Twins keep their hairstyle under hats, own Vault for the Twins and the Ogre, raised-fist casts for the Ogre.

## 1.9.9
- **14 new races (AllRaces v12)** with their own racials: Elune's Chosen, Qiraji, Sureki, Dreadlord, Titan, Wretched, Fungarian, Sporeling, Stormforged, Mo'arg, Nerubian, Lich, Mogu, Lost One. Needs the new `worldserver.exe`.
- Racials: Titan Aegis (shield = max health), Elune's Moonlit Veil (movable stealth), Fungarian Sporebloom Circle (8% heal every 2 s), Qiraji hover. List in `RACIALS.md`.
- Seating fixed for Titan F, Sporeling, Fungarian; jump, sheathe and emote animations for the new races.
- Stefan's subclass merge (Two-Headed Ogre, Thin Twins).

## V1.1 (races pack)
- **Gilgoblin** (the Unshackled, race 105) with racials, voices and creature looks.
- One zip per race (models, skins, DBC rows, SQL, racial code).

## V2
**Client**
- One client executable: `CoACustomRace.exe`, with your logo as icon. Start it from the game folder or with `Start-CoACustomRace.bat`.
- Vulkan renderer with **Lumenveil** lighting and volumetric fog, HD water and materials; presets in the `presets` folder (cvar `visuals`: 0 stock, 1 Lumenveil, 2 Azeroth Remastered).
- A model with a broken animation link can no longer freeze the client when a bot spawns; a model with a corrupt event list is skipped and logged in `Logs\AnimBad.log` instead of crashing.

**World**
- **Fading Island restored**: 2 boats (Booty Bay and Ratchet to Hardluck), 4 elevators, goblin passenger NPCs, spirit healer, teleports (`.tele FadingIsland`), cleaned map tiles.
- **Retail mounts and pets**: about 1700 mounts and 2600 battle pets as companions.
- **Dev menu** (`/dev`): in-game editor for NPCs, items, quests, shops, paths and objects; custom item creator and heirlooms. `CoA_Teleports` addon.

**Classes**
- Every incarnation slot is open to every class.
- Venomancer forms use the druid looks (Spider = Cat, Beetle = Bear, Venomwing = Travel, Sea Serpent = Aquatic, Vizier = Moonkin); Weaver Form can be Moonkin or Tree of Life.
- Bloodmage, Reaper, Starcaller, Felsworn (Inner Demon variants, Hellfire), Pyromancer Draconic Form and Necromancer Lich Form take their incarnation; Cultist Madness forms use Metamorphosis.
- Primalist, Tinker, Witch Hunter, Barbarian ancestors and the Stormbringer elemental wear any pet look (pet, demon, undead, dragonkin, elemental). Necromancer guardians are split over the pet categories. Per class: `INCARNATIONS.md`.
- Vanilla classes have CoA-average base health from level 2 to 60.

**Bots**
- Test Race bots are deleted at the next start and no longer created. Drakonid and Grummle bots are always male (no female body); existing female ones are switched to male.

**Fixes**
- Sylvari, Centaur and Faceless One player models: broken animation data repaired (exploding mesh, event crash).
- Saberon and Drakonid shown white or green; Drakonid is male only.
- Tuskarr invisible ankle (the model and skin files did not match).

**Known issues**
- Female Mechagnome bots can look broken. Grummle has no armour geosets. Chronomancer, Runemaster and Necromancer Rime have no incarnation yet.
- Lumenveil and the HD passes are new: if the world looks wrong, set `visuals` to 0.
