import os
os.chdir(r'C:\CoA-Build\coa64')
BS = chr(92)


def sub(p, a, b):
    s = open(p, encoding='utf-8', newline='').read()
    nl = '\r\n' if '\r\n' in s else '\n'
    a = a.replace('\n', nl)
    b = b.replace('\n', nl)
    assert a in s, (p, a[:50])
    open(p, 'w', encoding='utf-8', newline='').write(s.replace(a, b, 1))


p = 'CoA/Source/Modules/Esteria/EsteriaBody.cpp'
sub(p, '''        std::time_t now = std::time(nullptr);

        if (now != s_lastCheck) {
            s_lastCheck = now;
            Slots current = NonZero();
            Slots& stored = s_saved[key];

            if (current != stored) {
                if (current.empty()) {
                    s_saved.erase(key);
                } else {
                    stored = current;
                }

                Save();
            }
        }
    }
''', '''    }

    // The in-world Body panel closed without Save (or the creator closed): back to the last saved sliders.
    void Revert() {
        const char* name = ClientServices::GetCharacterName();
        Load();
        auto found = name && *name ? s_saved.find(KeyOf(name)) : s_saved.end();
        BodyShape::Local().v = found != s_saved.end() ? found->second : Slots{};
    }
''')
sub(p, '// Called every frame for the local player in the world, before the engine applies BodyShape::Local() to the model.',
    '// Called every frame for the local player in the world, before the engine applies BodyShape::Local() to the model. Sliders are only written by SaveNow.')
sub('CoA/Source/Modules/EsteriaRaces.h', '        void SaveNow();',
    '        void Revert();        // back to the last saved sliders\n        void SaveNow();')
p = 'WoW/Source/Ui/BodyShapeScripts.h'
sub(p, '#define BODY_SHAPE_SCRIPT_ENTRIES', '''// RevertBodyShape(): drop unsaved slider changes of the local player.
static int32_t Script_RevertBodyShape(lua_State* L) {
    CoA::Esteria::BodySliders::Revert();
    return 0;
}

#define BODY_SHAPE_SCRIPT_ENTRIES''')
sub(p, '    { "SaveBodyShape", &Script_SaveBodyShape }, ' + BS,
    '    { "SaveBodyShape", &Script_SaveBodyShape }, ' + BS + '\n    { "RevertBodyShape", &Script_RevertBodyShape }, ' + BS)
print('ok')
