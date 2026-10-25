// AscMysticEnchantPreset.cpp (see MysticEnchantPreset.h).
#include "CoA/Source/Modules/MysticEnchantPreset.h"
#include "CoA/Source/Modules/GameMode.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/MysticEnchant.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <vector>

namespace {
    using CoA::Script::PushBool;
    using CoA::Script::PushInt;
    using CoA::Script::PushNum;
    using CoA::Script::PushStr;
    using CoA::Script::ReadNumber;

    const char* const kSave[4] = {   // 0x10B1F9E4
        "RE_PRESET_SAVE_OK", "RE_PRESET_SAVE_UNKNOWN", "RE_PRESET_SAVE_BUILD_DRAFT", "RE_PRESET_SAVE_BAD_CLASS",
    };
    const char* const kSetActive[5] = {   // 0x10B1D0C8
        "RE_PRESET_SET_ACTIVE_OK", "RE_PRESET_SET_ACTIVE_UNKNOWN", "RE_PRESET_SET_ACTIVE_BUILD_DRAFT",
        "RE_PRESET_SET_ACTIVE_NOT_WHILE_CASTING", "RE_PRESET_SET_ACTIVE_BAD_CLASS",
    };
    const char* const kUnlock[7] = {   // 0x10B22924
        "RE_PRESET_UNLOCK_OK", "RE_PRESET_UNLOCK_UNKNOWN", "RE_PRESET_UNLOCK_MAX_VALUE_REACHED", "RE_PRESET_UNLOCK_NO_MONEY",
        "RE_PRESET_UNLOCK_BUILD_DRAFT", "RE_PRESET_UNLOCK_NOT_WHILE_CASTING", "RE_PRESET_UNLOCK_BAD_CLASS",
    };
    const uint32_t kUnlockToken = 0x1B9271;

    struct Presets {
        uint32_t active = 0;                       // +0x00
        std::vector<std::vector<uint32_t>> list;   // +0x04
        uint32_t pending = 0xFFFFFFFF;             // +0x10
    } s_presets;

    std::string EnumName(const char* const* table, uint32_t count, uint32_t v) {
        return v < count ? std::string(table[v]) : "UNEXPECTED_ENUM_VALUE_" + std::to_string(v);
    }

    // The error-list pushers FUN_102F18D0 / FUN_102F1AC0 / FUN_102F1CB0: {[i] = name}.
    void PushErrors(lua_State* L, const char* const* table, uint32_t count, const std::vector<uint32_t>& e) {
        lua_createtable(L, 0, static_cast<int>(e.size()));
        lua_checkstack(L, 2);

        for (size_t i = 0; i < e.size(); ++i) {
            PushNum(L, static_cast<double>(i + 1));
            PushStr(L, EnumName(table, count, e[i]).c_str());
            lua_settable(L, -3);
        }
    }

    bool Draft() {
        return CoA::GameMode::SpecBuildDraft(CoA::GameMode::ActiveSpecIndex());
    }

    std::vector<uint32_t> CheckActivate() {   // FUN_102F1EA0 (the preset index is not examined)
        std::vector<uint32_t> e;

        if (Draft()) {
            e.push_back(2);
        }

        if (CoA::MysticEnchant::PlayerCasting()) {
            e.push_back(3);
        }

        if (CoA::MysticEnchant::PlayerIsCoAClass()) {
            e.push_back(4);
        }

        return e;
    }

    std::vector<uint32_t> CheckSave() {   // FUN_102F1FC0
        std::vector<uint32_t> e;

        if (Draft()) {
            e.push_back(2);
        }

        if (CoA::MysticEnchant::PlayerIsCoAClass()) {
            e.push_back(3);
        }

        return e;
    }

    std::vector<uint32_t> CheckUnlock() {   // FUN_102F20B0
        std::vector<uint32_t> e;

        if (s_presets.list.size() > 99) {
            e.push_back(2);
        }

        if (CoA::MysticEnchant::BagItemCount(kUnlockToken) == 0) {
            e.push_back(3);
        }

        if (Draft()) {
            e.push_back(4);
        }

        if (CoA::MysticEnchant::PlayerCasting()) {
            e.push_back(5);
        }

        if (CoA::MysticEnchant::PlayerIsCoAClass()) {
            e.push_back(6);
        }

        return e;
    }

    int Activate(lua_State* L) {   // 0x102F3090
        uint32_t index;

        if (!ReadNumber(L, index) || index == 0) {
            return 0;
        }

        CoA::Packet(0x60C).U32(index - 1).Send();
        s_presets.pending = index - 1;
        PushBool(L, true);
        return 1;
    }

    int GetPresetData(lua_State* L) {   // 0x102F3490
        uint32_t index;

        if (!ReadNumber(L, index) || index == 0) {
            return 0;
        }

        static const std::vector<uint32_t> kEmpty;   // DAT_10BE3A2C
        const std::vector<uint32_t>& v = index - 1 < s_presets.list.size() ? s_presets.list[index - 1] : kEmpty;
        lua_createtable(L, 0, static_cast<int>(v.size()));
        lua_checkstack(L, 2);

        for (size_t i = 0; i < v.size(); ++i) {
            PushNum(L, static_cast<double>(i + 1));
            PushInt(L, static_cast<int32_t>(v[i]));
            lua_settable(L, -3);
        }

        PushBool(L, s_presets.active == index - 1);
        return 2;
    }

