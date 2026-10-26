#include "CoA/Source/Modules/ClientGuards.h"
#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/lua-5.1/src/lualib.h"
#include <string>

namespace {
    int s_skips = 0;

    void SkipBaseMip() { ++s_skips; }

    lua_State* NewState() {
        lua_State* L = luaL_newstate();
        lua_pushcfunction(L, luaopen_base);
        lua_call(L, 0, 0);
        lua_pushcfunction(L, luaopen_string);
        lua_call(L, 0, 0);
        return L;
    }

    // Runs `chunk` (which returns one value) and gives it as text, or "error: ..." when it raised.
    std::string Eval(lua_State* L, const char* chunk) {
        if (luaL_loadstring(L, chunk) || lua_pcall(L, 0, 1, 0)) {
            std::string e = std::string("error: ") + lua_tostring(L, -1);
            lua_pop(L, 1);
            return e;
        }

        std::string out = lua_isboolean(L, -1) ? (lua_toboolean(L, -1) ? "true" : "false") : lua_typename(L, lua_type(L, -1));
        lua_pop(L, 1);
        return out;
    }
}

// FUN_10297280 string.toboolean, added to the client's string library by FUN_10296fd0 over 0x8540D0.
COA_TEST(ClientGuards_StringToBoolean) {
    CoA::ClientGuards::Init();
    CoA::ClientHooks::afterUiReload.Clear();
    lua_State* L = NewState();
    CHECK(Eval(L, "return string.toboolean(true)") == "true");
    CHECK(Eval(L, "return string.toboolean(false)") == "false");
    CHECK(Eval(L, "return string.toboolean(1)") == "true");
    CHECK(Eval(L, "return string.toboolean(0)") == "false");
    CHECK(Eval(L, "return string.toboolean(0.5)") == "false");    // truncated to 0
    CHECK(Eval(L, "return string.toboolean(-1.9)") == "true");    // truncated to -1
    CHECK(Eval(L, "return string.toboolean('true')") == "true");
    CHECK(Eval(L, "return string.toboolean('1')") == "true");
    CHECK(Eval(L, "return string.toboolean('on')") == "true");
    CHECK(Eval(L, "return string.toboolean('t')") == "true");
    CHECK(Eval(L, "return string.toboolean('TRUE')") == "false");
    CHECK(Eval(L, "return string.toboolean('yes')") == "false");
    CHECK(Eval(L, "return string.toboolean('')") == "false");
    CHECK(Eval(L, "return ('on'):toboolean()") == "true");
    // Anything else is luaL_checklstring's argument error.
    CHECK(Eval(L, "return string.toboolean(nil)").find("error: ") == 0);
    CHECK(Eval(L, "return string.toboolean({})").find("string expected") != std::string::npos);
    // The client's own functions are still there.
    CHECK(Eval(L, "return string.format('%d', 3) == '3'") == "true");
    lua_close(L);
}

// FUN_10a4c0c0, on the after-0x52A980 list: after every UI reload the store of CGxDeviceD3d::DeviceSetBaseMipLevel
// (0x68E4AD) is skipped.
COA_TEST(ClientGuards_BaseMipStoreAfterReload) {
    CoA::ClientHooks::afterUiReload.Clear();
    CoA::ClientGuards::Dependencies().skipBaseMipStore = SkipBaseMip;
    s_skips = 0;
    CoA::ClientGuards::Init();
    CHECK(s_skips == 0);
    CoA::ClientHooks::afterUiReload.Run();
    CHECK(s_skips == 1);
    CoA::ClientHooks::afterUiReload.Run();
    CHECK(s_skips == 2);
    CoA::ClientHooks::afterUiReload.Clear();
}
