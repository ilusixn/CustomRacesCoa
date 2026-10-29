ASSETS  -  everything to rebuild these races inside YOUR OWN race setup (e.g. the big CoA race list)
==================================================================================================
assets\<Race>\...        every file the client patch uses, in game paths (Creature\..., Character\...), so you can drop them into your own MPQ.
                         Every .blp also has a .png copy for editing / previewing.
  Banshee\Creature\Banshee\BansheePlayer.m2 (+00-03.skin)          body: skin=type1, face=type8, hair=type6 (CharSections)
  FacelessOne\Creature\FacelessOne\FacelessPlayer.m2 (+skins)      body: skin=type1, glow=type8
  Qiraji\Creature\QuirajBattleguard\QirajiMale.m2 / QirajiFemale.m2 Swarmguard body with its own wings, skin=type1
  EluneChosen\Character\...                                        Nightborne character textures (ghost skins/faces/hair)
  Qiraji\...\QirajiSwarm_*.blp     NEW natural in-world skins (both sexes)
  Qiraji\alt_bright_skins\         OLD first-pass bright skins: QirajiFemale_* (Swarmguard body, M* = blue male set),
                                   QirajiGladiator_* (stock Creature\QuirajGladiator model), QirajiMaleBody/Shell_* (stock Twin Emperor model)
  <Model>_ANIMATIONS.txt           every animation in the body: id, name, length, original or borrowed-from
  <Race>\Voice\Male|Female\Emotes     emote voice lines (/hello /cheer /attacktarget ...) used by the race
  <Race>\Voice\Male|Female\UI_Errors  "not enough mana" / "can't do that yet" style lines
  <Race>\Voice\Male|Female\Combat     the body's attack / wound / death / aggro sounds (creature bodies)
data\*.csv                 the exact DBC rows used: ChrRaces, CharSections, CharHairGeosets, CreatureDisplayInfo, CreatureModelData, Spell
                           (raw field values + resolved strings) -> copy into your own DBCs with your own race ids.
tools\playerize_any_model.py   turn any creature model into a player body (animations + weapon points). Needs tools\anim_alias.py.

Our ids: races 84-87, displays 977610/11 977620/21 977630/31, spells 13990131/41/51. If those collide with your races, renumber
when you copy the rows (or ask for a rebuild with your ids).
Factions as shipped: Banshee Horde, Elune's Chosen Alliance, Faceless One Horde, Qiraji Horde (any can be swapped).
