#ifndef COA_SOURCE_MODULES_HELDGRIP_H
#define COA_SOURCE_MODULES_HELDGRIP_H

#include <cstdint>
#include <string_view>

// Two-Players ogre: pouches, books and orbs in the off hand hang down from the fist, turned outward, on the model's own
// held grip instead of the weapon grip. Weapons and shields keep their normal grips.
namespace CoA::HeldGrip {
    constexpr int32_t kLink = 47;

    bool HeldLike(int32_t classId, int32_t subclassId, int32_t inventoryType);
    // The Two-Players ogre models (and their Cho'Gall look) carry the held grip; known by file name before they load.
    bool HeldGripModel(std::string_view fileName);
}

#endif
