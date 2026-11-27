#include "CoA/Source/Modules/HeldGrip.h"
#include <algorithm>
#include <cctype>
#include <string>

namespace CoA::HeldGrip {
    bool HeldLike(int32_t classId, int32_t subclassId, int32_t inventoryType) {
        constexpr int32_t kWeapon = 2, kMiscWeapon = 14, kHoldable = 23;
        return inventoryType == kHoldable || (classId == kWeapon && subclassId == kMiscWeapon);
    }

    bool HeldGripModel(std::string_view fileName) {
        std::string lower(fileName);
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lower.find("twoheadedogremale") != std::string::npos || lower.find("chogall") != std::string::npos;
    }
}
