#include "CoA/Source/Modules/GlobalsJ.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lua.h"
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
        CoA::GlobalsJ::Reset();
        CoA::GlobalsJ::Init();
        return CoATest::NewState(false);
    }

    void Transaction(CoATest::Bytes& b, uint32_t id) {
        b.U32(id).U32(2).Name("first").U32(3).Name("second").U32(4).U32(0x105).U32(6).U32(7).U32(8).U32(9);
    }
}

// SMSG 0x9B7 / 0x9B8 / 0x9B9: the GM transaction list and its contents.
COA_TEST(GlobalsJ_GmTransactions) {
    lua_State* L = Setup();
    CoATest::RecordSignals();

    CoATest::Bytes list;
    list.U32(2);
    Transaction(list, 11);
    Transaction(list, 12);
    Feed(0x9B9, list);

    CoATest::Bytes created;
    Transaction(created, 13);
    Feed(0x9B7, created);

    CoATest::Bytes closed;
    closed.U64(11);
    Feed(0x9B8, closed);
    CoATest::StopRecordingSignals();

    CHECK((CoATest::Signals() == std::vector<std::string>{ "ASCENSION_CUSTOM_GM_TRANSACTION_LIST_UPDATED:true",
        "ASCENSION_CUSTOM_GM_TRANSACTION_CREATED:13", "ASCENSION_CUSTOM_GM_TRANSACTION_LIST_UPDATED:false",
        // Unregistered in every table: the client's signal drops it.
        "GM_TRANSACTION_REMOVED:11" }));
    CHECK(CoATest::Call(L, "", "GM_GetTransactionIdList")[0] == "table");
    CHECK((CoATest::Call(L, "", "GM_GetTransactionContent", { "n:13" }) ==
        std::vector<std::string>{ "13", "2", "first", "3", "second", "4", "5", "6", "7", "8", "9" }));
    CHECK(CoATest::Call(L, "", "GM_GetTransactionContent", { "n:11" }).empty());
}

// SMSG 0x925 and the arena reward natives.
COA_TEST(GlobalsJ_ArenaRewardInfo) {
    lua_State* L = Setup();
    CoATest::RecordPackets();
    CoATest::Call(L, "", "Ascension_QueryArenaRewardInfo");
    CoATest::StopRecordingPackets();
    CHECK(CoATest::SentPackets().size() == 1 && CoATest::SentPackets()[0] == CoATest::Expected(0x53E, CoATest::Bytes()));

    CoATest::Bytes b;
    b.U32(4).U32(8).U32(3).U32(0x1C).U32(0x14).U32(0x18).U32(0x10);
    Feed(0x925, b);
    CHECK((CoATest::Call(L, "", "Ascension_GetArenaRewardInfo") ==
        std::vector<std::string>{ "8", "4", "BRACKET_RIVAL", "28", "20", "24", "16" }));

    CoATest::Bytes odd;
    odd.U32(0).U32(0).U32(9).U32(0).U32(0).U32(0).U32(0);
    Feed(0x925, odd);
    CHECK(CoATest::Call(L, "", "Ascension_GetArenaRewardInfo")[2] == "UNEXPECTED_ENUM_VALUE_9");
}

// The request senders.
COA_TEST(GlobalsJ_WorldRequests) {
    lua_State* L = Setup();
    CoATest::RecordPackets();
    CoATest::Call(L, "", "RequestSingleAssetQuery", { "n:2", "n:77" });
    CoATest::Call(L, "", "RequestMultipleAssetQuery", { "n:3", "n:5", "n:6" });
    CoATest::Call(L, "", "GetBattlefieldList", { "n:32" });
    CHECK(CoATest::Call(L, "", "SubmitGameFeedback", { "n:4", "s:hello" })[0] == "true");
    CoATest::Call(L, "", "SubmitGameFeedback", { "n:5" });
    CoATest::StopRecordingPackets();

    const auto& sent = CoATest::SentPackets();
    CHECK(sent.size() == 5);
    CHECK(sent[0] == CoATest::Expected(0x775, CoATest::Bytes().U32(2).U32(77)));
    CHECK(sent[1] == CoATest::Expected(0x776, CoATest::Bytes().U32(3).U32(2).U32(5).U32(6)));
    CHECK(sent[2] == CoATest::Expected(0x23C, CoATest::Bytes().U32(32).U8(0).U8(0)));
    CHECK(sent[3] == CoATest::Expected(0x735, CoATest::Bytes().U32(4).U8(1).Str("hello")));
    CHECK(sent[4] == CoATest::Expected(0x735, CoATest::Bytes().U32(5).U8(0)));
}

