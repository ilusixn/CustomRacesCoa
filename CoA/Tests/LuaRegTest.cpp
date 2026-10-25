#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

namespace {
    int Ping(lua_State* L) {
        lua_pushnumber(L, 1.0);
        return 1;
    }

    // Names chosen from BindingStates.generated.inc: FadeScreen is in the glue list (both states),
    // FillLocalizedClassList is glue-only, AbandonSkillID is not in the glue list (world only).
    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "FadeScreen", Ping),
        COA_NATIVE("", "FillLocalizedClassList", Ping),
        COA_NATIVE("", "AbandonSkillID", Ping),
        COA_NATIVE("C_AccountInfo", "IsGM", Ping),
        COA_NATIVE("@TestMeta", "Probe", Ping),
        COA_NATIVE("#Frame", "WidgetOnly", Ping),
    };

    bool IsFunction(lua_State* L, const char* ns, const char* name) {
        int top = lua_gettop(L);
        bool found;

        if (*ns) {
            lua_getfield(L, LUA_GLOBALSINDEX, ns);
            found = lua_istable(L, -1) && (lua_getfield(L, -1, name), lua_isfunction(L, -1));
        } else {
            lua_getfield(L, LUA_GLOBALSINDEX, name);
            found = lua_isfunction(L, -1);
        }

        lua_settop(L, top);
        return found;
    }

    lua_State* NewState(bool glue) {
        CoA::LuaReg::Reset();
        CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
        lua_State* L = luaL_newstate();
        CoA::LuaReg::Register(L, glue);
        return L;
    }
}

COA_TEST(LuaReg_GlueStateGetsGlueListedNativesOnly) {
    lua_State* L = NewState(true);
    CHECK(IsFunction(L, "", "FadeScreen"));
    CHECK(IsFunction(L, "", "FillLocalizedClassList"));
    CHECK(!IsFunction(L, "", "AbandonSkillID"));
    CHECK(!IsFunction(L, "C_AccountInfo", "IsGM"));
    lua_close(L);
}

COA_TEST(LuaReg_WorldStateGetsAllButGlueOnly) {
    lua_State* L = NewState(false);
    CHECK(IsFunction(L, "", "FadeScreen"));
    CHECK(!IsFunction(L, "", "FillLocalizedClassList"));
    CHECK(IsFunction(L, "", "AbandonSkillID"));
    CHECK(IsFunction(L, "C_AccountInfo", "IsGM"));
    lua_close(L);
}

COA_TEST(LuaReg_RegistryMetatableMethodInBothStatesAndWidgetMethodsSkipped) {
    for (int glue = 0; glue <= 1; glue++) {
        lua_State* L = NewState(glue != 0);
        CHECK(IsFunction(L, "TestMeta", "Probe"));
        lua_getfield(L, LUA_REGISTRYINDEX, "TestMeta");
        CHECK(lua_istable(L, -1));

        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "__index");
            CHECK(lua_rawequal(L, -1, -2));
        }

        lua_settop(L, 0);
        CHECK(!IsFunction(L, "Frame", "WidgetOnly"));
        CHECK(!IsFunction(L, "", "WidgetOnly"));
        lua_close(L);
    }
}

COA_TEST(LuaReg_TwoConsecutiveStatesBothGetTheNatives) {
    lua_State* first = NewState(true);
    lua_State* second = luaL_newstate();
    CoA::LuaReg::Register(second, true);
    CHECK(IsFunction(first, "", "FadeScreen"));
    CHECK(IsFunction(second, "", "FadeScreen"));
    lua_getfield(second, LUA_GLOBALSINDEX, "FadeScreen");
    CHECK(lua_pcall(second, 0, 1, 0) == 0 && lua_tonumber(second, -1) == 1.0);
    lua_close(first);
    lua_close(second);
}

COA_TEST(LuaReg_RegisteredCallbacksRunPerState) {
    static int glueRuns;
    static int worldRuns;
    glueRuns = 0;
    worldRuns = 0;
    CoA::LuaReg::Reset();
    CoA::LuaReg::OnGlueRegistered([] { glueRuns++; });
    CoA::LuaReg::OnWorldRegistered([] { worldRuns++; });
    CoA::LuaReg::RunGlueRegistered();
    CoA::LuaReg::RunGlueRegistered();
    CoA::LuaReg::RunWorldRegistered();
    CHECK(glueRuns == 2);
    CHECK(worldRuns == 1);
}

COA_TEST(LuaReg_ForEachWidgetMethod) {
    CoA::LuaReg::Reset();
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    int count = 0;
    CoA::LuaReg::ForEachWidgetMethod("Frame", [](const char* name, lua_CFunction fn, void* ctx) { (*static_cast<int*>(ctx))++; }, &count);
    CHECK(count == 1);
}
