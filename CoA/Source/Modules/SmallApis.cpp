// Small namespaces with no shared state (port of AscSmallApis.cpp); the natives over client objects are in
// SmallApisClient.cpp.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/SmallApis.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include <ctime>
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>

namespace {
    // ---- C_CVar (FUN_10114940 / a50 / b60) ----------------------------------------------------------------------
    // The DLL calls CVarArrayWrapper::RawGetValue / RawSetValue (0x766860 / 0x766CA0) on a bare holder of the CVar*,
    // never the wrapper's constructor, so no value-version conversion runs first.
    struct BareWrapper {
        CVar* cvar;
        uint8_t defaultValue;
    };

    CVarArrayWrapper* Wrap(BareWrapper& holder) {
        return reinterpret_cast<CVarArrayWrapper*>(&holder);
    }

    int GetCVarBitfield(lua_State* L) {
        std::string name = CoA::Script::CheckString(L, 1);
        uint32_t index = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 2)));
        CVar* var = CVar::Lookup(name.c_str());

        if (!var) {
            lua_pushnil(L);
            return 1;
        }

        BareWrapper holder = { var, 0 };
        CoA::Script::PushBool(L, Wrap(holder)->RawGetValue(index));
        return 1;
    }

    int SetCVarBitfield(lua_State* L) {
        std::string name = CoA::Script::CheckString(L, 1);
        uint32_t index = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 2)));
        bool value = lua_toboolean(L, 3) != 0;
        CVar* var = CVar::Lookup(name.c_str());
        bool ok = false;

        if (var) {
            BareWrapper holder = { var, 0 };
            ok = Wrap(holder)->RawSetValue(index, value);
        }

        CoA::Script::PushBool(L, ok);
        return 1;
    }

    // CVar flag 0x80 = "do not save".
    int SetCVarSave(lua_State* L) {
        std::string name = CoA::Script::CheckString(L, 1);
        bool save = lua_toboolean(L, 2) != 0;

        if (CVar* var = CVar::Lookup(name.c_str())) {
            var->m_flags = save ? (var->m_flags & ~0x80u) : (var->m_flags | 0x80u);
        }

        return 0;
    }

    // C_EquipmentSet.PlaceInBank(index) (0x10228520): luaL_checknumber; below 20, CMSG 0x55E with index - 1 (index 0
    // wraps). Returns nothing.
    int PlaceInBank(lua_State* L) {
        uint32_t index = static_cast<uint32_t>(CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)));

        if (index < 0x14) {
            CoA::Packet(0x55E).U32(index - 1).Send();
        }

        return 0;
    }

    // C_CharacterCustomizationUnlocks (handler_GetCurrentOptionInfo / ...): constant in the original.
    int GetCurrentOptionInfo(lua_State* L) {
        if (lua_gettop(L) != 1) {
            return 0;
        }

        CoA::Script::CheckNumber(L, 1);
        CoA::Script::PushBool(L, false);
        CoA::Script::PushBool(L, true);
        CoA::Script::PushInt(L, 0);
        return 3;
    }

    int GetFirstLockedRequirement(lua_State* L) {
        CoA::Script::PushInt(L, 0);
        return 1;
    }

    int HasLockedSelections(lua_State* L) {
        CoA::Script::PushBool(L, false);
        return 1;
    }

    // ---- C_Chat infractions ---------------------------------------------------------------------------------------
    // SMSG_CHAT_INFRACTION 0x6E5 (FUN_10114ef0): u32, str, u32; sets "has infraction" (0x10BDD1C1) and fires
    // CHAT_INFRACTION_UPDATE. AcknowledgeChatInfraction sets 0x10BDD1C0, which nothing clears, and sends CMSG 0x6E4.
    struct {
        bool acknowledged = false;
        bool has = false;
        int32_t a = 0;
        int32_t b = 0;
        std::string text;
    } s_infraction;

    int32_t OnChatInfraction(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        s_infraction.a = static_cast<int32_t>(CoA::Read::U32(msg));
        s_infraction.text = CoA::Read::Str(msg);
        s_infraction.b = static_cast<int32_t>(CoA::Read::U32(msg));
        s_infraction.has = true;
        CoA::Events::Signal("CHAT_INFRACTION_UPDATE", nullptr);
        return 1;
    }

    // Returns 1 with nothing pushed, as the original does.
    int AcknowledgeChatInfraction(lua_State*) {
        s_infraction.acknowledged = true;
        CoA::Packet(0x6E4).Send();
        return 1;
    }

    int GetChatInfraction(lua_State* L) {
        if (!s_infraction.has) {
            lua_pushnil(L);
            return 1;
        }

        CoA::Script::PushInt(L, s_infraction.a);
        CoA::Script::PushStr(L, s_infraction.text.c_str());
        CoA::Script::PushInt(L, s_infraction.b);
        return 3;
    }

    int HasChatInfraction(lua_State* L) {
        CoA::Script::PushBool(L, !s_infraction.acknowledged && s_infraction.has);
        return 1;
    }

    // ---- C_LoyaltyPass ---------------------------------------------------------------------------------------------
    // SMSG 0x563 (handler_0x0563): u32 next reward time (0x10BE3074), u32 expiry (0x10BE3078). ClaimRewards sends
    // CMSG 0x564.
    uint32_t s_loyaltyNext = 0;
    uint32_t s_loyaltyExpiry = 0;
    int32_t (*s_timeOffset)() = nullptr;

    int32_t OnLoyaltyPass(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        s_loyaltyNext = CoA::Read::U32(msg);
        s_loyaltyExpiry = CoA::Read::U32(msg);
        return 1;
    }

    uint32_t LoyaltyNow() {
        return static_cast<uint32_t>(static_cast<int32_t>(_time64(nullptr)) + (s_timeOffset ? s_timeOffset() : 0));
    }

    bool LoyaltyActive() {
        return s_loyaltyNext != 0 && (s_loyaltyExpiry == 0 || LoyaltyNow() < s_loyaltyExpiry);
    }

    bool LoyaltyRedeemable() {   // FUN_10296880
        return LoyaltyActive() && s_loyaltyNext <= LoyaltyNow();
    }

    int ClaimRewards(lua_State*) {
        if (LoyaltyActive() && LoyaltyRedeemable()) {
            CoA::Packet(0x564).Send();
        }

        return 0;
    }

    int GetTimeUntilNextReward(lua_State* L) {
        const uint32_t now = LoyaltyNow();
        CoA::Script::PushInt(L, LoyaltyActive() && !LoyaltyRedeemable() && now < s_loyaltyNext ? static_cast<int32_t>(s_loyaltyNext - now) : 0);
        return 1;
    }

    int GetTimeUntilExpiration(lua_State* L) {
        const uint32_t now = LoyaltyNow();
        CoA::Script::PushInt(L, LoyaltyActive() && now < s_loyaltyExpiry ? static_cast<int32_t>(s_loyaltyExpiry - now) : 0);
        return 1;
    }

    int IsLoyaltyPassActive(lua_State* L) {
        CoA::Script::PushBool(L, LoyaltyActive());
        return 1;
    }

    int IsLoyaltyPassExpired(lua_State* L) {
        CoA::Script::PushBool(L, s_loyaltyExpiry != 0 && s_loyaltyExpiry <= LoyaltyNow());
        return 1;
    }

    int CanRedeemLoyaltyPassReward(lua_State* L) {
        CoA::Script::PushBool(L, LoyaltyRedeemable());
        return 1;
    }

    // ---- C_Scenario (FUN_103073e0 .. handler_IsInScenario): constant in the original --------------------------------
    int GetActiveStage(lua_State* L) {
        CoA::Script::PushNum(L, 0);
        CoA::Script::PushStr(L, "");
        return 2;
    }

    int GetEncounterAtIndex(lua_State* L) {
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        return 1;
    }

    int GetNumEncounters(lua_State* L) {
        CoA::Script::PushNum(L, 0);
        return 1;
    }

    int GetScenarioName(lua_State* L) {
        CoA::Script::PushStr(L, "");
        return 1;
    }

    int IsInScenario(lua_State* L) {
        CoA::Script::PushBool(L, false);
        return 1;
    }

    // ---- C_Taxi.GetTaxiPathDuration: TaxiTimes.dbc +4 from slot, +8 to slot (0-based), +0xC duration ---------------
    int GetTaxiPathDuration(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TNUMBER })) {
            return 0;
        }

        const int32_t from = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1)) - 1;
        const int32_t to = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 2)) - 1;
        CoA::Dbc::Table& t = CoA::Dbc::Get("DBFilesClient/TaxiTimes.dbc");

        for (uint32_t id = t.MinId(); t.Loaded() && id <= t.MaxId(); ++id) {
            const uint8_t* row = t.Row(id);

            if (row && CoA::Dbc::Table::I32(row, 4) == from && CoA::Dbc::Table::I32(row, 8) == to) {
                CoA::Script::PushInt(L, CoA::Dbc::Table::I32(row, 0xC));
                return 1;
            }
        }

        lua_pushnil(L);
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_Chat", "AcknowledgeChatInfraction", AcknowledgeChatInfraction),
        COA_NATIVE("C_Chat", "GetChatInfraction", GetChatInfraction),
        COA_NATIVE("C_Chat", "HasChatInfraction", HasChatInfraction),
        COA_NATIVE("C_LoyaltyPass", "ClaimRewards", ClaimRewards),
        COA_NATIVE("C_LoyaltyPass", "GetTimeUntilNextReward", GetTimeUntilNextReward),
        COA_NATIVE("C_LoyaltyPass", "GetTimeUntilExpiration", GetTimeUntilExpiration),
        COA_NATIVE("C_LoyaltyPass", "IsLoyaltyPassActive", IsLoyaltyPassActive),
        COA_NATIVE("C_LoyaltyPass", "IsLoyaltyPassExpired", IsLoyaltyPassExpired),
        COA_NATIVE("C_LoyaltyPass", "CanRedeemLoyaltyPassReward", CanRedeemLoyaltyPassReward),
        COA_NATIVE("C_Scenario", "GetActiveStage", GetActiveStage),
        COA_NATIVE("C_Scenario", "GetEncounterAtIndex", GetEncounterAtIndex),
        COA_NATIVE("C_Scenario", "GetNumEncounters", GetNumEncounters),
        COA_NATIVE("C_Scenario", "GetScenarioName", GetScenarioName),
        COA_NATIVE("C_Scenario", "IsInScenario", IsInScenario),
        COA_NATIVE("C_Taxi", "GetTaxiPathDuration", GetTaxiPathDuration),
        COA_NATIVE("C_CVar", "GetCVarBitfield", GetCVarBitfield),
        COA_NATIVE("C_CVar", "SetCVarBitfield", SetCVarBitfield),
        COA_NATIVE("C_CVar", "SetCVarSave", SetCVarSave),
        COA_NATIVE("C_EquipmentSet", "PlaceInBank", PlaceInBank),
        COA_NATIVE("C_CharacterCustomizationUnlocks", "GetCurrentOptionInfo", GetCurrentOptionInfo),
        COA_NATIVE("C_CharacterCustomizationUnlocks", "GetFirstLockedRequirement", GetFirstLockedRequirement),
        COA_NATIVE("C_CharacterCustomizationUnlocks", "HasLockedSelections", HasLockedSelections),
    };
}

void CoA::SmallApis::SetTimeOffsetSource(int32_t (*offset)()) {
    s_timeOffset = offset;
}

void CoA::SmallApis::Init() {
    COA_PACKET(0x563, OnLoyaltyPass);
    COA_PACKET(0x6E5, OnChatInfraction);
    COA_EVENT("CHAT_INFRACTION_UPDATE");
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
