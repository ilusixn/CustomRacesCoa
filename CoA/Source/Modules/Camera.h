#ifndef COA_SOURCE_MODULES_CAMERA_H
#define COA_SOURCE_MODULES_CAMERA_H

#include <cstdint>
#include <vector>

struct CVar;

// AscCamera.cpp: action camera, camera target focus and camera M2 collision (module init FUN_1019c510; its twelve
// FUN_10114540 CVars checked against the genuine DLL, 0x1019C7CC .. 0x1019C934). The callbacks' byte patches are
// ClientHooks::options flags; the decomp sites call the hooks below.
//
//   0x601D60 CGCamera::GetCameraPosition      after: + {cos, sin}(facing + cameraActionAngle) * dist, + Z (sub_101992c0)
//   0x605D60 CGCamera::CollideCameraWithWorld the faded M2s restored; with cameraM2Collision 0 the M2s between the
//                                             camera and the player fade; after: the ActionCam side shift (sub_10199050)
//   0x7A25F0 AddHitDebugModel                 REPLACED: each hit model and its alpha recorded (FUN_1019c2f0)
//   0x401480 SMSG_TRANSFER_PENDING            the faded M2s restored first, then 1 (sub_1019c970)
//   0x5FF950 / 0x5FFA60 ZoomIn / ZoomOut      pass-through
//   0x5241B0 CGGameUI::ClearTarget            ActionCam + enemy focus: the player's tracking cleared (sub_101993b0)
//   after 0x524BF0 (list 0x10BE2970)          FUN_1019c420: track a hostile / talkative new target
//   before 0x6ECF80 (list 0x10BE2BAC)         sub_10198e60: the active mover's teleport restores the faded M2s
namespace CoA::Camera {
    // FUN_1019c420's view of the player and the locked target.
    struct Target {
        uint64_t guid = 0;          // 0xBD07B0
        bool isPlayer = false;      // the unit's type id (+0x14) is 4
        bool canAttackNow = false;  // 0x729A70(player, unit)
        int32_t reaction = 0;       // 0x7251C0(player, unit)
    };

    struct Deps {
        bool (*inWorld)() = nullptr;                                // 0xBD0792
        void (*setModelAlpha)(void* model, float alpha) = nullptr;  // CM2Model +0x17C
        void (*setTrackingTurnRate)(float rate) = nullptr;          // 0xCA11DC
        void (*clearPlayerTracking)() = nullptr;                    // 0x7272C0(player, 0, 1); nothing without a player
        // False without a player or without the target as a unit.
        bool (*target)(Target& out) = nullptr;
        void (*trackTarget)(uint64_t guid) = nullptr;               // 0x72B4A0(player, &guid, 1)
        bool (*isActiveMover)(void* unit) = nullptr;                // 0x4D43C0
    };

    Deps& Dependencies();

    bool OnActionCam(CVar* var, const char* oldValue, const char* value, void* arg);     // FUN_10199420
    bool OnEnemyFocus(CVar* var, const char* oldValue, const char* value, void* arg);    // FUN_1019a9e0
    bool OnHeadBobs(CVar* var, const char* oldValue, const char* value, void* arg);      // FUN_10199da0
    bool OnTurnSpeed(CVar* var, const char* oldValue, const char* value, void* arg);     // FUN_1019bcb0
    bool OnM2Collision(CVar* var, const char* oldValue, const char* value, void* arg);   // FUN_1019a190
    bool OnAutoTarget(CVar* var, const char* oldValue, const char* value, void* arg);    // FUN_10199650

    bool ActionCam();
    // sub_101992c0, after 0x601D60: with ActionCam and the camera's target unit, the position moves by the offsets.
    void ActionTargetOffset(float facing, float& x, float& y, float& z);

    // The faded M2s (0x10BDED20): {model, its alpha before the fade}.
    struct Hit {
        void* model;
        float alpha;
    };
    const std::vector<Hit>& Hits();
    // FUN_1019c2f0 (replaces 0x7A25F0): returns the alpha's bits, as the original.
    int32_t RecordHit(void* model, float alpha);
    // FUN_1019c3d0: in the world each recorded model gets its alpha back; the list is cleared either way.
    void RestoreAlphas();

    // sub_10199050, before the original: true when the M2s between the camera and the player's head are to be faded
    // (cameraM2Collision 0); FadeAlpha is the alpha they get.
    bool FadeM2s();
    float FadeAlpha();
    // sub_10199050, after the original: with result bit 0x10000 and ActionCam, the camera moves back towards the
    // target by min(dist, max(0, dist - (distance - 1/9))) along (facing + angle): pos -= {dx, dy}. False otherwise.
    bool CollisionShift(uint32_t result, float facing, float distance, float& dx, float& dy);

    // sub_101993b0, before 0x5241B0: ActionCam + cameraTargetFocusEnemyEnable and the cleared guid is the locked one.
    void BeforeClearTarget(uint64_t guid, uint64_t lockedTarget);
    void AfterTargetChange();           // FUN_1019c420
    void BeforeTeleport(void* unit);    // sub_10198e60

    void Init();
    // Tests only.
    void Reset();
}

#endif
