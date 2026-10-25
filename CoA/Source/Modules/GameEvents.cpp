#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>
#include <ctime>
#include <map>
#include <string>

namespace {
    struct Record {
        uint8_t raw[0x34];
    };

    std::map<uint16_t, Record> s_events;

    uint16_t Id(const Record& r) {
        uint16_t v;
        memcpy(&v, r.raw, 2);
        return v;
    }

    uint8_t State(const Record& r) {
        return r.raw[2];
    }

    uint64_t Start(const Record& r) {
        uint64_t v;
        memcpy(&v, r.raw + 3, 8);
        return v;
    }

    uint64_t End(const Record& r) {
        uint64_t v;
        memcpy(&v, r.raw + 0xB, 8);
        return v;
    }

    bool Active(const Record& r) {
        return r.raw[0x13] != 0;
    }

    int32_t Length(const Record& r) {
        int32_t v;
        memcpy(&v, r.raw + 0x14, 4);
        return v;
    }

    // FUN_10256ca0: 0..5 -> GAMEEVENT_*, else "UNEXPECTED_ENUM_VALUE_<n>".
    std::string StateName(uint8_t state) {
        static const char* const names[6] = {
            "GAMEEVENT_NORMAL",
            "GAMEEVENT_WORLD_INACTIVE",
            "GAMEEVENT_WORLD_CONDITIONS",
            "GAMEEVENT_WORLD_NEXTPHASE",
            "GAMEEVENT_WORLD_FINISHED",
            "GAMEEVENT_INTERNAL",
        };

        if (state < 6) {
            return names[state];
        }

        return "UNEXPECTED_ENUM_VALUE_" + std::to_string(state);
    }

    // Seconds until start/end (0 once passed or unset); timeSinceStart = length - untilEnd, or 0. GetGameEventInfos
    // pushes eventId as a signed short, GetGameEventInfo as unsigned.
    void PushInfo(lua_State* L, const Record& r, uint64_t now, bool signedId) {
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);
        CoA::Script::SetField(L, "eventId", signedId ? static_cast<int16_t>(Id(r)) : Id(r));
        lua_pushstring(L, "active");
        CoA::Script::PushBool(L, Active(r));
        lua_settable(L, -3);
        lua_pushstring(L, "eventState");
        CoA::Script::PushStr(L, StateName(State(r)).c_str());
        lua_settable(L, -3);
        uint64_t start = Start(r);
        uint64_t end = End(r);
        CoA::Script::SetField(L, "startTime", (start == 0 || start < now) ? 0 : static_cast<int32_t>(start - now));
        uint64_t untilEnd = (end == 0 || end < now) ? 0 : end - now;
        CoA::Script::SetField(L, "endTime", static_cast<int32_t>(untilEnd));
        CoA::Script::SetField(L, "timeSinceStart", untilEnd == 0 ? 0 : Length(r) - static_cast<int32_t>(untilEnd));
    }

    int GetGameEventInfo(lua_State* L) {
        uint16_t id = static_cast<uint16_t>(static_cast<int32_t>(CoA::Script::CheckNumber(L, 1)));
        auto it = s_events.find(id);

        if (it == s_events.end()) {
            lua_pushnil(L);
            return 1;
        }

        PushInfo(L, it->second, static_cast<uint64_t>(_time64(nullptr)), false);
        return 1;
    }

    // { [eventId] = info, ... } in key order.
    int GetGameEventInfos(lua_State* L) {
        uint64_t now = static_cast<uint64_t>(_time64(nullptr));
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (const auto& e : s_events) {
            CoA::Script::PushInt(L, e.first);
            PushInfo(L, e.second, now, true);
            lua_settable(L, -3);
        }

        return 1;
    }

    int IsGameEventActive(lua_State* L) {
        uint16_t id = static_cast<uint16_t>(static_cast<int32_t>(CoA::Script::CheckNumber(L, 1)));
        CoA::Script::PushBool(L, CoA::GameEvents::IsActive(id));
        return 1;
    }

    int GetServerMaxLevel(lua_State* L) {
        CoA::Script::PushInt(L, static_cast<int32_t>(CoA::GameEvents::ServerMaxLevel()));
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetServerMaxLevel", GetServerMaxLevel),
        COA_NATIVE("", "GetGameEventInfo", GetGameEventInfo),
        COA_NATIVE("", "GetGameEventInfos", GetGameEventInfos),
        COA_NATIVE("", "IsGameEventActive", IsGameEventActive),
    };
}

// FUN_10256e70.
int32_t CoA::GameEvents::OnGameEventInfo(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    Record rec;
    CoA::Read::Data(msg, rec.raw, sizeof(rec.raw));
    uint16_t key = Id(rec);
    auto before = s_events.find(key);
    bool wasActive = before != s_events.end() && Active(before->second);
    s_events[key] = rec;

    if (Active(rec)) {
        if (!wasActive) {
            CoA::Events::Signal("GAME_EVENT_STARTED", "%u", static_cast<uint32_t>(key));
        }
    } else if (wasActive) {
        CoA::Events::Signal("GAME_EVENT_ENDED", "%u", static_cast<uint32_t>(key));
    }

    return 1;
}

bool CoA::GameEvents::IsActive(uint16_t eventId) {
    auto it = s_events.find(eventId);
    return it != s_events.end() && Active(it->second);
}

// handler_GetServerMaxLevel -> FUN_102fc4d0: the ruleset cap (realm object +8: 1 -> 70, 2 -> 80, else 60), lowered by
// whichever level-cap game event is active.
uint32_t CoA::GameEvents::ServerMaxLevel() {
    uint32_t ruleset = CoA::RealmInfo::Get().ruleset;
    uint32_t cap = ruleset == 1 ? 70 : ruleset == 2 ? 80 : 60;

    if (IsActive(0x164)) {
        return cap;
    }

    if (IsActive(0xAD)) {
        return cap < 70 ? cap : 70;
    }

    if (!IsActive(0x1EA)) {
        if (IsActive(0x1E9)) {
            return 55;
        }

        if (IsActive(0x1E8)) {
            return 45;
        }

        if (IsActive(0x1E7)) {
            return 35;
        }

        if (IsActive(0x1E6)) {
            return 25;
        }

        if (!IsActive(0x160)) {
            return cap;
        }
    }

    return cap < 60 ? cap : 60;
}

void CoA::GameEvents::Reset() {
    s_events.clear();
}

void CoA::GameEvents::Init() {
    COA_PACKET(0x9BD, CoA::GameEvents::OnGameEventInfo);
    COA_EVENT("GAME_EVENT_STARTED");
    COA_EVENT("GAME_EVENT_ENDED");
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
