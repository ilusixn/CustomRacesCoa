#include "CoA/Source/Modules/Misc.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <ctime>
#include <string>
#include <vector>

namespace {
    void Feed(uint32_t opcode, CoATest::Bytes& bytes) {
        CDataStore msg(bytes.data.data(), static_cast<uint32_t>(bytes.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
    }

    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::Misc::Init();
        return CoATest::NewState(false);
    }
}

// Original defaults before any packet: nil for the server-fed flags, 0 / false for the rest.
COA_TEST(Misc_WorldDefaults) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "", "GetCheckpointsEnabled")[0] == "nil");
    CHECK(CoATest::Call(L, "", "GetBankPermissions")[0] == "nil");
    CHECK(CoATest::Call(L, "", "GetFileStreamingStatus")[0] == "0");
    CHECK(CoATest::Call(L, "", "GetGossipIconOverride", { "n:40" })[0] == "0");
    CHECK(CoATest::Call(L, "", "CanJoinRatedArenaWithoutGroup", { "n:2" })[0] == "true");
    CHECK(CoATest::Call(L, "", "CanJoinRatedArenaWithoutGroup", { "n:4" })[0] == "false");
}

// The server-fed flags and lists.
COA_TEST(Misc_WorldPackets) {
    lua_State* L = Setup();
    CoATest::RecordSignals();

    CoATest::Bytes friendStatus;
    friendStatus.U8(1);
    Feed(0x672, friendStatus);
    CHECK(CoATest::Call(L, "", "FriendStatusIsOffline")[0] == "true");

    CoATest::Bytes checkpoints;
    checkpoints.U8(1);
    Feed(0x767, checkpoints);
    CHECK(CoATest::Call(L, "", "GetCheckpointsEnabled")[0] == "true");

    CoATest::Bytes bank;
    bank.U8(1).U8(0);
    Feed(0x769, bank);
    CHECK((CoATest::Call(L, "", "GetBankPermissions") == std::vector<std::string>{ "true", "false" }));

    CoATest::Bytes icons;
    icons.U32(2).U32(7).U32(9);
    Feed(0x669, icons);
    CHECK(CoATest::Call(L, "", "GetGossipIconOverride", { "n:1" })[0] == "9");

    CoATest::Bytes loot;
    loot.U32(258);
    Feed(0x67A, loot);
    CHECK(CoATest::Call(L, "", "GetLootSpecialization")[0] == "258");

    CoATest::Bytes zones;
    zones.U32(3).U32(22).U32(11).U32(22).U32(33);
    Feed(0x743, zones);
    CHECK(CoATest::Call(L, "", "GetZoneInstanceID")[0] == "2");
    CHECK(CoATest::Call(L, "", "GetZoneInstanceList")[0] == "3");

    CoATest::StopRecordingSignals();
    CHECK((CoATest::Signals() == std::vector<std::string>{ "BANK_PERMISSIONS_PAYLOAD", "PLAYER_LOOT_SPEC_UPDATED", "ZONE_INSTANCE_LIST" }));
}

// The world request senders.
COA_TEST(Misc_WorldRequests) {
    lua_State* L = Setup();
    CoATest::Bytes zones;
    zones.U32(2).U32(5).U32(5).U32(6);
    Feed(0x743, zones);

    CoATest::RecordPackets();
    CoATest::Call(L, "", "FriendStatusSetOffline", { "b:1" });
    CoATest::Call(L, "", "TaxiRequestEarlyLanding");
    CHECK(CoATest::Call(L, "", "TaxiIsEarlyLanding")[0] == "true");
    CoATest::Call(L, "", "TaxiCancelEarlyLanding");
    CoATest::Call(L, "", "SetZoneInstanceID", { "n:2" });
    CoATest::Call(L, "", "SetZoneInstanceID", { "n:3" });
    CoATest::StopRecordingPackets();

    const auto& sent = CoATest::SentPackets();
    CHECK(sent.size() == 4);
    CHECK(sent[0] == CoATest::Expected(0x671, CoATest::Bytes().U8(1)));
    CHECK(sent[1] == CoATest::Expected(0x5F2, CoATest::Bytes()));
    CHECK(sent[2] == CoATest::Expected(0x9C6, CoATest::Bytes()));
    CHECK(sent[3] == CoATest::Expected(0x744, CoATest::Bytes().U32(6)));
}

// FUN_10a4f500: the server's epoch time against the local clock, corrected for the time zone.
COA_TEST(Misc_ServerTimeOffset) {
    const __time64_t now = _time64(nullptr);
    tm gm = *_gmtime64(&now);
    tm local = *_localtime64(&now);
    const int32_t zone = static_cast<int32_t>(_mktime64(&local)) - static_cast<int32_t>(_mktime64(&gm));

    CoA::Misc::ServerTimeReceived(static_cast<int32_t>(now) + 3600);
    lua_State* L = Setup();
    const int32_t offset = std::stoi(CoATest::Call(L, "", "GetServerTimeOffset")[0]);
    CHECK(offset >= 3600 - zone - 2 && offset <= 3600 - zone);
}
