#include "CoA/Source/Modules/CarriedMount.h"

namespace CoA::CarriedMount {
    bool CarriesMounts(bool player, uint32_t race, uint32_t sex) {
        return player && race == kTwoHeadedOgre && sex == kMale;
    }
}
