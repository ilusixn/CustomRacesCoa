import sys
sys.path.insert(0, r'C:\CoA-Build\kv')
from merge_util import Merger
m = Merger(r'C:\CoA-Build\coa64\WoW\Source\Glue\CharacterCreation.cpp')
m.replace('''    data->m_model = CCharacterCreation::m_charCustomizeFrame->GetScene()->CreateModel(modelRec->m_modelName, 0);
''', '''    const uint32_t form = RetailRaceFormRestore(CCharacterCreation::m_character, data->m_preferences);
    char retailPath[260];
    const bool retailModel = RetailRaceModelPath(data->m_preferences.m_raceID, data->m_preferences.m_sexID, retailPath, sizeof(retailPath), form);
    data->m_model = CCharacterCreation::m_charCustomizeFrame->GetScene()->CreateModel(retailModel ? retailPath : modelRec->m_modelName, 0);
''')
m.replace('''    if (CCharacterCreation::m_character->m_data.m_model) {
        C3Vector position(0.0f, 0.0f, 0.0f);
        CCharacterCreation::m_character->m_data.m_model->SetWorldTransform(position, facing, 1.0f);
    }
''', '''    for (CCharacterComponent* character : { CCharacterCreation::m_character, CCharacterCreation::m_formMirror }) {
        if (character && character->m_data.m_model) {
            C3Vector position(0.0f, 0.0f, 0.0f);
            RetailRaceFormOffset(character, position.x, position.y);
            character->m_data.m_model->SetWorldTransform(position, facing,
                RetailRacePreviewScale(character->m_data.m_preferences.m_raceID));
        }
    }
''')
m.save()
