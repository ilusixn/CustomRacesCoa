#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"

// The request senders: one CMSG each; ToggleDraftMode answers true only for a valid boolean argument, and
// JoinArenaWithRole sends only with exactly two arguments.
COA_TEST(GlobalsF_RequestSenders) {
    CoA::LuaReg::Reset();
    CoA::GlobalsF::Init();
    lua_State* L = CoATest::NewState(false);

    CoATest::RecordPackets();
    CoATest::Call(L, "", "RequestRespawnAtCheckpoint");
    CHECK(CoATest::Call(L, "", "ToggleDraftMode", { "b:1" }) == std::vector<std::string>{ "true" });
    CHECK(CoATest::Call(L, "", "ToggleDraftMode", { "b:0" }) == std::vector<std::string>{ "true" });
    CHECK(CoATest::Call(L, "", "ToggleDraftMode", { "s:x" }).empty());
    CoATest::Call(L, "", "SetLootSpecialization", { "n:3" });
    CoATest::Call(L, "", "RequestChangeRuleset", { "n:258" });
    CoATest::Call(L, "", "RequestSetDungeonDifficulty", { "n:2" });
    CoATest::Call(L, "", "JoinArenaWithRole", { "n:1" });
    CoATest::Call(L, "", "JoinArenaWithRole", { "n:2", "n:4" });
    CoATest::StopRecordingPackets();

    const auto& sent = CoATest::SentPackets();
    CHECK(sent.size() == 7);
    CHECK(sent.size() == 7 && sent[0] == CoATest::Expected(0x774, CoATest::Bytes().U8(1)));
    CHECK(sent.size() == 7 && sent[1] == CoATest::Expected(0x749, CoATest::Bytes()));
    CHECK(sent.size() == 7 && sent[2] == CoATest::Expected(0x74A, CoATest::Bytes()));
    CHECK(sent.size() == 7 && sent[3] == CoATest::Expected(0x679, CoATest::Bytes().U32(3)));
    CHECK(sent.size() == 7 && sent[4] == CoATest::Expected(0x53F, CoATest::Bytes().U8(2)));   // truncated to a byte
    CHECK(sent.size() == 7 && sent[5] == CoATest::Expected(0x773, CoATest::Bytes().U8(2)));
    CHECK(sent.size() == 7 && sent[6] == CoATest::Expected(0x5C1, CoATest::Bytes().U32(2).U32(4)));
}
