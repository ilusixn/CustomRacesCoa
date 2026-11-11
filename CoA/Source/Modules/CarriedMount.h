#ifndef COA_SOURCE_MODULES_CARRIEDMOUNT_H
#define COA_SOURCE_MODULES_CARRIEDMOUNT_H

#include <cstdint>
#include <string_view>

// Two-Players ogre (mod-subclass): like Cho'Gall in Heroes of the Storm, the male Two-Headed Ogre does not sit on a
// mount. He picks it up while the mount spell casts (Grab) and carries it on his raised hand, standing (GrabClosed)
// or moving (GrabThrown); everything else he does while carrying keeps the carrying pose, so the mount stays put.
namespace CoA::CarriedMount {
    constexpr uint32_t kTwoHeadedOgre = 85;
    constexpr uint32_t kFemale = 1;
    constexpr float kScale = 1.0f;
    constexpr float kOffsetY = 0.5f;
    constexpr float kOffsetZ = -1.0f;
    constexpr int32_t kCarryLink = 48;
    constexpr uint32_t kCarryStand = 227;
    constexpr uint32_t kCarryMove = 228;
    constexpr uint32_t kPickUp = 226;

    bool CarriesMounts(bool player, uint32_t race, uint32_t sex);
    uint32_t CarryingAnim(uint32_t anim);
    uint32_t PickUpAnim(uint32_t anim);
    bool CarryPointModel(std::string_view fileName);
}

#endif
