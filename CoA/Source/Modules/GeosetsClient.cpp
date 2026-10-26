// AscGeosets.cpp's client half: CM2Model::SetGeometryVisible (0x82C7C0) and its loaded-model replacement 0x101A1440,
// the selected character-select entry's pet (0xB6B240 + index x 0x198) and SetUpModel's display (+0x24).
#include "CoA/Source/Modules/Geosets.h"
#include "CoA/Source/Modules/GeosetsClient.h"
#include "CoA/Source/Modules/Modules.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Shared.h"
#include "WoW/Source/DB/DBClient/CreatureStats_C.h"
#include "WoW/Source/Glue/CharacterSelection.h"
#include "WoW/Source/Glue/CharacterSelectionDisplay.h"

namespace {
    void SetGeometryVisible(void* model, uint32_t first, uint32_t last, uint32_t visible) {   // 0x82C7C0
        static_cast<CM2Model*>(model)->SetGeometryVisible(first, last, static_cast<int32_t>(visible));
    }

    // 0x101A1440, for models with +0x10 bit 0: +0x2C the shared model (none: nothing), its +0x170 skin profile's sections
    // (+0x1C count, +0x20 entries of 0x30 bytes, u16 id first), the model's flags +0x9C; 0x825D70 when one changed.
    void ReplacedSetGeometryVisible(CM2Model* model, uint32_t first, uint32_t last, int32_t visible) {
        CM2Shared* shared = model->m_shared;

        if (!shared) {
            return;
        }

        M2SkinProfile* skin = shared->m_skinData;
        const uint32_t count = skin->m_skinSections.m_count;

        if (!count) {
            return;
        }

        const uint8_t* ids = reinterpret_cast<const uint8_t*>(&skin->m_skinSections[0].m_skinSectionId);

        if (CoA::Geosets::SetRange(count, ids, sizeof(M2SkinSection), model->m_skinSections, first, last, static_cast<uint32_t>(visible))) {
            model->UnoptimizeVisibleGeometry();
        }
    }
}

void CoA::GeosetsClient::AfterApplyMonsterGeosets(CM2Model* model) {
    CoA::Geosets::AfterApplyMonsterGeosets(model);
}

void CoA::GeosetsClient::AfterShowCharacter() {
    // The original takes the entry at index x 0x198 past the list's data and tests only that address for 0; an index
    // outside the list reads past it (ledgered ruling: such an index has no entry).
    const int32_t index = CCharacterSelection::s_selectionIndex;
    const bool inList = index >= 0 && static_cast<uint32_t>(index) < CCharacterSelection::s_characterList.Count() &&
        CCharacterSelection::s_characterList.m_data;

    if (!inList) {
        return;
    }

    CharacterSelectionDisplay& entry = CCharacterSelection::s_characterList.m_data[index];
    CoA::Geosets::AfterShowCharacter(true, entry.m_characterInfo.m_petDisplayInfoID, entry.m_petModel);
}

void CoA::GeosetsClient::AfterSetUpModel(CreatureStats_C* stats) {
    // The original reads +0x24 of the argument unchecked; SetUpModel itself accepts nullptr (ledgered ruling).
    if (stats) {
        CoA::Geosets::AfterSetUpModel(stats->m_displayId[0]);
    }
}

void CoA::GeosetsClient::Init() {
    CoA::Geosets::Dependencies().setGeometryVisible = SetGeometryVisible;
    g_m2SetGeometryVisibleHook = ReplacedSetGeometryVisible;
}
