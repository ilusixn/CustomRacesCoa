#include "CoA/Source/Modules/HeldGrip.h"

namespace CoA::HeldGrip {
    bool HeldLike(int32_t classId, int32_t subclassId, int32_t inventoryType) {
        constexpr int32_t kWeapon = 2, kMiscWeapon = 14, kHoldable = 23;
        return inventoryType == kHoldable || (classId == kWeapon && subclassId == kMiscWeapon);
    }
}
