#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"

// The world request senders.
COA_TEST(GlobalsH_WorldRequests) {
    CoA::LuaReg::Reset();
    CoA::GlobalsH::Init();
    lua_State* L = CoATest::NewState(false);

    CoATest::RecordPackets();
    CoATest::Call(L, "", "SendDraftSkip");
    CoATest::Call(L, "", "RedraftAbilities");
    CoATest::Call(L, "", "SendDraftModeSpellSelection", { "n:1234" });
    CoATest::Call(L, "", "DeleteStablePet", { "n:7" });
    CoATest::Call(L, "", "RequestStatisticQuery", { "n:60" });
    CoATest::Call(L, "", "RequestDeliverVanityCollectionItem", { "n:9001" });
    CoATest::StopRecordingPackets();

    const auto& sent = CoATest::SentPackets();
    CHECK(sent.size() == 6);
    CHECK(sent[0] == CoATest::Expected(0x74D, CoATest::Bytes()));
    CHECK(sent[1] == CoATest::Expected(0x74E, CoATest::Bytes()));
    CHECK(sent[2] == CoATest::Expected(0x74C, CoATest::Bytes().U32(1234)));
    CHECK(sent[3] == CoATest::Expected(0x67C, CoATest::Bytes().U32(7)));
    CHECK(sent[4] == CoATest::Expected(0x777, CoATest::Bytes().U32(60)));
    CHECK(sent[5] == CoATest::Expected(0x523, CoATest::Bytes().U8(2).U32(9001)));
}
