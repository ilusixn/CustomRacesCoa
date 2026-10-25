#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/HighRisk.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/ObjectAddon.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;

    uint64_t s_guid = 0;
    uint32_t s_level = 0;

    void Realm(uint32_t ruleset, bool dev) {
        CoATest::Bytes b;
        b.U32(1).U32(ruleset).F32(0).F32(0).F32(0).U32(0).F32(0).F32(0).U32(0);

        for (int i = 0; i < 8; ++i) {
            b.U8(i == 4 && dev ? 1 : 0);
        }

        b.Str("").Str("Realm").U8(0);
        CoATest::Deliver(CoA::RealmInfo::OnRealmInfo, 0x9BC, b);
    }

    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::RealmInfo::Reset();
        CoA::GameEvents::Reset();
        CoA::ObjectAddon::Reset();
        CoA::HighRisk::Reset();
        CoA::HighRisk::Init();
        CoA::Seasonal::Reset();
        CoA::Seasonal::Init();
        s_guid = 0;
        s_level = 0;
        CoA::HighRisk::Deps& deps = CoA::HighRisk::Dependencies();
        deps.playerGuid = [] { return s_guid; };
        deps.playerLevel = [] { return s_level; };
        return CoATest::NewState(false);
    }

    V Feed(uint32_t opcode, CoATest::Bytes bytes) {
        CoATest::RecordSignals();
        CDataStore msg(bytes.data.data(), static_cast<uint32_t>(bytes.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }

    void SetField5(float v) {
        memcpy(&CoA::ObjectAddon::Unit(s_guid)[5].value, &v, 4);
    }

    std::string Run(lua_State* L, const char* code) {
        luaL_dostring(L, code);
        lua_getglobal(L, "r");
        std::string out = lua_isstring(L, -1) ? lua_tostring(L, -1) : "nil";
        lua_pop(L, 1);
        return out;
    }
}

// 0x73D sets the fel commutation flag; CanToggle / Toggle compare the request with it (CMSG 0x73B only on a change);
// the glue screen clears it.
COA_TEST(HighRisk_FelCommutation) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "C_HighRisk", "IsFelCommutationActive") == V{ "false" });
    CHECK(CoATest::Call(L, "C_HighRisk", "CanToggleFelCommutation", { "b:0" }) == (V{ "false", "TOGGLE_FEL_COMMUTATION_ALREADY_INACTIVE" }));
    CHECK(CoATest::Call(L, "C_HighRisk", "CanToggleFelCommutation", { "b:1" }) == (V{ "true", "nil" }));
    CoATest::RecordPackets();
    CHECK(CoATest::Call(L, "C_HighRisk", "ToggleFelCommutation", { "b:1" }) == V{ "true" });
    CHECK(CoATest::Call(L, "C_HighRisk", "ToggleFelCommutation", { "b:0" }) == V{ "false" });
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().size() == 1);
    CHECK(CoATest::SentPackets()[0] == CoATest::Expected(0x73B, CoATest::Bytes().U8(1)));

    Feed(0x73D, CoATest::Bytes().U8(1));
    CHECK(CoATest::Call(L, "C_HighRisk", "IsFelCommutationActive") == V{ "true" });
    CHECK(CoATest::Call(L, "C_HighRisk", "CanToggleFelCommutation", { "b:1" }) == (V{ "false", "TOGGLE_FEL_COMMUTATION_ALREADY_ACTIVE" }));
    CoA::Lifecycle::RunGlueScreen();
    CHECK(CoATest::Call(L, "C_HighRisk", "IsFelCommutationActive") == V{ "false" });
}

