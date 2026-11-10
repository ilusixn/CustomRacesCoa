#include "CoA/Source/Modules/CarriedMountClient.h"
#include "CoA/Source/Modules/CarriedMount.h"
#include "CoA/Source/Modules/HeldGrip.h"
#include "CoA/Source/Modules/HeldGripClient.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Magic/MagicClient/SpellEnums.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/WorldClient/World.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <windows.h>

namespace CoA::CarriedMountClient {
    namespace {
        constexpr uint32_t kMountHidden = 0x10000000;

        std::unordered_map<uint64_t, CM2Model*>& Carried() {
            static std::unordered_map<uint64_t, CM2Model*> s_carried;
            return s_carried;
        }

        uint32_t CarryLink(CM2Model* model) {
            if (model->IsLoaded(0, 0)) {
                return model->HasAttachment(CarriedMount::kCarryLink) ? CarriedMount::kCarryLink : HeldGrip::kLink;
            }

            return model->m_shared && CarriedMount::CarryPointModel(model->m_shared->m_fileName)
                ? CarriedMount::kCarryLink : HeldGrip::kLink;
        }

        bool CastsAMount(CGUnit_C* unit) {
            SpellRec spell;

            if (!unit->m_castingSpellId || !g_spellDB.GetRecord(static_cast<int32_t>(unit->m_castingSpellId), &spell)) {
                return false;
            }

            for (uint32_t i = 0; i < 3; i++) {
                if (spell.m_effectAura[i] == SPELL_AURA_MOUNTED) {
                    return true;
                }
            }

            return false;
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

    bool CarryingAnim(CGUnit_C* unit, uint32_t anim, CM2Model* model, uint32_t* carrying) {
        CM2Model* body = model ? model : unit->m_worldModel;

        if (body != unit->m_worldModel || !body) {
            return false;
        }

        uint32_t swapped = anim;

        if (Carried().count(unit->m_obj->m_guid.m_guid)) {
            swapped = CarriedMount::CarryingAnim(anim);
        } else if (CastsAMount(unit) && Carries(unit)) {
            swapped = CarriedMount::PickUpAnim(anim);
        }

        if (swapped == anim || !body->IsLoaded(0, 0) || !body->HasSequence(swapped)) {
            return false;
        }

        *carrying = swapped;
        return true;
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
        mount->AttachToParent(unit->m_worldModel, CarryLink(unit->m_worldModel), nullptr, 0);
        mount->m_fFlags |= 0x8000;
        mount->m_worldTransform = C44Matrix(CarriedMount::kScale * displayInfo->m_creatureModelScale);
        Carried()[unit->m_obj->m_guid.m_guid] = mount;
        unit->m_clientFlags |= kMountHidden;
        return true;
    }

    void Note(CGUnit_C* unit, const char* stage) {
        const uint8_t* bytes = unit->m_unit->m_unitFieldBytes0;
        const bool player = (unit->m_obj->m_type & TYPE_PLAYER) != 0;

        if (!player || bytes[0] != CarriedMount::kTwoHeadedOgre) {
            return;
        }

        char path[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
        char* slash = length && length < MAX_PATH ? strrchr(path, '\\') : nullptr;

        if (!slash) {
            return;
        }

        strcpy_s(slash + 1, MAX_PATH - (slash - path) - 1, "CarriedMount.log");
        FILE* file = nullptr;

        if (fopen_s(&file, path, "a") || !file) {
            return;
        }

        CM2Model* model = unit->m_worldModel;
        const bool loaded = model && model->IsLoaded(0, 0);
        const char* name = model && model->m_shared ? model->m_shared->m_fileName : "-";
        fprintf(file, "tick=%u stage=%s guid=%llx sex=%u model=%s loaded=%d grip=%d named=%d mountModel=%d "
            "mountDisplay=%u fieldMount=%u flags=%08x carries=%d\n",
            static_cast<uint32_t>(GetTickCount()), stage, static_cast<unsigned long long>(unit->m_obj->m_guid.m_guid),
            bytes[2], name, loaded, loaded ? model->HasAttachment(HeldGrip::kLink) : -1,
            model && model->m_shared ? HeldGrip::HeldGripModel(name) : -1, unit->m_mountModel != nullptr,
            unit->m_mountDisplayId, unit->m_unit->m_unitFieldMountdisplayid, unit->m_clientFlags, Carries(unit));
        fclose(file);
    }

    bool Drop(CGUnit_C* unit) {
        auto found = Carried().find(unit->m_obj->m_guid.m_guid);

        if (found == Carried().end()) {
            return false;
        }

        CM2Model* mount = found->second;
        Carried().erase(found);

        if (mount->m_attachParent) {
            mount->DetachFromParent();
        }

        mount->Release();
        unit->m_clientFlags &= ~kMountHidden;
        return true;
    }
}
