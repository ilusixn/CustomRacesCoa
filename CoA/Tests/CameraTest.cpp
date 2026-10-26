#include "CoA/Source/Modules/Camera.h"
#include "CoA/Source/Runtime/CVars.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {
    bool s_inWorld = true;
    std::map<void*, float> s_alphas;
    float s_turnRate = 0.0f;
    int s_clears = 0;
    std::vector<uint64_t> s_tracked;
    bool s_haveTarget = true;
    CoA::Camera::Target s_target;
    void* s_mover = nullptr;

    bool InWorld() { return s_inWorld; }
    void SetAlpha(void* model, float alpha) { s_alphas[model] = alpha; }
    void TurnRate(float rate) { s_turnRate = rate; }
    void ClearTracking() { ++s_clears; }
    bool Target(CoA::Camera::Target& out) { out = s_target; return s_haveTarget; }
    void Track(uint64_t guid) { s_tracked.push_back(guid); }
    bool IsMover(void* unit) { return unit == s_mover; }

    void Set(const char* name, const char* value) {
        CVar* var = CVar::Lookup(name);
        CHECK(var != nullptr);

        if (var) {
            var->Set(value, true, false, false, false);
        }
    }

    void Start() {
        CoA::CVars::Reset();
        CoA::Camera::Reset();
        CoA::ClientHooks::afterTarget.Clear();
        CoA::ClientHooks::beforeTeleport.Clear();
        s_inWorld = true;
        s_alphas.clear();
        s_clears = 0;
        s_tracked.clear();
        s_haveTarget = true;
        s_target = CoA::Camera::Target();
        s_mover = nullptr;
        CoA::Camera::Deps& d = CoA::Camera::Dependencies();
        d.inWorld = InWorld;
        d.setModelAlpha = SetAlpha;
        d.setTrackingTurnRate = TurnRate;
        d.clearPlayerTracking = ClearTracking;
        d.target = Target;
        d.trackTarget = Track;
        d.isActiveMover = IsMover;
        CoA::Camera::Init();
        CoA::CVars::RegisterWorld();
    }

    struct Spec {
        const char* name;
        const char* def;
        uint32_t flags;
        bool callback;
    };

    const Spec kSpecs[] = {
        { "ActionCam", "0", 1, true },
        { "cameraTargetFocusInteractEnable", "0", 1, false },
        { "cameraTargetFocusEnemyEnable", "0", 1, true },
        { "cameraActionDist", "1.25", 1, false },
        { "cameraActionAngle", "4.0", 1, false },
        { "cameraActionZ", "0.05", 1, false },
        { "cameraActionHeadBobs", "0", 1, true },
        { "cameraTargetFocusTurnSpeed", "3.15", 1, true },
        { "cameraM2Collision", "1", 0x21, true },
        { "cameraM2CollisionAlpha", "0.5", 0x21, false },
        { "cameraZoomFactor", "1.0", 1, false },
        { "autoTarget", "1", 1, true },
    };
}

COA_TEST(Camera_QueuesTwelveWorldCVars) {
    Start();
    size_t count = 0;
    const CoA::CVarEntry* world = CoA::CVars::World(count);
    CHECK(count == sizeof(kSpecs) / sizeof(kSpecs[0]));

    for (size_t i = 0; i < count && i < sizeof(kSpecs) / sizeof(kSpecs[0]); ++i) {
        CHECK(strcmp(world[i].name, kSpecs[i].name) == 0);
        CHECK(strcmp(world[i].defaultValue, kSpecs[i].def) == 0);
        CHECK(world[i].flags == kSpecs[i].flags);
        CHECK(world[i].category == 4);
        CHECK((world[i].callback != nullptr) == kSpecs[i].callback);
        CHECK(world[i].help == nullptr && !world[i].setCommand && world[i].slot != nullptr);
    }
}

// At registration: turn speed 3.15 written (and the client's writers NOPed), M2 collision and autoTarget stock.
COA_TEST(Camera_DefaultsAtRegistration) {
    Start();
    Set("cameraTargetFocusTurnSpeed", "3.15");
    Set("cameraM2Collision", "1");
    Set("autoTarget", "1");
    const CoA::ClientHooks::OptionPatches& p = CoA::ClientHooks::options;
    CHECK(s_turnRate == 3.15f);
    CHECK(p.trackingTurnRateFixed);
    CHECK(p.doodadFadeStore && p.cameraCollisionFlags == 0x100171);
    CHECK(p.autoTargetAttacker);
    CHECK(!p.movementKeepsTracking);
    CHECK(!p.bobAtAnyDistance);
}

