// AscGameMode.cpp: the game-mode manager, its packets and natives (see GameMode.h). The equipment-set lookups are
// answered by GameModeClient.cpp; the active spec by character advancement (Plan 5).
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <string>
#include <vector>

namespace {
    struct SpecRecord {
        uint32_t flags = 0;
        uint32_t preset = 0;
        uint8_t hasPreset = 0;
        uint64_t equipSet = 0;
        uint8_t hasEquipSet = 0;
    };

    uint32_t s_mode = 0;
    uint32_t s_mask = 0;
    SpecRecord s_specs[20];
    std::vector<void (*)()> s_modeListeners;
    CoA::GameMode::Deps s_deps;

    // handler_0x090b
    int32_t OnGameModeChanged(void*, uint32_t, uint32_t, CDataStore* msg) {
        s_mode = CoA::Read::U32(msg);
        s_mask = s_mode;

        for (auto cb : s_modeListeners) {   // FUN_1031bd50: the skill-card containers reset (their module registers)
            cb();
        }

        if (CoA::ClientState::InWorld()) {
            CoA::Events::Signal("ASCENSION_CUSTOM_GAME_MODE_CHANGED", "%u", s_mode);
        }

        return 1;
    }

    // FUN_1031eca0
    int32_t OnSpecializationFlags(void*, uint32_t, uint32_t, CDataStore* msg) {
        const uint32_t spec = CoA::Read::U32(msg);

        if (spec >= 20) {
            return 1;
        }

        const uint32_t flags = CoA::Read::U32(msg);
        const uint32_t old = s_specs[spec].flags;
        s_specs[spec].flags = flags;
        CoA::Events::Signal("SPECIALIZATION_FLAGS_UPDATED", "%u%u", spec, flags);

        if ((old & 1) != (flags & 1)) {
            CoA::Events::Signal("ASCENSION_CUSTOM_GAME_MODE_CHANGED", "%u", s_mode);
        }

        return 1;
    }

    // handler_0x0737
    int32_t OnMysticEnchantLink(void*, uint32_t, uint32_t, CDataStore* msg) {
        const uint32_t spec = CoA::Read::U32(msg);

        if (spec >= 20) {
            return 1;
        }

        const uint8_t has = CoA::Read::U8(msg);
        s_specs[spec].preset = has ? CoA::Read::U32(msg) : 0;
        s_specs[spec].hasPreset = has ? 1 : 0;
        CoA::Events::Signal("MYSTIC_ENCHANT_SPECIALIZATION_LINK_UPDATED", nullptr);
        return 1;
    }

    // handler_0x0738
    int32_t OnEquipmentSetLink(void*, uint32_t, uint32_t, CDataStore* msg) {
        const uint32_t spec = CoA::Read::U32(msg);

        if (spec >= 20) {
            return 1;
        }

        const uint8_t has = CoA::Read::U8(msg);
        s_specs[spec].equipSet = has ? CoA::Read::U64(msg) : 0;
        s_specs[spec].hasEquipSet = has ? 1 : 0;
        CoA::Events::Signal("EQUIPMENT_SET_SPECIALIZATION_LINK_UPDATED", nullptr);
        return 1;
    }

    // FUN_1031e8f0 (glue screen)
    void ResetState() {
        s_mode = 0;
        s_mask = 0;

        for (SpecRecord& r : s_specs) {
            r = SpecRecord();
        }
    }

    // handler_GetCustomGameMode: bit 0x800 (build draft) only while the active spec has draft on.
    int GetCustomGameMode(lua_State* L) {
        const int32_t spec = CoA::GameMode::ActiveSpecIndex();
        const bool draft = spec >= 0 && spec < 20 && (s_specs[spec].flags & 1);
        CoA::Script::PushInt(L, static_cast<int32_t>(draft ? s_mode : (s_mode & 0xFFFFF7FF)));
        return 1;
    }

