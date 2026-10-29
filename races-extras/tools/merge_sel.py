import re, sys
sys.path.insert(0, r'C:\CoA-Build\kv')
from merge_util import Merger
STR = open(r'C:\CoA-Build\sources\esteria\wowdecomp\WoW\Source\Glue\CharacterSelection.cpp', 'rb').read().decode('utf-8').replace('\r\n', '\n')
a = STR.index('// Equips a character select model with the character')
b = STR.index('\n}\n', a) + 3
dress = STR[a:b]
m = Merger(r'C:\CoA-Build\coa64\WoW\Source\Glue\CharacterSelection.cpp')
# 1. the inline loop -> call
start = m.s.index('        for (uint32_t slot = 0; slot < 23; slot++) {\n            int32_t displayID = CCharacterSelection::s_characterList')
end = m.s.index('        if (CCharacterSelection::s_characterList[CCharacterSelection::s_selectionIndex].m_characterInfo.m_flags & 0x2000) {', start)
m.s = m.s[:start] + '        CCharacterSelection::DressComponent(CCharacterSelection::s_characterList[CCharacterSelection::s_selectionIndex].m_characterInfo, CCharacterSelection::s_characterList[CCharacterSelection::s_selectionIndex].m_component);\n\n' + m.s[end:]
# 2. the function, before EnumerateCharactersCallback
m.replace('void CCharacterSelection::EnumerateCharactersCallback(CHARACTER_INFO& info, void* param) {', 'XXMARK')
m.s = m.s.replace('XXMARK', dress + '\nvoid CCharacterSelection::EnumerateCharactersCallback(CHARACTER_INFO& info, void* param) {', 1)
# VERIFY/OFFSET comment lines stay above EnumerateCharactersCallback: move them
m.replace('// VERIFY: ', '// VERIFY: ', optional=True)
# 3. retail model path + includes
m.replace('#include "CharacterSelection.h"\n', '#include "CharacterSelection.h"\n#include "WoW/Common/ComponentCore/RetailRace.h"\n')
m.replace('        data.m_model = CCharacterSelection::m_modelFrame->GetScene()->CreateModel(playerRec->m_modelName, 0);\n        data.m_flags |= 0x2;\n',
          '        char retailPath[260];\n        const bool retailModel = RetailRaceModelPath(info.m_raceID, info.m_sexID, retailPath, sizeof(retailPath), RetailRaceLookForm(info.m_guid.m_guid));\n        data.m_model = CCharacterSelection::m_modelFrame->GetScene()->CreateModel(retailModel ? retailPath : playerRec->m_modelName, 0);\n        data.m_flags |= 0x2;\n')
m.save()
h = Merger(r'C:\CoA-Build\coa64\WoW\Source\Glue\CharacterSelection.h')
h.replace('    static void EnumerateCharactersCallback(', '    static void DressComponent(const CHARACTER_INFO& info, CCharacterComponent* component);\n    static void EnumerateCharactersCallback(', optional=True)
h.save()
