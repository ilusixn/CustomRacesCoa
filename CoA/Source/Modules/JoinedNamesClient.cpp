#include "CoA/Source/Modules/JoinedNamesClient.h"
#include "CoA/Source/Modules/JoinedNames.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "WoW/Source/DB/Rec/VehicleRec.h"
#include "WoW/Source/Game/GameClient/PlayerName.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Object/ObjectClient/Vehicle_C.h"
#include "WoW/Source/Object/Passenger.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include <cstring>
#include <string>

namespace CoA::JoinedNamesClient {
    namespace {
        CGUnit_C* SeatedRider(CGUnit_C* body) {
            if (!(body->m_obj->m_type & TYPE_PLAYER)) {
                return nullptr;
            }

            CVehicle_C* vehicle = body->m_vehicle;

            if (!vehicle || !vehicle->m_vehicleRec || vehicle->m_vehicleRec->m_id != JoinedNames::kTwoPlayersVehicle) {
                return nullptr;
            }

            for (CPassenger* passenger = vehicle->m_passengers.Head(); passenger; passenger = vehicle->m_passengers.Next(passenger)) {
                CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(*passenger->m_ownerGuid, TYPE_UNIT, __FILE__, __LINE__));

                if (unit && (unit->m_obj->m_type & TYPE_PLAYER)) {
                    return unit;
                }
            }

            return nullptr;
        }

        bool NameKnown(CGUnit_C* rider) {
            const char* name = rider->GetUnitName(nullptr, 1);
            const char* unknown = FrameScript_GetText("UNKNOWNOBJECT", -1, GENDER_NOT_APPLICABLE);
            return name && *name && strcmp(name, "Unknown Being") != 0 && !(unknown && strcmp(name, unknown) == 0);
        }

        CGUnit_C* NamedRider(CGUnit_C* body) {
            CGUnit_C* rider = SeatedRider(body);
            return rider && NameKnown(rider) ? rider : nullptr;
        }
    }

    bool SharesABody(const CGUnit_C* unit) {
        if (!unit) {
            return false;
        }

        const CGUnit_C* body = unit->GetVehicleUnit();
        const VehicleRec* rec = (body ? body : unit)->GetVehicleRec();
        return rec && (rec->m_id == JoinedNames::kTwoPlayersVehicle || rec->m_id == JoinedNames::kTwinsTwoPlayersVehicle);
    }

    const char* BodyName(CGUnit_C* unit) {
        CGUnit_C* rider = NamedRider(unit);

        if (!rider) {
            return unit->GetUnitName(nullptr, 1);
        }

        static std::string s_name;
        std::string body = unit->GetUnitName(nullptr, 1);
        s_name = JoinedNames::Joined(body, rider->GetUnitName(nullptr, 1));
        return s_name.c_str();
    }

    void RefreshName(CGObject_C* object, PLAYERNAMEDESC* desc) {
        if (!(object->m_obj->m_type & TYPE_PLAYER)) {
            return;
        }

        CGUnit_C* rider = NamedRider(static_cast<CGUnit_C*>(object));
        uint64_t riderGuid = rider ? rider->m_obj->m_guid.m_guid : 0;

        if (JoinedNames::RiderChanged(object->m_obj->m_guid.m_guid, riderGuid)) {
            PlayerNameTriggerNameRegenerate(desc);
        }
    }
}