    int GetNumPresets(lua_State* L) {   // 0x102F3460
        PushInt(L, static_cast<int32_t>(s_presets.list.size()));
        return 1;
    }

    int Unlock(lua_State* L) {   // 0x102F3610
        CoA::Packet(0x615).Send();
        PushBool(L, true);
        return 1;
    }

    int PushCheck(lua_State* L, const std::vector<uint32_t>& e, const char* const* table, uint32_t count) {
        PushBool(L, e.empty());

        if (e.empty()) {
            lua_pushnil(L);
        } else {
            PushErrors(L, table, count, e);
        }

        return 2;
    }

    int CanSave(lua_State* L) {   // 0x102F32C0
        uint32_t index;

        if (!ReadNumber(L, index) || index == 0) {
            return 0;
        }

        return PushCheck(L, CheckSave(), kSave, 4);
    }

    int CanActivate(lua_State* L) {   // 0x102F31B0
        uint32_t index;

        if (!ReadNumber(L, index) || index == 0) {
            return 0;
        }

        return PushCheck(L, CheckActivate(), kSetActive, 5);
    }

    int CanUnlock(lua_State* L) {   // 0x102F33C0
        return PushCheck(L, CheckUnlock(), kUnlock, 7);
    }

    int ResultIndex(const std::string& s, const char* const* table, uint32_t count) {
        for (uint32_t i = 0; i < count; ++i) {
            if (s == table[i]) {
                return static_cast<int>(i);
            }
        }

        CoA::Log::Printf("Unexpected Value: %s", s.c_str());
        return -1;
    }

    int32_t OnPresetData(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x5FF
        const uint32_t n = CoA::Read::U32(msg);

        if (CoA::Read::Failed(msg) || n > CoA::Read::Remaining(msg) / 4) {
            return 1;
        }

        s_presets.list.assign(n, {});

        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t m = CoA::Read::U32(msg);

            if (CoA::Read::Failed(msg) || m > CoA::Read::Remaining(msg) / 4) {
                break;
            }

            s_presets.list[i].assign(m, 0);

            for (uint32_t k = 0; k < m; ++k) {
                s_presets.list[i][k] = CoA::Read::U32(msg);
            }
        }

        return 1;
    }

    int32_t OnActivePreset(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x600
        s_presets.active = CoA::Read::U32(msg);
        return 1;
    }

    int32_t OnSaveResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x602
        const std::string r = CoA::Read::Str(msg);

        if (ResultIndex(r, kSave, 4) == 0 && s_presets.active < s_presets.list.size()) {
            s_presets.list[s_presets.active] = CoA::MysticEnchant::Slots();
        }

        CoA::Events::Signal("MYSTIC_ENCHANT_PRESET_SAVE_RESULT", "%s", r.c_str());
        return 1;
    }

    int32_t OnSetActiveResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x60D
        const std::string r = CoA::Read::Str(msg);

        if (ResultIndex(r, kSetActive, 5) == 0) {
            s_presets.active = s_presets.pending;
        }

        s_presets.pending = 0xFFFFFFFF;
        CoA::Events::Signal("MYSTIC_ENCHANT_PRESET_SET_ACTIVE_RESULT", "%s", r.c_str());
        return 1;
    }

    int32_t OnUnlockResult(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x616
        const std::string r = CoA::Read::Str(msg);

        if (ResultIndex(r, kUnlock, 7) == 0) {
            s_presets.list.emplace_back();
        }

        CoA::Events::Signal("MYSTIC_ENCHANT_PRESET_UNLOCK_RESULT", "%s", r.c_str());
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_MysticEnchantPreset", "Activate", Activate),
        COA_NATIVE("C_MysticEnchantPreset", "GetPresetData", GetPresetData),
        COA_NATIVE("C_MysticEnchantPreset", "GetNumPresets", GetNumPresets),
        COA_NATIVE("C_MysticEnchantPreset", "Unlock", Unlock),
        COA_NATIVE("C_MysticEnchantPreset", "CanSave", CanSave),
        COA_NATIVE("C_MysticEnchantPreset", "CanActivate", CanActivate),
        COA_NATIVE("C_MysticEnchantPreset", "CanUnlock", CanUnlock),
    };
}

void CoA::MysticEnchantPreset::Reset() {
    s_presets.active = 0xFFFFFFFF;
    s_presets.list.clear();
    s_presets.pending = 0;
}

void CoA::MysticEnchantPreset::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_PACKET(0x5FF, OnPresetData);
    COA_PACKET(0x600, OnActivePreset);
    COA_PACKET(0x602, OnSaveResult);
    COA_PACKET(0x60D, OnSetActiveResult);
    COA_PACKET(0x616, OnUnlockResult);
    CoA::Lifecycle::OnGlueScreen(Reset);
    COA_EVENT("MYSTIC_ENCHANT_PRESET_SAVE_RESULT");
    COA_EVENT("MYSTIC_ENCHANT_PRESET_SET_ACTIVE_RESULT");
    COA_EVENT("MYSTIC_ENCHANT_PRESET_UNLOCK_RESULT");
}