    int IsBuildDraftModeEnabled(lua_State* L) {
        uint32_t spec;

        if (!CoA::Script::ReadNumber(L, spec) || spec == 0) {
            return 0;
        }

        CoA::Script::PushBool(L, spec - 1 < 20 && (s_specs[spec - 1].flags & 1));
        return 1;
    }

    int GetMysticEnchantPresetIDForSpecialization(lua_State* L) {
        uint32_t spec;

        if (!CoA::Script::ReadNumber(L, spec) || spec == 0) {
            return 0;
        }

        if (spec - 1 < 20 && s_specs[spec - 1].hasPreset) {
            CoA::Script::PushInt(L, static_cast<int32_t>(s_specs[spec - 1].preset));
            return 1;
        }

        lua_pushnil(L);
        return 1;
    }

    // handler_GetEquipmentSetIDForSpecialization: the name of the client's equipment set the link points at (a set
    // whose id is the link's low word, the high word zero).
    int GetEquipmentSetIDForSpecialization(lua_State* L) {
        uint32_t spec;

        if (!CoA::Script::ReadNumber(L, spec) || spec == 0) {
            return 0;
        }

        if (spec - 1 < 20 && s_specs[spec - 1].hasEquipSet) {
            const SpecRecord& r = s_specs[spec - 1];

            if ((r.equipSet >> 32) == 0 && s_deps.equipmentSetName) {
                if (const char* name = s_deps.equipmentSetName(static_cast<uint32_t>(r.equipSet))) {
                    CoA::Script::PushStr(L, name);
                    return 1;
                }
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // FUN_1031f390: {number[, number]} -> CMSG 0x739 { u32 spec-1, u8 has, [u32 preset] }
    int SetMysticEnchantPresetIDForSpecialization(lua_State* L) {
        const bool has = lua_type(L, 2) > 0;

        if (has ? !CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TNUMBER }) : !CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TNIL })) {
            return 0;
        }

