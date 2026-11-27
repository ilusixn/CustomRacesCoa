#ifndef COA_SOURCE_MODULES_HELDGRIPCLIENT_H
#define COA_SOURCE_MODULES_HELDGRIPCLIENT_H

#include "WoW/Common/ComponentCore/Types.h"

class CM2Model;

// HeldGrip's client half: CCharacterComponent::AddHandItem / RemoveHandItem (0x4EACD0 / 0x4EB0B0) ask it which link an
// off-hand item uses. Covers the world, corpses and the creation preview, which all end in AddHandItem.
namespace CoA::HeldGripClient {
    bool HeldDisplay(int32_t displayId);
    // Without waiting for the model to load: its attachment table once loaded, its file name before.
    bool HasHeldGrip(CM2Model* model);
    GEOCOMPONENTLINKS OffHandLink(CM2Model* model, int32_t displayId);
}

#endif
