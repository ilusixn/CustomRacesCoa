// AscLootLockout.cpp (see LootLockout.h): C_LootLockout, the per-unit loot lockouts and the instance binds.
#include "CoA/Source/Modules/LootLockout.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Lifecycle.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <ctime>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
    using CoA::Script::CheckNumber;
    using CoA::Script::CheckString;
    using CoA::Script::PushInt;
    using CoA::Script::PushStr;
    using CoA::Script::ToInt;

    struct Lock {
        uint32_t encounter;
        uint32_t expiry;
    };

    struct Bind {
        uint32_t unk;
        uint32_t map;
        uint32_t difficulty;
    };

    CoA::LootLockout::Deps s_deps;
    std::unordered_map<uint64_t, std::vector<Lock>> s_locks;   // +0x04
    std::vector<Bind> s_binds;                                 // +0x24

    int64_t Now() {
        return s_deps.now ? s_deps.now() : _time64(nullptr);
    }

    uint64_t TokenGuid(const char* token) {
        return s_deps.tokenGuid ? s_deps.tokenGuid(token) : 0;
    }

    bool EncounterRow(uint32_t id, CoA::LootLockout::Encounter& out) {
        out = CoA::LootLockout::Encounter();
        return s_deps.encounter && s_deps.encounter(id, out);
    }

    uint32_t EncounterMin() {
        return s_deps.encounterMinId ? s_deps.encounterMinId() : 0;
    }

    uint32_t EncounterMax() {
        return s_deps.encounterMaxId ? s_deps.encounterMaxId() : 0;
    }

    // FUN_10294820: the SpellIcon path, or the question mark.
    std::string IconPath(uint32_t icon) {
        const char* path = s_deps.spellIcon ? s_deps.spellIcon(icon) : nullptr;
        return path ? path : "Interface\\Icons\\inv_misc_questionmark";
    }

    // The arity gate every binding but the bind two opens with: exactly `count` arguments, a string first, numbers after.
    bool Args(lua_State* L, int count) {
        if (lua_gettop(L) != count || !lua_isstring(L, 1)) {
            return false;
        }

        for (int i = 2; i <= count; ++i) {
            if (!lua_isnumber(L, i)) {
                return false;
            }
        }

        return true;
    }

    void SetStr(lua_State* L, const char* key, const char* value) {
        lua_pushstring(L, key);
        PushStr(L, value);
        lua_settable(L, -3);
    }

    void SetInt(lua_State* L, const char* key, int32_t value) {
        lua_pushstring(L, key);
        PushInt(L, value);
        lua_settable(L, -3);
    }

    // FUN_10295220: name, map, difficulty + 1, icon, order, seconds remaining. An unknown encounter returns 6 having
    // pushed nothing, as the original does: the caller receives the top six stack slots (its own registers below the
    // call, the function and the two arguments).
    int GetEncounterData(lua_State* L) {
        if (!Args(L, 2)) {
            lua_pushnil(L);
            return 1;
        }

        const std::string token = CheckString(L, 1);
        const uint32_t encounter = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        const uint64_t guid = TokenGuid(token.c_str());
        CoA::LootLockout::Encounter row;

        if (EncounterRow(encounter, row)) {
            PushStr(L, row.name);
            PushInt(L, row.map);
            PushInt(L, row.difficulty + 1);
            PushStr(L, IconPath(row.icon).c_str());
            PushInt(L, row.order);
            PushInt(L, static_cast<int32_t>(CoA::LootLockout::SecondsRemaining(guid, encounter)));
        }

        return 6;
    }

    // FUN_102953c0: { [id] = {EncounterName, MapID, DifficultyID, SpellIconID, OrderIndex, TimeRemaining} } for the
    // ids 0 .. max - 1 (the container's max id itself is never visited) on the map at difficulty - 1.
    int GetEncounterDatasForMapAndDifficulty(lua_State* L) {
        if (!Args(L, 3)) {
            lua_pushnil(L);
            return 1;
        }

        const std::string token = CheckString(L, 1);
        const int32_t map = ToInt(CheckNumber(L, 2));
        const int32_t difficulty = ToInt(CheckNumber(L, 3));
        const uint32_t max = EncounterMax();
        const uint64_t guid = TokenGuid(token.c_str());
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < max; ++i) {
            CoA::LootLockout::Encounter row;

            if (!EncounterRow(i, row) || row.map != map || row.difficulty != difficulty - 1) {
                continue;
            }

            PushInt(L, static_cast<int32_t>(row.id));
            lua_createtable(L, 0, 0);
            lua_checkstack(L, 2);
            SetStr(L, "EncounterName", row.name);
            SetInt(L, "MapID", row.map);
            SetInt(L, "DifficultyID", row.difficulty + 1);
            SetStr(L, "SpellIconID", IconPath(row.icon).c_str());
            SetInt(L, "OrderIndex", row.order);
            SetInt(L, "TimeRemaining", static_cast<int32_t>(CoA::LootLockout::SecondsRemaining(guid, i)));
            lua_settable(L, -3);
        }

        return 1;
    }

    // FUN_10295ee0: whether any id min .. max on the map at difficulty - 1 is still locked for the unit.
    int HasEncounterDatasForMapAndDifficulty(lua_State* L) {
        if (!Args(L, 3)) {
            lua_pushnil(L);
            return 1;
        }

        const std::string token = CheckString(L, 1);
        const int32_t map = ToInt(CheckNumber(L, 2));
        const int32_t difficulty = ToInt(CheckNumber(L, 3));
        const uint64_t guid = TokenGuid(token.c_str());
        bool any = false;

        for (uint32_t i = EncounterMin(); i < EncounterMax() + 1; ++i) {
            CoA::LootLockout::Encounter row;

            if (EncounterRow(i, row) && row.map == map && row.difficulty == difficulty - 1 &&
                static_cast<int32_t>(CoA::LootLockout::SecondsRemaining(guid, i)) > 0) {
                any = true;
                break;
            }
        }

        CoA::Script::PushBool(L, any);
        return 1;
    }

    // FUN_102956c0 (GetLootLockoutTimeRemaining and GetUnitLootLockForEncounter).
    int GetLootLockoutTimeRemaining(lua_State* L) {
        if (!Args(L, 2)) {
            lua_pushnil(L);
            return 1;
        }

        const std::string token = CheckString(L, 1);
        const uint32_t encounter = static_cast<uint32_t>(ToInt(CheckNumber(L, 2)));
        PushInt(L, static_cast<int32_t>(CoA::LootLockout::SecondsRemaining(TokenGuid(token.c_str()), encounter)));
        return 1;
    }

    // FUN_102957d0: { [map] = { [difficulty] = { [encounter] = seconds left } } } over ordered maps of the raw
    // difficulty (no + 1) and a set of {encounter, expiry} pairs; expired locks still create their tables.
    int GetLootLockouts(lua_State* L) {
        if (!Args(L, 1)) {
            lua_pushnil(L);
            return 1;
        }

        const std::string token = CheckString(L, 1);
        std::map<uint32_t, std::map<uint32_t, std::set<std::pair<uint32_t, uint32_t>>>> tree;
        auto it = s_locks.find(TokenGuid(token.c_str()));

        if (it != s_locks.end()) {
            for (const Lock& lock : it->second) {
                CoA::LootLockout::Encounter row;

                if (EncounterRow(lock.encounter, row)) {
                    tree[static_cast<uint32_t>(row.map)][static_cast<uint32_t>(row.difficulty)].insert({ lock.encounter, lock.expiry });
                }
            }
        }

        const int64_t now = Now();
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (const auto& m : tree) {
            PushInt(L, static_cast<int32_t>(m.first));
            lua_createtable(L, 0, 0);
            lua_checkstack(L, 2);

            for (const auto& d : m.second) {
                PushInt(L, static_cast<int32_t>(d.first));
                lua_createtable(L, 0, 0);
                lua_checkstack(L, 2);

                for (const auto& e : d.second) {
                    if ((now >> 32) == 0 && static_cast<uint32_t>(now) < e.second) {
                        PushInt(L, static_cast<int32_t>(e.first));
                        PushInt(L, static_cast<int32_t>(e.second - static_cast<uint32_t>(now)));
                        lua_settable(L, -3);
                    }
                }

                lua_settable(L, -3);
            }

            lua_settable(L, -3);
        }

        return 1;
    }

    // FUN_10295d70: the id of the first DungeonEncounterExtra row (min .. max) whose +4 is the unit's entry.
    int GetUnitEncounterID(lua_State* L) {
        if (!Args(L, 1)) {
            lua_pushnil(L);
            return 1;
        }

        const std::string token = CheckString(L, 1);
        uint32_t entry = 0;

        if (s_deps.unitEntry && s_deps.unitEntry(token.c_str(), entry)) {
            CoA::Dbc::Table& t = CoA::Dbc::Get("DBFilesClient\\DungeonEncounterExtra.dbc");

            for (uint32_t id = t.MinId(); t.Loaded() && id < t.MaxId() + 1; ++id) {
                const uint8_t* row = t.Row(id);

                if (row && CoA::Dbc::Table::U32(row, 4) == entry) {
                    PushInt(L, CoA::Dbc::Table::I32(row, 0));
                    return 1;
                }
            }
        }

        lua_pushnil(L);
        return 1;
    }

    // FUN_10296060 (list FUN_10294760): the maps of the binds whose Map.dbc instance type is 2 exactly when raid, at
    // difficulty - 1. Difficulty 0 returns nothing.
    int ListInstanceBinds(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TBOOLEAN, LUA_TNUMBER })) {
            return 0;
        }

        const bool raid = lua_toboolean(L, 1) != 0;
        const int32_t difficulty = ToInt(CheckNumber(L, 2));

        if (difficulty == 0) {
            return 0;
        }

        std::vector<uint32_t> maps;

        for (const Bind& b : s_binds) {
            int32_t type = 0;

            if (s_deps.mapInstanceType && s_deps.mapInstanceType(b.map, type) && (type == 2) == raid &&
                static_cast<int32_t>(b.difficulty) == difficulty - 1) {
                maps.push_back(b.map);
            }
        }

        lua_createtable(L, 0, static_cast<int>(maps.size()));
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < maps.size(); ++i) {
            lua_pushnumber(L, static_cast<double>(i + 1));
            PushInt(L, static_cast<int32_t>(maps[i]));
            lua_settable(L, -3);
        }

        return 1;
    }

    // FUN_10296230: CMSG 0x6FD.
    int QueryInstanceBinds(lua_State* L) {
        CoA::Packet(0x6FD).Send();
        CoA::Script::PushBool(L, true);
        return 1;
    }

    // FUN_102962f0: CMSG 0x58C {u32 map, u8 difficulty - 1}; a difficulty whose low byte is 0 returns nothing.
    int ResetInstanceDifficulty(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TNUMBER })) {
            return 0;
        }

        const uint32_t map = static_cast<uint32_t>(ToInt(CheckNumber(L, 1)));
        const uint8_t difficulty = static_cast<uint8_t>(ToInt(CheckNumber(L, 2)));

        if (difficulty == 0) {
            return 0;
        }

        CoA::Packet(0x58C).U32(map).U8(static_cast<uint8_t>(difficulty - 1)).Send();
        CoA::Script::PushBool(L, true);
        return 1;
    }

    void OnGlueScreen() {   // FUN_10294740
        s_locks.clear();
        s_binds.clear();
    }

    // FUN_10296640: the C_LootLockout table, then the same ten as globals in this order.
    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_LootLockout", "GetLootLockouts", GetLootLockouts),
        COA_NATIVE("C_LootLockout", "GetLootLockoutTimeRemaining", GetLootLockoutTimeRemaining),
        COA_NATIVE("C_LootLockout", "GetEncounterData", GetEncounterData),
        COA_NATIVE("C_LootLockout", "GetEncounterDatasForMapAndDifficulty", GetEncounterDatasForMapAndDifficulty),
        COA_NATIVE("C_LootLockout", "GetUnitLootLockForEncounter", GetLootLockoutTimeRemaining),
        COA_NATIVE("C_LootLockout", "GetUnitEncounterID", GetUnitEncounterID),
        COA_NATIVE("C_LootLockout", "HasEncounterDatasForMapAndDifficulty", HasEncounterDatasForMapAndDifficulty),
        COA_NATIVE("C_LootLockout", "ResetInstanceDifficulty", ResetInstanceDifficulty),
        COA_NATIVE("C_LootLockout", "QueryInstanceBinds", QueryInstanceBinds),
        COA_NATIVE("C_LootLockout", "ListInstanceBinds", ListInstanceBinds),
        COA_NATIVE("", "GetLootLockouts", GetLootLockouts),
        COA_NATIVE("", "GetLootLockoutTimeRemaining", GetLootLockoutTimeRemaining),
        COA_NATIVE("", "GetEncounterData", GetEncounterData),
        COA_NATIVE("", "GetEncounterDatasForMapAndDifficulty", GetEncounterDatasForMapAndDifficulty),
        COA_NATIVE("", "GetUnitLootLockForEncounter", GetLootLockoutTimeRemaining),
        COA_NATIVE("", "GetUnitEncounterID", GetUnitEncounterID),
        COA_NATIVE("", "HasEncounterDatasForMapAndDifficulty", HasEncounterDatasForMapAndDifficulty),
        COA_NATIVE("", "ResetInstanceDifficulty", ResetInstanceDifficulty),
        COA_NATIVE("", "QueryInstanceBinds", QueryInstanceBinds),
        COA_NATIVE("", "ListInstanceBinds", ListInstanceBinds),
    };
}

