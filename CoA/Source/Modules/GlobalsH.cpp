// More GLOBAL bindings, mostly request senders (port of AscGlobalsH.cpp); AbandonSkillID is in GlobalsHClient.cpp.
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

    int SendDraftSkip(lua_State*) {
        CoA::Packet(0x74D).Send();
        return 0;
    }

    int RedraftAbilities(lua_State*) {
        CoA::Packet(0x74E).Send();
        return 0;
    }

    int SendDraftModeSpellSelection(lua_State* L) {
        CoA::Packet(0x74C).U32(static_cast<uint32_t>(ArgInt(L, 1))).Send();
        return 0;
    }

    int DeleteStablePet(lua_State* L) {
        CoA::Packet(0x67C).U32(static_cast<uint32_t>(static_cast<int>(lua_tonumber(L, 1)))).Send();
        return 0;
    }

    int RequestStatisticQuery(lua_State* L) {
        uint32_t id;

        if (CoA::Script::ReadNumber(L, id)) {
            CoA::Packet(0x777).U32(id).Send();
        }

        return 0;
    }

    // CMSG 0x523 { u8 2, u32 item } -- the vanity-collection flavour of the delivery request.
    int RequestDeliverVanityCollectionItem(lua_State* L) {
        CoA::Packet(0x523).U8(2).U32(static_cast<uint32_t>(ArgInt(L, 1))).Send();
        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "DeleteStablePet", DeleteStablePet),
        COA_NATIVE("", "RedraftAbilities", RedraftAbilities),
        COA_NATIVE("", "RequestDeliverVanityCollectionItem", RequestDeliverVanityCollectionItem),
        COA_NATIVE("", "RequestStatisticQuery", RequestStatisticQuery),
        COA_NATIVE("", "SendDraftModeSpellSelection", SendDraftModeSpellSelection),
        COA_NATIVE("", "SendDraftSkip", SendDraftSkip),
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
