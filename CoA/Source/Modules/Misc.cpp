// Loose globals from the original's misc-bindings block (handlers at 0x10a4xxxx..0x10a7xxxx); port of AscMisc.cpp.
// Each function names its original. Ported so far: the glue-state natives.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"

namespace {
    // ---- RealmInfo (FUN_1008e2d0, the object at 0x10bdb138) ----------------------------------------------------
    int GetRealmId(lua_State* L) {   // +0x04
        CoA::Script::PushInt(L, static_cast<int32_t>(CoA::RealmInfo::Get().field0));
        return 1;
    }

    int GetRealmExpansion(lua_State* L) {   // +0x08
        CoA::Script::PushInt(L, static_cast<int32_t>(CoA::RealmInfo::Get().ruleset));
        return 1;
    }

    int GetRealmAuctionCutRate(lua_State* L) {   // +0x1C
        CoA::Script::PushNum(L, CoA::RealmInfo::Get().rate3);
        return 1;
    }

    int GetAuctionHouseDepositRateVanity(lua_State* L) {   // +0x20
        CoA::Script::PushNum(L, CoA::RealmInfo::Get().rateAuctionDepositVanity);
        return 1;
    }

    int HasVisitedArea52ThisSession(lua_State* L) {   // +0x49
        CoA::Script::PushBool(L, CoA::RealmInfo::Get().visitedArea52 != 0);
        return 1;
    }

    int GetAuctionHouseDepositMaxVanity(lua_State* L) {   // +0x24
        CoA::Script::PushNum(L, static_cast<double>(CoA::RealmInfo::Get().maxAuctionDepositVanity));
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetRealmId", GetRealmId),
        COA_NATIVE("", "GetRealmExpansion", GetRealmExpansion),
        COA_NATIVE("", "GetRealmAuctionCutRate", GetRealmAuctionCutRate),
        COA_NATIVE("", "GetAuctionHouseDepositRateVanity", GetAuctionHouseDepositRateVanity),
        COA_NATIVE("", "HasVisitedArea52ThisSession", HasVisitedArea52ThisSession),
        COA_NATIVE("", "GetAuctionHouseDepositMaxVanity", GetAuctionHouseDepositMaxVanity),
    };
}

void CoA::Misc::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
