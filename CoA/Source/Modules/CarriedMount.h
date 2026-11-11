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
    constexpr uint32_t kAnimWalk = 4;
    constexpr uint32_t kAnimRun = 5;
    constexpr uint32_t kAnimFly = 135;
    constexpr uint32_t kAnimFlyStand = 229;
    constexpr uint32_t kStandInHorse = 2404;
    constexpr float kDriveScale = 0.8f;
    constexpr float kDriveRoll = 90.0f;
    constexpr float kDriveZ = 1.3f;
    constexpr float kDiscYaw = 0.7f;
    constexpr float kDiscRoll = 90.0f;
    constexpr float kDiscY = 0.3f;
    constexpr float kDiscZ = 1.0f;
    constexpr float kHotsHorseYaw = 10.0f;
    constexpr float kHotsHorseX = -0.2f;
    constexpr float kHotsHorseY = 0.0f;
    constexpr float kHotsHorseZ = -0.6f;
    constexpr float kHotsSheepZ = -0.3f;
    constexpr float kBattleBeastScale = 0.7f;
    constexpr float kBattleBeastRoll = -90.0f;
    constexpr float kBattleBeastX = 0.0f;
    constexpr float kBattleBeastY = -0.7f;
    constexpr float kBattleBeastZ = 0.5f;
    constexpr float kBillieYaw = 15.0f;
    constexpr float kBillieY = -0.1f;
    constexpr float kBillieZ = 0.0f;
    constexpr float kSeekerScale = 0.8f;
    constexpr float kSeekerY = 0.0f;
    constexpr float kSeekerZ = -0.2f;
    constexpr float kSoapBarYaw = 15.0f;
    constexpr float kSoapBarRoll = 90.0f;
    constexpr float kSoapBarScale = 0.8f;
    constexpr float kSoapBarX = -0.2f;
    constexpr float kSoapBarY = 0.4f;
    constexpr float kSoapBarZ = 0.7f;
    constexpr float kHareZ = -0.5f;
    constexpr float kUnicornX = 0.0f;
    constexpr float kUnicornY = 0.0f;
    constexpr float kPhoenixX = -0.2f;
    constexpr float kPhoenixY = -0.1f;
    constexpr float kPhoenixZ = -0.9f;
    constexpr float kSurfboardY = -0.6f;

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
    bool FlyingModel(bool flies, bool hovers, bool walks, bool runs);
    bool HoldsStill(const CarryFit& fit, bool flyingModel);
}

#endif