CoA::LootLockout::Deps& CoA::LootLockout::Dependencies() {
    return s_deps;
}

uint32_t CoA::LootLockout::SecondsRemaining(uint64_t guid, uint32_t encounter) {
    auto it = s_locks.find(guid);

    if (it == s_locks.end()) {
        return 0;
    }

    for (const Lock& lock : it->second) {
        if (lock.encounter == encounter) {
            const int64_t now = Now();

            if ((now >> 32) != 0 || lock.expiry <= static_cast<uint32_t>(now)) {
                return 0;
            }

            return lock.expiry - static_cast<uint32_t>(now);
        }
    }

    return 0;
}

// The reads are unchecked in the original; a short packet reads zeros (CoA::Read) and the count loop stops when the
// packet is used up.
int32_t CoA::LootLockout::OnAddLockouts(void*, uint32_t, uint32_t, CDataStore* msg) {
    const uint64_t guid = CoA::Read::U64(msg);

    for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg) != 0; --n) {
        Lock lock;
        lock.encounter = CoA::Read::U32(msg);
        lock.expiry = CoA::Read::U32(msg);
        s_locks[guid].push_back(lock);
    }

    return 1;
}

int32_t CoA::LootLockout::OnAddLockout(void*, uint32_t, uint32_t, CDataStore* msg) {
    const uint64_t guid = CoA::Read::U64(msg);
    Lock lock;
    lock.encounter = CoA::Read::U32(msg);
    lock.expiry = CoA::Read::U32(msg);
    s_locks[guid].push_back(lock);
    return 1;
}

