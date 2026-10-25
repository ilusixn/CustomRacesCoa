// AscAutoQuest.cpp (see AutoQuest.h).
#include "CoA/Source/Modules/AutoQuest.h"
#include "CoA/Source/Modules/AttachMisc.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <string>
#include <vector>

namespace {
    struct PopUp {
        uint32_t type;
        uint32_t questId;
    };

    std::vector<PopUp> s_popUps;

    const char* const kTypes[] = { "OFFER", "COMPLETE" };   // PTR_s_OFFER_10b2cf74

    int32_t ArgInt(lua_State* L, int idx) {
        return CoA::Script::ToInt(lua_tonumber(L, idx));
    }

    // (questId, "OFFER" | "COMPLETE") -> true; nothing for any other type.
    int AddAutoQuestPopUp(lua_State* L) {
        const uint32_t questId = static_cast<uint32_t>(ArgInt(L, 1));
        const std::string type = CoA::Script::CheckString(L, 2);

        for (uint32_t i = 0; i < 2; ++i) {
            if (type == kTypes[i]) {
                s_popUps.push_back({ i, questId });
                lua_pushboolean(L, 1);
                return 1;
            }
        }

        return 0;
    }

    // n (1-based) -> questId, type name.
    int GetAutoQuestPopUp(lua_State* L) {
        const uint32_t n = static_cast<uint32_t>(ArgInt(L, 1));

        if (s_popUps.size() < n || n == 0) {
            return 0;
        }

        const PopUp& p = s_popUps[n - 1];
        lua_pushinteger(L, static_cast<int>(p.questId));
        const std::string name = p.type < 2 ? kTypes[p.type] : "UNEXPECTED_ENUM_VALUE_" + std::to_string(p.type);
        lua_pushstring(L, name.c_str());
        return 2;
    }

    int GetNumAutoQuestPopUps(lua_State* L) {
        lua_pushinteger(L, CoA::ClientState::InWorld() ? static_cast<int>(s_popUps.size()) : 0);
        return 1;
    }

    int RemoveAutoQuestPopUp(lua_State* L) {
        const uint32_t questId = static_cast<uint32_t>(ArgInt(L, 1));
        s_popUps.erase(std::remove_if(s_popUps.begin(), s_popUps.end(), [questId](const PopUp& p) { return p.questId == questId; }),
            s_popUps.end());
        return 0;
    }

    // Pop-up n -> CMSG 0x542 { u32 questId }.
    int ShowQuestComplete(lua_State* L) {
        const uint32_t n = static_cast<uint32_t>(ArgInt(L, 1));

        if (n <= s_popUps.size() && n != 0) {
            CoA::Packet(0x542).U32(s_popUps[n - 1].questId).Send();
        }

        return 0;
    }

    // CMSG_QUESTGIVER_ACCEPT_QUEST (0x189) with no quest giver: { u64 0, u32 questId, u32 0 }.
    int Custom_SendAutoQuestAccept(lua_State* L) {
        const uint32_t questId = static_cast<uint32_t>(ArgInt(L, 1));
        CoA::Packet(0x189).U64(0).U32(questId).U32(0).Send();
        return 0;
    }

    // SMSG 0x927 (FUN_100e6220): u32 quest, u32 type. A listed quest only moves UP (a type not above its current one
    // ends here, without an event); an unlisted one is appended. Then type 0 fires QUEST_AUTO_OFFER and type 1
    // QUEST_AUTOCOMPLETE ("%u", quest).
    int32_t OnAutoQuest(void*, uint32_t, uint32_t, CDataStore* msg) {
        const uint32_t quest = CoA::Read::U32(msg);
        const uint32_t type = CoA::Read::U32(msg);
        auto it = std::find_if(s_popUps.begin(), s_popUps.end(), [&](const PopUp& u) { return u.questId == quest; });

        if (it != s_popUps.end()) {
            if (static_cast<int32_t>(it->type) >= static_cast<int32_t>(type)) {
                return 1;
            }

            it->type = type;
        } else {
            s_popUps.push_back({ type, quest });
        }

        if (type == 0) {
            CoA::Events::Signal("QUEST_AUTO_OFFER", "%u", quest);
        } else if (type == 1) {
            CoA::Events::Signal("QUEST_AUTOCOMPLETE", "%u", quest);
        }

        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "AddAutoQuestPopUp", AddAutoQuestPopUp),
        COA_NATIVE("", "Custom_SendAutoQuestAccept", Custom_SendAutoQuestAccept),
        COA_NATIVE("", "GetAutoQuestPopUp", GetAutoQuestPopUp),
        COA_NATIVE("", "GetNumAutoQuestPopUps", GetNumAutoQuestPopUps),
        COA_NATIVE("", "RemoveAutoQuestPopUp", RemoveAutoQuestPopUp),
        COA_NATIVE("", "ShowQuestComplete", ShowQuestComplete),
    };
}

void CoA::AutoQuest::Clear() {
    s_popUps.clear();
}

void CoA::AutoQuest::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_PACKET(0x927, OnAutoQuest);
    CoA::AttachMisc::loginVerifyWorld.Add(Clear);
    COA_EVENT("QUEST_AUTO_OFFER");
    COA_EVENT("QUEST_AUTOCOMPLETE");
}

void CoA::AutoQuest::Reset() {
    s_popUps.clear();
}
