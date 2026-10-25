#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/Base/CDataStore.h"
#include <string>

namespace {
    int s_calls;
    uint32_t s_lastOpcode;
    void* s_lastParam;

    int32_t CountingHandler(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        s_calls++;
        s_lastOpcode = opcode;
        s_lastParam = param;
        return 7;
    }

    void ResetCalls() {
        s_calls = 0;
        s_lastOpcode = 0;
        s_lastParam = nullptr;
    }
}

COA_TEST(Packets_StockOpcodesAreNotRouted) {
    ResetCalls();
    CoA::Packets::Clear();
    CoA::Packets::Set(0x55F, CountingHandler, nullptr);
    uint8_t payload[4] = {};
    CDataStore msg(payload, sizeof(payload));
    int32_t result = -1;
    CHECK(!CoA::Packets::Dispatch(0x55E, 0, &msg, result));
    CHECK(!CoA::Packets::Dispatch(0x100, 0, &msg, result));
    CHECK(s_calls == 0);
    CHECK(result == -1);
}

COA_TEST(Packets_CustomOpcodeCallsItsHandlerOnce) {
    ResetCalls();
    CoA::Packets::Clear();
    int marker;
    CoA::Packets::Set(0x55F, CountingHandler, &marker);
    uint8_t payload[4] = {};
    CDataStore msg(payload, sizeof(payload));
    int32_t result = 0;
    CHECK(CoA::Packets::Dispatch(0x55F, 0, &msg, result));
    CHECK(s_calls == 1);
    CHECK(s_lastOpcode == 0x55F);
    CHECK(s_lastParam == &marker);
    CHECK(result == 7);
}

COA_TEST(Packets_UnsetCustomOpcodeIsDroppedUnread) {
    ResetCalls();
    CoA::Packets::Clear();
    uint8_t payload[4] = {};
    CDataStore msg(payload, sizeof(payload));
    uint32_t readBefore = msg.m_read;
    int32_t result = 0;
    CHECK(CoA::Packets::Dispatch(0x9C6, 0, &msg, result));
    CHECK(result == 1);
    CHECK(msg.m_read == readBefore);
    CHECK(s_calls == 0);
}

COA_TEST(Packets_OpcodeNames) {
    CHECK(std::string(CoA::Packets::Name(0x000)) == "MSG_NULL");
    CHECK(std::string(CoA::Packets::Name(0x9D4)) == "MAX");
    CHECK(std::string(CoA::Packets::Name(0x9D5)) == "unknown");
    CHECK(std::string(CoA::Packets::Name(0xFFFF)) == "unknown");
}

COA_TEST(Packets_HighestNamedOpcodeDispatches) {
    ResetCalls();
    CoA::Packets::Clear();
    CoA::Packets::Set(0x9D4, CountingHandler, nullptr);
    CoA::Packets::Set(0xFFFF, CountingHandler, nullptr);
    uint8_t payload[1] = {};
    CDataStore msg(payload, sizeof(payload));
    int32_t result = 0;
    CHECK(CoA::Packets::Dispatch(0x9D4, 0, &msg, result));
    CHECK(CoA::Packets::Dispatch(0xFFFF, 0, &msg, result));
    CHECK(s_calls == 2);
}

COA_TEST(Packets_SetReplacesTheHandler) {
    ResetCalls();
    CoA::Packets::Clear();
    int first;
    int second;
    CoA::Packets::Set(0x563, CountingHandler, &first);
    CoA::Packets::Set(0x563, CountingHandler, &second);
    uint8_t payload[1] = {};
    CDataStore msg(payload, sizeof(payload));
    int32_t result = 0;
    CoA::Packets::Dispatch(0x563, 0, &msg, result);
    CHECK(s_calls == 1);
    CHECK(s_lastParam == &second);
}

COA_TEST(Packets_EraseDropsTheHandler) {
    ResetCalls();
    CoA::Packets::Clear();
    CoA::Packets::Set(0x563, CountingHandler, nullptr);
    CoA::Packets::Erase(0x563);
    uint8_t payload[1] = {};
    CDataStore msg(payload, sizeof(payload));
    int32_t result = 0;
    CHECK(CoA::Packets::Dispatch(0x563, 0, &msg, result));
    CHECK(s_calls == 0);
    CHECK(result == 1);
}

COA_TEST(Packets_GapBetweenStockTableAndCustomRange) {
    // NetClient's handler table holds 0x51F entries; 0x51F..0x55E belong to neither table.
    CHECK(!CoA::Packets::InGap(0x51E));
    CHECK(CoA::Packets::InGap(0x51F));
    CHECK(CoA::Packets::InGap(0x55E));
    CHECK(!CoA::Packets::InGap(0x55F));
    CHECK(!CoA::Packets::InGap(0));
}
