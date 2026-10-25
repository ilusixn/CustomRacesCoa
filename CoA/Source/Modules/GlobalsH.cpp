// More GLOBAL bindings, mostly request senders (port of AscGlobalsH.cpp). Ported so far: the glue-state natives.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/Os/System.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>

namespace {
    int32_t ArgInt(lua_State* L, int idx) {
        return CoA::Script::ToInt(lua_tonumber(L, idx));
    }

    // FUN_10296d60: the levels lua_getstack can walk.
    int GetCallstackHeight(lua_State* L) {
        lua_Debug ar;
        int n = 0;

        while (lua_getstack(L, n, &ar)) {
            ++n;
        }

        lua_pushinteger(L, n);
        return 1;
    }

    int CharacterActivate(lua_State* L) {
        if (int32_t id = ArgInt(L, 1)) {
            CoA::Packet(0x72E).U32(static_cast<uint32_t>(id)).Send();
        }

        return 0;
    }

    int CharacterDeactivate(lua_State* L) {
        if (int32_t id = ArgInt(L, 1)) {
            CoA::Packet(0x72F).U32(static_cast<uint32_t>(id)).Send();
        }

        return 0;
    }

    // FUN_10a4ebc0 / FUN_10a4ece0 / FUN_10a4ee00: a fixed site + the argument, opened by OsLaunchURL (0x86B790).
    int OpenUrl(lua_State* L, const char* base) {
        std::string url = std::string(base) + CoA::Script::CheckString(L, 1);
        OsLaunchURL(url.c_str());
        return 0;
    }

    int OpenAscensionDBURL(lua_State* L) {
        return OpenUrl(L, "https://db.ascension.gg/");
    }

    int OpenAscensionURL(lua_State* L) {
        return OpenUrl(L, "https://ascension.gg/");
    }

    int OpenAscensionWikiURL(lua_State* L) {
        return OpenUrl(L, "https://project-ascension.fandom.com/wiki/Project_Ascension_Wiki/");
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "CharacterActivate", CharacterActivate),
        COA_NATIVE("", "CharacterDeactivate", CharacterDeactivate),
        COA_NATIVE("", "GetCallstackHeight", GetCallstackHeight),
        COA_NATIVE("", "OpenAscensionDBURL", OpenAscensionDBURL),
        COA_NATIVE("", "OpenAscensionURL", OpenAscensionURL),
        COA_NATIVE("", "OpenAscensionWikiURL", OpenAscensionWikiURL),
    };
}

void CoA::GlobalsH::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
