// AscCallboardCache.cpp (see Callboard.h): the Callboard cache level table (SMSG 0x730) and C_CallboardCache.
#include "CoA/Source/Modules/Callboard.h"
#include "CoA/Source/Modules/GameEvents.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/Token.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <vector>

using CoA::Script::PushInt;

namespace {
    struct Entry {
        uint32_t a, b, event, threshold, info, points;
    };

    static_assert(sizeof(Entry) == 0x18, "0x730 entry");

    CoA::CallboardCache::Deps s_deps;
    std::vector<Entry> s_levels;   // 0x10BDD1C4

    int32_t OnCacheLevels(void*, uint32_t, uint32_t, CDataStore* msg) {   // 0x730 FUN_10115400
        std::vector<Entry> levels;

        for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg) != 0; --n) {
            Entry e;
            CoA::Read::Data(msg, &e, sizeof(e));
            levels.push_back(e);
        }

        s_levels.swap(levels);
        return 1;
    }

    struct Levels {
        uint32_t level;
        const Entry* next;
        const Entry* current;
        const Entry* upcoming;
    };

    Levels Resolve() {   // FUN_10115df0
        Levels r{ s_deps.cacheLevel ? s_deps.cacheLevel() : 0, nullptr, nullptr, nullptr };

        for (const Entry& e : s_levels) {
            if (e.event != 0 && !CoA::GameEvents::IsActive(static_cast<uint16_t>(e.event))) {
                continue;
            }

            if (r.current) {
                r.next = &e;
                break;
            }

            if (r.level < e.threshold) {
                r.upcoming = &e;
            } else {
                r.current = &e;
            }
        }

        if (r.upcoming && (!r.current || r.upcoming->event != r.current->event)) {
            r.upcoming = nullptr;
        }

        if (r.next && r.next->event != r.current->event) {
            r.next = nullptr;
        }

        return r;
    }

    void PushOr(lua_State* L, const Entry* e, uint32_t Entry::*field) {
        if (e) {
            PushInt(L, static_cast<int32_t>(e->*field));
        } else {
            lua_pushnil(L);
        }
    }

    int GetCallboardCacheInfo(lua_State* L) {   // FUN_10116270
        const Levels r = Resolve();
        PushOr(L, r.current, &Entry::info);
        PushOr(L, r.upcoming, &Entry::info);
        return 2;
    }

    // FUN_101162f0: TOKEN_TYPE_CALLBOARD_CACHE_POINTS (index 0x3C, FUN_1033c290's low dword), then the current entry's
    // points.
    int GetCurrentPoints(lua_State* L) {
        const int32_t points = CoA::Token::Amount(0x3C);
        const Levels r = Resolve();
        PushInt(L, points);
        PushOr(L, r.current, &Entry::points);
        return 2;
    }

    int GetItemLevelInfo(lua_State* L) {   // FUN_10116370
        const Levels r = Resolve();
        PushInt(L, static_cast<int32_t>(r.level));
        PushOr(L, r.current, &Entry::threshold);
        PushOr(L, r.upcoming, &Entry::threshold);
        return 3;
    }

    // FUN_10116400: (questID) -> the quest addon record's points, or nil while it is being fetched.
    int GetPointsForQuest(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER })) {
            return 0;
        }

        const uint32_t quest = static_cast<uint32_t>(static_cast<int64_t>(CoA::Script::CheckNumber(L, 1)));
        uint32_t points = 0;

        if (!s_deps.questAddonPoints || !s_deps.questAddonPoints(quest, points)) {
            lua_pushnil(L);
            return 1;
        }

        lua_pushinteger(L, static_cast<int32_t>(points));
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_CallboardCache", "GetCallboardCacheInfo", GetCallboardCacheInfo),
        COA_NATIVE("C_CallboardCache", "GetCurrentPoints", GetCurrentPoints),
        COA_NATIVE("C_CallboardCache", "GetItemLevelInfo", GetItemLevelInfo),
        COA_NATIVE("C_CallboardCache", "GetPointsForQuest", GetPointsForQuest),
    };
}

CoA::CallboardCache::Deps& CoA::CallboardCache::Dependencies() {
    return s_deps;
}

void CoA::CallboardCache::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_PACKET(0x730, OnCacheLevels);
}

void CoA::CallboardCache::Reset() {
    s_levels.clear();
    s_deps = Deps();
}
