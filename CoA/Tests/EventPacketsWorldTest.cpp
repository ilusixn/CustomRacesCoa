#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <string>
#include <vector>

namespace {
    std::vector<std::string> Feed(uint32_t opcode, CoATest::Bytes bytes) {
        CoA::Packets::Clear();
        CoA::EventPackets::Init();
        CoATest::RecordSignals();
        CDataStore msg(bytes.data.data(), static_cast<uint32_t>(bytes.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }

    using V = std::vector<std::string>;
}

// The SMSGs that turn straight into a UI event.
COA_TEST(EventPackets_WorldEvents) {
    CHECK(Feed(0x588, CoATest::Bytes().U32(5)) == V{ "BATTLEGROUND_COUNTDOWN_STARTED:5" });
    CHECK(Feed(0x588, CoATest::Bytes().U32(0)) == V{ "BATTLEGROUND_STARTED" });
    CHECK(Feed(0x5A5, CoATest::Bytes().Name("ok")) == V{ "TOGGLE_GAME_MODE_RESULT:ok" });
    CHECK((Feed(0x687, CoATest::Bytes().Str("a").Str("b").U32(1).U32(2).U32(3).Str("c").Str("d").U32(4).U8(1)) ==
        V{ "TALKING_HEAD_FRAME_DISPLAY:a,b,1,2,3,c,d,4,true" }));
    CHECK(Feed(0x736, CoATest::Bytes().Str("thanks")) == V{ "SUBMIT_GAME_FEEDBACK_RESULT:thanks" });
    CHECK(Feed(0x73E, CoATest::Bytes().U32(7)) == V{ "OPEN_CUSTOM_STORE:7" });
    CHECK(Feed(0x76A, CoATest::Bytes().U8(3).U32(133)) == V{ "SET_ACTION_BUTTON_SPELL_PAYLOAD:3,133" });
    CHECK(Feed(0x76B, CoATest::Bytes().U32(6948).U8(1).U8(2)) == V{ "ITEM_USED_PAYLOAD:6948,1,2" });
    CHECK(Feed(0x76C, CoATest::Bytes().U32(6948).U8(0).U8(9)) == V{ "ITEM_USE_FAILED_PAYLOAD:6948,0,9" });
    CHECK(Feed(0x76D, CoATest::Bytes().U32(4).U32(0xFFFFFFFF)) == V{ "PROCESSED_STATISTIC_QUERY_PAYLOAD:4,-1" });
    CHECK(Feed(0x76E, CoATest::Bytes().U32(4).U32(5)) == V{ "PROCESSED_ASSET_QUERY_PAYLOAD:4,5" });
    CHECK(Feed(0x661, CoATest::Bytes()) == V{ "BUILD_DRAFT_PICK_PROMPT" });
    CHECK(Feed(0x73C, CoATest::Bytes().Str("on")) == V{ "TOGGLE_FEL_COMMUTATION_STATUS_RESULT:on" });
    CHECK(Feed(0x92B, CoATest::Bytes().U32(1).U32(2).U32(3)) == V{ "ASCENSION_BG_QUEUE_ALERT:1,2,3" });
    CHECK(Feed(0x68E, CoATest::Bytes().Str("SOME_SERVER_EVENT")) == V{ "SOME_SERVER_EVENT" });
}

// 0x754..0x75D: every draft result opcode signals DRAFT_RESULT.
COA_TEST(EventPackets_DraftResults) {
    for (uint32_t op = 0x754; op <= 0x75D; ++op) {
        CHECK(Feed(op, CoATest::Bytes().Str("DRAFT_OK")) == V{ "DRAFT_RESULT:DRAFT_OK" });
    }
}

// 0x906: the window's event, ASCENSION_UNHANDLED_... past the seven names.
COA_TEST(EventPackets_WindowVisibility) {
    CHECK(Feed(0x906, CoATest::Bytes().U8(2).U8(1)) == V{ "ASCENSION_FEL_COMMUTATION_WINDOW_VISIBILITY_CHANGED:true" });
    CHECK(Feed(0x906, CoATest::Bytes().U8(6).U8(0)) == V{ "ASCENSION_MAX_WINDOW_VISIBILITY_CHANGED:false" });
    CHECK(Feed(0x906, CoATest::Bytes().U8(7).U8(5)) == V{ "ASCENSION_UNHANDLED_WINDOW_VISIBILITY_CHANGED:true" });
}

// 0x9B1 / 0x9B2: spell activation overlays, positions by name.
COA_TEST(EventPackets_SpellActivation) {
    CHECK((Feed(0x9B1, CoATest::Bytes().U32(44).Name("tex").U32(0x302).F32(1.5f).U32(1).U32(2).U32(3)) ==
        V{ "SPELL_ACTIVATION_SHOW:44,tex,LEFT,1.5,1,2,3" }));
    CHECK((Feed(0x9B1, CoATest::Bytes().U32(44).Name("tex").U32(20).F32(1.0f).U32(0).U32(0).U32(0)) ==
        V{ "SPELL_ACTIVATION_SHOW:44,tex,UNEXPECTED_ENUM_VALUE_20,1,0,0,0" }));
    CHECK(Feed(0x9B2, CoATest::Bytes().U32(44).U32(0x100)) == V{ "SPELL_ACTIVATION_HIDE:44" });
    CHECK(Feed(0x9B2, CoATest::Bytes().U32(44).U32(13)) == V{ "SPELL_ACTIVATION_HIDE:44,BOTTOM_FLIPPED" });
}

// 0x72D: the value is kept (no caller reads it), and nothing is signalled.
COA_TEST(EventPackets_Value72D) {
    CHECK(Feed(0x72D, CoATest::Bytes().U32(9)).empty());
    CHECK(CoA::EventPackets::Value72D() == 9);
}
