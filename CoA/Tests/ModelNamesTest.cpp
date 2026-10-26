#include "CoA/Source/Modules/ModelNames.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <string>
#include <vector>

namespace {
    using V = std::vector<std::string>;

    // ItemDisplayInfoCollections.dbc rows of 0x20 bytes: id, ..., +0x10 the model path.
    bool ReadCollections(const char* path, std::vector<uint8_t>& out) {
        if (!strstr(path, "ItemDisplayInfoCollections.dbc")) {
            return false;
        }

        // Strings: 1 "Item\Objects\Sword.m2", 23 "Creature\Wolf\Wolf.m2" (also a client path), 45 "".
        const std::string strings = std::string("Item\\Objects\\Sword.m2") + std::string(1, '\0') +
            "Creature\\Wolf\\Wolf.m2" + std::string(1, '\0');
        out = CoATest::Wdbc({ { 4, 0, 0, 0, 1, 0, 0, 0 }, { 6, 0, 0, 0, 23, 0, 0, 0 }, { 7, 0, 0, 0, 0, 0, 0, 0 } }, strings);
        return true;
    }

    void ClientPaths(void (*add)(const char* path, void* ctx), void* ctx) {
        add("Creature\\Wolf\\Wolf.m2", ctx);
        add(nullptr, ctx);
        add("Creature/Wolf/Wolf.m2", ctx);   // a different stored spelling: kept twice after the '/' rewrite
        add("World\\Generic\\Tree.m2", ctx);
        add("Creature\\Wolf\\Wolf.m2", ctx);
        add("Spells\\Fire_Wolf.m2", ctx);
    }

    void Start() {
        CoA::LuaReg::Reset();
        CoA::ModelNames::Reset();
        CoA::Dbc::SetFileReader(ReadCollections);
        CoA::Dbc::Get("DBFilesClient\\ItemDisplayInfoCollections.dbc").Unload();
        CoA::ModelNames::Dependencies().clientPaths = ClientPaths;
        CoA::ModelNames::Init();
    }

    void Stop() {
        CoA::Dbc::Get("DBFilesClient\\ItemDisplayInfoCollections.dbc").Unload();
        CoA::Dbc::SetFileReader(nullptr);
        CoA::ModelNames::Reset();
        CoA::LuaReg::Reset();
    }

    V Run(lua_State* L, const std::string& chunk) {
        V out;
        const int base = lua_gettop(L);

        if (luaL_loadstring(L, chunk.c_str()) != 0 || lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
            out.push_back(std::string("<error> ") + lua_tostring(L, -1));
            lua_settop(L, base);
            return out;
        }

        for (int i = base + 1; i <= lua_gettop(L); ++i) {
            out.push_back(lua_isnil(L, i) ? "nil" : lua_istable(L, i) ? "table" : lua_tostring(L, i));
        }

        lua_settop(L, base);
        return out;
    }
}

// FUN_10a701b0: client paths first, then the collections (min .. max), each stored spelling once, '\' -> '/'.
COA_TEST(ModelNames_PopulateCache) {
    Start();
    CoA::ModelNames::Populate();
    CHECK((CoA::ModelNames::Paths() == V{ "Creature/Wolf/Wolf.m2", "Creature/Wolf/Wolf.m2", "World/Generic/Tree.m2",
        "Spells/Fire_Wolf.m2", "Item/Objects/Sword.m2", "" }));
    CoA::ModelNames::Populate();                      // rebuilt, not appended
    CHECK(CoA::ModelNames::Paths().size() == 6);
    Stop();
}

// Tokens < 3 ignored; quoted terms gate; score = terms contained; highest first; cut to the limit.
COA_TEST(ModelNames_Completions) {
    Start();
    CoA::ModelNames::Populate();
    CHECK((CoA::ModelNames::Completions("Tree", 10) == V{ "World/Generic/Tree.m2" }));
    CHECK((CoA::ModelNames::Completions("tree", 10) == V{}));                 // case-sensitive
    CHECK((CoA::ModelNames::Completions("Wo", 10) == V{}));                   // too short
    V wolf = CoA::ModelNames::Completions("Wolf Fire", 10);
    CHECK(wolf.size() == 3 && wolf[0] == "Spells/Fire_Wolf.m2");             // two terms beat one
    CHECK((CoA::ModelNames::Completions("Wolf Fire", 1) == V{ "Spells/Fire_Wolf.m2" }));
    CHECK(CoA::ModelNames::Completions("Wolf Fire", 2).size() == 2);         // cut at exactly the limit
    CHECK(CoA::ModelNames::Completions("Wolf Fire", 3).size() == 3);
    CHECK((CoA::ModelNames::Completions("\"Item\" Wolf", 10) == V{ "Item/Objects/Sword.m2" }));   // quoted gate
    CHECK((CoA::ModelNames::Completions("\"Nope\" Wolf", 10) == V{}));
    CHECK((CoA::ModelNames::Completions("\"\" Tree", 10) == V{ "World/Generic/Tree.m2" }));   // "" is 2 characters: ignored
    CHECK((CoA::ModelNames::Completions("\"m2\"", 10).size() == 5));         // every path but the empty one
    CHECK((CoA::ModelNames::Completions("Tree  ", 10) == V{ "World/Generic/Tree.m2" }));         // empty tokens ignored
    Stop();
}

COA_TEST(ModelNames_Natives) {
    Start();
    lua_State* L = CoATest::NewState(false);
    // One result with nothing pushed: Lua hands back the top slot, the native itself.
    CHECK((Run(L, "return type(PopulateModelNameCache())") == V{ "function" }));
    CHECK((Run(L, "local t = GetModelNameCompletions('Wolf Fire', 2) return #t, t[1]") == V{ "2", "Spells/Fire_Wolf.m2" }));
    CHECK((Run(L, "local t = GetModelNameCompletions('zzz', 5) return #t") == V{ "0" }));
    const V missing = Run(L, "return GetModelNameCompletions('Wolf')");   // the limit is required
    CHECK(missing.size() == 1 && missing[0].find("<error>") == 0);
    lua_close(L);
    Stop();
}