COA_TEST(Camera_ByteFlagCallbacks) {
    Start();
    using CoA::ClientHooks::options;
    // FUN_10199420 writes the stock jp only on "1"; FUN_10199da0 writes jmp on "1", the jp otherwise.
    CoA::Camera::OnHeadBobs(nullptr, nullptr, "1", nullptr);
    CHECK(options.bobAtAnyDistance);
    CoA::Camera::OnActionCam(nullptr, nullptr, "0", nullptr);
    CHECK(options.bobAtAnyDistance);
    CoA::Camera::OnActionCam(nullptr, nullptr, "1", nullptr);
    CHECK(!options.bobAtAnyDistance);
    CoA::Camera::OnHeadBobs(nullptr, nullptr, "1", nullptr);
    CoA::Camera::OnHeadBobs(nullptr, nullptr, "0", nullptr);
    CHECK(!options.bobAtAnyDistance);

    CoA::Camera::OnEnemyFocus(nullptr, nullptr, "1", nullptr);
    CHECK(options.movementKeepsTracking);
    CoA::Camera::OnEnemyFocus(nullptr, nullptr, "0", nullptr);
    CHECK(!options.movementKeepsTracking);

    CoA::Camera::OnM2Collision(nullptr, nullptr, "0", nullptr);
    CHECK(!options.doodadFadeStore && options.cameraCollisionFlags == 0x170);
    CoA::Camera::OnM2Collision(nullptr, nullptr, "1", nullptr);
    CHECK(options.doodadFadeStore && options.cameraCollisionFlags == 0x100171);

    CoA::Camera::OnAutoTarget(nullptr, nullptr, "0", nullptr);
    CHECK(!options.autoTargetAttacker);
    CoA::Camera::OnAutoTarget(nullptr, nullptr, "1", nullptr);
    CHECK(options.autoTargetAttacker);
}

COA_TEST(Camera_TurnSpeedClamped) {
    Start();
    CoA::Camera::OnTurnSpeed(nullptr, nullptr, "0.1", nullptr);
    CHECK(s_turnRate == 0.5f);
    CoA::Camera::OnTurnSpeed(nullptr, nullptr, "20", nullptr);
    CHECK(s_turnRate == 16.5f);
    CoA::Camera::OnTurnSpeed(nullptr, nullptr, "17", nullptr);
    CHECK(s_turnRate == 16.5f);
    CoA::Camera::OnTurnSpeed(nullptr, nullptr, "7", nullptr);
    CHECK(s_turnRate == 7.0f);
    CoA::Camera::OnTurnSpeed(nullptr, nullptr, "nan", nullptr);
    CHECK(std::isnan(s_turnRate));
}

COA_TEST(Camera_ActionTargetOffset) {
    Start();
    float x = 10.0f, y = 20.0f, z = 30.0f;
    Set("ActionCam", "0");
    CoA::Camera::ActionTargetOffset(1.0f, x, y, z);
    CHECK(x == 10.0f && y == 20.0f && z == 30.0f);

    Set("ActionCam", "1");
    Set("cameraActionDist", "2");
    Set("cameraActionAngle", "0.5");
    Set("cameraActionZ", "0.25");
    CoA::Camera::ActionTargetOffset(1.0f, x, y, z);
    CHECK(std::fabs(x - (10.0f + static_cast<float>(cos(1.5)) * 2.0f)) < 1e-5f);
    CHECK(std::fabs(y - (20.0f + static_cast<float>(sin(1.5)) * 2.0f)) < 1e-5f);
    CHECK(z == 30.25f);
}

COA_TEST(Camera_HitsRestoredInWorldOnly) {
    Start();
    int a = 0, b = 0;
    float alpha = 0.75f;
    int32_t bits = 0;
    memcpy(&bits, &alpha, 4);
    CHECK(CoA::Camera::RecordHit(&a, 0.75f) == bits);
    CoA::Camera::RecordHit(&b, 0.25f);
    CHECK(CoA::Camera::Hits().size() == 2);
    CoA::Camera::RestoreAlphas();
    CHECK(s_alphas[&a] == 0.75f && s_alphas[&b] == 0.25f);
    CHECK(CoA::Camera::Hits().empty());

    s_alphas.clear();
    CoA::Camera::RecordHit(&a, 0.5f);
    s_inWorld = false;
    CoA::Camera::RestoreAlphas();
    CHECK(s_alphas.empty() && CoA::Camera::Hits().empty());
}

