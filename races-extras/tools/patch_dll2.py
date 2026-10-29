import shutil
C = r'C:\CoA-Build\esteria\dll2\src\client-customization\NativeAppearance.cpp'
shutil.copy2(C, C + '.bak_kv2')
s = open(C, encoding='utf-8', newline='').read()
nl = '\r\n' if '\r\n' in s else '\n'
old = '        // Some source-hidden groups have no customization selector. Apply their hide after all selections.'.replace('\n', nl)
assert s.count(old) == 1
new = '''        // Kyrian / Venthyr: the player robe (1302) and the raw garment atlas (texture type 15) belong to the equipment
        // fitting of the original client, which we do not have. Keep the robe hidden and give type 15 the empty garment.
        if (RaceOf(Field<unsigned>(character, 0x18)) == 60 || RaceOf(Field<unsigned>(character, 0x18)) == 61)
        {
            Native<void(__thiscall*)(void*, unsigned, unsigned, int)>(0x0082C7C0)(instance, 1302, 1302, 0);
            void* empty = Native<void*(__cdecl*)(char const*, void*)>(0x004E8D30)(
                "custom\\\\equipment\\\\native\\\\empty-garment.blp", reinterpret_cast<void*>(Address(0x00AC46D0)));
            if (empty)
            {
                Native<void(__thiscall*)(void*, unsigned, void*)>(0x00825260)(instance, 15, empty);
                Native<void(__cdecl*)(void*)>(0x0047BF30)(empty);
            }
        }
'''.replace('\n', nl)
s = s.replace(old, new + old)
open(C, 'w', encoding='utf-8', newline='').write(s)
print('ok')
