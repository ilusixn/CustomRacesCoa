#include "CoA/Source/Modules/TwoPlayersArt.h"

namespace CoA::TwoPlayersArt {
    uint32_t ItemSex(uint32_t sex, int32_t maleDisplay, int32_t femaleDisplay) {
        if (sex == 1 && maleDisplay != 0 && maleDisplay == femaleDisplay) {
            return 0;
        }

        return sex;
    }
}