COA_TEST(Camera_FadeOnlyWithoutM2Collision) {
    Start();
    Set("cameraM2Collision", "1");
    CHECK(!CoA::Camera::FadeM2s());
    Set("cameraM2Collision", "0");
    Set("cameraM2CollisionAlpha", "0.3");
    CHECK(CoA::Camera::FadeM2s());
    CHECK(CoA::Camera::FadeAlpha() == 0.3f);
}

COA_TEST(Camera_CollisionShift) {
    Start();
    float dx = 0.0f, dy = 0.0f;
    Set("ActionCam", "1");
    Set("cameraActionDist", "2");
    Set("cameraActionAngle", "0");
    CHECK(!CoA::Camera::CollisionShift(0x20000, 0.0f, 1.0f, dx, dy));
    CHECK(CoA::Camera::CollisionShift(0x10000, 0.0f, 1.0f, dx, dy));
    // shift = min(2, max(0, 2 - (1 - 1/9)))
    const float shift = 2.0f - (1.0f - 0.11111111f);
    CHECK(std::fabs(dx - shift) < 1e-6f && std::fabs(dy) < 1e-6f);
    CHECK(CoA::Camera::CollisionShift(0x10000, 0.0f, 5.0f, dx, dy));
    CHECK(dx == 0.0f);
    CHECK(CoA::Camera::CollisionShift(0x10000, 0.0f, -5.0f, dx, dy));
    CHECK(dx == 2.0f);
    Set("ActionCam", "0");
    CHECK(!CoA::Camera::CollisionShift(0x10000, 0.0f, 1.0f, dx, dy));
}

COA_TEST(Camera_ClearTargetFocus) {
    Start();
    Set("ActionCam", "1");
    Set("cameraTargetFocusEnemyEnable", "1");
    CoA::Camera::BeforeClearTarget(5, 6);
    CHECK(s_clears == 0);
    CoA::Camera::BeforeClearTarget(6, 6);
    CHECK(s_clears == 1);
    Set("cameraTargetFocusEnemyEnable", "0");
    CoA::Camera::BeforeClearTarget(6, 6);
    CHECK(s_clears == 1);
}

COA_TEST(Camera_AfterTargetChangeTracks) {
    Start();
    CHECK(!CoA::ClientHooks::afterTarget.Empty());
    s_target.guid = 42;
    s_target.canAttackNow = true;
    Set("ActionCam", "1");
    Set("cameraTargetFocusEnemyEnable", "0");
    Set("cameraTargetFocusInteractEnable", "0");
    CoA::ClientHooks::afterTarget.Run();
    CHECK(s_tracked.empty());
    Set("cameraTargetFocusEnemyEnable", "1");
    CoA::ClientHooks::afterTarget.Run();
    CHECK(s_tracked.size() == 1 && s_tracked[0] == 42);

    // Not attackable: a talkative target (reaction > 3) with interact focus.
    s_target.canAttackNow = false;
    s_target.reaction = 4;
    CoA::ClientHooks::afterTarget.Run();
    CHECK(s_tracked.size() == 1);
    Set("cameraTargetFocusInteractEnable", "1");
    CoA::ClientHooks::afterTarget.Run();
    CHECK(s_tracked.size() == 2);
    s_target.reaction = 3;
    CoA::ClientHooks::afterTarget.Run();
    CHECK(s_tracked.size() == 2);

    // A player target or none: nothing.
    s_target.reaction = 5;
    s_target.isPlayer = true;
    CoA::ClientHooks::afterTarget.Run();
    CHECK(s_tracked.size() == 2);
    s_target.isPlayer = false;
    s_haveTarget = false;
    CoA::ClientHooks::afterTarget.Run();
    CHECK(s_tracked.size() == 2);

    Set("ActionCam", "0");
    s_haveTarget = true;
    CoA::ClientHooks::afterTarget.Run();
    CHECK(s_tracked.size() == 2);
}

COA_TEST(Camera_TeleportOfTheMoverRestores) {
    Start();
    int model = 0, mover = 0, other = 0;
    s_mover = &mover;
    CoA::Camera::RecordHit(&model, 0.6f);
    CoA::ClientHooks::beforeTeleport.Run(&other);
    CHECK(CoA::Camera::Hits().size() == 1);
    CoA::ClientHooks::beforeTeleport.Run(&mover);
    CHECK(CoA::Camera::Hits().empty() && s_alphas[&model] == 0.6f);
}
