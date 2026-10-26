#ifndef COA_SOURCE_MODULES_CAMERACLIENT_H
#define COA_SOURCE_MODULES_CAMERACLIENT_H

#include <cstdint>

class C3Vector;
class CGCamera;
class CM2Model;

// AscCamera.cpp's client half: what its hooks read from client objects. The decomp sites call these.
namespace CoA::CameraClient {
    // sub_101992c0, after 0x601D60 (CGCamera::GetCameraPosition): the ActionCam offset from the target unit's facing.
    void AfterCameraPosition(const CGCamera* camera, C3Vector& position);
    // sub_10199050 before 0x605D60 (CGCamera::CollideCameraWithWorld): the faded M2s restored, then, with
    // cameraM2Collision 0, the M2s between the camera and the player's head (target + 1.5 x the player's scale) faded.
    void BeforeCollide(const CGCamera* camera, const C3Vector& target);
    // sub_10199050 after it: the ActionCam side shift of the target position (the caller's vector, written in place).
    void AfterCollide(const CGCamera* camera, uint32_t result, C3Vector& target, float distance);
    // FUN_1019c2f0, replacing 0x7A25F0 (AddHitDebugModel).
    void RecordHit(CM2Model* model);
}

#endif
