# Ascension client MPQ file list

Research note, 2026-10-25. Built from the archives of an installed Conquest of Azeroth client by reading each archive's internal `(listfile)` and resolving every listed name against the archive's hash table.

## What this is and its limits

- MPQ archives do not index their contents by name. They carry a hash table, and an optional `(listfile)` entry that names the files. This document is the union of those `(listfile)` entries across every archive in the client's `Data` folder.
- A file whose name is missing from `(listfile)` is **not** in this list even though it is in the archive. Some Ascension patches have incomplete listfiles (for example, custom map terrain is findable by name but not always listed). To check a specific path, probe the name hash directly; to find unlisted content, scan the block table by content.
- Archives with no readable `(listfile)`: none. Archives whose listfile is empty or tiny are shown with their counts below.
- A name can appear in several archives. WoW's patch chain makes later-loaded archives override earlier ones, so a file's effective copy is not necessarily the first one listed here; the archive column below lists every holder alphabetically, not in load order.
- Names are shown as stored. Paths are case-insensitive in MPQ, so entries are grouped case-insensitively.

## Totals

- Archives read: 70
- Listfile entries (with duplicates across archives): 770,789
- Unique file names (case-insensitive): 639,153
- Unique `.dbc` names: 368

## Archives

| Archive | Listed entries | Listed but not resolvable by name |
| --- | ---: | ---: |
| area-52/patch-D.MPQ | 12 | 0 |
| common-2.MPQ | 43,083 | 0 |
| common.MPQ | 83,670 | 0 |
| enUS/backup-enUS.MPQ | 192 | 0 |
| enUS/base-enUS.MPQ | 188 | 0 |
| enUS/expansion-locale-enUS.MPQ | 420 | 0 |
| enUS/expansion-speech-enUS.MPQ | 1,502 | 0 |
| enUS/lichking-locale-enUS.MPQ | 55 | 0 |
| enUS/lichking-speech-enUS.MPQ | 1,846 | 0 |
| enUS/locale-enUS.MPQ | 11,054 | 0 |
| enUS/patch-enUS-2.MPQ | 2,337 | 2 |
| enUS/patch-enUS-3.MPQ | 949 | 0 |
| enUS/patch-enUS.MPQ | 5,398 | 26 |
| enUS/speech-enUS.MPQ | 5,106 | 0 |
| expansion.MPQ | 11,871 | 0 |
| lichking.MPQ | 16,179 | 0 |
| patch-2.MPQ | 11,811 | 1,234 |
| patch-3.MPQ | 2,994 | 39 |
| patch-A.MPQ | 8,406 | 0 |
| patch-B.MPQ | 1,590 | 0 |
| patch-CA.MPQ | 1,897 | 0 |
| patch-CB.MPQ | 3,043 | 0 |
| patch-CC.MPQ | 1,453 | 0 |
| patch-CD.MPQ | 3,833 | 0 |
| patch-CE.MPQ | 580 | 0 |
| patch-CF.MPQ | 2,009 | 0 |
| patch-CG.MPQ | 1,817 | 0 |
| patch-CH.MPQ | 1,054 | 0 |
| patch-CHA.MPQ | 49,120 | 0 |
| patch-CI.MPQ | 479 | 0 |
| patch-CJ.MPQ | 67 | 0 |
| patch-CK.MPQ | 757 | 0 |
| patch-CL.MPQ | 790 | 0 |
| patch-CM.MPQ | 2,417 | 0 |
| patch-CN.MPQ | 1,826 | 0 |
| patch-CO.MPQ | 678 | 0 |
| patch-CP.MPQ | 1,145 | 0 |
| patch-CQ.MPQ | 42 | 0 |
| patch-CR.MPQ | 1,438 | 0 |
| patch-CS.MPQ | 3,891 | 0 |
| patch-CT.MPQ | 1,456 | 0 |
| patch-CU.MPQ | 87 | 0 |
| patch-CV.MPQ | 1,415 | 0 |
| patch-CW.MPQ | 1,757 | 0 |
| patch-CX.MPQ | 135 | 0 |
| patch-CY.MPQ | 195 | 0 |
| patch-CZ.MPQ | 314 | 0 |
| patch-I.MPQ | 76,062 | 0 |
| patch-M.MPQ | 288 | 0 |
| patch-N.MPQ | 28,813 | 0 |
| patch-O.MPQ | 46,625 | 0 |
| patch-P.mpq | 1 | 0 |
| patch-Q.MPQ | 32,564 | 0 |
| patch-R.MPQ | 15,182 | 0 |
| patch-S.MPQ | 48 | 0 |
| patch-T.MPQ | 2 | 0 |
| patch-TA.MPQ | 91,960 | 0 |
| patch-TM.MPQ | 412 | 0 |
| patch-TW.MPQ | 11,249 | 0 |
| patch-U.MPQ | 38,202 | 0 |
| patch-V.MPQ | 6,763 | 0 |
| patch-WA.MPQ | 8,690 | 0 |
| patch-WB1.MPQ | 3,395 | 0 |
| patch-WB2.MPQ | 3,565 | 0 |
| patch-WC1.MPQ | 9,637 | 0 |
| patch-WC2.MPQ | 49,779 | 0 |
| patch-WC3.MPQ | 9,131 | 0 |
| patch-X.MPQ | 376 | 0 |
| patch-Z.MPQ | 4,527 | 0 |
| patch.MPQ | 41,160 | 3,610 |

## File types (entries, with cross-archive duplicates)

| Extension | Entries |
| --- | ---: |
| .blp | 402,213 |
| .skin | 115,056 |
| .m2 | 110,425 |
| .anim | 37,991 |
| .ogg | 34,989 |
| .wmo | 23,454 |
| .wav | 20,611 |
| .adt | 17,339 |
| .bls | 2,166 |
| .mp3 | 1,787 |
| .lua | 1,335 |
| .dbc | 992 |
| .xml | 825 |
| .wdl | 338 |
| .wdt | 291 |
| .toc | 172 |
| .html | 126 |
| .sig | 100 |
| .jpg | 84 |
| .tga | 83 |
| .avi | 59 |
| .(none) | 33 |
| .ttf | 27 |
| .gif | 26 |
| .nib | 22 |
| .bone | 22 |
| .wfx | 17 |
| .dll | 16 |
| .pub | 14 |
| .bin | 14 |
| .png | 13 |
| .exe | 12 |
| .txt | 11 |
| .zmp | 11 |
| .plist | 10 |
| .icns | 10 |
| .sbt | 10 |
| .wtf | 8 |
| .pdf | 6 |
| .not | 5 |

## Top-level folders (entries)

| Folder | Entries |
| --- | ---: |
| item | 190,586 |
| world | 137,007 |
| interface | 102,718 |
| character | 99,589 |
| textures | 71,059 |
| sound | 57,378 |
| creature | 56,241 |
| spells | 33,423 |
| dungeons | 10,647 |
| tileset | 4,797 |
| environments | 3,136 |
| shaders | 1,287 |
| dbfilesclient | 992 |
| _shaders | 894 |
| xtextures | 410 |
| data | 251 |
| particles | 128 |
| (root) | 46 |
| cameras | 41 |
| world of warcraft.app | 40 |
| background downloader.app | 36 |
| fonts | 28 |
| custom | 21 |
| world of warcraft launcher.app | 20 |
| wtf | 5 |
| interiors | 4 |
| test | 3 |
| spell | 2 |

## DBC files

Every `.dbc` named in any listfile. "Archives" lists each holder with the stored size in bytes (`?` if the name did not resolve).

