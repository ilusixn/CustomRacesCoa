# Framework wiring reference

These excerpts are not complete replacement files. The existing native custom-race/appearance/equipment framework is required.

## WoW/Common/ComponentCore/EsteriaAppearance.cpp

Merge the race-specific branches into your equivalent shared framework; numbered excerpts are reference only.

```cpp
101:         fields=NormalElfAppearance::ApplyEyePalette(p.m_raceID,p.m_sexID,fields,p.m_classId);
102:     }
103:     if(p.m_raceID==60) fields=CreatureAppearance::DecodeKyrian(p.m_sexID,fields);
104:     return fields;
105: }
106: void SetHighmountainFields(CCharacterComponent* c,Fields const& input) {
107:     auto& p=c->m_data.m_preferences;
108:     Fields fields=p.m_raceID==60 ? CreatureAppearance::EncodeKyrian(p.m_sexID,input) : input;
109:     p.m_skinID=fields[0]; p.m_faceId=fields[1]; p.m_hairStyleId=fields[2];
110:     p.m_hairColorId=fields[3]; p.m_facialHairId=fields[4];
166:             // Renderer fields are logical; validate the corresponding wire layout.
167:             if (race == 60)
168:                 appearance = CreatureAppearance::EncodeKyrian(gender, appearance);
169:             return std::all_of(fields.begin() + 5, fields.end(), [](std::uint8_t value) { return value == 0; })
170:                 && CreatureAppearance::Validate(race, gender, appearance);
472:         static auto const tarasek = ReadSelections(L"EsteriaTarasek.bin");
473:         static auto const saberon = ReadSelections(L"EsteriaSaberon.bin");
474:         static auto const kyrian = ReadSelections(L"Data\\Retail\\EsteriaKyrian.bin");
475:         static auto const venthyr = ReadSelections(L"Data\\Retail\\EsteriaVenthyr.bin");
476:         static auto const naga = ReadSelections(L"EsteriaNaga.bin");
477:         static auto const tuskarr = ReadSelections(L"EsteriaTuskarr.bin");
511:             case 70: return drakonid;
512:             case 72: return tarasek;
513:             case 60: return kyrian;
514:             case 61: return venthyr;
515:             case 64: return saberon;
516:             case 54: return naga;
562:             case 70: return L"EsteriaDrakonidTextures.bin";
563:             case 72: return L"EsteriaTarasekTextures.bin";
564:             case 60: return L"Data\\Retail\\EsteriaKyrianTextures.bin";
565:             case 61: return L"Data\\Retail\\EsteriaVenthyrTextures.bin";
566:             case 64: return L"EsteriaSaberonTextures.bin";
567:             case 54: return L"EsteriaNagaTextures.bin";
613:             case 70: return "Drakonid";
614:             case 72: return "Tarasek";
615:             case 60: return "Kyrian";
616:             case 61: return "Venthyr";
617:             case 64: return "Saberon";
618:             case 54: return "Naga";
1761:         if (race == 60) {
1762:             static const std::array<HighmountainAppearance::Requirement, 0> none{};
1763:             if (gender) return HighmountainControl(state, command, character, CreatureAppearance::KyrianFemaleControls, CreatureAppearance::KyrianFemaleLabels, none);
1764:             return HighmountainControl(state, command, character, CreatureAppearance::KyrianMaleControls, CreatureAppearance::KyrianMaleLabels, none);
1765:         }
1766:         if (race == 61) {
1767:             static const std::array<HighmountainAppearance::Requirement, 0> none{};
1768:             if (gender) return HighmountainControl(state, command, character, CreatureAppearance::VenthyrFemaleControls, CreatureAppearance::VenthyrFemaleLabels, none);
1769:             return HighmountainControl(state, command, character, CreatureAppearance::VenthyrMaleControls, CreatureAppearance::VenthyrMaleLabels, none);
1770:         }
1771:         if (race == 63) {
2070:                 if (p.m_sexID) hideGroup(32);
2071:                 hideGroup(34);
2072:                 // Venthyr outfit meshes include the entire body. Use its basic
2073:                 // body instead of hiding the body along with the built-in coat.
2074:                 hideGroup(31);
2118:         else {if(g) callback(OrcAppearanceV2::MagharFemaleOptions);else callback(OrcAppearanceV2::MagharMaleOptions);}return true;
2119:     }
2120:     if(r==60){callback(g?CreatureAppearance::KyrianFemaleControls:CreatureAppearance::KyrianMaleControls);return true;}
2121:     if(r==61){callback(g?CreatureAppearance::VenthyrFemaleControls:CreatureAppearance::VenthyrMaleControls);return true;}
2122:     if(r==63){callback(CreatureAppearance::GnollControls);return true;}
2123:     if(r==69||r==70){callback(CreatureAppearance::DrakonidControls);return true;}
```

## WoW/Common/ComponentCore/CharacterComponent.cpp

Merge the race-specific branches into your equivalent shared framework; numbered excerpts are reference only.

```cpp
3972:     displayRec = &rec;
3973:     const auto race = this->m_data.m_preferences.m_raceID;
3974:     // Native Kyrian/Venthyr bodies use fitted Human equipment donors.
3975:     ChrRacesRec* raceRec = g_chrRacesDB.GetRecord((race == 60 || race == 61) ? 1 : race);
3976: 
5748: 
5749:     ChrRacesRec* raceRec = g_chrRacesDB.GetRecord(this->m_data.m_preferences.m_raceID);
5750:     // Native Kyrian and Venthyr have feet that accept player boot textures.
5751:     // Their inherited barefoot race flag would otherwise discard the foot layer.
5752:     bool nativeBoots = this->m_data.m_preferences.m_raceID == 60 || this->m_data.m_preferences.m_raceID == 61;
```

## WoW/Common/ComponentCore/RetailRace.cpp

Merge the race-specific branches into your equivalent shared framework; numbered excerpts are reference only.

```cpp
1126: float RetailRacePreviewScale(uint32_t race, uint32_t sex) {
1127:     // These authored models use larger world units than the glue pedestal.
1128:     // Venthyr need extra pedestal framing; world and creation retain their full height.
1129:     if (race == 61) return Retail::NativeHumanoidScale(race, sex) * .80f;
1130:     if (Retail::NativeHumanoidRace(race)) return Retail::NativeHumanoidScale(race, sex);
```

## Engine/Source/Retail/RacePolicy.h

Merge the race-specific branches into your equivalent shared framework; numbered excerpts are reference only.

```cpp
13: }
14: 
15: // Kyrian match same-sex Humans; Venthyr stand just 1% above same-sex Night Elves. Donor
16: // bounds include wings/headdresses and cannot be used as a player's body height.
17: inline constexpr bool NativeHumanoidRace(uint32_t race) { return race == 60 || race == 61; }
```