// Insurance: -1 without a player; field 5 (float, truncated) times the last tier reached; +100000 at the server max
// level, or at 60 / 70 / 80 on a development realm. Both natives are the same function.
COA_TEST(HighRisk_InsuranceCost) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "C_HighRisk", "GetInsuranceCostPerSlot") == V{ "-1" });
    s_guid = 0x42;
    s_level = 10;
    Realm(2, false);   // max level 80
    SetField5(25.9f);
    CHECK(CoATest::Call(L, "C_HighRisk", "GetInsuranceCostPerSlot") == V{ "700" });   // 25 x 28
    SetField5(26.0f);
    CHECK(CoATest::Call(L, "C_HighRisk", "GetInsuranceTotalCost") == V{ "2210" });    // 26 x 85
    SetField5(124.0f);
    CHECK(CoATest::Call(L, "C_HighRisk", "GetInsuranceCostPerSlot") == V{ "341000" });   // 124 x 2750
    s_level = 80;
    CHECK(CoATest::Call(L, "C_HighRisk", "GetInsuranceCostPerSlot") == V{ "441000" });
    s_level = 70;
    CHECK(CoATest::Call(L, "C_HighRisk", "GetInsuranceCostPerSlot") == V{ "341000" });
    Realm(2, true);
    CHECK(CoATest::Call(L, "C_HighRisk", "GetInsuranceCostPerSlot") == V{ "441000" });
    s_level = 65;
    CHECK(CoATest::Call(L, "C_HighRisk", "GetInsuranceCostPerSlot") == V{ "341000" });
}

// 0x9BE appends achievements (never cleared) and sets both {icon, name, description} slots; 0x9BF unlocks; 0x523.
COA_TEST(Seasonal_Collection) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "", "IsSeasonalCollectionUnlocked") == V{ "false" });
    CoATest::Bytes b;
    b.U32(1).I32(77).U8(1).U8(2).U8(3).U8(1).Name("i0").Name("n0").Name("d0").Name("i1").Name("n1").Name("d1");
    Feed(0x9BE, b);
    CoATest::Bytes c;
    c.U32(1).I32(78).U8(4).U8(5).U8(6).U8(0).Name("I0").Name("N0").Name("D0").Name("I1").Name("N1").Name("D1");
    Feed(0x9BE, c);
    CHECK(Run(L, "local t = GetSeasonalCollectionInfo() local a = t.achievements r = #a .. ':' .. a[1].id .. a[1].node .. a[1].chapter .. "
        "a[1].sortOrder .. tostring(a[1].isReward) .. ':' .. a[2].id .. tostring(a[2].isReward) .. ':' .. t.icons[1] .. t.names[2] .. t.descriptions[2]") ==
        "2:77123true:78false:I0N1D1");
    Feed(0x9BF, CoATest::Bytes());
    CHECK(CoATest::Call(L, "", "IsSeasonalCollectionUnlocked") == V{ "true" });
    CoATest::RecordPackets();
    CoATest::Call(L, "", "RequestDeliverSeasonalCollectionItem", { "n:900" });
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().size() == 1);
    CHECK(CoATest::SentPackets()[0] == CoATest::Expected(0x523, CoATest::Bytes().U8(1).U32(900)));
    CoA::Lifecycle::RunGlueScreen();
    CHECK(CoATest::Call(L, "", "IsSeasonalCollectionUnlocked") == V{ "true" });
}

// 0x905: initial packets insert silently (keeping an existing value); later ones store and fire the type's event with
// old and new (old 0 for a new type); types past 4 use the UNHANDLED event.
COA_TEST(Seasonal_CustomPoints) {
    lua_State* L = Setup();
    CHECK(Feed(0x905, CoATest::Bytes().U8(1).U8(1).I32(50)).empty());
    CHECK(Feed(0x905, CoATest::Bytes().U8(1).U8(1).I32(60)).empty());
    CHECK(CoATest::Call(L, "", "GetAscensionSeasonalPoints") == V{ "50" });
    CHECK(Feed(0x905, CoATest::Bytes().U8(0).U8(1).I32(70)) == V{ "ASCENSION_CUSTOM_POINTS_SEASONAL_POINTS_VALUE_CHANGED:50,70" });
    CHECK(Feed(0x905, CoATest::Bytes().U8(0).U8(2).I32(5)) == V{ "ASCENSION_CUSTOM_POINTS_DONATION_POINTS_VALUE_CHANGED:0,5" });
    CHECK(Feed(0x905, CoATest::Bytes().U8(0).U8(9).I32(1)) == V{ "ASCENSION_CUSTOM_POINTS_UNHANDLED_VALUE_CHANGED:0,1" });
    CHECK(CoATest::Call(L, "", "GetAscensionDonationPoints") == V{ "5" });
    CHECK(CoATest::Call(L, "", "GetCustomAscensionPointValue", { "n:265" }) == V{ "1" });   // low byte 9
    CHECK(CoATest::Call(L, "", "GetCustomAscensionPointValue", { "n:3" }) == V{ "0" });
}