// The values the client hooks read: the combat log filter, the attack animation / distance and game-object fading.
COA_TEST(GlobalsJ_ClientValues) {
    lua_State* L = Setup();
    CHECK(CoA::GlobalsJ::CombatLogAllowed(5, 0, 0, 0, 0));
    CoATest::Call(L, "", "CombatLogDisable", { "n:5" });
    CHECK(!CoA::GlobalsJ::CombatLogAllowed(5, 0, 0, 0, 0));
    CHECK(CoA::GlobalsJ::CombatLogAllowed(6, 0, 0, 0, 0));
    CoATest::Call(L, "", "CombatLogDisableAll");
    CHECK(!CoA::GlobalsJ::CombatLogAllowed(6, 0, 0, 0, 0));

    CoATest::Call(L, "", "SetAttackAnimation", { "n:42" });
    CoATest::Call(L, "", "SetAttackDistance", { "n:7.5" });
    CHECK(CoA::GlobalsJ::AttackAnimation() == 42 && CoA::GlobalsJ::AttackDistance() == 7.5f);

    CHECK(CoA::GlobalsJ::FadeOutGameObjects());
    CoATest::Call(L, "", "SetFadeOutGameObjects", { "s:x" });
    CHECK(!CoA::GlobalsJ::FadeOutGameObjects());
    CoATest::Call(L, "", "SetFadeOutGameObjects", { "nil" });
    CHECK(CoA::GlobalsJ::FadeOutGameObjects());
}

// Store-backed natives with empty stores: an empty table for an unknown class, nil for an unknown creature / quest.
COA_TEST(GlobalsJ_EmptyStores) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "", "GetAdditionalRoleRequirementsByClassID", { "n:99" })[0] == "table");
    CHECK(CoATest::Call(L, "", "GetCreatureInfoInstant", { "n:1234" })[0] == "nil");
    CHECK(CoATest::Call(L, "", "IsQuestSequenced", { "n:1234" })[0] == "nil");
}

// FindScriptEvents(text): the custom names, then the client's 600, whose lower-case form contains the lower-case text.
COA_TEST(GlobalsJ_FindScriptEvents) {
    lua_State* L = Setup();
    CoA::Events::ResetQueue();
    CoA::Events::Queue("ASCENSION_CUSTOM_PET_THING");
    CHECK((CoATest::Call(L, "", "FindScriptEvents", { "s:uNiT_pEt" }) == std::vector<std::string>{ "table" }));
    lua_getglobal(L, "FindScriptEvents");
    lua_pushstring(L, "unit_pet");
    lua_call(L, 1, 1);
    lua_rawgeti(L, -1, 1);
    CHECK(std::string(lua_tostring(L, -1)) == "UNIT_PET");
    lua_pop(L, 2);

    lua_getglobal(L, "FindScriptEvents");
    lua_pushstring(L, "pet_thing");
    lua_call(L, 1, 1);
    CHECK(lua_objlen(L, -1) == 1);
    lua_rawgeti(L, -1, 1);
    CHECK(std::string(lua_tostring(L, -1)) == "ASCENSION_CUSTOM_PET_THING");
    lua_pop(L, 2);

    lua_getglobal(L, "FindScriptEvents");
    lua_pushstring(L, "");
    lua_call(L, 1, 1);
    CHECK(lua_objlen(L, -1) == 601);
    lua_pop(L, 1);
    CoA::Events::ResetQueue();
}
