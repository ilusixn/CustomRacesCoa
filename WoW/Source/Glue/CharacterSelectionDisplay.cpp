#include "CharacterSelectionDisplay.h"
#include "Engine/Source/Model2/M2Model.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Common/ComponentCore/RetailRace.h"
#include "WoW/Source/Glue/CGlueMgr.h"
#include "WoW/Source/Glue/CharacterSelection.h"
#include "WoW/Source/Glue/ComponentData.h"
#include "WoW/Source/Glue/CGlueLoading.h"
#include "WoW/Source/Glue/CSimpleModelFFX.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "CoA/Source/Modules/Json.h"
#include "WoW/Source/DB/Rec/CreatureModelDataRec.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include <cstdlib>
#include <string>

CCharacterComponent* CharacterSelectionDisplay::m_formMirror;
bool CharacterSelectionDisplay::m_formMirrorIsTwin;

namespace {
constexpr uint8_t kTwinsRace = 96;
constexpr uint32_t kTwinGearPayload = 0x54574E53;
std::string s_twinPayload;

std::string TwinPayload() {
    return CoA::Json::Get("MISC_PLAYER_DATA_PAYLOAD", kTwinGearPayload);
}

// The payload maps a character name to [[slot, displayID, inventoryType], ...] for a brother who wears his own gear.
void ApplyTwinGear(CHARACTER_INFO& twin, const std::string& payload) {
    if (payload.empty()) {
        return;
    }
    nlohmann::json root = nlohmann::json::parse(payload, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        return;
    }
    auto found = root.find(twin.m_name);
    if (found == root.end() || !found->is_array()) {
        return;
    }
    for (uint32_t slot = 0; slot < 23; slot++) {
        twin.m_inventoryItemDisplayID[slot] = 0;
        twin.m_inventoryItemType[slot] = 0;
        twin.m_inventoryItemVisualID[slot] = 0;
    }
    for (const auto& item : *found) {
        if (!item.is_array() || item.size() < 3 || !item[0].is_number_unsigned() || !item[1].is_number_unsigned()
            || !item[2].is_number_unsigned()) {
            continue;
        }
        uint32_t slot = item[0].get<uint32_t>();
        if (slot >= 23) {
            continue;
        }
        twin.m_inventoryItemDisplayID[slot] = item[1].get<uint32_t>();
        twin.m_inventoryItemType[slot] = item[2].get<uint32_t>();
    }
    twin.m_flags &= ~(0x400u | 0x800u);
}

void TwinMirrorOffset(float& x, float& y) {
    const char* depth = std::getenv("WOW_TWIN_SELECT_DEPTH");
    const char* side = std::getenv("WOW_TWIN_SELECT_SIDE");
    x = depth ? static_cast<float>(std::atof(depth)) : -1.0f;
    y = side ? static_cast<float>(std::atof(side)) : 0.9f;
}
}

// Sideways offset of the second form from the selected one, along the model frame's horizontal axis.
// WOW_FORM_SELECT_SPACING and WOW_FORM_SELECT_AXIS ("x" or "y") tune it without a rebuild.
static void FormMirrorOffset(float& x, float& y) {
    x = y = 0.0f;
    const char* spacing = std::getenv("WOW_FORM_SELECT_SPACING");
    const char* axis = std::getenv("WOW_FORM_SELECT_AXIS");
    (axis && (axis[0] == 'x' || axis[0] == 'X') ? x : y) = spacing ? static_cast<float>(std::atof(spacing)) : 1.2f;
}

void CharacterSelectionDisplay::ClearFormMirror() {
    CCharacterComponent* mirror = m_formMirror;
    if (!mirror) {
        return;
    }

    m_formMirror = nullptr;
    m_formMirrorIsTwin = false;

    if (mirror->m_data.m_model && mirror->m_data.m_model->m_attachParent) {
        mirror->m_data.m_model->DetachFromParent();
    }

    CCharacterComponent::FreeComponent(mirror);
}

void CharacterSelectionDisplay::ShowFormMirror(CharacterSelectionDisplay& display) {
    ClearFormMirror();

    const CHARACTER_INFO& info = display.m_characterInfo;
    uint32_t form = 0;

    if (!CCharacterSelection::m_modelFrame || !CCharacterSelection::m_modelFrame->m_model || !display.m_component || !display.m_component->m_data.m_model
        || !RetailRaceParkedForm(info.m_guid.m_guid, form)) {
        return;
    }

    char path[260];

    if (!RetailRaceModelPath(info.m_raceID, info.m_sexID, path, sizeof(path), form)) {
        return;
    }

    ComponentData data;
    data.m_preferences.m_raceID = info.m_raceID;
    data.m_preferences.m_sexID = info.m_sexID;
    data.m_preferences.m_classId = info.m_classID;
    data.m_model = CCharacterSelection::m_modelFrame->GetScene()->CreateModel(path, 0);

    if (!data.m_model) {
        return;
    }

    data.m_flags |= 0x2;
    CCharacterComponent* mirror = CCharacterComponent::AllocComponent();
    mirror->Init(&data, nullptr);

    if (!RetailRaceApplyParkedTo(mirror, info.m_guid.m_guid)) {
        CCharacterComponent::FreeComponent(mirror);
        return;
    }

    data.m_model->m_lightingCallback = &CCharacterSelection::GenericLightingCallback;
    data.m_model->m_lightingArg = CCharacterSelection::m_modelFrame->m_lightArray2;
    data.m_model->SetBoneSequence(0xFFFFFFFF, 0, 0xFFFFFFFF, 0, 1.0f, 1, 1);

    CCharacterSelection::DressComponent(info, mirror);
    data.m_model->AttachToParent(CCharacterSelection::m_modelFrame->m_model, 0, nullptr, 0);
    m_formMirror = mirror;
    CGlueLoading::StartLoad(mirror, 1);
}

