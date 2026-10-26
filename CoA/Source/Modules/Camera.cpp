// AscCamera.cpp (see Camera.h): action camera, camera target focus and camera M2 collision.
#include "CoA/Source/Modules/Camera.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {
    CoA::Camera::Deps s_deps;

    // The slots 0x10BDECF0 .. 0x10BDED1C.
    CVar* s_actionCam = nullptr;          // 0x10BDECF0
    CVar* s_m2Collision = nullptr;        // 0x10BDECF4
    CVar* s_m2CollisionAlpha = nullptr;   // 0x10BDECF8
    CVar* s_headBobs = nullptr;           // 0x10BDECFC
    CVar* s_interactFocus = nullptr;      // 0x10BDED00
    CVar* s_enemyFocus = nullptr;         // 0x10BDED04
    CVar* s_actionDist = nullptr;         // 0x10BDED08
    CVar* s_actionAngle = nullptr;        // 0x10BDED0C
    CVar* s_actionZ = nullptr;            // 0x10BDED10
    CVar* s_turnSpeed = nullptr;          // 0x10BDED14
    CVar* s_zoomFactor = nullptr;         // 0x10BDED18 (nothing reads it)
    CVar* s_autoTarget = nullptr;         // 0x10BDED1C

    std::vector<CoA::Camera::Hit> s_hits;   // 0x10BDED20 .. 0x10BDED28

    bool IsOne(const char* value) {
        return value && strcmp(value, "1") == 0;
    }

    int32_t Int(const CVar* var) {
        return var ? var->m_intValue : 0;
    }

    float Float(const CVar* var) {
        return var ? var->m_floatValue : 0.0f;
    }
}

CoA::Camera::Deps& CoA::Camera::Dependencies() {
    return s_deps;
}

bool CoA::Camera::OnActionCam(CVar*, const char*, const char* value, void*) {
    if (IsOne(value)) {
        CoA::ClientHooks::options.bobAtAnyDistance = false;   // 0x5FFD34 = 7A
    }

    return true;
}

bool CoA::Camera::OnHeadBobs(CVar*, const char*, const char* value, void*) {
    CoA::ClientHooks::options.bobAtAnyDistance = IsOne(value);   // 0x5FFD34 = EB / 7A
    return true;
}

bool CoA::Camera::OnEnemyFocus(CVar*, const char*, const char* value, void*) {
    CoA::ClientHooks::options.movementKeepsTracking = IsOne(value);
    return true;
}

bool CoA::Camera::OnTurnSpeed(CVar*, const char*, const char* value, void*) {
    CoA::ClientHooks::options.trackingTurnRateFixed = true;
    const float v = static_cast<float>(atof(value));
    float rate = v < 0.5f ? 0.5f : v;   // the cmovbe pair: NaN passes through

    if (v > 16.5f) {
        rate = 16.5f;
    }

    if (s_deps.setTrackingTurnRate) {
        s_deps.setTrackingTurnRate(rate);
    }

    return true;
}

bool CoA::Camera::OnM2Collision(CVar*, const char*, const char* value, void*) {
    const bool on = IsOne(value);
    CoA::ClientHooks::options.doodadFadeStore = on;
    CoA::ClientHooks::options.cameraCollisionFlags = on ? 0x100171u : 0x170u;
    return true;
}

bool CoA::Camera::OnAutoTarget(CVar*, const char*, const char* value, void*) {
    CoA::ClientHooks::options.autoTargetAttacker = IsOne(value);
    return true;
}

bool CoA::Camera::ActionCam() {
    return Int(s_actionCam) != 0;
}

void CoA::Camera::ActionTargetOffset(float facing, float& x, float& y, float& z) {
    if (!CoA::Camera::ActionCam()) {
        return;
    }

    const double angle = static_cast<double>(static_cast<long double>(facing) + Float(s_actionAngle));
    x = static_cast<float>(cos(angle)) * Float(s_actionDist) + x;
    y = static_cast<float>(sin(angle)) * Float(s_actionDist) + y;
    z = Float(s_actionZ) + z;
}

const std::vector<CoA::Camera::Hit>& CoA::Camera::Hits() {
    return s_hits;
}

int32_t CoA::Camera::RecordHit(void* model, float alpha) {
    s_hits.push_back({ model, alpha });
    int32_t bits;
    memcpy(&bits, &alpha, 4);
    return bits;
}

void CoA::Camera::RestoreAlphas() {
    if (s_deps.inWorld && s_deps.inWorld() && s_deps.setModelAlpha) {
        for (const Hit& hit : s_hits) {
            s_deps.setModelAlpha(hit.model, hit.alpha);
        }
    }

    s_hits.clear();
}