| DBC | Archives (size) |
| --- | --- |
| DBFilesClient\Achievement.dbc | enUS/locale-enUS.MPQ (312358), enUS/patch-enUS-2.MPQ (603960), enUS/patch-enUS-3.MPQ (579340), enUS/patch-enUS.MPQ (542688), patch-M.MPQ (7065257) |
| DBFilesClient\Achievement_Category.dbc | enUS/locale-enUS.MPQ (7851), enUS/patch-enUS-2.MPQ (7956), enUS/patch-enUS.MPQ (7980), patch-M.MPQ (22089) |
| DBFilesClient\Achievement_Criteria.dbc | enUS/locale-enUS.MPQ (465049), enUS/patch-enUS-2.MPQ (1020924), enUS/patch-enUS-3.MPQ (1045678), enUS/patch-enUS.MPQ (974946), patch-M.MPQ (4840191) |
| DBFilesClient\AchievementRewards.dbc | patch-M.MPQ (26644) |
| DBFilesClient\AnimationData.dbc | enUS/locale-enUS.MPQ (21098), enUS/patch-enUS-2.MPQ (22578), enUS/patch-enUS-3.MPQ (22578), enUS/patch-enUS.MPQ (21638), patch-M.MPQ (87154) |
| DBFilesClient\AppearanceCategories.dbc | patch-M.MPQ (9815) |
| DBFilesClient\AppearanceDetails.dbc | patch-M.MPQ (1452969) |
| DBFilesClient\Appearances.dbc | patch-M.MPQ (2917676) |
| DBFilesClient\AreaGroup.dbc | enUS/locale-enUS.MPQ (6869), enUS/patch-enUS-2.MPQ (8949), enUS/patch-enUS-3.MPQ (9333), enUS/patch-enUS.MPQ (8757), patch-M.MPQ (9748) |
| DBFilesClient\AreaPOI.dbc | enUS/locale-enUS.MPQ (117556), enUS/patch-enUS-2.MPQ (167242), enUS/patch-enUS-3.MPQ (167242), enUS/patch-enUS.MPQ (167228), patch-M.MPQ (191929) |
| DBFilesClient\AreaTable.dbc | enUS/locale-enUS.MPQ (344902), enUS/patch-enUS-2.MPQ (366801), enUS/patch-enUS-3.MPQ (366962), enUS/patch-enUS.MPQ (363135), patch-M.MPQ (451248) |
| DBFilesClient\AreaTrigger.dbc | enUS/locale-enUS.MPQ (38581), enUS/patch-enUS-2.MPQ (48061), enUS/patch-enUS-3.MPQ (48821), enUS/patch-enUS.MPQ (43901), patch-M.MPQ (59460) |
| DBFilesClient\AttackAnimKits.dbc | enUS/locale-enUS.MPQ (541) |
| DBFilesClient\AttackAnimTypes.dbc | enUS/locale-enUS.MPQ (160) |
| DBFilesClient\AuctionHouse.dbc | enUS/locale-enUS.MPQ (778), patch-M.MPQ (891) |
| DBFilesClient\BankBagSlotPrices.dbc | enUS/locale-enUS.MPQ (117), patch-M.MPQ (116) |
| DBFilesClient\BannedAddOns.dbc | enUS/patch-enUS-2.MPQ (505), enUS/patch-enUS-3.MPQ (4509), enUS/patch-enUS.MPQ (505), patch-M.MPQ (4508) |
| DBFilesClient\BarberShopStyle.dbc | enUS/locale-enUS.MPQ (85792), enUS/patch-enUS-3.MPQ (80954), enUS/patch-enUS.MPQ (80954), patch-M.MPQ (104321) |
| DBFilesClient\BattlemasterList.dbc | enUS/locale-enUS.MPQ (1648), enUS/patch-enUS-2.MPQ (1846), enUS/patch-enUS-3.MPQ (1900), enUS/patch-enUS.MPQ (2054), patch-M.MPQ (10766) |
| DBFilesClient\CameraShakes.dbc | enUS/locale-enUS.MPQ (2133), enUS/patch-enUS-2.MPQ (2325), enUS/patch-enUS-3.MPQ (2517), enUS/patch-enUS.MPQ (2229), patch-M.MPQ (2868) |
| DBFilesClient\Cfg_Categories.dbc | enUS/locale-enUS.MPQ (2818), enUS/patch-enUS-2.MPQ (3356), enUS/patch-enUS.MPQ (2818), patch-M.MPQ (3725) |
| DBFilesClient\Cfg_Configs.dbc | enUS/locale-enUS.MPQ (229) |
| DBFilesClient\Challenge.dbc | patch-M.MPQ (156060) |
| DBFilesClient\ChallengeConditions.dbc | patch-M.MPQ (14347) |
| DBFilesClient\ChallengeConditionTypes.dbc | patch-M.MPQ (9374) |
| DBFilesClient\ChallengeFeatured.dbc | patch-M.MPQ (1100) |
| DBFilesClient\ChallengeGroupRewards.dbc | patch-M.MPQ (5872) |
| DBFilesClient\ChallengeGroups.dbc | patch-M.MPQ (14456) |
| DBFilesClient\ChallengeLevels.dbc | patch-M.MPQ (26700) |
| DBFilesClient\ChallengeModifiers.dbc | patch-M.MPQ (120) |
| DBFilesClient\ChallengeModifierTypes.dbc | patch-M.MPQ (2040) |
| DBFilesClient\ChallengeRequirements.dbc | patch-M.MPQ (74035) |
| DBFilesClient\ChallengeRequirementTypes.dbc | patch-M.MPQ (7613) |
| DBFilesClient\ChallengeRewards.dbc | patch-M.MPQ (954413) |
| DBFilesClient\ChallengeRules.dbc | patch-M.MPQ (77916) |
| DBFilesClient\ChallengeRuleTypes.dbc | patch-M.MPQ (33460) |
| DBFilesClient\ChallengeSpells.dbc | patch-S.MPQ (308100) |
| DBFilesClient\CharacterAdvancement.dbc | area-52/patch-D.MPQ (5597994), patch-M.MPQ (7327863) |
| DBFilesClient\CharacterAdvancementCategories.dbc | patch-M.MPQ (12790) |
| DBFilesClient\CharacterAdvancementClassTypes.dbc | patch-M.MPQ (4879) |
| DBFilesClient\CharacterAdvancementEssence.dbc | area-52/patch-D.MPQ (195860), patch-M.MPQ (201620) |
| DBFilesClient\CharacterAdvancementTabTypes.dbc | patch-M.MPQ (8074) |
| DBFilesClient\CharacterCreationArchetypeCategories.dbc | patch-M.MPQ (5737) |
| DBFilesClient\CharacterCreationArchetypeDetails.dbc | patch-M.MPQ (125460) |
| DBFilesClient\CharacterCreationArchetypeRoles.dbc | patch-M.MPQ (2376) |
| DBFilesClient\CharacterCreationArchetypes.dbc | patch-M.MPQ (94445) |
| DBFilesClient\CharacterCreationClassDetails.dbc | patch-M.MPQ (51988) |
| DBFilesClient\CharacterCreationClassGuideRoles.dbc | patch-M.MPQ (943) |
| DBFilesClient\CharacterCreationClassGuideSubroleClasses.dbc | patch-M.MPQ (484) |
| DBFilesClient\CharacterCreationClassGuideSubroles.dbc | patch-M.MPQ (1709) |
| DBFilesClient\CharacterCreationPetDetails.dbc | patch-M.MPQ (8180) |
| DBFilesClient\CharacterCreationShapeshiftDetails.dbc | patch-M.MPQ (8420) |
| DBFilesClient\CharacterFacialHairStyles.dbc | enUS/locale-enUS.MPQ (6997), enUS/patch-enUS.MPQ (7125), patch-M.MPQ (41012) |
| DBFilesClient\CharBaseInfo.dbc | enUS/locale-enUS.MPQ (145), patch-M.MPQ (438), patch-T.MPQ (610) |
| DBFilesClient\CharHairGeosets.dbc | enUS/locale-enUS.MPQ (8157), enUS/patch-enUS.MPQ (8157), patch-M.MPQ (22700) |
| DBFilesClient\CharHairTextures.dbc | enUS/locale-enUS.MPQ (2869), patch-M.MPQ (2997) |
| DBFilesClient\CharSections.dbc | enUS/locale-enUS.MPQ (845641), enUS/patch-enUS-2.MPQ (845287), enUS/patch-enUS-3.MPQ (845271), enUS/patch-enUS.MPQ (843753), patch-M.MPQ (4187665) |
| DBFilesClient\CharStartOutfit.dbc | enUS/locale-enUS.MPQ (37317), enUS/patch-enUS-2.MPQ (37317), enUS/patch-enUS.MPQ (37317), patch-M.MPQ (290396) |
| DBFilesClient\CharTitles.dbc | enUS/locale-enUS.MPQ (7837), enUS/patch-enUS-2.MPQ (23527), enUS/patch-enUS.MPQ (22511), patch-M.MPQ (31254) |
| DBFilesClient\CharVariations.dbc | enUS/locale-enUS.MPQ (885), enUS/patch-enUS.MPQ (?), patch-M.MPQ (981) |
| DBFilesClient\ChatChannels.dbc | enUS/locale-enUS.MPQ (1046), enUS/patch-enUS-2.MPQ (1046), enUS/patch-enUS-3.MPQ (1046), patch-M.MPQ (1135) |
| DBFilesClient\ChatProfanity.dbc | enUS/locale-enUS.MPQ (89444), enUS/patch-enUS-2.MPQ (101841), enUS/patch-enUS-3.MPQ (101815), enUS/patch-enUS.MPQ (101915), patch-M.MPQ (102072) |
| DBFilesClient\ChrClasses.dbc | enUS/locale-enUS.MPQ (2580), enUS/patch-enUS-2.MPQ (2580), enUS/patch-enUS-3.MPQ (2580), patch-M.MPQ (8294) |
| DBFilesClient\ChrClassesRoles.dbc | patch-M.MPQ (1428) |
| DBFilesClient\ChrRaces.dbc | enUS/locale-enUS.MPQ (6201), enUS/patch-enUS-2.MPQ (6201), enUS/patch-enUS-3.MPQ (6201), enUS/patch-enUS.MPQ (6117), patch-M.MPQ (12291) |
| DBFilesClient\ChrSpecs.dbc | patch-M.MPQ (39101) |
| DBFilesClient\CinematicCamera.dbc | enUS/locale-enUS.MPQ (758), patch-M.MPQ (916) |
| DBFilesClient\CinematicSequences.dbc | enUS/locale-enUS.MPQ (581), patch-M.MPQ (620) |
| DBFilesClient\CollectorCacheItems.dbc | patch-M.MPQ (1476) |
| DBFilesClient\CollectorCacheRarityRates.dbc | patch-M.MPQ (5636) |
| DBFilesClient\CollectorCacheRarityTypes.dbc | patch-M.MPQ (690) |
| DBFilesClient\CollectorCacheTypes.dbc | patch-M.MPQ (202) |
| DBFilesClient\Creature.dbc | patch-M.MPQ (14274089) |
| DBFilesClient\CreatureDisplayInfo.dbc | enUS/locale-enUS.MPQ (1405921), enUS/patch-enUS-2.MPQ (1598981), enUS/patch-enUS-3.MPQ (1610064), enUS/patch-enUS.MPQ (1562323), patch-M.MPQ (5653511) |
| DBFilesClient\CreatureDisplayInfoExtra.dbc | enUS/locale-enUS.MPQ (1610262), enUS/patch-enUS-2.MPQ (1812812), enUS/patch-enUS-3.MPQ (1825335), enUS/patch-enUS.MPQ (1767572), patch-M.MPQ (2728278) |
| DBFilesClient\CreatureDisplayInfoGeosetData.dbc | patch-M.MPQ (302980) |
| DBFilesClient\CreatureFamily.dbc | enUS/locale-enUS.MPQ (5813), enUS/patch-enUS.MPQ (6246), patch-M.MPQ (53964) |
| DBFilesClient\CreatureModelData.dbc | enUS/locale-enUS.MPQ (144979), enUS/patch-enUS-2.MPQ (193731), enUS/patch-enUS-3.MPQ (197135), enUS/patch-enUS.MPQ (184036), patch-M.MPQ (5546053) |
| DBFilesClient\CreatureMovementInfo.dbc | enUS/patch-enUS-2.MPQ (2053), enUS/patch-enUS.MPQ (2021), patch-M.MPQ (2084) |
| DBFilesClient\CreatureSoundData.dbc | enUS/locale-enUS.MPQ (158529), enUS/patch-enUS-2.MPQ (197317), enUS/patch-enUS-3.MPQ (198533), enUS/patch-enUS.MPQ (191693), patch-M.MPQ (1580516) |
| DBFilesClient\CreatureSpellData.dbc | enUS/locale-enUS.MPQ (28749), enUS/patch-enUS.MPQ (28893), patch-S.MPQ (28928) |
| DBFilesClient\CreatureType.dbc | enUS/locale-enUS.MPQ (1127), patch-M.MPQ (1468) |
| DBFilesClient\CurrencyCategory.dbc | enUS/locale-enUS.MPQ (715), enUS/patch-enUS.MPQ (737), patch-M.MPQ (1300) |
| DBFilesClient\CurrencyTypes.dbc | enUS/locale-enUS.MPQ (117), enUS/patch-enUS-2.MPQ (437), enUS/patch-enUS.MPQ (421), patch-M.MPQ (1124) |
| DBFilesClient\DanceMoves.dbc | enUS/locale-enUS.MPQ (3018), enUS/patch-enUS-3.MPQ (3018), enUS/patch-enUS.MPQ (3018) |
| DBFilesClient\DeathThudLookups.dbc | enUS/locale-enUS.MPQ (921) |
| DBFilesClient\DeclinedWord.dbc | enUS/locale-enUS.MPQ (770822), enUS/patch-enUS-2.MPQ (1299790), enUS/patch-enUS-3.MPQ (1313560), enUS/patch-enUS.MPQ (1135136) |
| DBFilesClient\DeclinedWordCases.dbc | enUS/locale-enUS.MPQ (4070023), enUS/patch-enUS-2.MPQ (6692122), enUS/patch-enUS-3.MPQ (6734878), enUS/patch-enUS.MPQ (5950417) |
| DBFilesClient\DestructibleModelData.dbc | enUS/patch-enUS-2.MPQ (3137), enUS/patch-enUS.MPQ (2673), patch-M.MPQ (3136) |
| DBFilesClient\DungeonEncounter.dbc | enUS/patch-enUS-2.MPQ (62024), enUS/patch-enUS-3.MPQ (63033), patch-M.MPQ (200727) |
| DBFilesClient\DungeonEncounterExtra.dbc | patch-M.MPQ (33124) |
| DBFilesClient\DungeonMap.dbc | enUS/locale-enUS.MPQ (245), enUS/patch-enUS-2.MPQ (1781), enUS/patch-enUS.MPQ (1461), patch-M.MPQ (6548) |
| DBFilesClient\DungeonMapChunk.dbc | enUS/locale-enUS.MPQ (3621), enUS/patch-enUS-2.MPQ (12441), enUS/patch-enUS-3.MPQ (12461), enUS/patch-enUS.MPQ (11121), patch-M.MPQ (54540) |
| DBFilesClient\DurabilityCosts.dbc | enUS/locale-enUS.MPQ (36021), patch-M.MPQ (36020) |
| DBFilesClient\DurabilityQuality.dbc | enUS/locale-enUS.MPQ (133), enUS/patch-enUS-3.MPQ (149), enUS/patch-enUS.MPQ (149), patch-M.MPQ (148) |
| DBFilesClient\Emotes.dbc | enUS/locale-enUS.MPQ (7237), enUS/patch-enUS-2.MPQ (8117), enUS/patch-enUS.MPQ (7927), patch-M.MPQ (22603) |
| DBFilesClient\EmotesText.dbc | enUS/locale-enUS.MPQ (20712), enUS/patch-enUS-2.MPQ (20791), enUS/patch-enUS.MPQ (20712), patch-M.MPQ (20790) |
| DBFilesClient\EmotesTextData.dbc | enUS/locale-enUS.MPQ (132755), enUS/patch-enUS-2.MPQ (133239), enUS/patch-enUS-3.MPQ (133239), enUS/patch-enUS.MPQ (132755), patch-M.MPQ (133239) |
| DBFilesClient\EmotesTextSound.dbc | enUS/locale-enUS.MPQ (10461), enUS/patch-enUS-2.MPQ (10861), patch-M.MPQ (10860) |
| DBFilesClient\EnchantEnchantSuggestions.dbc | patch-M.MPQ (6080148) |
| DBFilesClient\EnchantRoleSuggestions.dbc | patch-M.MPQ (20) |
| DBFilesClient\EnchantStatSuggestions.dbc | patch-M.MPQ (20) |
| DBFilesClient\EnvironmentalDamage.dbc | enUS/locale-enUS.MPQ (93) |
| DBFilesClient\Exhaustion.dbc | enUS/locale-enUS.MPQ (624) |
| DBFilesClient\ExtraActionButtons.dbc | patch-M.MPQ (1666) |
| DBFilesClient\Faction.dbc | enUS/locale-enUS.MPQ (98897), enUS/patch-enUS-2.MPQ (108787), enUS/patch-enUS-3.MPQ (108787), enUS/patch-enUS.MPQ (100992), patch-M.MPQ (113034) |
| DBFilesClient\FactionGroup.dbc | enUS/locale-enUS.MPQ (371), patch-M.MPQ (371) |
| DBFilesClient\FactionTemplate.dbc | enUS/locale-enUS.MPQ (45157), enUS/patch-enUS-2.MPQ (47117), enUS/patch-enUS.MPQ (46445), patch-M.MPQ (48796) |
| DBFilesClient\FileData.dbc | enUS/locale-enUS.MPQ (224), enUS/patch-enUS-3.MPQ (352), enUS/patch-enUS.MPQ (291), patch-M.MPQ (433) |
| DBFilesClient\FootprintTextures.dbc | enUS/locale-enUS.MPQ (268) |
| DBFilesClient\FootstepTerrainLookup.dbc | enUS/locale-enUS.MPQ (4361), enUS/patch-enUS-2.MPQ (4361), enUS/patch-enUS.MPQ (4361), patch-M.MPQ (4820) |
| DBFilesClient\GameObjectArtKit.dbc | enUS/locale-enUS.MPQ (2011), patch-M.MPQ (2073) |
| DBFilesClient\GameObjectDisplayInfo.dbc | enUS/locale-enUS.MPQ (453656), enUS/patch-enUS-2.MPQ (526806), enUS/patch-enUS-3.MPQ (534007), enUS/patch-enUS.MPQ (512615), patch-M.MPQ (11689109) |
| DBFilesClient\GameObjectDisplayInfoAddon.dbc | patch-M.MPQ (2206431) |
| DBFilesClient\GameTables.dbc | enUS/locale-enUS.MPQ (2503), enUS/patch-enUS-2.MPQ (2640), enUS/patch-enUS-3.MPQ (2665), enUS/patch-enUS.MPQ (2542) |
| DBFilesClient\GameTips.dbc | enUS/locale-enUS.MPQ (20723), enUS/patch-enUS-2.MPQ (26904), enUS/patch-enUS-3.MPQ (26915), enUS/patch-enUS.MPQ (24534), patch-M.MPQ (33885) |
| DBFilesClient\GemProperties.dbc | enUS/locale-enUS.MPQ (12201), enUS/patch-enUS.MPQ (12541), patch-M.MPQ (13380) |
| DBFilesClient\GlobalStrings.dbc | patch-M.MPQ (1970954) |
| DBFilesClient\GlyphProperties.dbc | enUS/locale-enUS.MPQ (2485), enUS/patch-enUS-2.MPQ (5813), enUS/patch-enUS.MPQ (5765), patch-M.MPQ (5812) |
| DBFilesClient\GlyphSlot.dbc | enUS/locale-enUS.MPQ (141), patch-M.MPQ (140) |
| DBFilesClient\GMSurveyAnswers.dbc | enUS/patch-enUS.MPQ (3715), patch-M.MPQ (3715) |
| DBFilesClient\GMSurveyCurrentSurvey.dbc | enUS/locale-enUS.MPQ (93), enUS/patch-enUS.MPQ (93) |
| DBFilesClient\GMSurveyQuestions.dbc | enUS/locale-enUS.MPQ (927), enUS/patch-enUS.MPQ (1849), patch-M.MPQ (1861) |
| DBFilesClient\GMSurveySurveys.dbc | enUS/locale-enUS.MPQ (65), enUS/patch-enUS.MPQ (109) |
| DBFilesClient\GMTicketCategory.dbc | enUS/locale-enUS.MPQ (3396), enUS/patch-enUS.MPQ (3396) |
| DBFilesClient\GroundEffectDoodad.dbc | enUS/locale-enUS.MPQ (16766), enUS/patch-enUS-3.MPQ (14558), enUS/patch-enUS.MPQ (14462), patch-M.MPQ (53456) |
| DBFilesClient\GroundEffectTexture.dbc | enUS/locale-enUS.MPQ (1024649), enUS/patch-enUS-2.MPQ (1095709), enUS/patch-enUS-3.MPQ (1099185), enUS/patch-enUS.MPQ (1081937), patch-M.MPQ (1691204) |
| DBFilesClient\gtBarberShopCostBase.dbc | enUS/locale-enUS.MPQ (421), patch-M.MPQ (1040) |
| DBFilesClient\gtChanceToMeleeCrit.dbc | enUS/locale-enUS.MPQ (4421), enUS/patch-enUS.MPQ (4421), patch-M.MPQ (12820) |
| DBFilesClient\gtChanceToMeleeCritBase.dbc | enUS/locale-enUS.MPQ (65), enUS/patch-enUS.MPQ (65), patch-M.MPQ (148) |
| DBFilesClient\gtChanceToSpellCrit.dbc | enUS/locale-enUS.MPQ (4421), patch-S.MPQ (12820) |
| DBFilesClient\gtChanceToSpellCritBase.dbc | enUS/locale-enUS.MPQ (65), patch-S.MPQ (148) |
| DBFilesClient\gtCombatRatings.dbc | enUS/locale-enUS.MPQ (12821), enUS/patch-enUS.MPQ (12821), patch-M.MPQ (12820) |
| DBFilesClient\gtNPCManaCostScaler.dbc | enUS/locale-enUS.MPQ (421), patch-M.MPQ (420) |
| DBFilesClient\gtOCTClassCombatRatingScalar.dbc | enUS/patch-enUS-2.MPQ (2837), enUS/patch-enUS.MPQ (2837), patch-M.MPQ (8212) |
| DBFilesClient\gtOCTRegenHP.dbc | enUS/locale-enUS.MPQ (4421), enUS/patch-enUS-2.MPQ (4421), patch-M.MPQ (12820) |
| DBFilesClient\gtOCTRegenMP.dbc | enUS/locale-enUS.MPQ (4421), patch-M.MPQ (12821) |
| DBFilesClient\gtRegenHPPerSpt.dbc | enUS/locale-enUS.MPQ (4421), enUS/patch-enUS-2.MPQ (4421), patch-M.MPQ (12820) |
| DBFilesClient\gtRegenMPPerSpt.dbc | enUS/locale-enUS.MPQ (4421), enUS/patch-enUS-2.MPQ (4421), enUS/patch-enUS.MPQ (4421), patch-M.MPQ (12820) |
| DBFilesClient\HDCharacterFacialHairStyles.dbc | patch-M.MPQ (9812) |
| DBFilesClient\HDCharHairGeosets.dbc | patch-M.MPQ (8948) |
| DBFilesClient\HDCharSections.dbc | patch-M.MPQ (2762142) |
| DBFilesClient\HDCreatureDisplayInfo.dbc | patch-M.MPQ (468332) |
| DBFilesClient\HDCreatureDisplayInfoExtra.dbc | patch-M.MPQ (1670007) |
| DBFilesClient\HDCreatureModelData.dbc | patch-M.MPQ (80822) |
| DBFilesClient\HDEmotesTextSound.dbc | patch-M.MPQ (15660) |
| DBFilesClient\HDHelmetGeosetVisData.dbc | patch-M.MPQ (2132) |
| DBFilesClient\HDSpellVisualEffectName.dbc | patch-S.MPQ (27447) |
| DBFilesClient\HDSpellVisualKitModelAttach.dbc | patch-S.MPQ (29500) |
| DBFilesClient\HelmetGeosetVisData.dbc | enUS/locale-enUS.MPQ (661), enUS/patch-enUS-2.MPQ (693), enUS/patch-enUS.MPQ (661), patch-M.MPQ (2132) |
| DBFilesClient\HolidayDescriptions.dbc | enUS/locale-enUS.MPQ (4739), enUS/patch-enUS-2.MPQ (6617), enUS/patch-enUS.MPQ (6275), patch-M.MPQ (7195) |
| DBFilesClient\HolidayNames.dbc | enUS/locale-enUS.MPQ (1759), enUS/patch-enUS-2.MPQ (2431), enUS/patch-enUS.MPQ (2248), patch-M.MPQ (2613) |
| DBFilesClient\Holidays.dbc | enUS/locale-enUS.MPQ (3981), enUS/patch-enUS-2.MPQ (6114), enUS/patch-enUS-3.MPQ (6114), enUS/patch-enUS.MPQ (5674), patch-M.MPQ (13191) |
| DBFilesClient\Item.dbc | enUS/locale-enUS.MPQ (610277), enUS/patch-enUS-2.MPQ (1469013), enUS/patch-enUS-3.MPQ (1475093), enUS/patch-enUS.MPQ (1394869), patch-M.MPQ (18040628) |
| DBFilesClient\ItemAddon.dbc | patch-M.MPQ (115209653) |
| DBFilesClient\ItemAppearances.dbc | patch-M.MPQ (2435204) |
| DBFilesClient\ItemBagFamily.dbc | enUS/locale-enUS.MPQ (1383), enUS/patch-enUS.MPQ (1383), patch-M.MPQ (1383) |
| DBFilesClient\ItemClass.dbc | enUS/locale-enUS.MPQ (1538), enUS/patch-enUS.MPQ (1538), patch-M.MPQ (1632) |
| DBFilesClient\ItemCondExtCosts.dbc | enUS/locale-enUS.MPQ (9125), enUS/patch-enUS-3.MPQ (18629), enUS/patch-enUS.MPQ (15253), patch-M.MPQ (18628) |
| DBFilesClient\ItemDisplayInfo.dbc | enUS/locale-enUS.MPQ (5413036), enUS/patch-enUS-2.MPQ (6670819), enUS/patch-enUS-3.MPQ (6732827), enUS/patch-enUS.MPQ (6406258), patch-M.MPQ (16565854) |
| DBFilesClient\ItemDisplayInfoCollections.dbc | patch-M.MPQ (261929) |
| DBFilesClient\ItemExtendedCost.dbc | enUS/locale-enUS.MPQ (27741), enUS/patch-enUS-2.MPQ (58837), enUS/patch-enUS-3.MPQ (62229), enUS/patch-enUS.MPQ (45141), patch-M.MPQ (881748) |
| DBFilesClient\ItemGroupSounds.dbc | enUS/locale-enUS.MPQ (501), patch-M.MPQ (600) |
| DBFilesClient\ItemLimitCategory.dbc | enUS/locale-enUS.MPQ (297), enUS/patch-enUS-2.MPQ (7524), enUS/patch-enUS-3.MPQ (8442), enUS/patch-enUS.MPQ (4020), patch-M.MPQ (264646) |
| DBFilesClient\ItemPetFood.dbc | enUS/locale-enUS.MPQ (651), patch-M.MPQ (651) |
| DBFilesClient\ItemPurchaseGroup.dbc | enUS/locale-enUS.MPQ (158), enUS/patch-enUS.MPQ (158), patch-M.MPQ (158) |
| DBFilesClient\ItemRandomProperties.dbc | enUS/locale-enUS.MPQ (193808), patch-M.MPQ (972299) |
| DBFilesClient\ItemRandomSuffix.dbc | enUS/locale-enUS.MPQ (12210), enUS/patch-enUS-3.MPQ (13084), enUS/patch-enUS.MPQ (13084), patch-M.MPQ (37735) |
| DBFilesClient\ItemSet.dbc | enUS/locale-enUS.MPQ (93335), enUS/patch-enUS-2.MPQ (118552), enUS/patch-enUS-3.MPQ (118552), enUS/patch-enUS.MPQ (113622), patch-M.MPQ (539281) |
| DBFilesClient\ItemSetAppearances.dbc | patch-M.MPQ (19256) |
| DBFilesClient\ItemSpells.dbc | patch-S.MPQ (19494876) |
| DBFilesClient\ItemStat.dbc | patch-M.MPQ (236173256) |
| DBFilesClient\ItemSubClass.dbc | enUS/locale-enUS.MPQ (22002), enUS/patch-enUS-3.MPQ (22190), enUS/patch-enUS.MPQ (22190), patch-M.MPQ (22563) |
| DBFilesClient\ItemSubClassMask.dbc | enUS/locale-enUS.MPQ (383), patch-M.MPQ (383) |
| DBFilesClient\ItemVisualEffects.dbc | enUS/locale-enUS.MPQ (3860), enUS/patch-enUS-2.MPQ (4008), enUS/patch-enUS.MPQ (3901), patch-M.MPQ (9030) |
| DBFilesClient\ItemVisuals.dbc | enUS/locale-enUS.MPQ (1797), enUS/patch-enUS-2.MPQ (1941), enUS/patch-enUS.MPQ (1845), patch-M.MPQ (5468) |
| DBFilesClient\Languages.dbc | enUS/locale-enUS.MPQ (1399), enUS/patch-enUS.MPQ (1399), patch-M.MPQ (1399) |
| DBFilesClient\LanguageWords.dbc | enUS/locale-enUS.MPQ (26919), enUS/patch-enUS-2.MPQ (26903) |
| DBFilesClient\LFGActivities.dbc | patch-M.MPQ (31705) |
| DBFilesClient\LFGActivityCategories.dbc | patch-M.MPQ (1416) |
| DBFilesClient\LFGActivityGroupType.dbc | patch-M.MPQ (454) |
| DBFilesClient\LFGDungeonExpansion.dbc | enUS/patch-enUS-2.MPQ (949), patch-M.MPQ (916) |
| DBFilesClient\LFGDungeonGroup.dbc | enUS/patch-enUS-2.MPQ (1000), enUS/patch-enUS-3.MPQ (1097), patch-M.MPQ (1954) |
| DBFilesClient\LFGDungeons.dbc | enUS/locale-enUS.MPQ (17668), enUS/patch-enUS-2.MPQ (27371), enUS/patch-enUS-3.MPQ (41932), enUS/patch-enUS.MPQ (20539), patch-M.MPQ (88974) |
| DBFilesClient\Light.dbc | enUS/locale-enUS.MPQ (41421), enUS/patch-enUS-2.MPQ (42861), enUS/patch-enUS-3.MPQ (42921), enUS/patch-enUS.MPQ (42441), patch-M.MPQ (81020) |
| DBFilesClient\LightFloatBand.dbc | enUS/locale-enUS.MPQ (669957), enUS/patch-enUS-2.MPQ (691989), enUS/patch-enUS-3.MPQ (693621), enUS/patch-enUS.MPQ (683829), patch-M.MPQ (930260) |
| DBFilesClient\LightIntBand.dbc | enUS/locale-enUS.MPQ (2009829), enUS/patch-enUS-2.MPQ (2075925), enUS/patch-enUS-3.MPQ (2080821), enUS/patch-enUS.MPQ (2051445), patch-M.MPQ (2790876) |
| DBFilesClient\LightParams.dbc | enUS/locale-enUS.MPQ (29577), enUS/patch-enUS-2.MPQ (30549), enUS/patch-enUS-3.MPQ (30621), enUS/patch-enUS.MPQ (30189), patch-M.MPQ (41096) |
| DBFilesClient\LightSkybox.dbc | enUS/locale-enUS.MPQ (3139), enUS/patch-enUS-2.MPQ (3613), enUS/patch-enUS-3.MPQ (3663), enUS/patch-enUS.MPQ (3263), patch-M.MPQ (8921) |
| DBFilesClient\LiquidMaterial.dbc | enUS/locale-enUS.MPQ (57) |
| DBFilesClient\LiquidType.dbc | enUS/locale-enUS.MPQ (4273), enUS/patch-enUS.MPQ (5420), patch-M.MPQ (5812) |
| DBFilesClient\LoadingScreens.dbc | enUS/locale-enUS.MPQ (6565), enUS/patch-enUS-2.MPQ (7961), enUS/patch-enUS-3.MPQ (8054), enUS/patch-enUS.MPQ (7218), patch-M.MPQ (14268) |
| DBFilesClient\LoadingScreenTaxiSplines.dbc | enUS/locale-enUS.MPQ (933), enUS/patch-enUS.MPQ (2073) |
| DBFilesClient\LoadingScreenVariation.dbc | patch-M.MPQ (20) |
| DBFilesClient\Lock.dbc | enUS/locale-enUS.MPQ (48465), enUS/patch-enUS-2.MPQ (51105), enUS/patch-enUS-3.MPQ (51237), enUS/patch-enUS.MPQ (51105), patch-M.MPQ (59816) |
| DBFilesClient\LockType.dbc | enUS/locale-enUS.MPQ (4672), enUS/patch-enUS.MPQ (4916), patch-M.MPQ (5913) |
| DBFilesClient\MailTemplate.dbc | enUS/locale-enUS.MPQ (48296), enUS/patch-enUS-2.MPQ (62234), enUS/patch-enUS-3.MPQ (64321), enUS/patch-enUS.MPQ (61278), patch-M.MPQ (64579) |
| DBFilesClient\Manastorm.dbc | area-52/patch-D.MPQ (36920), patch-M.MPQ (36632) |
| DBFilesClient\ManastormMessages.dbc | area-52/patch-D.MPQ (67282), patch-M.MPQ (67282) |
| DBFilesClient\ManastormModifiers.dbc | area-52/patch-D.MPQ (1966100), patch-M.MPQ (1966100) |
| DBFilesClient\ManastormPlayerGroupModifiers.dbc | area-52/patch-D.MPQ (320), patch-M.MPQ (320) |
| DBFilesClient\Map.dbc | enUS/locale-enUS.MPQ (65283), enUS/patch-enUS-2.MPQ (42287), enUS/patch-enUS-3.MPQ (43226), enUS/patch-enUS.MPQ (39001), patch-M.MPQ (113315) |
| DBFilesClient\MapDifficulty.dbc | enUS/patch-enUS-2.MPQ (18607), enUS/patch-enUS-3.MPQ (19067), enUS/patch-enUS.MPQ (16603), patch-M.MPQ (65656) |
| DBFilesClient\Material.dbc | enUS/locale-enUS.MPQ (181), patch-M.MPQ (181) |
| DBFilesClient\MentorSpecializations.dbc | patch-M.MPQ (1047) |
| DBFilesClient\Movie.dbc | enUS/locale-enUS.MPQ (114), enUS/patch-enUS-3.MPQ (126), patch-M.MPQ (457) |
| DBFilesClient\MovieFileData.dbc | enUS/locale-enUS.MPQ (69), enUS/patch-enUS-3.MPQ (85), patch-M.MPQ (117) |
| DBFilesClient\MovieVariation.dbc | enUS/locale-enUS.MPQ (117), enUS/patch-enUS-3.MPQ (141), patch-M.MPQ (177) |
| DBFilesClient\MysticEnchant.dbc | patch-M.MPQ (972663) |
| DBFilesClient\MythicAffixes.dbc | patch-M.MPQ (858196) |
| DBFilesClient\MythicKeystones.dbc | patch-M.MPQ (81632) |
| DBFilesClient\MythicPlusScaling.dbc | patch-M.MPQ (6420) |
| DBFilesClient\NameGen.dbc | enUS/locale-enUS.MPQ (44746), enUS/patch-enUS.MPQ (144728), patch-M.MPQ (144727) |
| DBFilesClient\NamesProfanity.dbc | enUS/locale-enUS.MPQ (119432), enUS/patch-enUS-2.MPQ (134417), enUS/patch-enUS.MPQ (134486), patch-M.MPQ (134440) |
| DBFilesClient\NamesReserved.dbc | enUS/locale-enUS.MPQ (229846), enUS/patch-enUS-2.MPQ (229876), enUS/patch-enUS.MPQ (229900), patch-M.MPQ (229866) |
| DBFilesClient\NPCSounds.dbc | enUS/locale-enUS.MPQ (5441), enUS/patch-enUS-2.MPQ (5981), enUS/patch-enUS-3.MPQ (6041), enUS/patch-enUS.MPQ (5741), patch-M.MPQ (49760) |
| DBFilesClient\NPCTrainer.dbc | patch-M.MPQ (209924) |
| DBFilesClient\ObjectEffect.dbc | enUS/locale-enUS.MPQ (3297), enUS/patch-enUS-2.MPQ (7868), enUS/patch-enUS-3.MPQ (8545), enUS/patch-enUS.MPQ (7332), patch-M.MPQ (9616) |
| DBFilesClient\ObjectEffectGroup.dbc | enUS/locale-enUS.MPQ (1580), enUS/patch-enUS-2.MPQ (3369), enUS/patch-enUS-3.MPQ (3493), enUS/patch-enUS.MPQ (3203), patch-M.MPQ (3715) |
| DBFilesClient\ObjectEffectModifier.dbc | enUS/locale-enUS.MPQ (53), enUS/patch-enUS-2.MPQ (373), enUS/patch-enUS.MPQ (277), patch-M.MPQ (372) |
| DBFilesClient\ObjectEffectPackage.dbc | enUS/locale-enUS.MPQ (177), enUS/patch-enUS-2.MPQ (783), enUS/patch-enUS-3.MPQ (829), enUS/patch-enUS.MPQ (655), patch-M.MPQ (859) |
| DBFilesClient\ObjectEffectPackageElem.dbc | enUS/locale-enUS.MPQ (773), enUS/patch-enUS-2.MPQ (2341), enUS/patch-enUS-3.MPQ (2437), enUS/patch-enUS.MPQ (2213), patch-M.MPQ (3188) |
| DBFilesClient\ObjectSpawnVisibilitySettings.dbc | patch-M.MPQ (20) |
| DBFilesClient\OcclusionVolume.dbc | patch-M.MPQ (12545) |
| DBFilesClient\OcclusionVolumePoints.dbc | patch-M.MPQ (30600) |
| DBFilesClient\OverrideSpellData.dbc | enUS/patch-enUS-2.MPQ (309), enUS/patch-enUS-3.MPQ (405), enUS/patch-enUS.MPQ (213), patch-S.MPQ (2372) |
| DBFilesClient\Package.dbc | enUS/locale-enUS.MPQ (125) |
| DBFilesClient\PageTextMaterial.dbc | enUS/locale-enUS.MPQ (132) |
| DBFilesClient\PaperDollItemFrame.dbc | enUS/locale-enUS.MPQ (1602) |
| DBFilesClient\ParticleColor.dbc | enUS/locale-enUS.MPQ (3781), enUS/patch-enUS-2.MPQ (4261), enUS/patch-enUS.MPQ (3901), patch-M.MPQ (66660) |
| DBFilesClient\PetitionType.dbc | enUS/locale-enUS.MPQ (62) |
| DBFilesClient\PetPersonality.dbc | enUS/locale-enUS.MPQ (247) |
| DBFilesClient\PowerDisplay.dbc | enUS/patch-enUS-2.MPQ (210), enUS/patch-enUS-3.MPQ (242), enUS/patch-enUS.MPQ (110), patch-M.MPQ (241) |
| DBFilesClient\PvpDifficulty.dbc | enUS/patch-enUS-2.MPQ (2613), patch-M.MPQ (15812) |
| DBFilesClient\Quest.dbc | patch-M.MPQ (2153096) |
| DBFilesClient\QuestFactionReward.dbc | enUS/patch-enUS-2.MPQ (109), patch-M.MPQ (108) |
| DBFilesClient\QuestInfo.dbc | enUS/locale-enUS.MPQ (733), enUS/patch-enUS.MPQ (897), patch-M.MPQ (897) |
| DBFilesClient\QuestSort.dbc | enUS/locale-enUS.MPQ (3195), enUS/patch-enUS-2.MPQ (3440), enUS/patch-enUS.MPQ (3349), patch-M.MPQ (13386) |
| DBFilesClient\QuestSuperTrack.dbc | patch-M.MPQ (4597772) |
| DBFilesClient\QuestTemplateScaling.dbc | patch-M.MPQ (332120) |
| DBFilesClient\QuestXP.dbc | enUS/patch-enUS-2.MPQ (4421), patch-M.MPQ (4420) |
| DBFilesClient\RandPropPoints.dbc | enUS/locale-enUS.MPQ (19221), enUS/patch-enUS.MPQ (19221), patch-M.MPQ (19220) |
| DBFilesClient\Resistances.dbc | enUS/locale-enUS.MPQ (627), patch-M.MPQ (627) |
| DBFilesClient\ScalingStatDistribution.dbc | enUS/locale-enUS.MPQ (1517), enUS/patch-enUS-2.MPQ (5037), enUS/patch-enUS-3.MPQ (5037), enUS/patch-enUS.MPQ (4949), patch-M.MPQ (14364) |
| DBFilesClient\ScalingStatValues.dbc | enUS/locale-enUS.MPQ (6421), enUS/patch-enUS-2.MPQ (7701), enUS/patch-enUS.MPQ (7701), patch-M.MPQ (7700) |
| DBFilesClient\ScreenEffect.dbc | enUS/locale-enUS.MPQ (1300), enUS/patch-enUS-2.MPQ (2341), enUS/patch-enUS-3.MPQ (2746), enUS/patch-enUS.MPQ (2191), patch-M.MPQ (3468) |
| DBFilesClient\ScreenLocations.dbc | patch-M.MPQ (266) |
| DBFilesClient\SealedCardCosts.dbc | patch-M.MPQ (10120) |
| DBFilesClient\SeasonalAppearances.dbc | patch-M.MPQ (10380) |
| DBFilesClient\ServerMessages.dbc | enUS/locale-enUS.MPQ (486), enUS/patch-enUS.MPQ (912), patch-M.MPQ (921) |
| DBFilesClient\SheatheSoundLookups.dbc | enUS/locale-enUS.MPQ (945), patch-M.MPQ (944) |
| DBFilesClient\SkillCard.dbc | patch-M.MPQ (373549) |
| DBFilesClient\SkillCostsData.dbc | enUS/locale-enUS.MPQ (30021), patch-M.MPQ (30020) |
| DBFilesClient\SkillLine.dbc | enUS/locale-enUS.MPQ (38507), enUS/patch-enUS-3.MPQ (38771), enUS/patch-enUS.MPQ (38771), patch-M.MPQ (210211) |
| DBFilesClient\SkillLineAbility.dbc | area-52/patch-D.MPQ (2158596), enUS/locale-enUS.MPQ (519365), enUS/patch-enUS-2.MPQ (570661), enUS/patch-enUS-3.MPQ (572285), enUS/patch-enUS.MPQ (568365), patch-M.MPQ (2295964) |
| DBFilesClient\SkillLineCategory.dbc | enUS/locale-enUS.MPQ (740), patch-M.MPQ (826) |
| DBFilesClient\SkillRaceClassInfo.dbc | enUS/locale-enUS.MPQ (7733), enUS/patch-enUS-2.MPQ (7733), enUS/patch-enUS.MPQ (7765), patch-M.MPQ (7380) |
| DBFilesClient\SkillRaceClassInfo_335.dbc | patch-M.MPQ (20) |
| DBFilesClient\SkillTiers.dbc | enUS/locale-enUS.MPQ (3453), patch-M.MPQ (3452) |
| DBFilesClient\SoundAmbience.dbc | enUS/locale-enUS.MPQ (2265), enUS/patch-enUS-2.MPQ (2469), enUS/patch-enUS-3.MPQ (2493), enUS/patch-enUS.MPQ (2409), patch-M.MPQ (2924) |
| DBFilesClient\SoundEmitters.dbc | enUS/locale-enUS.MPQ (3093), enUS/patch-enUS-2.MPQ (26223), enUS/patch-enUS-3.MPQ (26779), enUS/patch-enUS.MPQ (23138), patch-M.MPQ (26778) |
| DBFilesClient\SoundEntries.dbc | enUS/locale-enUS.MPQ (1883226), enUS/patch-enUS-2.MPQ (2346457), enUS/patch-enUS-3.MPQ (2405359), enUS/patch-enUS.MPQ (2177107), patch-M.MPQ (10040754) |
| DBFilesClient\SoundEntriesAdvanced.dbc | enUS/locale-enUS.MPQ (4443), enUS/patch-enUS-2.MPQ (167072), enUS/patch-enUS-3.MPQ (192415), enUS/patch-enUS.MPQ (95160), patch-M.MPQ (1840913) |
| DBFilesClient\SoundFilter.dbc | enUS/locale-enUS.MPQ (699), patch-M.MPQ (698) |
| DBFilesClient\SoundFilterElem.dbc | enUS/locale-enUS.MPQ (2673), patch-M.MPQ (2672) |
| DBFilesClient\SoundProviderPreferences.dbc | enUS/locale-enUS.MPQ (3593), patch-M.MPQ (3592) |
| DBFilesClient\SoundSamplePreferences.dbc | enUS/locale-enUS.MPQ (157), patch-M.MPQ (156) |
| DBFilesClient\SoundWaterType.dbc | enUS/locale-enUS.MPQ (213), patch-M.MPQ (212) |
| DBFilesClient\SpamMessages.dbc | enUS/locale-enUS.MPQ (15123), enUS/patch-enUS-2.MPQ (15004), enUS/patch-enUS-3.MPQ (14950), enUS/patch-enUS.MPQ (15041) |
| DBFilesClient\Spell.dbc | area-52/patch-D.MPQ (237835846), enUS/locale-enUS.MPQ (35452130), enUS/patch-enUS-2.MPQ (49531245), enUS/patch-enUS-3.MPQ (48967121), enUS/patch-enUS.MPQ (46699377), patch-T.MPQ (209501336) |
| DBFilesClient\SpellActivationOverlays.dbc | patch-S.MPQ (80598) |
| DBFilesClient\SpellAddon.dbc | patch-S.MPQ (517888) |
| DBFilesClient\SpellAffect.dbc | patch-S.MPQ (442208) |
| DBFilesClient\SpellAlternativeCost.dbc | patch-S.MPQ (20) |
| DBFilesClient\SpellAlternativePowerType.dbc | patch-S.MPQ (387) |
| DBFilesClient\SpellCastTimes.dbc | enUS/locale-enUS.MPQ (1061), enUS/patch-enUS-2.MPQ (1141), enUS/patch-enUS.MPQ (1125), patch-S.MPQ (1156) |
| DBFilesClient\SpellCategory.dbc | enUS/locale-enUS.MPQ (1829), enUS/patch-enUS-2.MPQ (2117), enUS/patch-enUS.MPQ (2101), patch-S.MPQ (41556) |
| DBFilesClient\SpellChainEffects.dbc | enUS/locale-enUS.MPQ (111470), enUS/patch-enUS-2.MPQ (171075), enUS/patch-enUS-3.MPQ (174517), enUS/patch-enUS.MPQ (152234), patch-S.MPQ (1200886) |
| DBFilesClient\SpellCharges.dbc | area-52/patch-D.MPQ (3804), patch-S.MPQ (3340) |
| DBFilesClient\SpellChargesCategory.dbc | area-52/patch-D.MPQ (1316), patch-S.MPQ (1328) |
| DBFilesClient\SpellCustomAttr.dbc | patch-S.MPQ (2580708) |
| DBFilesClient\SpellDescriptionVariables.dbc | enUS/patch-enUS-2.MPQ (2273), enUS/patch-enUS-3.MPQ (2787), enUS/patch-enUS.MPQ (123), patch-S.MPQ (2879) |
| DBFilesClient\SpellDifficulty.dbc | enUS/patch-enUS-2.MPQ (10721), enUS/patch-enUS-3.MPQ (11641), patch-S.MPQ (76240) |
| DBFilesClient\SpellDispelType.dbc | enUS/locale-enUS.MPQ (1146), patch-S.MPQ (1146) |
| DBFilesClient\SpellDuration.dbc | enUS/locale-enUS.MPQ (1813), enUS/patch-enUS-2.MPQ (2053), enUS/patch-enUS-3.MPQ (2101), enUS/patch-enUS.MPQ (2021), patch-S.MPQ (13876) |
| DBFilesClient\SpellEffectCameraShakes.dbc | enUS/locale-enUS.MPQ (437), enUS/patch-enUS-2.MPQ (469), enUS/patch-enUS-3.MPQ (501), enUS/patch-enUS.MPQ (453), patch-S.MPQ (612) |
| DBFilesClient\SpellEnchantSuggestions.dbc | patch-S.MPQ (18317828) |
| DBFilesClient\SpellFocusObject.dbc | enUS/locale-enUS.MPQ (33312), enUS/patch-enUS-2.MPQ (35886), enUS/patch-enUS-3.MPQ (35886), enUS/patch-enUS.MPQ (35543), patch-S.MPQ (40647) |
| DBFilesClient\SpellIcon.dbc | enUS/locale-enUS.MPQ (104116), enUS/patch-enUS-2.MPQ (150305), enUS/patch-enUS-3.MPQ (151540), enUS/patch-enUS.MPQ (145999), patch-S.MPQ (787311) |
| DBFilesClient\SpellItemEnchantment.dbc | enUS/locale-enUS.MPQ (417898), enUS/patch-enUS-2.MPQ (451203), enUS/patch-enUS-3.MPQ (451214), enUS/patch-enUS.MPQ (450700), patch-S.MPQ (3056457) |
| DBFilesClient\SpellItemEnchantmentCondition.dbc | enUS/locale-enUS.MPQ (2517), enUS/patch-enUS.MPQ (3157), patch-S.MPQ (3156) |
| DBFilesClient\SpellMechanic.dbc | enUS/locale-enUS.MPQ (2523), enUS/patch-enUS.MPQ (2521), patch-S.MPQ (2521) |
| DBFilesClient\SpellMissile.dbc | enUS/locale-enUS.MPQ (2597), enUS/patch-enUS-2.MPQ (6081), enUS/patch-enUS-3.MPQ (6321), enUS/patch-enUS.MPQ (5721), patch-S.MPQ (10220) |
| DBFilesClient\SpellMissileMotion.dbc | enUS/locale-enUS.MPQ (39549), enUS/patch-enUS-2.MPQ (59178), enUS/patch-enUS-3.MPQ (61609), enUS/patch-enUS.MPQ (50076), patch-S.MPQ (792889) |
| DBFilesClient\SpellRadius.dbc | enUS/locale-enUS.MPQ (773), enUS/patch-enUS.MPQ (949), patch-S.MPQ (5108) |
| DBFilesClient\SpellRange.dbc | enUS/locale-enUS.MPQ (9306), enUS/patch-enUS-2.MPQ (11474), enUS/patch-enUS-3.MPQ (11480), enUS/patch-enUS.MPQ (11114), patch-S.MPQ (55407) |
| DBFilesClient\SpellRank.dbc | area-52/patch-D.MPQ (313636), patch-S.MPQ (371668) |
| DBFilesClient\SpellRoleSuggestions.dbc | patch-S.MPQ (20) |
| DBFilesClient\SpellRuneCost.dbc | enUS/locale-enUS.MPQ (6001), enUS/patch-enUS-2.MPQ (9501), enUS/patch-enUS-3.MPQ (9521), enUS/patch-enUS.MPQ (7781), patch-S.MPQ (9540) |
| DBFilesClient\SpellShapeshiftForm.dbc | enUS/locale-enUS.MPQ (4834), enUS/patch-enUS-2.MPQ (4890), enUS/patch-enUS-3.MPQ (4890), enUS/patch-enUS.MPQ (4890), patch-S.MPQ (9394) |
| DBFilesClient\SpellSpellSuggestions.dbc | patch-S.MPQ (5651108) |
| DBFilesClient\SpellStatSuggestions.dbc | patch-S.MPQ (17956) |
| DBFilesClient\SpellTags.dbc | patch-S.MPQ (5863964) |
| DBFilesClient\SpellTagTypes.dbc | patch-S.MPQ (59216) |
| DBFilesClient\SpellVisual.dbc | enUS/locale-enUS.MPQ (689221), enUS/patch-enUS-2.MPQ (1180437), enUS/patch-enUS-3.MPQ (1203989), enUS/patch-enUS.MPQ (1096981), patch-S.MPQ (2962964) |
| DBFilesClient\SpellVisualEffectName.dbc | enUS/locale-enUS.MPQ (184536), enUS/patch-enUS-2.MPQ (254375), enUS/patch-enUS-3.MPQ (259675), enUS/patch-enUS.MPQ (232040), patch-S.MPQ (1298801) |
| DBFilesClient\SpellVisualKit.dbc | enUS/locale-enUS.MPQ (966441), enUS/patch-enUS-2.MPQ (1289589), enUS/patch-enUS-3.MPQ (1316797), enUS/patch-enUS.MPQ (1187445), patch-S.MPQ (4496788) |
| DBFilesClient\SpellVisualKitAreaModel.dbc | enUS/patch-enUS-2.MPQ (696), enUS/patch-enUS.MPQ (571), patch-S.MPQ (780) |
| DBFilesClient\SpellVisualKitModelAttach.dbc | enUS/patch-enUS-2.MPQ (20061), enUS/patch-enUS-3.MPQ (23661), enUS/patch-enUS.MPQ (3981), patch-S.MPQ (316180) |
| DBFilesClient\SpellVisualPrecastTransitions.dbc | enUS/locale-enUS.MPQ (115), patch-S.MPQ (114) |
| DBFilesClient\StableSlotPrices.dbc | enUS/locale-enUS.MPQ (53), patch-M.MPQ (52) |
| DBFilesClient\Startup_Strings.dbc | enUS/locale-enUS.MPQ (1574), patch-M.MPQ (1534) |
| DBFilesClient\Stationery.dbc | enUS/locale-enUS.MPQ (193), enUS/patch-enUS.MPQ (224) |
| DBFilesClient\StringLookups.dbc | enUS/locale-enUS.MPQ (384) |
| DBFilesClient\SummonProperties.dbc | enUS/locale-enUS.MPQ (2541), enUS/patch-enUS-2.MPQ (4701), enUS/patch-enUS-3.MPQ (4821), enUS/patch-enUS.MPQ (4317), patch-M.MPQ (5228) |
| DBFilesClient\SuperTrack.dbc | patch-M.MPQ (312436) |
| DBFilesClient\Talent.dbc | area-52/patch-D.MPQ (217876), enUS/locale-enUS.MPQ (80245), enUS/patch-enUS-2.MPQ (82085), enUS/patch-enUS.MPQ (82085), patch-M.MPQ (219348) |
| DBFilesClient\TalentTab.dbc | enUS/locale-enUS.MPQ (3972), enUS/patch-enUS.MPQ (3972), patch-M.MPQ (4458) |
| DBFilesClient\TaxiNodes.dbc | enUS/locale-enUS.MPQ (42015), enUS/patch-enUS-2.MPQ (45245), enUS/patch-enUS-3.MPQ (46504), enUS/patch-enUS.MPQ (44041), patch-M.MPQ (51127) |
| DBFilesClient\TaxiPath.dbc | enUS/locale-enUS.MPQ (12917), enUS/patch-enUS-2.MPQ (14581), enUS/patch-enUS-3.MPQ (14661), enUS/patch-enUS.MPQ (14197), patch-M.MPQ (15172) |
| DBFilesClient\TaxiPathNode.dbc | enUS/locale-enUS.MPQ (859297), enUS/patch-enUS-2.MPQ (986809), enUS/patch-enUS-3.MPQ (993805), enUS/patch-enUS.MPQ (962785), patch-M.MPQ (1025836) |
| DBFilesClient\TaxiTimes.dbc | patch-M.MPQ (36) |
| DBFilesClient\TeamContributionPoints.dbc | enUS/patch-enUS-2.MPQ (11221), enUS/patch-enUS-3.MPQ (11221), patch-M.MPQ (11220) |
| DBFilesClient\TerrainType.dbc | enUS/locale-enUS.MPQ (355), enUS/patch-enUS.MPQ (385) |
| DBFilesClient\TerrainTypeSounds.dbc | enUS/locale-enUS.MPQ (57), enUS/patch-enUS.MPQ (61) |
| DBFilesClient\TimedDungeons.dbc | patch-M.MPQ (1964) |
| DBFilesClient\TokenTypes.dbc | patch-M.MPQ (35271) |
| DBFilesClient\TotemCategory.dbc | enUS/locale-enUS.MPQ (2912), enUS/patch-enUS-3.MPQ (3128), enUS/patch-enUS.MPQ (3115), patch-M.MPQ (3962) |
| DBFilesClient\TransportAnimation.dbc | enUS/locale-enUS.MPQ (129885), enUS/patch-enUS-2.MPQ (149037), enUS/patch-enUS-3.MPQ (147357), enUS/patch-enUS.MPQ (147049), patch-M.MPQ (151836) |
| DBFilesClient\TransportPhysics.dbc | enUS/locale-enUS.MPQ (153), enUS/patch-enUS.MPQ (153), patch-M.MPQ (328) |
| DBFilesClient\TransportRotation.dbc | enUS/patch-enUS-2.MPQ (6377), enUS/patch-enUS-3.MPQ (6377), enUS/patch-enUS.MPQ (6377), patch-M.MPQ (6376) |
| DBFilesClient\Tutorial.dbc | patch-M.MPQ (287518) |
| DBFilesClient\TutorialCategories.dbc | patch-M.MPQ (261) |
| DBFilesClient\TutorialKeywords.dbc | patch-M.MPQ (355484) |
| DBFilesClient\TutorialObjectives.dbc | patch-M.MPQ (39037) |
| DBFilesClient\TutorialObjectiveTypes.dbc | patch-M.MPQ (199) |
| DBFilesClient\TutorialRewards.dbc | patch-M.MPQ (9476) |
| DBFilesClient\UICameraAppearanceChrRaces.dbc | patch-M.MPQ (4820) |
| DBFilesClient\UICameraAppearanceWeapons.dbc | patch-M.MPQ (740) |
| DBFilesClient\UICameras.dbc | patch-M.MPQ (10860) |
| DBFilesClient\UISoundLookups.dbc | enUS/locale-enUS.MPQ (3928), patch-M.MPQ (3959) |
| DBFilesClient\UnitBlood.dbc | enUS/locale-enUS.MPQ (641), patch-M.MPQ (1611) |
| DBFilesClient\UnitBloodLevels.dbc | enUS/locale-enUS.MPQ (85), patch-M.MPQ (356) |
| DBFilesClient\VanityCollection.dbc | patch-M.MPQ (3612410) |
| DBFilesClient\Vehicle.dbc | enUS/locale-enUS.MPQ (33956), enUS/patch-enUS-2.MPQ (64731), enUS/patch-enUS-3.MPQ (66651), enUS/patch-enUS.MPQ (59291), patch-M.MPQ (98896) |
| DBFilesClient\VehicleSeat.dbc | enUS/locale-enUS.MPQ (54561), enUS/patch-enUS-2.MPQ (163349), enUS/patch-enUS-3.MPQ (167061), enUS/patch-enUS.MPQ (154301), patch-M.MPQ (238980) |
| DBFilesClient\VehicleUIIndicator.dbc | enUS/patch-enUS-2.MPQ (359), enUS/patch-enUS-3.MPQ (419), patch-M.MPQ (1202) |
| DBFilesClient\VehicleUIIndSeat.dbc | enUS/patch-enUS-2.MPQ (381), enUS/patch-enUS-3.MPQ (401), patch-M.MPQ (720) |
| DBFilesClient\VideoHardware.dbc | enUS/locale-enUS.MPQ (17893) |
| DBFilesClient\VocalUISounds.dbc | enUS/locale-enUS.MPQ (17969), patch-M.MPQ (17968) |
| DBFilesClient\WeaponImpactSounds.dbc | enUS/locale-enUS.MPQ (2781) |
| DBFilesClient\WeaponSwingSounds2.dbc | enUS/locale-enUS.MPQ (117) |
| DBFilesClient\Weather.dbc | enUS/locale-enUS.MPQ (870), enUS/patch-enUS-2.MPQ (1201), enUS/patch-enUS-3.MPQ (1201), enUS/patch-enUS.MPQ (870), patch-M.MPQ (1292) |
| DBFilesClient\WMOAreaTable.dbc | enUS/locale-enUS.MPQ (2413737), enUS/patch-enUS-2.MPQ (2539943), enUS/patch-enUS-3.MPQ (2540951), enUS/patch-enUS.MPQ (2503610), patch-M.MPQ (2727847) |
| DBFilesClient\WorldChunkSounds.dbc | enUS/locale-enUS.MPQ (21), patch-M.MPQ (20) |
| DBFilesClient\WorldMapArea.dbc | enUS/locale-enUS.MPQ (4370), enUS/patch-enUS-2.MPQ (6016), enUS/patch-enUS-3.MPQ (6075), enUS/patch-enUS.MPQ (5779), patch-M.MPQ (17978) |
| DBFilesClient\WorldMapContinent.dbc | enUS/locale-enUS.MPQ (245), patch-M.MPQ (244) |
| DBFilesClient\WorldMapOverlay.dbc | enUS/locale-enUS.MPQ (77730), enUS/patch-enUS.MPQ (79710), patch-M.MPQ (86272) |
| DBFilesClient\WorldMapTransforms.dbc | enUS/locale-enUS.MPQ (93), enUS/patch-enUS-2.MPQ (381), enUS/patch-enUS-3.MPQ (381), enUS/patch-enUS.MPQ (341), patch-M.MPQ (500) |
| DBFilesClient\WorldSafeLocs.dbc | enUS/locale-enUS.MPQ (71710), enUS/patch-enUS-2.MPQ (81128), enUS/patch-enUS-3.MPQ (81245), enUS/patch-enUS.MPQ (80664), patch-M.MPQ (107039) |
| DBFilesClient\WorldStateUI.dbc | enUS/locale-enUS.MPQ (25624), enUS/patch-enUS-2.MPQ (38728), enUS/patch-enUS-3.MPQ (46474), enUS/patch-enUS.MPQ (36778), patch-M.MPQ (77612) |
| DBFilesClient\WorldStateZoneSounds.dbc | enUS/locale-enUS.MPQ (981), enUS/patch-enUS-2.MPQ (1365), enUS/patch-enUS.MPQ (1141), patch-M.MPQ (1364) |
| DBFilesClient\WowError_Strings.dbc | enUS/locale-enUS.MPQ (1474) |
| DBFilesClient\ZoneIntroMusicTable.dbc | enUS/locale-enUS.MPQ (4116), enUS/patch-enUS-2.MPQ (6500), enUS/patch-enUS.MPQ (5820), patch-M.MPQ (6962) |
| DBFilesClient\ZoneLight.dbc | patch-M.MPQ (961) |
| DBFilesClient\ZoneLightPoint.dbc | patch-M.MPQ (9240) |
| DBFilesClient\ZoneMusic.dbc | enUS/locale-enUS.MPQ (12356), enUS/patch-enUS-2.MPQ (19864), enUS/patch-enUS-3.MPQ (20214), enUS/patch-enUS.MPQ (18535), patch-M.MPQ (27245) |
| DBFilesClient\ZoneStory.dbc | patch-M.MPQ (636) |
