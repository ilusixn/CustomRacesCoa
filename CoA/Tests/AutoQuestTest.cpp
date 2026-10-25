#include "CoA/Source/Modules/AttachMisc.h"
#include "CoA/Source/Modules/AutoQuest.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;

    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::AttachMisc::loginVerifyWorld.Clear();
        CoA::AutoQuest::Reset();
        CoA::AutoQuest::Init();
        CoA::ClientState::SetInWorldProbe([] { return true; });
        return CoATest::NewState(false);
    }

    V Feed(uint32_t quest, uint32_t type) {
        CoATest::Bytes b;
        b.U32(quest).U32(type);
        CoATest::RecordSignals();
        CDataStore msg(b.data.data(), static_cast<uint32_t>(b.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(0x927, 0, &msg, result);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }
}

// The Lua list: Add (OFFER / COMPLETE only), Get (1-based, quest then type name), Num (0 outside the world), Remove.
COA_TEST(AutoQuest_LuaList) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "", "AddAutoQuestPopUp", { "n:10", "s:OFFER" }) == V{ "true" });
    CHECK(CoATest::Call(L, "", "AddAutoQuestPopUp", { "n:11", "s:COMPLETE" }) == V{ "true" });
    CHECK(CoATest::Call(L, "", "AddAutoQuestPopUp", { "n:12", "s:offer" }).empty());
    CHECK(CoATest::Call(L, "", "GetNumAutoQuestPopUps") == V{ "2" });
    CHECK((CoATest::Call(L, "", "GetAutoQuestPopUp", { "n:2" }) == V{ "11", "COMPLETE" }));
    CHECK(CoATest::Call(L, "", "GetAutoQuestPopUp", { "n:0" }).empty());
    CHECK(CoATest::Call(L, "", "GetAutoQuestPopUp", { "n:3" }).empty());
    CoATest::Call(L, "", "RemoveAutoQuestPopUp", { "n:10" });
    CHECK((CoATest::Call(L, "", "GetAutoQuestPopUp", { "n:1" }) == V{ "11", "COMPLETE" }));
    CoA::ClientState::SetInWorldProbe([] { return false; });
    CHECK(CoATest::Call(L, "", "GetNumAutoQuestPopUps") == V{ "0" });
    CoA::ClientState::SetInWorldProbe(nullptr);
}

// SMSG 0x927: a listed quest only moves up a type; an unlisted one is appended; the event follows the type.
COA_TEST(AutoQuest_Packet) {
    lua_State* L = Setup();
    CHECK(Feed(20, 0) == V{ "QUEST_AUTO_OFFER:20" });
    CHECK(Feed(20, 0).empty());
    CHECK(Feed(20, 1) == V{ "QUEST_AUTOCOMPLETE:20" });
    CHECK(Feed(20, 0).empty());
    CHECK(Feed(21, 5).empty());   // appended, no event
    CHECK((CoATest::Call(L, "", "GetAutoQuestPopUp", { "n:2" }) == V{ "21", "UNEXPECTED_ENUM_VALUE_5" }));
    CoA::ClientState::SetInWorldProbe(nullptr);
}

// ShowQuestComplete(n) sends CMSG 0x542 with pop-up n's quest; Custom_SendAutoQuestAccept a giverless accept.
COA_TEST(AutoQuest_Requests) {
    lua_State* L = Setup();
    CoATest::Call(L, "", "AddAutoQuestPopUp", { "n:30", "s:COMPLETE" });
    CoATest::RecordPackets();
    CoATest::Call(L, "", "ShowQuestComplete", { "n:1" });
    CoATest::Call(L, "", "ShowQuestComplete", { "n:2" });
    CoATest::Call(L, "", "Custom_SendAutoQuestAccept", { "n:31" });
    CoATest::StopRecordingPackets();
    const auto& sent = CoATest::SentPackets();
    CHECK(sent.size() == 2);
    CHECK(sent.size() == 2 && sent[0] == CoATest::Expected(0x542, CoATest::Bytes().U32(30)));
    CHECK(sent.size() == 2 && sent[1] == CoATest::Expected(0x189, CoATest::Bytes().U64(0).U32(31).U32(0)));
    CoA::ClientState::SetInWorldProbe(nullptr);
}

// SMSG_LOGIN_VERIFY_WORLD empties the list.
COA_TEST(AutoQuest_LoginVerifyWorldClears) {
    lua_State* L = Setup();
    CoATest::Call(L, "", "AddAutoQuestPopUp", { "n:40", "s:OFFER" });
    CoA::AttachMisc::loginVerifyWorld.Run();
    CHECK(CoATest::Call(L, "", "GetNumAutoQuestPopUps") == V{ "0" });
    CoA::ClientState::SetInWorldProbe(nullptr);
}
