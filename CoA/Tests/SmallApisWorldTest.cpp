#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/SmallApis.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>
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
        CoA::SmallApis::Init();
        return CoATest::NewState(false);
    }

    // TaxiTimes.dbc: id, from slot, to slot, duration.
    bool ReadTaxiTimes(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "TaxiTimes") == nullptr) {
            return false;
        }

        out = CoATest::Wdbc({ { 1, 0, 2, 95 }, { 2, 2, 0, 97 } }, "");
        return true;
    }
}

// SMSG 0x6E5: the infraction, CHAT_INFRACTION_UPDATE; acknowledging sends CMSG 0x6E4 and returns 1 with nothing pushed.
COA_TEST(SmallApis_ChatInfraction) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "C_Chat", "GetChatInfraction")[0] == "nil");
    CHECK(CoATest::Call(L, "C_Chat", "HasChatInfraction")[0] == "false");

    CoATest::RecordSignals();
    CoATest::Bytes b;
    b.U32(3).Str("spam").U32(60);
    Feed(0x6E5, b);
    CoATest::StopRecordingSignals();
    CHECK(CoATest::Signals().size() == 1 && CoATest::Signals()[0] == "CHAT_INFRACTION_UPDATE");
    CHECK((CoATest::Call(L, "C_Chat", "GetChatInfraction") == std::vector<std::string>{ "3", "spam", "60" }));
    CHECK(CoATest::Call(L, "C_Chat", "HasChatInfraction")[0] == "true");

    CoATest::RecordPackets();
    CoATest::Call(L, "C_Chat", "AcknowledgeChatInfraction");
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().size() == 1 && CoATest::SentPackets()[0] == CoATest::Expected(0x6E4, CoATest::Bytes()));
    CHECK(CoATest::Call(L, "C_Chat", "HasChatInfraction")[0] == "false");
}

// SMSG 0x563: the next reward time and the expiry against now (plus the client's time offset).
COA_TEST(SmallApis_LoyaltyPass) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "C_LoyaltyPass", "IsLoyaltyPassActive")[0] == "false");

    const uint32_t now = static_cast<uint32_t>(_time64(nullptr));
    CoATest::Bytes b;
    b.U32(now - 10).U32(now + 1000);
    Feed(0x563, b);
    CHECK(CoATest::Call(L, "C_LoyaltyPass", "IsLoyaltyPassActive")[0] == "true");
    CHECK(CoATest::Call(L, "C_LoyaltyPass", "CanRedeemLoyaltyPassReward")[0] == "true");
    CHECK(CoATest::Call(L, "C_LoyaltyPass", "IsLoyaltyPassExpired")[0] == "false");
    CHECK(CoATest::Call(L, "C_LoyaltyPass", "GetTimeUntilNextReward")[0] == "0");

    CoATest::RecordPackets();
    CoATest::Call(L, "C_LoyaltyPass", "ClaimRewards");
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().size() == 1 && CoATest::SentPackets()[0] == CoATest::Expected(0x564, CoATest::Bytes()));

    CoATest::Bytes expired;
    expired.U32(now + 500).U32(now - 5);
    Feed(0x563, expired);
    CHECK(CoATest::Call(L, "C_LoyaltyPass", "IsLoyaltyPassExpired")[0] == "true");
    CHECK(CoATest::Call(L, "C_LoyaltyPass", "IsLoyaltyPassActive")[0] == "false");
}

// C_Scenario: constant in the original.
COA_TEST(SmallApis_ScenarioConstants) {
    lua_State* L = Setup();
    CHECK((CoATest::Call(L, "C_Scenario", "GetActiveStage") == std::vector<std::string>{ "0", "" }));
    CHECK(CoATest::Call(L, "C_Scenario", "GetEncounterAtIndex")[0] == "table");
    CHECK(CoATest::Call(L, "C_Scenario", "GetNumEncounters")[0] == "0");
    CHECK(CoATest::Call(L, "C_Scenario", "GetScenarioName")[0] == "");
    CHECK(CoATest::Call(L, "C_Scenario", "IsInScenario")[0] == "false");
}

// TaxiTimes.dbc: slots are 1-based in the call.
COA_TEST(SmallApis_TaxiPathDuration) {
    CoA::Dbc::SetFileReader(ReadTaxiTimes);
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "C_Taxi", "GetTaxiPathDuration", { "n:1", "n:3" })[0] == "95");
    CHECK(CoATest::Call(L, "C_Taxi", "GetTaxiPathDuration", { "n:3", "n:1" })[0] == "97");
    CHECK(CoATest::Call(L, "C_Taxi", "GetTaxiPathDuration", { "n:2", "n:3" })[0] == "nil");
    CoA::Dbc::SetFileReader(nullptr);
}
