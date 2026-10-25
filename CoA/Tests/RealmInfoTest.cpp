#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lua.h"

namespace {
    // The CoA server's SendRealmInfo: id, ruleset, three floats, u32, two floats, u32, eight flags, data path,
    // realm name, addons byte.
    CoATest::Bytes ServerRealmInfo(uint32_t id, uint32_t ruleset, const uint8_t (&flags)[8], const char* name) {
        CoATest::Bytes b;
        b.U32(id).U32(ruleset).F32(1.5f).F32(0.0f).F32(0.0f).U32(0).F32(0.05f).F32(0.25f).U32(0);
        for (uint8_t f : flags) {
            b.U8(f);
        }
        b.Str("").Str(name).U8(1);
        return b;
    }

    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::RealmInfo::Reset();
        CoA::GameEvents::Reset();
        CoATest::InitRealmModules();
        return CoATest::NewState(true);
    }
}

COA_TEST(RealmInfo_ParsesServerPacket) {
    lua_State* L = Setup();
    const uint8_t flags[8] = { 0, 0, 0, 0, 1, 0, 1, 0 };
    CoATest::Bytes b = ServerRealmInfo(7, 2, flags, "CoA Dev");
    CoATest::Deliver(CoA::RealmInfo::OnRealmInfo, 0x9BC, b);

    const CoA::RealmInfo::Info& info = CoA::RealmInfo::Get();
    CHECK(info.field0 == 7);
    CHECK(info.ruleset == 2);
    CHECK(info.rateXpQuest == 1.5f);
    CHECK(strcmp(info.realmName, "CoA Dev") == 0);
    CHECK(info.trailingFlag == 1);
    CHECK(info.field64 == 0);
    CHECK(info.received == 1);

    CHECK(CoATest::Call(L, "C_Realm", "IsDevelopment") == std::vector<std::string>{ "true" });
    CHECK(CoATest::Call(L, "C_Realm", "IsLive") == std::vector<std::string>{ "false" });
    CHECK(CoATest::Call(L, "C_Realm", "IsProduction") == std::vector<std::string>{ "false" });
    CHECK(CoATest::Call(L, "", "GetRealmId") == std::vector<std::string>{ "7" });
    CHECK(CoATest::Call(L, "", "GetRealmExpansion") == std::vector<std::string>{ "2" });
    CHECK(CoATest::Call(L, "", "GetRealmAuctionCutRate") == std::vector<std::string>{ "0.05000000075" });
    CHECK(CoATest::Call(L, "", "GetAuctionHouseDepositRateVanity") == std::vector<std::string>{ "0.25" });
    CHECK(CoATest::Call(L, "", "GetRealmMaxLevel") == std::vector<std::string>{ "80" });
    CHECK(CoATest::Call(L, "", "GetServerMaxLevel") == std::vector<std::string>{ "80" });
    lua_close(L);
}

COA_TEST(RealmInfo_DefaultsBeforePacket) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "C_Realm", "IsLive") == std::vector<std::string>{ "false" });
    CHECK(CoATest::Call(L, "C_Realm", "IsSeasonal") == std::vector<std::string>{ "false" });
    CHECK(CoATest::Call(L, "C_Realm", "IsLeague") == std::vector<std::string>{ "false" });
    CHECK(CoATest::Call(L, "C_Realm", "IsPTR") == std::vector<std::string>{ "false" });
    CHECK(CoATest::Call(L, "C_Realm", "IsDevelopment") == std::vector<std::string>{ "false" });
    CHECK(CoATest::Call(L, "C_Realm", "IsProduction") == std::vector<std::string>{ "false" });
    CHECK(CoATest::Call(L, "", "GetRealmId") == std::vector<std::string>{ "0" });
    CHECK(CoATest::Call(L, "", "GetRealmExpansion") == std::vector<std::string>{ "0" });
    CHECK(CoATest::Call(L, "", "GetRealmAuctionCutRate") == std::vector<std::string>{ "0" });
    CHECK(CoATest::Call(L, "", "GetAuctionHouseDepositRateVanity") == std::vector<std::string>{ "0" });
    CHECK(CoATest::Call(L, "", "GetAuctionHouseDepositMaxVanity") == std::vector<std::string>{ "0" });
    CHECK(CoATest::Call(L, "", "GetRealmMaintenance") == std::vector<std::string>{ "false" });
    CHECK(CoATest::Call(L, "", "GetRealmMaxLevel") == std::vector<std::string>{ "60" });
    CHECK(CoATest::Call(L, "", "GetServerMaxLevel") == std::vector<std::string>{ "60" });
    CHECK(CoATest::Call(L, "", "HasVisitedArea52ThisSession") == std::vector<std::string>{ "false" });
    lua_close(L);
}

COA_TEST(RealmInfo_MaintenanceFiresAlert) {
    lua_State* L = Setup();
    CoATest::RecordSignals();
    CoATest::Bytes b;
    b.U32(11).U32(0).F32(0).F32(0).F32(0).U32(1).F32(0).F32(0).U32(0);
    for (int i = 0; i < 8; ++i) {
        b.U8(i == 0 ? 1 : 0);
    }
    b.Str("").Str("x").U8(0).U32(99);
    CoATest::Deliver(CoA::RealmInfo::OnRealmInfo, 0x9BC, b);
    CoATest::StopRecordingSignals();

    CHECK(CoATest::Signals() == std::vector<std::string>{ "REALM_ALERT_MAINTENANCE" });
    CHECK(CoA::RealmInfo::Get().field64 == 99);
    CHECK(CoATest::Call(L, "", "GetRealmMaintenance") == std::vector<std::string>{ "true" });
    CHECK(CoATest::Call(L, "", "HasVisitedArea52ThisSession") == std::vector<std::string>{ "true" });
    lua_close(L);
}

COA_TEST(RealmInfo_TruncatedPacketNoCrash) {
    Setup();
    CoATest::Bytes b;
    b.U32(5).U32(1).F32(2.0f);
    CoATest::Deliver(CoA::RealmInfo::OnRealmInfo, 0x9BC, b);
    CHECK(CoA::RealmInfo::Get().field0 == 5);
    CHECK(CoA::RealmInfo::Get().realmName[0] == '\0');
}

COA_TEST(GameEvents_LevelCapEventLowersMax) {
    lua_State* L = Setup();
    const uint8_t flags[8] = {};
    CoATest::Bytes realm = ServerRealmInfo(1, 2, flags, "r");
    CoATest::Deliver(CoA::RealmInfo::OnRealmInfo, 0x9BC, realm);

    // A 0x34-byte record: id 0x1E8 (cap 45), state 0, start 0, end 0, active 1, length 0.
    CoATest::Bytes ev;
    ev.U16(0x1E8).U8(0).U64(0).U64(0).U8(1).I32(0);
    while (ev.data.size() < 0x34) {
        ev.U8(0);
    }
    CoATest::RecordSignals();
    CoATest::Deliver(CoA::GameEvents::OnGameEventInfo, 0x9BD, ev);
    CoATest::StopRecordingSignals();

    CHECK(CoATest::Signals() == std::vector<std::string>{ "GAME_EVENT_STARTED:488" });
    CHECK(CoA::GameEvents::IsActive(0x1E8));
    CHECK(CoATest::Call(L, "", "GetServerMaxLevel") == std::vector<std::string>{ "45" });
    CHECK(CoATest::Call(L, "", "GetRealmMaxLevel") == std::vector<std::string>{ "45" });
    lua_close(L);
}
