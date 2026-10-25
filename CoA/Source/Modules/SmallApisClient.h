#ifndef COA_SOURCE_MODULES_SMALLAPISCLIENT_H
#define COA_SOURCE_MODULES_SMALLAPISCLIENT_H

#include <cstdint>

// AscSmallApis.cpp's client half; DayNight::SetDirection asks whether C_Sun froze the sun direction.
namespace CoA::SmallApisClient {
    // FUN_1026c920: after the first C_Sun:SetX / SetY / SetZ the three sun-direction stores (0x7EECA0 / A9 / B2) are
    // NOPed for good.
    bool SunFrozen();
}

#endif
