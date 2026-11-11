#include "CoA/Source/Modules/CarriedMount.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
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
        return (lower.find("twoheadedogremale.m2") != std::string::npos
            || lower.find("\\chogall.m2") != std::string::npos);
    }

    bool FlyingModel(bool flies, bool hovers, bool walks, bool runs) {
        return flies || hovers || (!walks && !runs);
    }

    bool HoldsStill(const CarryFit& fit, bool flyingModel) {
        if (fit.Pose < 0.0f) {
            return false;
        }

        return fit.Pose > 0.0f || fit.Frame > 0.0f || flyingModel;
    }

    namespace {
        std::string Lower(std::string_view text) {
            std::string lower(text);
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return lower;
        }

        float* FieldOf(CarryFit& fit, size_t field) {
            float* fields[] = { &fit.Scale, &fit.Yaw, &fit.Pitch, &fit.Roll, &fit.X, &fit.Y, &fit.Z, nullptr, &fit.Pose,
                &fit.Frame };
            return field < sizeof(fields) / sizeof(fields[0]) ? fields[field] : nullptr;
        }
    }

    float FitValue(const CarryFit& fit, size_t field) {
        if (field == kFitAs) {
            return static_cast<float>(fit.As);
        }

        const float* value = FieldOf(const_cast<CarryFit&>(fit), field);
        return value ? *value : 0.0f;
    }

    void SetFitValue(CarryFit& fit, size_t field, float value) {
        if (field == kFitAs) {
            fit.As = value > 0.0f ? static_cast<uint32_t>(value + 0.5f) : 0;
        } else if (float* target = FieldOf(fit, field)) {
            *target = value;
        }
    }

    CarryFit DefaultFit(std::string_view mountModel) {
        static constexpr const char* kVehicles[] = { "hotrod", "rocketcar", "goblincar", "goblinkart", "gokart",
            "motorcycle", "navicar", "azucar" };
        static constexpr const char* kBillies[] = { "\\billiegoatmount\\", "\\billiegoatcelestial\\",
            "\\goatmount\\", "\\mountgoatbirthday19\\", "\\mounthellbillie\\", "\\mountspiderenforcer\\",
            "\\moneypig" };
        static constexpr const char* kSeekerLike[] = { "owlseeker", "raptor", "vulture", "raccoon", "speederbike",
            "mountdogsummer18", "cerberus", "pigpinata", "mountpig_rank18",
            "mounttigeraltered20" };
        const std::string lower = Lower(mountModel);
        CarryFit fit;

        for (const char* vehicle : kVehicles) {
            if (lower.find(vehicle) != std::string::npos) {
                fit.As = kStandInHorse;
            }
        }

        if (lower.find("drivemount") != std::string::npos) {
            fit.Scale = kDriveScale;
            fit.Roll = kDriveRoll;
            fit.Z = kDriveZ;
        }

        if (lower.find("nexagon") != std::string::npos) {
            fit.Yaw = kDiscYaw;
            fit.Roll = kDiscRoll;
            fit.Y = kDiscY;
            fit.Z = kDiscZ;
        }

        if (lower.find("hotsmounts") != std::string::npos && lower.find("horse") != std::string::npos) {
            fit.Yaw = kHotsHorseYaw;
            fit.X = kHotsHorseX;
            fit.Y = kHotsHorseY;
            fit.Z = kHotsHorseZ;
        }

        if (lower.find("hotsmounts") != std::string::npos && lower.find("sheep") != std::string::npos) {
            fit.Yaw = kHotsHorseYaw;
            fit.X = kHotsHorseX;
            fit.Y = kHotsHorseY;
            fit.Z = kHotsSheepZ;
        }

        if (lower.find("hotsmounts") != std::string::npos && (lower.find("battlebeast") != std::string::npos
            || lower.find("voidspeeder") != std::string::npos)) {
            fit.Yaw = kHotsHorseYaw;
            fit.Scale = kBattleBeastScale;
            fit.Roll = kBattleBeastRoll;
            fit.X = kBattleBeastX;
            fit.Y = kBattleBeastY;
            fit.Z = kBattleBeastZ;
        }

        for (const char* billie : kBillies) {
            if (lower.find("hotsmounts") != std::string::npos && lower.find(billie) != std::string::npos) {
                fit.Yaw = kBillieYaw;
                fit.Y = kBillieY;
                fit.Z = kBillieZ;
            }
        }

        for (const char* seekerLike : kSeekerLike) {
            if (lower.find("hotsmounts") != std::string::npos && lower.find(seekerLike) != std::string::npos) {
                fit.Scale = kSeekerScale;
                fit.Yaw = kBillieYaw;
                fit.Y = kSeekerY;
                fit.Z = kSeekerZ;
            }
        }

        if (lower.find("hotsmounts") != std::string::npos && lower.find("hearthstonecard") != std::string::npos) {
            fit.Scale = kSoapBarScale;
            fit.Yaw = kSoapBarYaw;
            fit.Roll = kSoapBarRoll;
            fit.X = kSoapBarX;
            fit.Y = kSoapBarY;
            fit.Z = kSoapBarZ;
        }

        if (lower.find("hotsmounts") != std::string::npos && lower.find("redhare") != std::string::npos) {
            fit.Scale = kSeekerScale;
            fit.Yaw = kBillieYaw;
            fit.Y = kSeekerY;
            fit.Z = kHareZ;
        }

        return fit;
    }

    std::map<uint32_t, CarryFit> ParseCarryFits(std::string_view text) {
        std::map<uint32_t, CarryFit> fits;
        size_t start = 0;

        while (start < text.size()) {
            size_t end = text.find('\n', start);
            end = end == std::string_view::npos ? text.size() : end;
            const std::string line(text.substr(start, end - start));
            start = end + 1;
            char* rest = nullptr;
            const unsigned long display = std::strtoul(line.c_str(), &rest, 10);

            if (!display || rest == line.c_str()) {
                continue;
            }

            CarryFit fit;
            std::string token;

            for (const char* c = rest;; ++c) {
                if (*c && !std::isspace(static_cast<unsigned char>(*c))) {
                    token += *c;
                    continue;
                }

                const size_t equals = token.find('=');

                for (size_t field = 0; equals != std::string::npos && field < kFitFieldCount; ++field) {
                    if (Lower(token.substr(0, equals)) == kFitFields[field]) {
                        SetFitValue(fit, field, std::strtof(token.c_str() + equals + 1, nullptr));
                    }
                }

                token.clear();

                if (!*c) {
                    break;
                }
            }

            fits[static_cast<uint32_t>(display)] = fit;
        }

        return fits;
    }

    std::string FormatCarryFits(const std::map<uint32_t, CarryFit>& fits) {
        std::string text;

        for (const auto& [display, fit] : fits) {
            text += std::to_string(display);

            for (size_t field = 0; field < kFitFieldCount; ++field) {
                char value[32];
                std::snprintf(value, sizeof(value), " %s=%g", kFitFields[field], FitValue(fit, field));
                text += value;
            }

            text += "\n";
        }

        return text;
    }
}
