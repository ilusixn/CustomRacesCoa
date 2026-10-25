// AscHighRisk.cpp (see HighRisk.h).
#include "CoA/Source/Modules/HighRisk.h"
#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/ObjectAddon.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cmath>

namespace {
    bool s_felActive = false;
    CoA::HighRisk::Deps s_deps;

    int32_t OnFelStatus(void*, uint32_t, uint32_t, CDataStore* msg) {
        s_felActive = CoA::Read::U8(msg) != 0;
        return 1;
    }

    // PTR 0x10B40A34: TOGGLE_FEL_COMMUTATION_* by result code.
    const char* const kToggleResults[5] = { "TOGGLE_FEL_COMMUTATION_OK", "TOGGLE_FEL_COMMUTATION_UNKNOWN", "TOGGLE_FEL_COMMUTATION_ALREADY_ACTIVE",
        "TOGGLE_FEL_COMMUTATION_ALREADY_INACTIVE", "TOGGLE_FEL_COMMUTATION_NO_ALTAR_NEARBY" };

    // (true, nil) when the requested state differs from the current one, else (false, reason).
    int CanToggleFelCommutation(lua_State* L) {
        bool want;

        if (!CoA::Script::ReadBool(L, want)) {
            return 0;
        }

        if (want != s_felActive) {
            CoA::Script::PushBool(L, true);
            lua_pushnil(L);
            return 2;
        }

        CoA::Script::PushBool(L, false);
        CoA::Script::PushStr(L, kToggleResults[want ? 2 : 3]);
        return 2;
    }

    int IsFelCommutationActive(lua_State* L) {
        CoA::Script::PushBool(L, s_felActive);
        return 1;
    }

    int ToggleFelCommutation(lua_State* L) {
        bool want;

        if (!CoA::Script::ReadBool(L, want)) {
            return 0;
        }

        if (want == s_felActive) {
            CoA::Script::PushBool(L, false);
            return 1;
        }

        CoA::Packet(0x73B).U8(want ? 1 : 0).Send();
        CoA::Script::PushBool(L, true);
        return 1;
    }

    // FUN_1008e380: at (or above) the server's max level; on a development realm levels 60 / 70 / 80 also count.
    bool AtMaxLevel() {
        const uint32_t level = s_deps.playerLevel ? s_deps.playerLevel() : 0;

        if (level >= CoA::GameEvents::ServerMaxLevel()) {
            return true;
        }

        return CoA::RealmInfo::Get().Dev() && (level == 60 || level == 70 || level == 80);
    }

    // FUN_10275060: the player's addon field 5 (a float, truncated) times the multiplier of the last tier whose
    // threshold it reaches, +100000 at max level, floored. -1 without a player.
    int32_t InsuranceCost() {
        const uint64_t guid = s_deps.playerGuid ? s_deps.playerGuid() : 0;

        if (!guid) {
            return -1;
        }

        const uint32_t value = static_cast<uint32_t>(static_cast<int64_t>(CoA::ObjectAddon::AsFloat(CoA::ObjectAddon::Unit(guid)[5])));

        struct Tier {
            uint32_t threshold;
            double multiplier;
        };

        static const Tier kTiers[9] = { { 0, 28.0 }, { 26, 85.0 }, { 36, 256.0 }, { 46, 361.0 }, { 55, 1250.0 }, { 63, 1500.0 }, { 106, 2000.0 },
            { 115, 2500.0 }, { 124, 2750.0 } };
        double multiplier = kTiers[8].multiplier;

        for (const Tier& t : kTiers) {
            if (t.threshold > value) {
                break;
            }

            multiplier = t.multiplier;
        }

        double cost = static_cast<double>(static_cast<int32_t>(value)) * multiplier;

        if (AtMaxLevel()) {
            cost += 100000.0;
        }

        return static_cast<int32_t>(std::floor(cost));
    }

    int GetInsuranceCost(lua_State* L) {
        CoA::Script::PushNum(L, InsuranceCost());
        return 1;
    }

    void ResetFel() {   // FUN_10275050
        s_felActive = false;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_HighRisk", "CanToggleFelCommutation", CanToggleFelCommutation),
        COA_NATIVE("C_HighRisk", "GetInsuranceCostPerSlot", GetInsuranceCost),
        COA_NATIVE("C_HighRisk", "GetInsuranceTotalCost", GetInsuranceCost),
        COA_NATIVE("C_HighRisk", "IsFelCommutationActive", IsFelCommutationActive),
        COA_NATIVE("C_HighRisk", "ToggleFelCommutation", ToggleFelCommutation),
    };
}

CoA::HighRisk::Deps& CoA::HighRisk::Dependencies() {
    return s_deps;
}

void CoA::HighRisk::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    CoA::Lifecycle::OnGlueScreen(ResetFel);
    COA_PACKET(0x73D, OnFelStatus);
}

void CoA::HighRisk::Reset() {
    ResetFel();
}