void CharacterSelectionDisplay::ShowTwinMirror(CharacterSelectionDisplay& display) {
    ClearFormMirror();
    s_twinPayload = TwinPayload();

    const CHARACTER_INFO& info = display.m_characterInfo;
    if (info.m_raceID != kTwinsRace || !CCharacterSelection::m_modelFrame || !CCharacterSelection::m_modelFrame->m_model
        || !display.m_component || !display.m_component->m_data.m_model) {
        return;
    }

    CreatureModelDataRec* modelRec = GetModelDataRec(info.m_raceID, info.m_sexID);
    if (!modelRec || !*modelRec->m_modelName) {
        return;
    }

    ComponentData data;
    data.m_preferences.m_raceID = info.m_raceID;
    data.m_preferences.m_sexID = info.m_sexID;
    data.m_preferences.m_classId = info.m_classID;
    data.m_preferences.m_skinID = info.m_skinColorID;
    data.m_preferences.m_faceId = info.m_faceID;
    data.m_preferences.m_hairStyleId = info.m_hairStyleID;
    data.m_preferences.m_hairColorId = info.m_hairColorID;
    data.m_preferences.m_facialHairId = info.m_facialHairStyleID;
    data.m_model = CCharacterSelection::m_modelFrame->GetScene()->CreateModel(modelRec->m_modelName, 0);
    if (!data.m_model) {
        return;
    }

    data.m_flags |= 0x2;
    CCharacterComponent* mirror = CCharacterComponent::AllocComponent();
    if (!mirror) {
        data.m_model->Release();
        return;
    }
    mirror->Init(&data, nullptr);
    data.m_model->m_lightingCallback = &CCharacterSelection::GenericLightingCallback;
    data.m_model->m_lightingArg = CCharacterSelection::m_modelFrame->m_lightArray2;
    data.m_model->SetBoneSequence(0xFFFFFFFF, 0, 0xFFFFFFFF, 0, 1.0f, 1, 1);

    CHARACTER_INFO twin = info;
    ApplyTwinGear(twin, s_twinPayload);
    CCharacterSelection::DressComponent(twin, mirror);
    data.m_model->AttachToParent(CCharacterSelection::m_modelFrame->m_model, 0, nullptr, 0);
    m_formMirror = mirror;
    m_formMirrorIsTwin = true;
    CGlueLoading::StartLoad(mirror, 1);
    SetCharacterFacing(CCharacterSelection::s_charFacing);
}

// The gear payload can arrive after the character list: rebuild the brother once it does.
void CharacterSelectionDisplay::KeepTwinMirror(CharacterSelectionDisplay& display) {
    if (display.m_characterInfo.m_raceID != kTwinsRace || !display.m_component || !display.m_component->m_data.m_model) {
        return;
    }
    if (!m_formMirror || !m_formMirrorIsTwin || TwinPayload() != s_twinPayload) {
        ShowTwinMirror(display);
    }
}

// INLINED
CharacterSelectionDisplay::CharacterSelectionDisplay() {
    this->m_component = nullptr;
    this->m_petAlpha = 1.0f;
    this->m_petModel = nullptr;
}

// INLINED
CharacterSelectionDisplay::~CharacterSelectionDisplay() {
    if (this->m_petModel) {
        this->m_petModel->Release();
    }

    if (this->m_component) {
        CCharacterComponent::FreeComponent(this->m_component);
        this->m_component = nullptr;
    }
}

// VERIFY: 4e7bb4fe auto
// OFFSET: 0x004D9160
int32_t CharacterSelectionDisplay::SetSelectedCharacterInfo(CHARACTER_INFO* info) {
    CGlueMgr::m_characterInfo = info;

    if (!info) {
        return 0;
    }

    CGlueMgr::s_displayedCharacterIndex = CCharacterSelection::s_selectionIndex;
    return 1;
}

// VERIFY: 96adbffe manual
// OFFSET: 0x004E2E70
void CharacterSelectionDisplay::SetCharacterFacing(float rotation) {
    if (!CCharacterSelection::s_characterCount) {
        return;
    }

    CCharacterSelection::s_charFacing = rotation;

    if (CCharacterSelection::s_characterList.Count() <= 0) {
        return;
    }

    CharacterSelectionDisplay& display = CCharacterSelection::s_characterList[CCharacterSelection::s_selectionIndex];

    if (display.m_component->m_data.m_model) {
        C3Vector position(0.0f, 0.0f, 0.0f);
        display.m_component->m_data.m_model->SetWorldTransform(position, rotation,
            RetailRacePreviewScale(display.m_characterInfo.m_raceID));
    }

    if (m_formMirror && m_formMirror->m_data.m_model) {
        C3Vector position(0.0f, 0.0f, 0.0f);
        if (m_formMirrorIsTwin) {
            TwinMirrorOffset(position.x, position.y);
        } else {
            FormMirrorOffset(position.x, position.y);
        }
        m_formMirror->m_data.m_model->SetWorldTransform(position, rotation,
            RetailRacePreviewScale(display.m_characterInfo.m_raceID));
    }
}
