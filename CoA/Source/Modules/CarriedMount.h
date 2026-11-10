#ifndef COA_SOURCE_MODULES_CARRIEDMOUNT_H
#define COA_SOURCE_MODULES_CARRIEDMOUNT_H

#include <cstdint>
#include <string_view>

// Two-Players ogre (mod-subclass): like Cho'Gall in Heroes of the Storm, the male Two-Headed Ogre does not sit on a
// mount. He carries it on his raised hand over the shoulder and runs at the mount's speed, with HotS's carrying
// animations stored in his model as GrabClosed (standing) and GrabThrown (moving).
namespace CoA::CarriedMount {
    constexpr uint32_t kTwoHeadedOgre = 85;
    constexpr uint32_t kFemale = 1;
    constexpr float kScale = 0.4f;
    constexpr int32_t kCarryLink = 48;
    constexpr uint32_t kCarryStand = 227;
    constexpr uint32_t kCarryMove = 228;

    bool CarriesMounts(bool player, uint32_t race, uint32_t sex);
    uint32_t CarryingAnim(uint32_t anim);
    bool CarryPointModel(std::string_view fileName);
}

#endif
