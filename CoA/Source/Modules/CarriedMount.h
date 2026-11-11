#ifndef COA_SOURCE_MODULES_CARRIEDMOUNT_H
#define COA_SOURCE_MODULES_CARRIEDMOUNT_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
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
    constexpr uint32_t kStandInHorse = 2404;

    struct CarryFit {
        float Scale = kScale;
        float Yaw = 0.0f;
        float Pitch = 0.0f;
        float Roll = 0.0f;
        float X = 0.0f;
        float Y = kOffsetY;
        float Z = kOffsetZ;
        uint32_t As = 0;
        float Pose = 0.0f;
        float Frame = 0.0f;
    };

    constexpr const char* kFitFields[] = { "scale", "yaw", "pitch", "roll", "x", "y", "z", "as", "pose", "frame" };
    constexpr size_t kFitAs = 7;
    constexpr size_t kFitFieldCount = sizeof(kFitFields) / sizeof(kFitFields[0]);

    float FitValue(const CarryFit& fit, size_t field);
    void SetFitValue(CarryFit& fit, size_t field, float value);
    CarryFit DefaultFit(std::string_view mountModel);
    std::map<uint32_t, CarryFit> ParseCarryFits(std::string_view text);
    std::string FormatCarryFits(const std::map<uint32_t, CarryFit>& fits);

    bool CarriesMounts(bool player, uint32_t race, uint32_t sex);
    uint32_t CarryingAnim(uint32_t anim);
    uint32_t PickUpAnim(uint32_t anim);
    bool CarryPointModel(std::string_view fileName);
}

#endif
