// AscCanToggle.cpp (see CanToggle.h).
#include "CoA/Source/Modules/CanToggle.h"
#include "CoA/Source/Modules/Account.h"
#include "CoA/Source/Modules/Config.h"
#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <vector>

namespace {
    CoA::CanToggle::Deps s_deps;

    const char* const kStandard[] = { "OK", "OUTSIDE_INTERACTION", "DISABLED", "ALREADY_ACTIVE", "ALREADY_INACTIVE", "AUCTION_HOUSE", "LEVEL_UP", "BANK",
        "GUILD_BANK", "REALM_BANK", "MAIL", "TRADE" };
    const char* const kFelforged[] = { "OK", "OUTSIDE_INTERACTION", "DISABLED", "ALREADY_ACTIVE", "ALREADY_INACTIVE", "CANNOT_BE_DISABLED",
        "OTHER_GAME_MODE_ACTIVE", "BLOCKED_AURAS", "AUCTION_HOUSE", "LEVEL_UP", "BANK", "GUILD_BANK", "REALM_BANK", "MAIL", "TRADE" };
    const char* const kBuildDraft[] = { "OK", "LEVEL_TOO_HIGH", "DISABLED", "ALREADY_ACTIVE", "ALREADY_INACTIVE", "NOT_HERO", "DRAFT_ACTIVE",
        "NOT_NEAR_GAME_MODE_NPC" };

    bool ConfigOn(const char* name) {
        const bool* b = CoA::Config::Bool(name);
        return b && *b;
    }

    bool HasPlayer() {
        return s_deps.hasPlayer && s_deps.hasPlayer();
    }

    // Mode bits 15..21 -> first + 0..6, then any of them -> OUTSIDE_INTERACTION.
    void InteractionReasons(std::vector<uint32_t>& out, uint32_t first) {
        const uint32_t mode = CoA::GameMode::Mode();

        for (uint32_t bit = 15; bit <= 21; ++bit) {
            if ((mode >> bit) & 1) {
                out.push_back(first + (bit - 15));
            }
        }

        if (mode & 0x3F8000) {
            out.push_back(1);
        }
    }

    int Result(lua_State* L, const std::vector<uint32_t>& reasons, const char* prefix, const char* const* names, uint32_t count) {
        lua_pushboolean(L, reasons.empty());

        if (reasons.empty()) {
            lua_pushnil(L);
            return 2;
        }

        lua_createtable(L, 0, static_cast<int>(reasons.size()));

        for (size_t i = 0; i < reasons.size(); ++i) {
            const std::string s = reasons[i] < count ? std::string("TOGGLE_") + prefix + "_" + names[reasons[i]] : "UNEXPECTED_ENUM_VALUE_" + std::to_string(reasons[i]);
            lua_pushnumber(L, static_cast<double>(i + 1));
            lua_pushstring(L, s.c_str());
            lua_settable(L, -3);
        }

        return 2;
    }

    int Standard(lua_State* L, const char* config, uint32_t modeBit, const char* prefix) {
        bool enable;

        if (!CoA::Script::ReadBool(L, enable)) {
            return 0;
        }

        std::vector<uint32_t> reasons;

        if (enable) {
            if (!ConfigOn(config)) {
                reasons.push_back(2);
            }

            InteractionReasons(reasons, 5);
        }

        if (enable == (((CoA::GameMode::Mode() >> modeBit) & 1) != 0)) {
            reasons.push_back(enable ? 3 : 4);
        }

        return Result(L, reasons, prefix, kStandard, 12);
    }

    int CanToggleCrusaderMode(lua_State* L) {
        return Standard(L, "CONFIG_CRUSADER_ENABLED", 12, "CRUSADER");
    }

    int CanToggleIronmanMode(lua_State* L) {
        return Standard(L, "CONFIG_IRONMAN_ENABLE", 1, "IRONMAN");
    }

    int CanToggleNightmareMode(lua_State* L) {
        return Standard(L, "CONFIG_NIGHTMARE_ENABLE", 8, "NIGHTMARE");
    }

    int CanToggleResoluteMode(lua_State* L) {
        return Standard(L, "CONFIG_RESOLUTE_ENABLE", 5, "RESOLUTE");
    }

    int CanToggleSurvivalistMode(lua_State* L) {   // the original's config name typo kept
        return Standard(L, "CONFIG_SURVIALIST_ENABLE", 2, "SURVIVALIST");
    }

