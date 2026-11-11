#include "CoA/Source/Modules/CarriedMountClient.h"
#include "CoA/Source/Modules/CarriedMount.h"
#include "CoA/Source/Modules/HeldGrip.h"
#include "CoA/Source/Modules/HeldGripClient.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Magic/MagicClient/SpellEnums.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/WorldClient/World.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Model2/M2Scene.h"
#include "Engine/Source/Tempest/C44Matrix.h"
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <windows.h>

namespace CoA::CarriedMountClient {
    namespace {
        constexpr uint32_t kMountHidden = 0x10000000;

        struct CarriedModel {
            CM2Model* Model;
            uint32_t Display;
        };

        std::unordered_map<uint64_t, CarriedModel>& Carried() {
            static std::unordered_map<uint64_t, CarriedModel> s_carried;
            return s_carried;
        }

        bool s_mirroring = false;

        const CarriedModel* LiveCarry(const CGUnit_C* unit) {
            auto found = Carried().find(unit->m_obj->m_guid.m_guid);

            if (found == Carried().end() || unit->m_mountDisplayId <= 0 || !unit->m_worldModel
                || found->second.Model->m_attachParent != unit->m_worldModel) {
                return nullptr;
            }

            return &found->second;
        }

        bool BesideClient(const char* file, char (&path)[MAX_PATH]) {
            const DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
            char* slash = length && length < MAX_PATH ? strrchr(path, '\\') : nullptr;

            if (!slash) {
                return false;
            }

            strcpy_s(slash + 1, MAX_PATH - (slash - path) - 1, file);
            return true;
        }

        std::map<uint32_t, CarriedMount::CarryFit>& Fits() {
            static std::map<uint32_t, CarriedMount::CarryFit> s_fits = [] {
                char path[MAX_PATH] = {};
                FILE* file = nullptr;
                std::string text;

                if (BesideClient("CarriedMounts.txt", path) && !fopen_s(&file, path, "rb") && file) {
                    char chunk[4096];

                    for (size_t read; (read = fread(chunk, 1, sizeof(chunk), file)) > 0;) {
                        text.append(chunk, read);
                    }

                    fclose(file);
                }

                return CarriedMount::ParseCarryFits(text);
            }();
            return s_fits;
        }

        void SaveFits() {
            char path[MAX_PATH] = {};
            FILE* file = nullptr;

            if (!BesideClient("CarriedMounts.txt", path) || fopen_s(&file, path, "wb") || !file) {
                return;
            }

            const std::string text = CarriedMount::FormatCarryFits(Fits());
            fwrite(text.data(), 1, text.size(), file);
            fclose(file);
        }

        const char* ModelOf(uint32_t display) {
            CreatureDisplayInfoRec* displayInfo = g_creatureDisplayInfoDB.GetRecord(display);
            CreatureModelDataRec* modelData = displayInfo ? g_creatureModelDataDB.GetRecord(displayInfo->m_modelID) : nullptr;
            return modelData ? modelData->m_modelName : "";
        }

        CarriedMount::CarryFit FitOf(uint32_t display) {
            auto found = Fits().find(display);
            return found != Fits().end() ? found->second : CarriedMount::DefaultFit(ModelOf(display));
        }

        void Mirror(const CarriedMount::CarryFit& fit);

        uint32_t CarryLink(CM2Model* model) {
            if (model->IsLoaded(0, 0)) {
                return model->HasAttachment(CarriedMount::kCarryLink) ? CarriedMount::kCarryLink : HeldGrip::kLink;
            }

            return model->m_shared && CarriedMount::CarryPointModel(model->m_shared->m_fileName)
                ? CarriedMount::kCarryLink : HeldGrip::kLink;
        }