bool CoA::Camera::FadeM2s() {
    return Int(s_m2Collision) == 0;
}

float CoA::Camera::FadeAlpha() {
    return Float(s_m2CollisionAlpha);
}

bool CoA::Camera::CollisionShift(uint32_t result, float facing, float distance, float& dx, float& dy) {
    if (!(result & 0x10000) || !CoA::Camera::ActionCam()) {
        return false;
    }

    const float angle = static_cast<float>(facing + static_cast<long double>(Float(s_actionAngle)));
    const float d = Float(s_actionDist);
    float shift = d - (distance - 0.11111111f);

    if (shift > d) {
        shift = d;
    }

    shift = 0.0f > shift ? 0.0f : shift;
    dx = static_cast<float>(cos(static_cast<double>(angle))) * shift;
    dy = static_cast<float>(sin(static_cast<double>(angle))) * shift;
    return true;
}

void CoA::Camera::BeforeClearTarget(uint64_t guid, uint64_t lockedTarget) {
    if (CoA::Camera::ActionCam() && Int(s_enemyFocus) != 0 && guid == lockedTarget && s_deps.clearPlayerTracking) {
        s_deps.clearPlayerTracking();
    }
}

void CoA::Camera::AfterTargetChange() {
    CoA::Camera::Target target;

    if (!s_deps.target || !s_deps.target(target) || target.isPlayer) {
        return;
    }

    bool focus = false;

    if (target.canAttackNow && CoA::Camera::ActionCam() && Int(s_enemyFocus) != 0) {   // FUN_1019c940
        focus = true;
    } else if (target.reaction > 3 && CoA::Camera::ActionCam() && Int(s_interactFocus) != 0) {
        focus = true;
    }

    if (focus && s_deps.trackTarget) {
        s_deps.trackTarget(target.guid);
    }
}

void CoA::Camera::BeforeTeleport(void* unit) {
    if (s_deps.isActiveMover && s_deps.isActiveMover(unit)) {
        CoA::Camera::RestoreAlphas();
    }
}

void CoA::Camera::Init() {
    CoA::ClientHooks::afterTarget.Add(CoA::Camera::AfterTargetChange);
    CoA::ClientHooks::beforeTeleport.Add(CoA::Camera::BeforeTeleport);

    using namespace CoA::Camera;
    // name, help, flags, default, callback, category, setCommand, slot, world
    COA_CVAR("ActionCam", nullptr, 1, "0", OnActionCam, 4, false, &s_actionCam, true);
    COA_CVAR("cameraTargetFocusInteractEnable", nullptr, 1, "0", nullptr, 4, false, &s_interactFocus, true);
    COA_CVAR("cameraTargetFocusEnemyEnable", nullptr, 1, "0", OnEnemyFocus, 4, false, &s_enemyFocus, true);
    COA_CVAR("cameraActionDist", nullptr, 1, "1.25", nullptr, 4, false, &s_actionDist, true);
    COA_CVAR("cameraActionAngle", nullptr, 1, "4.0", nullptr, 4, false, &s_actionAngle, true);
    COA_CVAR("cameraActionZ", nullptr, 1, "0.05", nullptr, 4, false, &s_actionZ, true);
    COA_CVAR("cameraActionHeadBobs", nullptr, 1, "0", OnHeadBobs, 4, false, &s_headBobs, true);
    COA_CVAR("cameraTargetFocusTurnSpeed", nullptr, 1, "3.15", OnTurnSpeed, 4, false, &s_turnSpeed, true);
    COA_CVAR("cameraM2Collision", nullptr, 0x21, "1", OnM2Collision, 4, false, &s_m2Collision, true);
    COA_CVAR("cameraM2CollisionAlpha", nullptr, 0x21, "0.5", nullptr, 4, false, &s_m2CollisionAlpha, true);
    COA_CVAR("cameraZoomFactor", nullptr, 1, "1.0", nullptr, 4, false, &s_zoomFactor, true);
    COA_CVAR("autoTarget", nullptr, 1, "1", OnAutoTarget, 4, false, &s_autoTarget, true);
}

void CoA::Camera::Reset() {
    s_deps = Deps();
    s_hits.clear();
    CoA::ClientHooks::options = CoA::ClientHooks::OptionPatches();

    for (CVar** slot : { &s_actionCam, &s_m2Collision, &s_m2CollisionAlpha, &s_headBobs, &s_interactFocus, &s_enemyFocus,
             &s_actionDist, &s_actionAngle, &s_actionZ, &s_turnSpeed, &s_zoomFactor, &s_autoTarget }) {
        *slot = nullptr;
    }
}
