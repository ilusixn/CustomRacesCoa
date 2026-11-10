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
            case 1: case 6: case 7: case 131: case 132: case 196:
                return anim;
            case 4: case 5: case 11: case 12: case 13: case 42: case 43: case 44: case 45: case 92: case 93:
            case 119: case 143: case 223:
                return kCarryMove;
            default:
                return kCarryStand;
        }
    }

    uint32_t PickUpAnim(uint32_t anim) {
        switch (anim) {
            case 2: case 31: case 32: case 33: case 51: case 52: case 53: case 54: case 124: case 125:
                return kPickUp;
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
