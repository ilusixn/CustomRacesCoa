import re, shutil, sys

SRC = r'C:\CoA-Build\esteria\dll2\src'
H = SRC + r'\src\server\shared\CreatureAppearance.h'
C = SRC + r'\client-customization\NativeAppearance.cpp'
PKG = r'C:\Users\ilusi\Downloads\Kyrian-Venthyr-Approved-20261021\Kyrian-Venthyr-Approved-20261021\source-reference\client\WoW\Common\ComponentCore\CreatureAppearance.h'
for f in (H, C):
    shutil.copy2(f, f + '.bak_kv')


def edit(path, pairs):
    s = open(path, encoding='utf-8', newline='').read()
    nl = '\r\n' if '\r\n' in s else '\n'
    for old, new, count in pairs:
        old = old.replace('\n', nl)
        new = new.replace('\n', nl)
        assert s.count(old) == count, (path, old[:60], s.count(old))
        s = s.replace(old, new)
    open(path, 'w', encoding='utf-8', newline='').write(s)


# arrays taken from the package header, unchanged
pkg = open(PKG, encoding='utf-8', newline='').read().replace('\r\n', '\n')
a = pkg.index('    inline constexpr std::array<Option, 5> KyrianMale')
b = pkg.index('    constexpr bool Uses(unsigned race)')
arrays = pkg[a:b]

edit(H, [
    ('    constexpr bool Uses(unsigned race)\n', arrays + '    constexpr bool Uses(unsigned race)\n', 1),
    ('return (race >= 54 && race <= 59) || race == 62', 'return (race >= 54 && race <= 61) || race == 62', 1),
    ('|| ((race == 54 || race == 56 || race == 57 || race == 62 || race == 66) && gender == 1));',
     '|| ((race == 60 || race == 61 || race == 54 || race == 56 || race == 57 || race == 62 || race == 66) && gender == 1));', 1),
    ('            case 62:\n                return gender ? SethrakFemale : SethrakMale;\n',
     '            case 60:\n                return gender ? KyrianFemale : KyrianMale;\n            case 61:\n                return gender ? VenthyrFemale : VenthyrMale;\n            case 62:\n                return gender ? SethrakFemale : SethrakMale;\n', 1),
])

edit(C, [
    ('        case 21: return 54; // Naga',
     '        case 101: return 61; // Venthyr (Horde; id 101 so the 32-bit race masks wrap to Undead)\n        case 107: return 60; // Kyrian (Alliance; id 107 so the 32-bit race masks wrap to Draenei)\n        case 21: return 54; // Naga', 1),
    ('        case 60: return 2;\n        case 61: return 1;\n', '        case 60: return 11; // Kyrian: Draenei skill lines\n        case 61: return 5; // Venthyr: Undead skill lines\n', 1),
    ('        if (CreatureAppearance::Uses(RaceOf(Field<unsigned>(character, 0x18))))\n            return false;\n        if (RaceOf(Field<unsigned>(character, 0x18)) == 20)',
     '''        if (RaceOf(Field<unsigned>(character, 0x18)) == 60 || RaceOf(Field<unsigned>(character, 0x18)) == 61)
        {
            constexpr std::array<VulperaAppearance::Requirement, 0> requirements{};
            bool const female = Field<unsigned>(character, 0x1C) != 0;
            if (RaceOf(Field<unsigned>(character, 0x18)) == 60)
                return female ? HighmountainControl(state, command, character, CreatureAppearance::KyrianFemaleControls,
                        CreatureAppearance::KyrianFemaleLabels, requirements)
                    : HighmountainControl(state, command, character, CreatureAppearance::KyrianMaleControls,
                        CreatureAppearance::KyrianMaleLabels, requirements);
            return female ? HighmountainControl(state, command, character, CreatureAppearance::VenthyrFemaleControls,
                    CreatureAppearance::VenthyrFemaleLabels, requirements)
                : HighmountainControl(state, command, character, CreatureAppearance::VenthyrMaleControls,
                    CreatureAppearance::VenthyrMaleLabels, requirements);
        }
        if (CreatureAppearance::Uses(RaceOf(Field<unsigned>(character, 0x18))))
            return false;
        if (RaceOf(Field<unsigned>(character, 0x18)) == 20)''', 1),
    ('        static auto const drakonid = ReadSelections(L"EsteriaDrakonid.bin");\n',
     '        static auto const drakonid = ReadSelections(L"EsteriaDrakonid.bin");\n        static auto const kyrian = ReadSelections(L"EsteriaKyrian.bin");\n        static auto const venthyr = ReadSelections(L"EsteriaVenthyr.bin");\n', 1),
    ('            case 66: return tarasek;\n            case 69:\n            case 70: return drakonid;\n',
     '            case 60: return kyrian;\n            case 61: return venthyr;\n            case 66: return tarasek;\n            case 69:\n            case 70: return drakonid;\n', 1),
    ('            case 66: return L"EsteriaTarasekTextures.bin";\n',
     '            case 60: return L"EsteriaKyrianTextures.bin";\n            case 61: return L"EsteriaVenthyrTextures.bin";\n            case 66: return L"EsteriaTarasekTextures.bin";\n', 1),
    ('            if (race == 63)\n                return ExtendedGeometry(character, CreatureAppearance::GnollControls, RetailSelections(race));\n',
     '''            if (race == 63)
                return ExtendedGeometry(character, CreatureAppearance::GnollControls, RetailSelections(race));
            if (race == 60 || race == 61)
            {
                bool const female = Field<unsigned>(character, 0x1C) != 0;
                if (race == 60)
                    return female ? ExtendedGeometry(character, CreatureAppearance::KyrianFemaleControls, RetailSelections(race))
                        : ExtendedGeometry(character, CreatureAppearance::KyrianMaleControls, RetailSelections(race));
                return female ? ExtendedGeometry(character, CreatureAppearance::VenthyrFemaleControls, RetailSelections(race))
                    : ExtendedGeometry(character, CreatureAppearance::VenthyrMaleControls, RetailSelections(race));
            }
''', 1),
])
print('patched')
