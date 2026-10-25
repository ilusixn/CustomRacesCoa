// AscGameMode.cpp's client questions: the equipment-set list (0xACFC00 / 0x5AE600) and the game mode SpellText's
// markup reads.
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/SpellText.h"
#include "WoW/Source/Ui/EquipmentManager.h"

namespace {
    // CGEquipmentManager::GetSetByName: the set's id is its GUID's low word (set +8).
    bool EquipmentSetId(const char* name, uint32_t& id) {
        const EQUIPMENT_SET* set = CGEquipmentManager::GetSetByName(name);

        if (!set) {
            return false;
        }

        id = static_cast<uint32_t>(set->m_guid.m_guid);
        return true;
    }

    // The first set in the client's list whose id (GUID low word) is `id`.
    const char* EquipmentSetName(uint32_t id) {
        for (EQUIPMENT_SET_NODE* node = CGEquipmentManager::s_equipmentSets.Head(); node; node = CGEquipmentManager::s_equipmentSets.Next(node)) {
            if (static_cast<uint32_t>(node->m_set->m_guid.m_guid) == id) {
                return node->m_set->m_name;
            }
        }

        return nullptr;
    }
}

void CoA::GameModeClient::Init() {
    CoA::GameMode::Deps& deps = CoA::GameMode::Dependencies();
    deps.equipmentSetId = EquipmentSetId;
    deps.equipmentSetName = EquipmentSetName;
    CoA::SpellText::Dependencies().gameMode = CoA::GameMode::Mode;
}
