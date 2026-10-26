#ifndef COA_SOURCE_MODULES_MOVEMENTFORCECLIENT_H
#define COA_SOURCE_MODULES_MOVEMENTFORCECLIENT_H

#include "CoA/Source/Modules/MovementForce.h"
#include <cstdint>

class C3Vector;
class CMovementShared;
class CMovementStatus;
class CMovement_C;

// AscMovementForce.cpp's client half: what the decomp sites call (MovementForce.h has the logic).
namespace CoA::MovementForceClient {
    // FUN_102b8cb0 after CMovementShared::PlotUnitMovement(time, move) (0x987D00).
    uint32_t AfterPlot(CMovementShared* movement, uint32_t time, C3Vector* move, uint32_t result);
    // FUN_102b8da0 around CMovement_C::UpdateStatusInternal (0x6EB730); nothing while 0x765 / 0x766 apply their movement
    // info (the DLL calls the original there, not its hook).
    CoA::MovementForce::StatusUpdate BeforeStatus(CMovement_C* movement, const CMovementStatus& status);
    void AfterStatus(const CoA::MovementForce::StatusUpdate& update, CMovement_C* movement, const CMovementStatus& status);
}

#endif
