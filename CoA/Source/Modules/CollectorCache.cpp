// AscCollectorCache.cpp: C_CollectorCache over the DLL's four CollectorCache DBCs (FUN_101942*..101948*), patched at
// runtime by SMSG_PATCH_COLLECTOR_CACHE_* 0x6D9..0x6DC (DbcPatch). GetCollectorCacheRarityRatesInfo is in Bindings.cpp.
//   CollectorCacheItems       +4 type, +8, +0xC, +0x10 float, +0x14..+0x60 20 values
//   CollectorCacheTypes       +4, +8, +0xC / +0x10 / +0x14 strings
//   CollectorCacheRarityTypes +4 flag, +8 string
//   CollectorCacheRarityRates +8 (1 = the row GetCollectorCacheItemInfo reports), +0xC float
// GetCollectorCacheItemInfo (FUN_10194310) and GetCollectorCacheItems (FUN_10194490) checked against the genuine DLL.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packet.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <vector>

namespace {
    using CoA::Dbc::Table;

    Table& Items() {
        return CoA::Dbc::Get("DBFilesClient\\CollectorCacheItems.dbc");
    }

    Table& Types() {
        return CoA::Dbc::Get("DBFilesClient\\CollectorCacheTypes.dbc");
    }

    Table& RarityTypes() {
        return CoA::Dbc::Get("DBFilesClient\\CollectorCacheRarityTypes.dbc");
    }

    Table& RarityRates() {
        return CoA::Dbc::Get("DBFilesClient\\CollectorCacheRarityRates.dbc");
    }

    // Four values, then the first RarityRates row with +8 == 1 -- and 5 returned either way, so without such a row
    // Lua sees the argument first and the four values after it (the original's stack).
    int GetCollectorCacheItemInfo(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        const uint8_t* row = Items().Row(id);

        if (!row) {
            return 0;
        }

        lua_pushinteger(L, Table::I32(row, 8));
        lua_pushinteger(L, Table::I32(row, 0xC));
        lua_pushnumber(L, static_cast<double>(Table::F32(row, 0x10)));
        lua_createtable(L, 0, 0x14);
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < 0x14; ++i) {
            lua_pushnumber(L, static_cast<double>(i + 1));
            lua_pushinteger(L, Table::I32(row, 0x14 + i * 4));
            lua_settable(L, -3);
        }

        Table& rates = RarityRates();
        const uint32_t max = rates.MaxId();

        for (uint32_t r = rates.MinId(); r <= max; ++r) {
            const uint8_t* rate = rates.Row(r);

            if (rate && Table::U32(rate, 8) == 1) {
                lua_pushnumber(L, static_cast<double>(Table::F32(rate, 0xC)));
                break;
            }

            if (r == 0xFFFFFFFF) {
                break;
            }
        }

        return 5;
    }

    // The ids of the items of a type, in id order.
    std::vector<uint32_t> ItemsOfType(uint32_t type) {
        std::vector<uint32_t> ids;
        Table& t = Items();
        const uint32_t max = t.MaxId();

        for (uint32_t id = t.MinId(); id <= max; ++id) {
            const uint8_t* row = t.Row(id);

            if (row && Table::U32(row, 4) == type) {
                ids.push_back(id);
            }

            if (id == 0xFFFFFFFF) {
                break;
            }
        }

        return ids;
    }

    int GetCollectorCacheItems(lua_State* L) {
        uint32_t type;

        if (!CoA::Script::ReadNumber(L, type)) {
            return 0;
        }

        const std::vector<uint32_t> ids = ItemsOfType(type);
        lua_createtable(L, 0, static_cast<int>(ids.size()));
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < ids.size(); ++i) {
            lua_pushnumber(L, static_cast<double>(i + 1));
            lua_pushinteger(L, static_cast<int32_t>(ids[i]));
            lua_settable(L, -3);
        }

        return 1;
    }

    int GetNumCollectorCacheItems(lua_State* L) {
        uint32_t type;

        if (!CoA::Script::ReadNumber(L, type)) {
            return 0;
        }

        lua_pushinteger(L, static_cast<int32_t>(ItemsOfType(type).size()));
        return 1;
    }

    int GetCollectorCacheRarityTypeInfo(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        const uint8_t* row = RarityTypes().Row(id);

        if (!row) {
            return 0;
        }

        CoA::Script::PushBool(L, Table::U32(row, 4) != 0);
        CoA::Script::PushStr(L, RarityTypes().Str(row, 8));
        return 2;
    }

    int GetCollectorCacheTypeInfo(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        const uint8_t* row = Types().Row(id);

        if (!row) {
            return 0;
        }

        lua_pushinteger(L, Table::I32(row, 4));
        lua_pushinteger(L, Table::I32(row, 8));
        CoA::Script::PushStr(L, Types().Str(row, 0xC));
        CoA::Script::PushStr(L, Types().Str(row, 0x10));
        CoA::Script::PushStr(L, Types().Str(row, 0x14));
        return 5;
    }

    // CMSG_OPEN_COLLECTOR_CACHE (0x6DF) {u32 id}.
    int OpenCollectorCache(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        CoA::Packet(0x6DF).U32(id).Send();
        CoA::Script::PushBool(L, true);
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_CollectorCache", "GetCollectorCacheItemInfo", GetCollectorCacheItemInfo),
        COA_NATIVE("C_CollectorCache", "GetCollectorCacheItems", GetCollectorCacheItems),
        COA_NATIVE("C_CollectorCache", "GetCollectorCacheRarityTypeInfo", GetCollectorCacheRarityTypeInfo),
        COA_NATIVE("C_CollectorCache", "GetCollectorCacheTypeInfo", GetCollectorCacheTypeInfo),
        COA_NATIVE("C_CollectorCache", "GetNumCollectorCacheItems", GetNumCollectorCacheItems),
        COA_NATIVE("C_CollectorCache", "OpenCollectorCache", OpenCollectorCache),
    };
}

void CoA::CollectorCache::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