    int CanToggleWildCardMode(lua_State* L) {
        return Standard(L, "CONFIG_WILD_CARD_ENABLE", 6, "WILDCARD");
    }

    // FUN_100d6180: also needs game event 0x148; only GM level >= 2 may disable; other modes and four auras (0x7282A0 on
    // the player) block enabling.
    int CanToggleFelforgedMode(lua_State* L) {
        bool enable;

        if (!CoA::Script::ReadBool(L, enable)) {
            return 0;
        }

        std::vector<uint32_t> reasons;

        if (!enable) {
            if (CoA::Account::GmLevel() < 2) {
                reasons.push_back(5);
            }
        } else {
            if (!ConfigOn("CONFIG_FELFORGED_ENABLE") || !CoA::GameEvents::IsActive(0x148)) {
                reasons.push_back(2);
            }

            InteractionReasons(reasons, 8);
        }

        const uint32_t mode = CoA::GameMode::Mode();

        if (enable == (((mode >> 7) & 1) != 0)) {
            reasons.push_back(enable ? 3 : 4);
        }

        if (enable) {
            if (mode & (0x2 | 0x4 | 0x100 | 0x20 | 0x1000)) {
                reasons.push_back(6);
            }

            if (HasPlayer() && s_deps.hasAura &&
                (s_deps.hasAura(0xC7B7E) || s_deps.hasAura(0x9788BF) || s_deps.hasAura(0xC7B8B) || s_deps.hasAura(0xF619C))) {
                reasons.push_back(7);
            }
        }

        return Result(L, reasons, "FELFORGED", kFelforged, 15);
    }

    // FUN_100d57d0: level <= 9, Hero class (10) on a player object, and no draft mode (mode bit 3); "already" compares
    // against the active spec's build-draft flag.
    int CanToggleBuildDraftMode(lua_State* L) {
        bool enable;

        if (!CoA::Script::ReadBool(L, enable)) {
            return 0;
        }

        std::vector<uint32_t> reasons;

        if (enable && !ConfigOn("CONFIG_BUILD_DRAFT_ENABLED")) {
            reasons.push_back(2);
        }

        const bool player = HasPlayer();

        if (enable && (!player || (s_deps.playerLevel ? s_deps.playerLevel() : 0) > 9)) {
            reasons.push_back(1);
        }

        if (enable == CoA::GameMode::SpecBuildDraft(CoA::GameMode::ActiveSpecIndex())) {
            reasons.push_back(enable ? 3 : 4);
        }

        if (enable) {
            if (!player || (s_deps.playerClass ? s_deps.playerClass() : 0) != 10) {
                reasons.push_back(5);
            }

            if (CoA::GameMode::Mode() & 8) {
                reasons.push_back(6);
            }
        }

        return Result(L, reasons, "BUILDDRAFT", kBuildDraft, 8);
    }

    // Enabling asks CONFIG_DRAFT_ENABLE (true when the config is absent); no reasons list.
    int CanToggleDraftMode(lua_State* L) {
        bool enable;

        if (!CoA::Script::ReadBool(L, enable)) {
            return 0;
        }

        bool ok = true;

        if (enable) {
            if (const bool* b = CoA::Config::Bool("CONFIG_DRAFT_ENABLE")) {
                ok = *b;
            }
        }

        lua_pushboolean(L, ok);
        lua_pushnil(L);
        return 2;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "CanToggleBuildDraftMode", CanToggleBuildDraftMode),
        COA_NATIVE("", "CanToggleCrusaderMode", CanToggleCrusaderMode),
        COA_NATIVE("", "CanToggleDraftMode", CanToggleDraftMode),
        COA_NATIVE("", "CanToggleFelforgedMode", CanToggleFelforgedMode),
        COA_NATIVE("", "CanToggleIronmanMode", CanToggleIronmanMode),
        COA_NATIVE("", "CanToggleNightmareMode", CanToggleNightmareMode),
        COA_NATIVE("", "CanToggleResoluteMode", CanToggleResoluteMode),
        COA_NATIVE("", "CanToggleSurvivalistMode", CanToggleSurvivalistMode),
        COA_NATIVE("", "CanToggleWildCardMode", CanToggleWildCardMode),
    };
}

CoA::CanToggle::Deps& CoA::CanToggle::Dependencies() {
    return s_deps;
}

void CoA::CanToggle::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
