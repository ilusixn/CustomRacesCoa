// GetChatCheck (ChatCheck.cpp): SMSG 0x928 / 0x929, the chat type name table (CHAT_MSG_ADDON listed twice), the
// substring rule and the SMSG_LOGIN_VERIFY_WORLD clear.
#include "CoA/Source/Modules/AttachMisc.h"
#include "CoA/Source/Modules/ChatCheck.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;

    lua_State* Start() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::AttachMisc::loginVerifyWorld.Clear();
        CoA::ChatCheck::Reset();
        CoA::ChatCheck::Init();
        return CoATest::NewState(false);
    }

    void Stop(lua_State* L) {
        lua_close(L);
        CoA::AttachMisc::loginVerifyWorld.Clear();
        CoA::ChatCheck::Reset();
    }

    // The sender's string: the phrase and its NUL, counted in the length.
    void Send(uint32_t opcode, uint32_t type, const std::string& phrase, bool nul = true) {
        CoATest::Bytes b;
        b.U32(type).U32(static_cast<uint32_t>(phrase.size() + (nul ? 1 : 0))).Raw(phrase.data(), static_cast<uint32_t>(phrase.size()));

        if (nul) {
            b.U8(0);
        }

        CDataStore msg(b.data.data(), static_cast<uint32_t>(b.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
    }

    V Check(lua_State* L, const char* type, const char* text) {
        return CoATest::Call(L, "", "GetChatCheck", { std::string("s:") + type, std::string("s:") + text });
    }
}

COA_TEST(ChatCheck_AddRemove) {
    lua_State* L = Start();
    CHECK(Check(L, "CHAT_MSG_SAY", "buy gold") == V{ "false" });
    Send(0x928, 1, "gold");
    CHECK(Check(L, "CHAT_MSG_SAY", "buy gold now") == V{ "true" });
    CHECK(Check(L, "CHAT_MSG_SAY", "gold") == V{ "true" });
    CHECK(Check(L, "CHAT_MSG_SAY", "GOLD") == V{ "false" });   // case-sensitive
    CHECK(Check(L, "CHAT_MSG_SAY", "gol") == V{ "false" });
    CHECK(Check(L, "CHAT_MSG_YELL", "gold") == V{ "false" });
    // CHAT_MSG_ADDON is listed first as 1: it shares SAY's set.
    CHECK(Check(L, "CHAT_MSG_ADDON", "gold") == V{ "true" });
    Send(0x928, 6, "spam");
    CHECK(Check(L, "CHAT_MSG_YELL", "no spam here") == V{ "true" });
    // Unknown names are type 0 (CHAT_MSG_SYSTEM's).
    Send(0x928, 0, "sys");
    CHECK(Check(L, "CHAT_MSG_NOPE", "a sys b") == V{ "true" });
    CHECK(Check(L, "CHAT_MSG_SYSTEM", "a sys b") == V{ "true" });
    Send(0x928, 51, "lead");
    CHECK(Check(L, "CHAT_MSG_PARTY_LEADER", "leader") == V{ "true" });
    // The last byte is dropped whatever it is.
    Send(0x928, 2, "abcX", false);
    CHECK(Check(L, "CHAT_MSG_PARTY", "abc") == V{ "true" });
    Send(0x929, 1, "gold");
    CHECK(Check(L, "CHAT_MSG_SAY", "buy gold") == V{ "false" });
    Send(0x929, 9, "x");   // removing from an empty type is harmless
    CHECK(Check(L, "CHAT_MSG_YELL", "spam") == V{ "true" });
    Stop(L);
}

// A one-byte string is the empty phrase, which every text contains; a zero length or a length past the packet is
// dropped.
COA_TEST(ChatCheck_EdgeLengths) {
    lua_State* L = Start();
    Send(0x928, 3, "", true);
    CHECK(Check(L, "CHAT_MSG_RAID", "") == V{ "true" });
    CHECK(Check(L, "CHAT_MSG_RAID", "anything") == V{ "true" });
    Send(0x928, 4, "", false);
    CHECK(Check(L, "CHAT_MSG_GUILD", "anything") == V{ "false" });
    CoATest::Bytes b;
    b.U32(5).U32(50).Raw("ab", 2);
    CDataStore msg(b.data.data(), static_cast<uint32_t>(b.data.size()));
    int32_t result = 0;
    CoA::Packets::Dispatch(0x928, 0, &msg, result);
    CHECK(Check(L, "CHAT_MSG_OFFICER", "anything") == V{ "false" });
    CHECK(CoATest::Call(L, "", "GetChatCheck", { "s:CHAT_MSG_SAY" }).at(0).compare(0, 7, "<error>") == 0);
    Stop(L);
}

COA_TEST(ChatCheck_LoginVerifyWorldClears) {
    lua_State* L = Start();
    Send(0x928, 1, "gold");
    CoA::AttachMisc::loginVerifyWorld.Run();
    CHECK(Check(L, "CHAT_MSG_SAY", "gold") == V{ "false" });
    Stop(L);
}