        C44Matrix CarryTransform(const CarriedMount::CarryFit& fit, float modelScale) {
            constexpr float kDegrees = 3.14159265f / 180.0f;
            C44Matrix transform = C44Matrix(fit.Scale * modelScale)
                * C44Matrix::RotationAroundX(fit.Roll * kDegrees)
                * C44Matrix::RotationAroundY(fit.Pitch * kDegrees)
                * C44Matrix::RotationAroundZ(fit.Yaw * kDegrees);
            transform.d0 = fit.X;
            transform.d1 = fit.Y;
            transform.d2 = fit.Z;
            return transform;
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

        if (LiveCarry(unit)) {
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
        const uint32_t display = unit->m_mountDisplayId;
        const CarriedMount::CarryFit fit = FitOf(display);
        CreatureDisplayInfoRec* displayInfo = g_creatureDisplayInfoDB.GetRecord(fit.As ? fit.As : display);
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
        mount->m_worldTransform = CarryTransform(fit, displayInfo->m_creatureModelScale);
        const bool flying = CarriedMount::FlyingModel(mount->HasSequence(CarriedMount::kAnimFly),
            mount->HasSequence(CarriedMount::kAnimFlyStand), mount->HasSequence(CarriedMount::kAnimWalk),
            mount->HasSequence(CarriedMount::kAnimRun));

        if (CarriedMount::HoldsStill(fit, flying)) {
            mount->SetBoneSequence(0xFFFFFFFF, static_cast<uint32_t>(fit.Pose), 0xFFFFFFFF,
                static_cast<uint32_t>(fit.Frame > 0.0f ? fit.Frame : 0.0f), 0.0f, 1, 1);
        }
        Carried()[unit->m_obj->m_guid.m_guid] = { mount, display };
        unit->m_clientFlags |= kMountHidden;

        if (unit->m_obj->m_guid.m_guid == ClntObjMgrGetActivePlayer().m_guid) {
            Mirror(fit);
        }

        return true;
    }

    void Note(CGUnit_C* unit, const char* stage) {
        const uint8_t* bytes = unit->m_unit->m_unitFieldBytes0;
        const bool player = (unit->m_obj->m_type & TYPE_PLAYER) != 0;

        if (!player || bytes[0] != CarriedMount::kTwoHeadedOgre) {
            return;
        }

        char path[MAX_PATH] = {};
        FILE* file = nullptr;

        if (!BesideClient("CarriedMount.log", path) || fopen_s(&file, path, "a") || !file) {
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

    bool Carrying(CGUnit_C* unit) {
        return LiveCarry(unit) != nullptr;
    }

    bool CountsAsMounted(const CGUnit_C* unit) {
        return unit->m_mountDisplayId > 0 && (!(unit->m_clientFlags & kMountHidden) || LiveCarry(unit));
    }

    bool Drop(CGUnit_C* unit) {
        auto found = Carried().find(unit->m_obj->m_guid.m_guid);

        if (found == Carried().end()) {
            return false;
        }

        CM2Model* mount = found->second.Model;
        Carried().erase(found);
        unit->m_clientFlags &= ~kMountHidden;

        if (!unit->m_worldModel || mount->m_attachParent != unit->m_worldModel) {
            return true;
        }

        mount->DetachFromParent();
        mount->Release();
        return true;
    }

    namespace {
        bool Retune(CVar*, const char*, const char* value, void* arg) {
            if (s_mirroring) {
                return true;
            }

            const uint64_t guid = ClntObjMgrGetActivePlayer().m_guid;
            auto found = Carried().find(guid);
            CGUnit_C* unit = found != Carried().end()
                ? static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(ClntObjMgrGetActivePlayer(), TYPE_UNIT, __FILE__, __LINE__))
                : nullptr;

            if (!unit) {
                return true;
            }

            CarriedMount::CarryFit fit = FitOf(found->second.Display);
            CarriedMount::SetFitValue(fit, reinterpret_cast<uintptr_t>(arg), static_cast<float>(atof(value)));
            Fits()[found->second.Display] = fit;
            SaveFits();
            Carry(unit);
            return true;
        }

        void Mirror(const CarriedMount::CarryFit& fit) {
            s_mirroring = true;

            for (size_t field = 0; field < CarriedMount::kFitFieldCount; ++field) {
                char name[32];
                char value[32];
                snprintf(name, sizeof(name), "coaCarry%c%s", static_cast<char>(toupper(CarriedMount::kFitFields[field][0])),
                    CarriedMount::kFitFields[field] + 1);
                snprintf(value, sizeof(value), "%g", CarriedMount::FitValue(fit, field));
                CVar* var = CVar::Lookup(name);

                if (!var || var->m_callback != Retune) {
                    var = CVar::Register(name, "", 0, value, Retune, 0, false, reinterpret_cast<void*>(field), false);
                }

                if (var) {
                    var->Set(value, true, false, false, true);
                }
            }

            s_mirroring = false;
        }
    }
}