int32_t CoA::LootLockout::OnRemoveLockout(void*, uint32_t, uint32_t, CDataStore* msg) {
    const uint64_t guid = CoA::Read::U64(msg);
    const uint32_t encounter = CoA::Read::U32(msg);
    auto it = s_locks.find(guid);

    if (it == s_locks.end()) {
        return 1;
    }

    for (auto e = it->second.begin(); e != it->second.end(); ++e) {
        if (e->encounter == encounter) {
            it->second.erase(e);
            break;
        }
    }

    return 1;
}

int32_t CoA::LootLockout::OnInstanceBinds(void*, uint32_t, uint32_t, CDataStore* msg) {
    s_binds.clear();
    const std::string result = CoA::Read::Str(msg);
    const bool ok = result == "QUERY_INSTANCE_BINDS_OK";   // FUN_10294370 -> enum 0

    if (ok) {
        for (uint32_t n = CoA::Read::U32(msg); n && CoA::Read::Remaining(msg) != 0; --n) {
            Bind b;
            b.unk = CoA::Read::U32(msg);
            b.map = CoA::Read::U32(msg);
            b.difficulty = CoA::Read::U32(msg);
            s_binds.push_back(b);
        }
    }

    CoA::Events::Signal("QUERY_INSTANCE_BINDS_RESULT", "%b%s", static_cast<int>(ok), result.c_str());
    return 1;
}

void CoA::LootLockout::Init() {
    COA_PACKET(0x9B4, CoA::LootLockout::OnAddLockouts);
    COA_PACKET(0x9B5, CoA::LootLockout::OnAddLockout);
    COA_PACKET(0x9B6, CoA::LootLockout::OnRemoveLockout);
    COA_PACKET(0x6FE, CoA::LootLockout::OnInstanceBinds);
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
    COA_EVENT("QUERY_INSTANCE_BINDS_RESULT");
    CoA::Lifecycle::OnGlueScreen(OnGlueScreen);
}

void CoA::LootLockout::Reset() {
    s_deps = Deps();
    s_locks.clear();
    s_binds.clear();
}
