#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/lua-5.1/src/lualib.h"
#include <cstdio>
#include <string>

extern "C" int luaopen_bit(lua_State* L);

namespace {
    // The 32-bit client converts a Lua number to a 32-bit integer through __ftol2 (0x88B9F6): a 64-bit truncation of
    // which the low dword is kept, so numbers past 2^31 wrap. A plain (int) cast on x64 gives 0x80000000 instead --
    // Ascension's bit.contains(Enum.ClassMask.COA = 0xFFFFF800, UnitClassMask) was false, LootCollector said "WR mode".
    std::string Eval(const char* expression) {
        lua_State* L = luaL_newstate();
        luaL_openlibs(L);
        luaopen_bit(L);
        std::string out = "error";
        const std::string code = std::string("return tostring(") + expression + ")";

        if (luaL_loadstring(L, code.c_str()) == 0 && lua_pcall(L, 0, 1, 0) == 0) {
            out = lua_tostring(L, -1);
        }

        lua_close(L);
        return out;
    }
}

COA_TEST(LuaBit_NumbersPast2To31WrapLikeThe32BitClient) {
    CHECK(Eval("bit.band(4294965248, 4194304)") == "4194304");
    CHECK(Eval("bit.band(4294965248, 1024)") == "0");
    CHECK(Eval("bit.bor(2147483648, 1)") == "2147483649");
    CHECK(Eval("bit.bxor(4294967295, 1)") == "4294967294");
    CHECK(Eval("bit.bnot(0)") == "4294967295");
    CHECK(Eval("bit.bnot(4294967295)") == "0");
    CHECK(Eval("bit.lshift(1, 31)") == "2147483648");
    CHECK(Eval("bit.rshift(4294967295, 1)") == "2147483647");
    CHECK(Eval("bit.arshift(4294967295, 1)") == "-1");
    CHECK(Eval("bit.mod(4294967295, 10)") == "-1");
    CHECK(Eval("bit.band(-1, 255)") == "255");
}

COA_TEST(LuaFormat_PercentDWrapsLikeThe32BitClient) {
    CHECK(Eval("string.format('%d', 4294967295)") == "-1");
    CHECK(Eval("string.format('%d', 2147483648)") == "-2147483648");
    CHECK(Eval("string.format('%d', 12.9)") == "12");
    CHECK(Eval("string.format('%x', 4294965248)") == "fffff800");
    CHECK(Eval("string.format('%u', -1)") == "4294967295");
}

// lua_number2int (lua_tointeger / luaL_checkint): the 32-bit client's MSVC build is "fld; fistp dword" (0x84E0A4) --
// round to nearest even, 0x80000000 when out of range. The x64 fallback (int)d truncated instead.
COA_TEST(LuaToInteger_RoundsLikeTheX87Fistp) {
    CHECK(Eval("('abcdef'):sub(2.7)") == "cdef");
    CHECK(Eval("('abcdef'):sub(2.5)") == "bcdef");   // 2.5 -> 2 (even)
    CHECK(Eval("('abcdef'):sub(3.5)") == "def");     // 3.5 -> 4 (even)
    CHECK(Eval("string.rep('a', 2.6)") == "aaa");
    CHECK(Eval("('abcdef'):sub(-1.6)") == "ef");     // -1.6 -> -2
}
