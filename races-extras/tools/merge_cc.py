"""Hand merge of the retail hooks into CoA64's CharacterComponent.cpp (the replace-type blocks port_hunks.py will not touch)."""
import sys
sys.path.insert(0, r'C:\CoA-Build\kv')
from merge_util import Merger
m = Merger(r'C:\CoA-Build\coa64\WoW\Common\ComponentCore\CharacterComponent.cpp')
m.replace('#include "CoA/Source/Modules/ItemCollections.h"\n',
          '#include "WoW/Common/ComponentCore/RetailRace.h"\n#include "Engine/Source/Retail/RacePolicy.h"\n#include "CoA/Source/Modules/ItemCollections.h"\n')
m.replace('this->m_flags = (this->m_flags & ~0x10) | (((raceRec->m_flags & 0x2) != 0) << 4);',
          'this->m_flags = (this->m_flags & ~0x10) | (Retail::BareFeet(this->m_data.m_preferences.m_raceID, raceRec->m_flags) << 4);')
m.replace('if (static_cast<int32_t>(data->m_preferences.m_raceID) < 0 || static_cast<int32_t>(data->m_preferences.m_raceID) > g_chrRacesDB.m_maxID || !ComponentValidateBase(CCharacterComponent::s_chrVarArray, this->m_data.m_preferences.m_raceID, this->m_data.m_preferences.m_sexID, CHARTEXTUREVAR_SKIN, 0, this->m_data.m_preferences.m_skinID)) {',
          'if (static_cast<int32_t>(data->m_preferences.m_raceID) < 0 || static_cast<int32_t>(data->m_preferences.m_raceID) > g_chrRacesDB.m_maxID ||\n        (!RetailRaceOwns(this->m_data.m_preferences.m_raceID, this->m_data.m_preferences.m_sexID) &&\n         !ComponentValidateBase(CCharacterComponent::s_chrVarArray, this->m_data.m_preferences.m_raceID, this->m_data.m_preferences.m_sexID, CHARTEXTUREVAR_SKIN, 0, this->m_data.m_preferences.m_skinID))) {')
m.replace('    CoA::Esteria::ForgetCharacter(component); // CoA: EsteriaForgetCharacter (orig patch 0x4F16C0)\n',
          '    CoA::Esteria::ForgetCharacter(component); // CoA: EsteriaForgetCharacter (orig patch 0x4F16C0)\n    RetailRaceForget(component);\n')
m.replace('    this->m_data.m_model->ReplaceTexture(M2TEX_SKIN, this->m_baseSkinTexture);\n}\n\n// VERIFY: 51c5a21d manual',
          '    if (!RetailRaceActive(this)) {\n        this->m_data.m_model->ReplaceTexture(M2TEX_SKIN, this->m_baseSkinTexture);\n    }\n}\n\n// VERIFY: 51c5a21d manual')
m.replace('uint32_t raceMask = 1 << this->m_data.m_preferences.m_raceID;', 'uint32_t raceMask = RetailRaceHelmetMask(this->m_data.m_preferences.m_raceID);')
m.replace('    model->SetGeometryVisible(0, 0, 1);\n\n    for (int32_t i = 0; i < NUM_CHARGEOSETS; i++) {',
          '    model->SetGeometryVisible(0, 0, 1);\n    RetailRaceGeometry(this);\n\n    for (int32_t i = 0; i < NUM_CHARGEOSETS; i++) {')
m.replace('''    if (CoA::Esteria::DirectSection(this, 0, noArguments)) {
        return;
    }
''', '''    if (CoA::Esteria::DirectSection(this, 0, noArguments)) {
        return;
    }

    if (RetailRaceActive(this)) {
        return; // the Retail compositor owns the body sheet
    }
''')
m.replace('''        if (CoA::Esteria::DirectSection(this, 1, arguments)) {
            return;
        }
    }
''', '''        if (CoA::Esteria::DirectSection(this, 1, arguments)) {
            return;
        }
    }

    if (RetailRaceActive(this)) {
        return;
    }
''')
m.replace('''    *ext = '_';
    strcpy(ext + 1, raceRec->m_clientPrefix);
    strcat(path, s_fileDecorations[this->m_data.m_preferences.m_sexID]);''', '''    *ext = '_';
    if (const auto* donor = g_chrRacesDB.GetRecord(RetailRaceHelmetRace(this->m_data.m_preferences.m_raceID))) raceRec = donor;
    strcpy(ext + 1, raceRec->m_clientPrefix);
    strcat(path, s_fileDecorations[this->m_data.m_preferences.m_sexID]);''')
m.save()
