// AscChatCheck.cpp (see ChatCheck.h): the server's chat-check phrases and GetChatCheck.
#include "CoA/Source/Modules/ChatCheck.h"
#include "CoA/Source/Modules/AttachMisc.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

namespace {
    // FUN_1006c3b0's initializer list: CHAT_MSG_ADDON is listed twice (1, then -1); the map keeps the first.
    const std::pair<const char*, int8_t> kChatTypes[] = {
        { "CHAT_MSG_ADDON", 1 }, { "CHAT_MSG_ADDON", -1 }, { "CHAT_MSG_SYSTEM", 0 }, { "CHAT_MSG_SAY", 1 },
        { "CHAT_MSG_PARTY", 2 }, { "CHAT_MSG_RAID", 3 }, { "CHAT_MSG_GUILD", 4 }, { "CHAT_MSG_OFFICER", 5 },
        { "CHAT_MSG_YELL", 6 }, { "CHAT_MSG_WHISPER", 7 }, { "CHAT_MSG_WHISPER_FOREIGN", 8 }, { "CHAT_MSG_WHISPER_INFORM", 9 },
        { "CHAT_MSG_EMOTE", 10 }, { "CHAT_MSG_TEXT_EMOTE", 11 }, { "CHAT_MSG_MONSTER_SAY", 12 }, { "CHAT_MSG_MONSTER_PARTY", 13 },
        { "CHAT_MSG_MONSTER_YELL", 14 }, { "CHAT_MSG_MONSTER_WHISPER", 15 }, { "CHAT_MSG_MONSTER_EMOTE", 16 },
        { "CHAT_MSG_CHANNEL", 17 }, { "CHAT_MSG_CHANNEL_JOIN", 18 }, { "CHAT_MSG_CHANNEL_LEAVE", 19 },
        { "CHAT_MSG_CHANNEL_LIST", 20 }, { "CHAT_MSG_CHANNEL_NOTICE", 21 }, { "CHAT_MSG_CHANNEL_NOTICE_USER", 22 },
        { "CHAT_MSG_AFK", 23 }, { "CHAT_MSG_DND", 24 }, { "CHAT_MSG_IGNORED", 25 }, { "CHAT_MSG_SKILL", 26 },
        { "CHAT_MSG_LOOT", 27 }, { "CHAT_MSG_MONEY", 28 }, { "CHAT_MSG_OPENING", 29 }, { "CHAT_MSG_TRADESKILLS", 30 },
        { "CHAT_MSG_PET_INFO", 31 }, { "CHAT_MSG_COMBAT_MISC_INFO", 32 }, { "CHAT_MSG_COMBAT_XP_GAIN", 33 },
        { "CHAT_MSG_COMBAT_HONOR_GAIN", 34 }, { "CHAT_MSG_COMBAT_FACTION_CHANGE", 35 }, { "CHAT_MSG_BG_SYSTEM_NEUTRAL", 36 },
        { "CHAT_MSG_BG_SYSTEM_ALLIANCE", 37 }, { "CHAT_MSG_BG_SYSTEM_HORDE", 38 }, { "CHAT_MSG_RAID_LEADER", 39 },
        { "CHAT_MSG_RAID_WARNING", 40 }, { "CHAT_MSG_RAID_BOSS_EMOTE", 41 }, { "CHAT_MSG_RAID_BOSS_WHISPER", 42 },
        { "CHAT_MSG_FILTERED", 43 }, { "CHAT_MSG_BATTLEGROUND", 44 }, { "CHAT_MSG_BATTLEGROUND_LEADER", 45 },
        { "CHAT_MSG_RESTRICTED", 46 }, { "CHAT_MSG_BATTLENET", 47 }, { "CHAT_MSG_ACHIEVEMENT", 48 },
        { "CHAT_MSG_GUILD_ACHIEVEMENT", 49 }, { "CHAT_MSG_ARENA_POINTS", 50 }, { "CHAT_MSG_PARTY_LEADER", 51 },
    };

    const std::unordered_map<std::string, int8_t>& ChatTypeIds() {   // 0x10BDEB68
        static std::unordered_map<std::string, int8_t> ids;

        if (ids.empty()) {
            for (const auto& t : kChatTypes) {
                ids.emplace(t.first, t.second);   // the first wins
            }
        }

        return ids;
    }

    std::map<uint32_t, std::set<std::string>> s_checks;   // Chat manager +0x04

    // u32 type, then FUN_100d3680's u32-length string with its last byte dropped. The original shortens a zero-length
    // string to size -1 (an out-of-range std::string); such an entry, and a length past the packet, is dropped here.
    bool ReadCheck(CDataStore* msg, uint32_t& type, std::string& phrase) {
        type = CoA::Read::U32(msg);
        phrase = CoA::Read::Name(msg);

        if (phrase.empty()) {
            return false;
        }

        phrase.pop_back();
        return true;
    }

    int32_t OnChatCheckAdd(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x928 FUN_101929e0
        uint32_t type;
        std::string phrase;

        if (ReadCheck(msg, type, phrase)) {
            s_checks[type].insert(phrase);   // FUN_10192540
        }

        return 1;
    }

    int32_t OnChatCheckRemove(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x929 FUN_10192ac0
        uint32_t type;
        std::string phrase;

        if (ReadCheck(msg, type, phrase)) {
            auto it = s_checks.find(type);

            if (it != s_checks.end()) {
                it->second.erase(phrase);
            }
        }

        return 1;
    }

    // 0x10192C40: (chatType, text) -> true when the text contains any phrase of the type (an empty phrase always does).
    int GetChatCheck(lua_State* L) {
        const std::string name = CoA::Script::CheckString(L, 1);
        const std::string text = CoA::Script::CheckString(L, 2);
        const auto& ids = ChatTypeIds();
        const auto id = ids.find(name);
        const uint32_t type = id == ids.end() ? 0u : static_cast<uint32_t>(static_cast<int32_t>(id->second));
        const auto set = s_checks.find(type);
        bool found = false;

        if (set != s_checks.end()) {
            for (const std::string& phrase : set->second) {   // FUN_10192720 copies the set
                if (phrase == text || (phrase.size() <= text.size() && text.find(phrase) != std::string::npos)) {
                    found = true;
                    break;
                }
            }
        }

        CoA::Script::PushBool(L, found);
        return 1;
    }

    void Clear() {   // FUN_101929d0
        s_checks.clear();
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetChatCheck", GetChatCheck),
    };
}

void CoA::ChatCheck::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_PACKET(0x928, OnChatCheckAdd);
    COA_PACKET(0x929, OnChatCheckRemove);
    CoA::AttachMisc::loginVerifyWorld.Add(Clear);
}

void CoA::ChatCheck::Reset() {
    Clear();
}
