#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
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
        CoA::Lifecycle::Reset();
        CoA::GlobalsC::Init();
        return CoATest::NewState(false);
    }
}

// SMSG 0x720: words 8..10 of the 11; nil x 3 before it and again after a glue screen.
COA_TEST(GlobalsC_WildcardPrestigeInfo) {
    lua_State* L = Setup();
    CHECK((CoATest::Call(L, "", "GetWildcardPrestigeInfo") == std::vector<std::string>{ "nil", "nil", "nil" }));

    CoATest::Bytes b;
    for (uint32_t i = 0; i < 11; ++i) {
        b.U32(i * 10);
    }
    Feed(0x720, b);
    CHECK((CoATest::Call(L, "", "GetWildcardPrestigeInfo") == std::vector<std::string>{ "80", "90", "100" }));

    CoA::Lifecycle::RunGlueScreen();
    CHECK(CoATest::Call(L, "", "GetWildcardPrestigeInfo")[0] == "nil");
}

// The empty requests.
COA_TEST(GlobalsC_EmptyRequests) {
    lua_State* L = Setup();
    CoATest::RecordPackets();
    CoATest::Call(L, "", "ResetRaids");
    CoATest::Call(L, "", "ResetDungeons");
    CoATest::Call(L, "", "PortGraveyard");
    CoATest::Call(L, "", "ResetWorldTime");
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().size() == 4);
    CHECK(CoATest::SentPackets()[0] == CoATest::Expected(0x620, CoATest::Bytes()));
    CHECK(CoATest::SentPackets()[1] == CoATest::Expected(0x61F, CoATest::Bytes()));
    CHECK(CoATest::SentPackets()[2] == CoATest::Expected(0x544, CoATest::Bytes()));
    CHECK(CoATest::SentPackets()[3] == CoATest::Expected(0x543, CoATest::Bytes()));
}

// Without an ItemAddon record: nil.
COA_TEST(GlobalsC_ItemLevelInstantEmptyStore) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "", "GetItemLevelInstant", { "n:12345" })[0] == "nil");
}
