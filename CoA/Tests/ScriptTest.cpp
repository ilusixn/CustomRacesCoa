#include "CoA/Source/Runtime/Script.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

COA_TEST(Script_BoolSlotAcceptsNilAndNumber) {
    lua_State* L = luaL_newstate();

    lua_settop(L, 0);
    lua_pushnil(L);
    CHECK(CoA::Script::ValidateInput(L, { LUA_TBOOLEAN }));

    lua_settop(L, 0);
    lua_pushnumber(L, 1.0);
    CHECK(CoA::Script::ValidateInput(L, { LUA_TBOOLEAN }));

    lua_settop(L, 0);
    lua_pushboolean(L, 1);
    CHECK(CoA::Script::ValidateInput(L, { LUA_TBOOLEAN }));

    lua_settop(L, 0);
    lua_pushstring(L, "x");
    CHECK(!CoA::Script::ValidateInput(L, { LUA_TBOOLEAN }));

    lua_close(L);
}

COA_TEST(Script_TrailingNilSlotNotRequired) {
    lua_State* L = luaL_newstate();
    lua_pushnumber(L, 3.0);
    CHECK(CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TNIL }));
    lua_pushstring(L, "present");
    CHECK(!CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TNIL }));
    lua_close(L);
}

COA_TEST(Script_ReadInt2ValidatesBoth) {
    lua_State* L = luaL_newstate();
    int32_t a = 0;
    int32_t b = 0;

    lua_pushnumber(L, 5.0);
    CHECK(!CoA::Script::ReadInt2(L, a, b));

    lua_pushnumber(L, -7.9);
    CHECK(CoA::Script::ReadInt2(L, a, b));
    CHECK(a == 5);
    CHECK(b == -7);

    lua_close(L);
}

COA_TEST(Script_ToIntTruncates) {
    CHECK(CoA::Script::ToInt(-1.9) == -1);
    CHECK(CoA::Script::ToInt(2.9) == 2);
}

COA_TEST(Script_ReadNumberIsUnsignedTruncation) {
    lua_State* L = luaL_newstate();
    uint32_t v = 0;
    lua_pushnumber(L, -1.0);
    CHECK(CoA::Script::ReadNumber(L, v));
    CHECK(v == 0xFFFFFFFFu);
    lua_close(L);
}
