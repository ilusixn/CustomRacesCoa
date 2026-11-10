#include "CoA/Source/Modules/CarriedMount.h"
#include <algorithm>
#include <cctype>
#include <string>

namespace CoA::CarriedMount {
    bool CarriesMounts(bool player, uint32_t race, uint32_t sex) {
        return player && race == kTwoHeadedOgre && sex != kFemale;
    }

    uint32_t CarryingAnim(uint32_t anim) {
        switch (anim) {
            case 0:
                return kCarryStand;
            case 4: case 5: case 11: case 12: case 13: case 92: case 93: case 119: case 143:
                return kCarryMove;
            default:
                return anim;
        }
    }

    bool CarryPointModel(std::string_view fileName) {
        std::string lower(fileName);
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lower.find("twoheadedogremale.m2") != std::string::npos;
    }
}
