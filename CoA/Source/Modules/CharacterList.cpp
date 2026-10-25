// C_CharacterList over CharacterListMgr (FUN_10191230 -> static 0x10BDEB48); port of AscCharacterList.cpp.
// SMSG_CHARACTER_LIST_INFO 0x75E (FUN_10190d60): u32 (capped at 0x80), u32 n, u32, u32 inactive, then n x {u32 guid,
// u8 active, u8 online, u8 level, u8 race, u8 class, u8 gender, u32 zone, str name}; fires CHARACTER_LIST_UPDATED.
// Refresh sends CMSG 0x37 (CMSG_CHAR_ENUM); RequestActivate / RequestDeactivate send CMSG 0x72E / 0x72F {u32 guid}.
#include "CoA/Source/Modules/Modules.h"
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
    struct Entry {
        uint32_t guid = 0;
        std::string name;
        uint8_t race = 0;
        uint8_t cls = 0;
        uint8_t gender = 0;
        uint8_t level = 0;
        uint32_t zone = 0;
        bool active = false;
        bool online = false;
    };

    struct List {
        uint32_t counts[4] = {};
        std::vector<Entry> entries;
    };

    List s_list;

    int32_t OnCharacterList(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        s_list.counts[0] = std::min<uint32_t>(CoA::Read::U32(msg), 0x80);
        s_list.counts[1] = CoA::Read::U32(msg);
        s_list.counts[2] = CoA::Read::U32(msg);
        s_list.counts[3] = CoA::Read::U32(msg);
        s_list.entries.clear();

        for (uint32_t i = 0; i < s_list.counts[1] && !CoA::Read::Failed(msg); ++i) {
            Entry e;
            e.guid = CoA::Read::U32(msg);
            e.active = CoA::Read::U8(msg) != 0;
            e.online = CoA::Read::U8(msg) != 0;
            e.level = CoA::Read::U8(msg);
            e.race = CoA::Read::U8(msg);
            e.cls = CoA::Read::U8(msg);
            e.gender = CoA::Read::U8(msg);
            e.zone = CoA::Read::U32(msg);
            e.name = CoA::Read::Str(msg);

            if (CoA::Read::Failed(msg)) {
                break;
            }

            s_list.entries.push_back(e);
        }

        CoA::Events::Signal("CHARACTER_LIST_UPDATED", nullptr);
        return 1;
    }

    int GetCounts(lua_State* L) {
        for (uint32_t c : s_list.counts) {
            CoA::Script::PushInt(L, static_cast<int32_t>(c));
        }

        return 4;
    }

    void SetInt(lua_State* L, const char* key, int32_t value) {
        lua_pushstring(L, key);
        CoA::Script::PushInt(L, value);
        lua_settable(L, -3);
    }

    void SetBool(lua_State* L, const char* key, bool value) {
        lua_pushstring(L, key);
        CoA::Script::PushBool(L, value);
        lua_settable(L, -3);
    }

    int GetEntries(lua_State* L) {
        lua_createtable(L, static_cast<int>(s_list.entries.size()), 0);
        lua_checkstack(L, 2);
        int i = 1;

        for (const Entry& e : s_list.entries) {
            lua_createtable(L, 0, 9);
            SetInt(L, "guid", static_cast<int32_t>(e.guid));
            lua_pushstring(L, "name");
            CoA::Script::PushStr(L, e.name.c_str());
            lua_settable(L, -3);
            SetBool(L, "active", e.active);
            SetBool(L, "online", e.online);
            SetInt(L, "level", e.level);
            SetInt(L, "race", e.race);
            SetInt(L, "class", e.cls);
            SetInt(L, "gender", e.gender);
            SetInt(L, "zoneId", static_cast<int32_t>(e.zone));
            lua_rawseti(L, -2, i++);
        }

        return 1;
    }

    int HasInactiveCharacters(lua_State* L) {
        CoA::Script::PushBool(L, s_list.counts[3] != 0);
        return 1;
    }

    int Refresh(lua_State* L) {
        CoA::Packet(0x37).Send();
        CoA::Script::PushBool(L, true);
        return 1;
    }

    int SendGuid(lua_State* L, uint32_t opcode) {
        uint32_t guid;

        if (!CoA::Script::ReadNumber(L, guid)) {
            return 0;
        }

        CoA::Packet(opcode).U32(guid).Send();
        CoA::Script::PushBool(L, true);
        return 1;
    }

    int RequestActivate(lua_State* L) {
        return SendGuid(L, 0x72E);
    }

    int RequestDeactivate(lua_State* L) {
        return SendGuid(L, 0x72F);
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_CharacterList", "GetCounts", GetCounts),
        COA_NATIVE("C_CharacterList", "GetEntries", GetEntries),
        COA_NATIVE("C_CharacterList", "HasInactiveCharacters", HasInactiveCharacters),
        COA_NATIVE("C_CharacterList", "Refresh", Refresh),
        COA_NATIVE("C_CharacterList", "RequestActivate", RequestActivate),
        COA_NATIVE("C_CharacterList", "RequestDeactivate", RequestDeactivate),
    };
}

void CoA::CharacterList::Init() {
    COA_PACKET(0x75E, OnCharacterList);
    COA_EVENT("CHARACTER_LIST_UPDATED");
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
