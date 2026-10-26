// AscCamera.cpp's client half: the Deps and what the decomp sites ask with client objects.
#include "CoA/Source/Modules/Camera.h"
#include "CoA/Source/Modules/CameraClient.h"
#include "CoA/Source/Modules/Modules.h"
#include "Engine/Source/Model2/M2Model.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/UIUtil/Camera.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/WorldClient/World.h"

namespace {
    bool InWorld() {   // 0xBD0792
        return CGGameUI::s_inWorld != 0;
    }

    void SetModelAlpha(void* model, float alpha) {   // +0x17C
        static_cast<CM2Model*>(model)->m_baseAlpha = alpha;
    }

    void SetTrackingTurnRate(float rate) {   // 0xCA11DC
        UnitSetTrackingTurnRate(rate);
    }

    void ClearPlayerTracking() {   // 0x7272C0(player, 0, 1)
        if (CGPlayer_C* player = ClntObjMgrGetActivePlayerObj()) {
            player->ClearTrackingTarget(false, true);
        }
    }

    bool Target(CoA::Camera::Target& out) {   // FUN_1019c420's lookups
        CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

        if (!player) {
            return false;
        }

        out.guid = CGGameUI::m_lockedTarget.m_guid;
        CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(CGGameUI::m_lockedTarget, TYPE_UNIT, __FILE__, __LINE__));

        if (!unit) {
            return false;
        }

        out.isPlayer = unit->m_typeId == ID_PLAYER;
        out.canAttackNow = player->CanAttackNow(unit);
        out.reaction = static_cast<int32_t>(player->UnitReaction(unit));
        return true;
    }

    void TrackTarget(uint64_t guid) {   // 0x72B4A0(player, &guid, 1)
        if (CGPlayer_C* player = ClntObjMgrGetActivePlayerObj()) {
            WOWGUID target;
            target.m_guid = guid;
            player->SetTrackingTarget(target, 1);
        }
    }

    bool IsActiveMover(void* unit) {   // 0x4D43C0
        return static_cast<CGUnit_C*>(unit)->IsActiveMover() != 0;
    }

    CGUnit_C* CameraUnit(const CGCamera* camera) {   // 0x4D4DB0(camera +0x88, 8)
        return static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(camera->m_targetGUID, TYPE_UNIT, __FILE__, __LINE__));
    }
}

void CoA::CameraClient::AfterCameraPosition(const CGCamera* camera, C3Vector& position) {
    if (!CoA::Camera::ActionCam()) {
        return;
    }

    if (CGUnit_C* unit = CameraUnit(camera)) {
        CoA::Camera::ActionTargetOffset(unit->GetFacing(), position.x, position.y, position.z);
    }
}

void CoA::CameraClient::BeforeCollide(const CGCamera* camera, const C3Vector& target) {
    CoA::Camera::RestoreAlphas();

    if (!CoA::Camera::FadeM2s()) {
        return;
    }

    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (!player) {
        return;
    }

    C3Vector from = target;
    C3Vector hit(0.0f, 0.0f, 0.0f);
    float distance = 1.0f;
    World::HitTestDebug scratch = {};
    const float scale = player->GetScale();
    from.z = static_cast<float>(scale * 1.5L + from.z);

    if (!World::Intersect(camera->m_position, from, &hit, &distance, 0x100171, &scratch)) {
        return;
    }

    const float alpha = CoA::Camera::FadeAlpha();

    for (const CoA::Camera::Hit& h : CoA::Camera::Hits()) {
        CM2Model* model = static_cast<CM2Model*>(h.model);
        const bool attached = model->m_attachParent != nullptr;
        model->m_fFlags |= attached ? 0x80u : 0x8u;
        model->m_fFlags |= attached ? 0x20000u : 0x10000u;
        model->m_baseAlpha = alpha;
    }
}

void CoA::CameraClient::AfterCollide(const CGCamera* camera, uint32_t result, C3Vector& target, float distance) {
    if (!(result & 0x10000) || !CoA::Camera::ActionCam()) {
        return;
    }

    CGUnit_C* unit = CameraUnit(camera);
    float dx = 0.0f;
    float dy = 0.0f;

    if (unit && CoA::Camera::CollisionShift(result, unit->GetFacing(), distance, dx, dy)) {
        target.x -= dx;
        target.y -= dy;
    }
}

void CoA::CameraClient::RecordHit(CM2Model* model) {
    CoA::Camera::RecordHit(model, model->m_baseAlpha);
}

void CoA::CameraClient::Init() {
    CoA::Camera::Deps& d = CoA::Camera::Dependencies();
    d.inWorld = InWorld;
    d.setModelAlpha = SetModelAlpha;
    d.setTrackingTurnRate = SetTrackingTurnRate;
    d.clearPlayerTracking = ClearPlayerTracking;
    d.target = Target;
    d.trackTarget = TrackTarget;
    d.isActiveMover = IsActiveMover;
}