        const int32_t spec = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1));
        const int32_t preset = has ? CoA::Script::ToInt(CoA::Script::CheckNumber(L, 2)) : 0;

        if (spec != 0 && static_cast<uint32_t>(spec - 1) < 20) {
            CoA::Packet p(0x739);
            p.U32(static_cast<uint32_t>(spec - 1)).U8(has ? 1 : 0);

            if (has) {
                p.U32(static_cast<uint32_t>(preset));
            }

            p.Send();
        }

        return 0;
    }

    // FUN_1031f1e0 / FUN_100d0500: {number[, string]}. A named set is looked up; an unknown name sends nothing.
    // -> CMSG 0x73A { u32 spec-1, u8 has, [u32 set id, u32 0] }
    int SetEquipmentSetIDForSpecialization(lua_State* L) {
        const bool named = lua_type(L, 2) > 0;

        if (named ? !CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TSTRING }) : !CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TNIL })) {
            return 0;
        }

        const uint32_t spec = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)));

        if (spec == 0) {
            return 0;
        }

        uint32_t setId = 0;

        if (named) {
            const std::string name = CoA::Script::CheckString(L, 2);

            if (!s_deps.equipmentSetId || !s_deps.equipmentSetId(name.c_str(), setId)) {
                return 0;
            }
        }

        if (spec - 1 < 20) {
            CoA::Packet p(0x73A);
            p.U32(spec - 1).U8(named ? 1 : 0);

            if (named) {
                p.U32(setId).U32(0);
            }

            p.Send();
        }

        return 0;
    }

    // handler_IsDraftMode: mode bit 3.
    int IsDraftMode(lua_State* L) {
        CoA::Script::PushBool(L, ((s_mode >> 3) & 1) != 0);
        return 1;
    }

    // FUN_100de480: CMSG 0x5A4 {u32 length, mode name (no NUL), u8 enable}; unknown bits send "None".
    void SendToggleMode(uint32_t bit, bool enable) {
        const char* name;

        switch (bit) {
            case 0x2: name = "Ironman"; break;
            case 0x4: name = "Survivalist"; break;
            case 0x8: name = "Draft"; break;
            case 0x20: name = "Resolute"; break;
            case 0x40: name = "WildCard"; break;
            case 0x80: name = "Felforged"; break;
            case 0x100: name = "Nightmare"; break;
            case 0x800: name = "BuildDraft"; break;
            case 0x1000: name = "Crusader"; break;
            default: name = "None"; break;
        }

        const uint32_t len = static_cast<uint32_t>(strlen(name));
        CoA::Packet(0x5A4).U32(len).Data(name, len).U8(enable ? 1 : 0).Send();
    }

    template <uint32_t Bit>
    int ToggleMode(lua_State* L) {
        bool enable;

        if (!CoA::Script::ReadBool(L, enable)) {
            return 0;
        }

        SendToggleMode(Bit, enable);
        CoA::Script::PushBool(L, true);
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "IsDraftMode", IsDraftMode),
        COA_NATIVE("", "ToggleIronmanMode", ToggleMode<0x2>),
        COA_NATIVE("", "ToggleSurvivalistMode", ToggleMode<0x4>),
        COA_NATIVE("", "ToggleResoluteMode", ToggleMode<0x20>),
        COA_NATIVE("", "ToggleWildCardMode", ToggleMode<0x40>),
        COA_NATIVE("", "ToggleFelforgedMode", ToggleMode<0x80>),
        COA_NATIVE("", "ToggleNightmareMode", ToggleMode<0x100>),
        COA_NATIVE("", "ToggleBuildDraftMode", ToggleMode<0x800>),
        COA_NATIVE("", "ToggleCrusaderMode", ToggleMode<0x1000>),
        COA_NATIVE("", "GetCustomGameMode", GetCustomGameMode),
        COA_NATIVE("", "IsBuildDraftModeEnabled", IsBuildDraftModeEnabled),
        COA_NATIVE("", "GetMysticEnchantPresetIDForSpecialization", GetMysticEnchantPresetIDForSpecialization),
        COA_NATIVE("", "SetMysticEnchantPresetIDForSpecialization", SetMysticEnchantPresetIDForSpecialization),
        COA_NATIVE("", "GetEquipmentSetIDForSpecialization", GetEquipmentSetIDForSpecialization),
        COA_NATIVE("", "SetEquipmentSetIDForSpecialization", SetEquipmentSetIDForSpecialization),
    };
}

uint32_t CoA::GameMode::Mode() {
    return s_mode;
}

uint32_t CoA::GameMode::ActiveMask() {
    return s_mask;
}

bool CoA::GameMode::SpecBuildDraft(int32_t spec) {
    return static_cast<uint32_t>(spec) < 20 && (s_specs[spec].flags & 1) != 0;
}

// FUN_1016f580: the player's record in the CharacterAdvancement manager, +0x14; -1 without one.
int32_t CoA::GameMode::ActiveSpecIndex() {
    return s_deps.activeSpecIndex ? s_deps.activeSpecIndex() : -1;
}

void CoA::GameMode::OnModeChanged(void (*cb)()) {
    s_modeListeners.push_back(cb);
}

CoA::GameMode::Deps& CoA::GameMode::Dependencies() {
    return s_deps;
}

void CoA::GameMode::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    CoA::Lifecycle::OnGlueScreen(ResetState);
    COA_PACKET(0x90B, OnGameModeChanged);
    COA_PACKET(0x5CC, OnSpecializationFlags);
    COA_PACKET(0x737, OnMysticEnchantLink);
    COA_PACKET(0x738, OnEquipmentSetLink);
    COA_EVENT("ASCENSION_CUSTOM_GAME_MODE_CHANGED");
    COA_EVENT("SPECIALIZATION_FLAGS_UPDATED");
    COA_EVENT("MYSTIC_ENCHANT_SPECIALIZATION_LINK_UPDATED");
    COA_EVENT("EQUIPMENT_SET_SPECIALIZATION_LINK_UPDATED");
}

void CoA::GameMode::Reset() {
    ResetState();
    s_modeListeners.clear();
}
