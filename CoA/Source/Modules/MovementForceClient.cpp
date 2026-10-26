// AscMovementForce.cpp's client half: the movement / unit accessors and client calls MovementForce.cpp asks for (the
// original's pointer table 0x10bcbb84..0x10bcbbdc), and the decomp sites' conversions.
#include "CoA/Source/Modules/MovementForceClient.h"
#include "CoA/Source/Modules/Modules.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/Net/Types.h"
#include "WoW/Source/Object/CMovementStatus.h"
#include "WoW/Source/Object/Movement.h"
#include "WoW/Source/Object/MovementShared.h"
#include "WoW/Source/Object/ObjectClient/Movement_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

namespace {
    // While 0x765 / 0x766 apply their movement info through UpdateStatusInternal, the hook stays out.
    bool s_rawStatus = false;

    CMovement_C* Mv(void* m) {
        return static_cast<CMovement_C*>(m);
    }

    CGUnit_C* Unit(void* u) {
        return static_cast<CGUnit_C*>(u);
    }

    void* MovementOf(uint64_t guid) {   // FUN_102b8fc0: 0x4D4DB0(guid, 0x18) +0xD8
        WOWGUID g;
        g.m_guid = guid;
        CGObject_C* object = ClntObjMgrObjectPtr(g, static_cast<OBJECT_TYPE>(0x18), __FILE__, __LINE__);
        return object ? static_cast<CGUnit_C*>(object)->m_movement : nullptr;
    }

    uint64_t ActiveMover() {
        return CGUnit_C::s_activeMover.m_guid;
    }

    int32_t Now() {
        return static_cast<int32_t>(OsGetAsyncTimeMs());
    }

    uint32_t IdleTime() {
        return MovementGetIdleTime();
    }

    uint32_t* Flags(void* m) {
        return &Mv(m)->m_flags;
    }

    float* Position(void* m) {
        return &Mv(m)->m_position.x;
    }

    float* Anchor(void* m) {
        return &Mv(m)->m_anchorPos.x;
    }

    uint32_t TransportLow(void* m) {
        return static_cast<uint32_t>(Mv(m)->m_transportGuid.m_guid);
    }

    void* Controller(void* m) {
        return Mv(m)->m_unit;
    }

    uint64_t ObjectGuid(void* u) {
        return Unit(u)->m_obj->m_guid.m_guid;
    }

    uint32_t TypeId(void* u) {
        return static_cast<uint32_t>(Unit(u)->m_typeId);
    }

    bool IsActiveMover(void* u) {
        return Unit(u)->IsActiveMover() != 0;
    }

    void UpdateAnchors(void* m) {
        Mv(m)->UpdateAnchors(0);
    }

    void ApplyMovement(void* m, int32_t now, uint32_t dt) {
        Mv(m)->ApplyMovement(static_cast<uint32_t>(now), dt);
    }

    void OnMoveUpdate(void* u, int32_t now) {
        Unit(u)->OnMoveUpdate(static_cast<uint32_t>(now), 1, 0);
    }

    void StopAnimation(void* u) {
        Unit(u)->UpdateBaseAnimation(0, -1);
    }

    void Heartbeat(void* u, int32_t now) {   // MSG_MOVE_HEARTBEAT
        Unit(u)->SendMovementUpdate(static_cast<uint32_t>(now), 0xEE, 0.0f, 0, 0, 0xFF);
    }

    bool ReadMovement(CDataStore* msg, void* m) {   // 0x4F4C50 / 0x4F4D40, then the original 0x6EB730
        CMovementStatus status;
        *msg >> status;

        if (msg->m_read > msg->m_size) {
            return false;
        }

        if (m) {
            int32_t timeOffset = 0;
            s_rawStatus = true;
            Mv(m)->UpdateStatusInternal(OsGetAsyncTimeMs(), status, timeOffset, 0, 0);
            s_rawStatus = false;
        }

        return true;
    }

    void WriteMovement(void* m, uint32_t opcode, int32_t time, CDataStore* msg) {   // 0x7164B0
        Mv(m)->WriteMovementStatusToPacket(opcode, static_cast<uint32_t>(time), msg);
    }
}

uint32_t CoA::MovementForceClient::AfterPlot(CMovementShared* movement, uint32_t time, C3Vector* move, uint32_t result) {
    void* m = movement ? static_cast<CMovement_C*>(movement) : nullptr;
    return static_cast<uint32_t>(CoA::MovementForce::AfterPlot(m, time, move ? &move->x : nullptr, static_cast<int32_t>(result)));
}

CoA::MovementForce::StatusUpdate CoA::MovementForceClient::BeforeStatus(CMovement_C* movement, const CMovementStatus& status) {
    if (s_rawStatus) {
        return CoA::MovementForce::StatusUpdate();
    }

    return CoA::MovementForce::BeforeStatus(movement, status.m_moveFlags);
}

void CoA::MovementForceClient::AfterStatus(const CoA::MovementForce::StatusUpdate& update, CMovement_C* movement, const CMovementStatus& status) {
    CoA::MovementForce::AfterStatus(update, movement, static_cast<int32_t>(status.m_gameTime));
}

void CoA::MovementForceClient::Init() {
    CoA::MovementForce::Deps& d = CoA::MovementForce::Dependencies();
    d.movementOf = MovementOf;
    d.activeMover = ActiveMover;
    d.now = Now;
    d.idleTime = IdleTime;
    d.flags = Flags;
    d.position = Position;
    d.anchor = Anchor;
    d.transportLow = TransportLow;
    d.controller = Controller;
    d.objectGuid = ObjectGuid;
    d.typeId = TypeId;
    d.isActiveMover = IsActiveMover;
    d.updateAnchors = UpdateAnchors;
    d.applyMovement = ApplyMovement;
    d.onMoveUpdate = OnMoveUpdate;
    d.stopAnimation = StopAnimation;
    d.heartbeat = Heartbeat;
    d.readMovement = ReadMovement;
    d.writeMovement = WriteMovement;
}
