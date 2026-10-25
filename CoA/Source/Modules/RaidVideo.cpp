#include "CoA/Source/Modules/RaidVideo.h"

namespace {
    const CoA::RaidVideo::Entry kSettings[CoA::RaidVideo::kCount] = {
        { "farclip", "farclip_raid", "507" },
        { "particleDensity", "particleDensity_raid", "0.4" },
        { "extShadowQuality", "extShadowQuality_raid", "0" },
        { "environmentDetail", "environmentDetail_raid", "0.75" },
        { "groundEffectDensity", "groundEffectDensity_raid", "24" },
        { "groundEffectDist", "groundEffectDist_raid", "80.0" },
        { "textureFilteringMode", "textureFilteringMode_raid", "1" },
        { "projectedTextures", "projectedTextures_raid", "1" },
        { "fogDistance", "fogDistance_raid", "548" },
        { "entityShadows", "entityShadows_raid", "0" },
        { "animateClouds", "animateClouds_raid", "0" },
    };
}

const CoA::RaidVideo::Entry& CoA::RaidVideo::Setting(size_t index) {
    return kSettings[index];
}

std::string CoA::RaidVideo::BaseName(const std::string& name) {
    const size_t at = name.size() >= 5 ? name.find("_raid") : std::string::npos;

    if (at == std::string::npos) {
        return "";
    }

    std::string base = name;
    base.erase(at, 5);
    return base;
}
