#include "CoA/Source/Modules/CarriedMountClient.h"
#include "CoA/Source/Modules/CarriedMount.h"
#include "CoA/Source/Modules/HeldGrip.h"
#include "CoA/Source/Modules/HeldGripClient.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/WorldClient/World.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include <unordered_map>

namespace CoA::CarriedMountClient {
    namespace {
        constexpr uint32_t kMountHidden = 0x10000000;

        std::unordered_map<uint64_t, CM2Model*>& Carried() {
            static std::unordered_map<uint64_t, CM2Model*> s_carried;
            return s_carried;
        }

        bool Carries(CGUnit_C* unit) {
            if (!unit->m_worldModel) {
                return false;
            }

            const uint8_t* bytes = unit->m_unit->m_unitFieldBytes0;
            return CarriedMount::CarriesMounts((unit->m_obj->m_type & TYPE_PLAYER) != 0, bytes[0], bytes[2])
                && HeldGripClient::HasHeldGrip(unit->m_worldModel);
        }
    }

    bool Carry(CGUnit_C* unit) {
        if (!Carries(unit)) {
            return false;
        }

        Drop(unit);
        CreatureDisplayInfoRec* displayInfo = g_creatureDisplayInfoDB.GetRecord(unit->m_mountDisplayId);
        CreatureModelDataRec* modelData = displayInfo ? g_creatureModelDataDB.GetRecord(displayInfo->m_modelID) : nullptr;

        if (!modelData) {
            return true;
        }

        CM2Model* mount = World::s_m2Scene->CreateModel(modelData->m_modelName, 0);

        if (!mount) {
            return true;
        }

        CCharacterComponent::ApplyMonsterGeosets(mount, displayInfo);
        CCharacterComponent::ReplaceMonsterSkin(mount, displayInfo, modelData);
        mount->AttachToParent(unit->m_worldModel, HeldGrip::kLink, nullptr, 0);
        mount->m_fFlags |= 0x8000;
        mount->m_worldTransform = C44Matrix(CarriedMount::kScale * displayInfo->m_creatureModelScale);
        Carried()[unit->m_obj->m_guid.m_guid] = mount;
        unit->m_clientFlags |= kMountHidden;
        return true;
    }

    void Drop(CGUnit_C* unit) {
        auto found = Carried().find(unit->m_obj->m_guid.m_guid);

        if (found == Carried().end()) {
            return;
        }

        CM2Model* mount = found->second;
        Carried().erase(found);

        if (mount->m_attachParent) {
            mount->DetachFromParent();
        }

        mount->Release();
        unit->m_clientFlags &= ~kMountHidden;
    }
}
