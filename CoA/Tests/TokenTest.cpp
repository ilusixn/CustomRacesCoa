#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/Token.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;

    // TokenTypes.dbc: id, name, 5 strings, bool, int. Ids 3 and 5.
    bool ReadTables(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "TokenTypes.dbc")) {
            const std::string s = std::string("TOKEN_TYPE_X") + '\0' + "Marks" + '\0' + "desc" + '\0' + "icon" + '\0' + "s1" + '\0' + "s2" + '\0' + "TOKEN_TYPE_Y";
            // Offsets (Wdbc prepends the NUL): X 1, Marks 14, desc 20, icon 25, s1 30, s2 33, Y 36.
            out = CoATest::Wdbc({ { 3, 1, 14, 20, 25, 30, 33, 1, 7 }, { 5, 36, 0, 0, 0, 0, 0, 0, 0 } }, s);
            return true;
        }

        return false;
    }

    lua_State* Setup() {
        CoA::LuaReg::Reset();
        CoA::Packets::Clear();
        CoA::Lifecycle::Reset();
        CoA::Dbc::SetFileReader(ReadTables);
        CoA::Dbc::Get("DBFilesClient\\TokenTypes.dbc").Unload();
        CoA::Token::Reset();
        CoA::Token::Init();
        return CoATest::NewState(false);
    }

    V Feed(uint32_t opcode, CoATest::Bytes bytes) {
        CoATest::RecordSignals();
        CDataStore msg(bytes.data.data(), static_cast<uint32_t>(bytes.data.size()));
        int32_t result = 0;
        CoA::Packets::Dispatch(static_cast<int32_t>(opcode), 0, &msg, result);
        CoATest::StopRecordingSignals();
        return CoATest::Signals();
    }

    void Done() {
        CoA::Dbc::Get("DBFilesClient\\TokenTypes.dbc").Unload();
        CoA::Dbc::SetFileReader(nullptr);
    }
}

// Read::Inflate: fewer than 4 bytes or a bad stream fail; a good stream yields the plain bytes; the packet is consumed.
COA_TEST(Inflate_ShortBadGood) {
    std::vector<uint8_t> out;
    CoATest::Bytes shortB;
    shortB.U8(1).U8(2).U8(3);
    CDataStore a(shortB.data.data(), static_cast<uint32_t>(shortB.data.size()));
    CHECK(!CoA::Read::Inflate(&a, out));
    CHECK(CoA::Read::Remaining(&a) == 0);

    CoATest::Bytes bad;
    bad.U32(8).U32(0xDEADBEEF).U32(0x12345678);
    CDataStore b(bad.data.data(), static_cast<uint32_t>(bad.data.size()));
    CHECK(!CoA::Read::Inflate(&b, out));
    CHECK(CoA::Read::Remaining(&b) == 0);

    CoATest::Bytes plain;
    plain.U32(7).Str("hello");
    CoATest::Bytes good = CoATest::Compressed(plain);
    CDataStore c(good.data.data(), static_cast<uint32_t>(good.data.size()));
    CHECK(CoA::Read::Inflate(&c, out));
    CHECK(out == plain.data);
    CHECK(CoA::Read::Remaining(&c) == 0);

    // A declared size past the stream's real length: the result is what inflated.
    CoATest::Bytes big = CoATest::Compressed(plain);
    big.data[0] = 100;
    CDataStore d(big.data.data(), static_cast<uint32_t>(big.data.size()));
    CHECK(CoA::Read::Inflate(&d, out));
    CHECK(out == plain.data);
}

// 0x66F sets the amounts of known names; an unknown name is skipped without consuming its two dwords, so the next name
// is read from them (kept bug).
COA_TEST(Token_ListSkipsUnknownNames) {
    lua_State* L = Setup();
    Feed(0x66F, CoATest::Bytes().U32(2).Str("TOKEN_TYPE_SCROLL_OF_FORTUNE_II").U32(5).U32(0).Str("TOKEN_TYPE_SCROLL_OF_FORTUNE_III_TALENTS").U32(9).U32(1));
    CHECK(CoA::Token::Amount(1) == 5);
    CHECK(CoA::Token::Amount(42) == 9);
    CHECK(CoA::Token::Amount64(42) == ((1ull << 32) | 9));
    CHECK(CoATest::Call(L, "C_Token", "GetTokenAmount", { "s:TOKEN_TYPE_SCROLL_OF_FORTUNE_II" }) == V{ "5" });

    CoA::Token::Reset();
    Feed(0x66F, CoATest::Bytes().U32(2).Str("NOPE").Str("TOKEN_TYPE_SCROLL_OF_FORTUNE_I").U32(4).U32(0));
    CHECK(CoA::Token::Amount(0) == 4);

    // TOKEN_TYPE_MAX names index 61, past the 0x3D entries: skipped.
    Feed(0x66F, CoATest::Bytes().U32(1).Str("TOKEN_TYPE_MAX").U32(4).U32(0));
    CHECK(CoA::Token::Amount(61) == 0);
    Done();
}

