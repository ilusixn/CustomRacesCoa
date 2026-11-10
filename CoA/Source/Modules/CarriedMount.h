#ifndef COA_SOURCE_MODULES_CARRIEDMOUNT_H
#define COA_SOURCE_MODULES_CARRIEDMOUNT_H

#include <cstdint>

// Two-Players ogre (mod-subclass): like Cho'Gall in Heroes of the Storm, the male Two-Headed Ogre does not sit on a
// mount. He tucks it under his arm, on the model's held grip, and runs at the mount's speed.
namespace CoA::CarriedMount {
    constexpr uint32_t kTwoHeadedOgre = 85;
    constexpr uint32_t kFemale = 1;
    constexpr float kScale = 0.3f;

    bool CarriesMounts(bool player, uint32_t race, uint32_t sex);
}

#endif