// 0x670 fires TOKEN_UPDATED(name, old, new) only when a dword changed; the glue screen zeroes every amount.
COA_TEST(Token_UpdateAndGlueReset) {
    lua_State* L = Setup();
    CHECK(Feed(0x670, CoATest::Bytes().Str("TOKEN_TYPE_CALLBOARD_CACHE_POINTS").U32(3).U32(0)) == V{ "TOKEN_UPDATED:TOKEN_TYPE_CALLBOARD_CACHE_POINTS,0,3" });
    CHECK(Feed(0x670, CoATest::Bytes().Str("TOKEN_TYPE_CALLBOARD_CACHE_POINTS").U32(3).U32(0)).empty());
    CHECK(Feed(0x670, CoATest::Bytes().Str("TOKEN_TYPE_CALLBOARD_CACHE_POINTS").U32(3).U32(2)) == V{ "TOKEN_UPDATED:TOKEN_TYPE_CALLBOARD_CACHE_POINTS,3,3" });
    CHECK(Feed(0x670, CoATest::Bytes().Str("NOPE").U32(3).U32(2)).empty());
    CHECK(CoATest::Call(L, "C_Token", "GetTokenAmount", { "s:NOPE" }).empty());
    CoA::Lifecycle::RunGlueScreen();
    CHECK(CoA::Token::Amount64(60) == 0);
    Done();
}

// AtLeast compares {extra:amount} with {hi:lo}; out of range reads zeros. TypeName / TypeIndex cover 62 names.
COA_TEST(Token_AtLeastAndNames) {
    Setup();
    Feed(0x670, CoATest::Bytes().Str("TOKEN_TYPE_SCROLL_OF_FORTUNE_XX_ABILITIES").U32(10).U32(1));
    uint32_t i = 0;
    CHECK(CoA::Token::TypeIndex("TOKEN_TYPE_SCROLL_OF_FORTUNE_XX_ABILITIES", i) && i == 39);
    CHECK(CoA::Token::AtLeast(39, 999, 0));
    CHECK(CoA::Token::AtLeast(39, 10, 1));
    CHECK(!CoA::Token::AtLeast(39, 11, 1));
    CHECK(!CoA::Token::AtLeast(39, 0, 2));
    CHECK(CoA::Token::AtLeast(70, 0, 0));
    CHECK(!CoA::Token::AtLeast(70, 1, 0));
    CHECK(std::string(CoA::Token::TypeName(61)) == "TOKEN_TYPE_MAX");
    CHECK(CoA::Token::TypeName(62) == nullptr);
    Done();
}

// GetTokenInfo returns the row's five strings, bool and int by name, else seven nils; GetTokenTypes lists names in id
// order.
COA_TEST(Token_InfoAndTypes) {
    lua_State* L = Setup();
    CHECK(CoATest::Call(L, "C_Token", "GetTokenInfo", { "s:TOKEN_TYPE_X" }) == (V{ "Marks", "desc", "icon", "s1", "s2", "true", "7" }));
    CHECK(CoATest::Call(L, "C_Token", "GetTokenInfo", { "s:TOKEN_TYPE_Z" }) == (V{ "nil", "nil", "nil", "nil", "nil", "nil", "nil" }));
    luaL_dostring(L, "local t = C_Token.GetTokenTypes() r = #t .. t[1] .. t[2]");
    lua_getglobal(L, "r");
    CHECK(std::string(lua_tostring(L, -1)) == "2TOKEN_TYPE_XTOKEN_TYPE_Y");
    lua_pop(L, 1);
    Done();
}
