// AscWorldMap.cpp: C_WorldMap over the client's own WorldMapArea (id, map, area, file name, bounds) and AreaTable (map)
// tables. "AreaID" arguments are WorldMapArea ids + 1.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Ui/WorldMap.h"
#include <algorithm>

namespace {
    // id - 1, and the row's own id must match.
    const WorldMapAreaRec* MapArea(uint32_t areaId) {
        const WorldMapAreaRec* row = g_worldMapAreaDB.GetRecord(static_cast<int32_t>(areaId - 1));
        return (row && static_cast<uint32_t>(row->m_id) == areaId - 1) ? row : nullptr;
    }

    int GetMapFileByAreaID(lua_State* L) {
        uint32_t area;

        if (!CoA::Script::ReadNumber(L, area) || area == 0) {
            return 0;
        }

        if (const WorldMapAreaRec* row = MapArea(area)) {
            CoA::Script::PushStr(L, row->m_areaName);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    // The first WorldMapArea (in id order) whose area column is the zone.
    int GetMapFileByZoneID(lua_State* L) {
        uint32_t zone;

        if (!CoA::Script::ReadNumber(L, zone)) {
            return 0;
        }

        for (int32_t id = g_worldMapAreaDB.m_minID; id <= g_worldMapAreaDB.m_maxID; ++id) {
            if (const WorldMapAreaRec* row = g_worldMapAreaDB.GetRecord(id)) {
                if (static_cast<uint32_t>(row->m_areaID) == zone) {
                    CoA::Script::PushStr(L, row->m_areaName);
                    return 1;
                }
            }
        }

        lua_pushnil(L);
        return 1;
    }

    int GetMapIDByAreaID(lua_State* L) {
        uint32_t area;

        if (!CoA::Script::ReadNumber(L, area) || area == 0) {
            return 0;
        }

        const WorldMapAreaRec* row = g_worldMapAreaDB.GetRecord(static_cast<int32_t>(area - 1));

        if (!row) {
            return 0;
        }

        CoA::Script::PushInt(L, row->m_mapID);
        return 1;
    }

    int GetMapIDByZoneID(lua_State* L) {
        uint32_t zone;

        if (!CoA::Script::ReadNumber(L, zone)) {
            return 0;
        }

        const AreaTableRec* row = g_areaTableDB.GetRecord(static_cast<int32_t>(zone));

        if (!row) {
            return 0;
        }

        CoA::Script::PushInt(L, row->m_continentID);
        return 1;
    }

    int GetZoneIDByAreaID(lua_State* L) {
        uint32_t area;

        if (!CoA::Script::ReadNumber(L, area) || area == 0) {
            return 0;
        }

        if (const WorldMapAreaRec* row = MapArea(area)) {
            CoA::Script::PushInt(L, row->m_areaID);
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    // FUN_10a3a490: the client's world -> map projection (CGWorldMap::GetWorldPosition(pos, area, &x, &y, 0, 0, 0)).
    int GetMapPosition(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TNUMBER, LUA_TNUMBER, LUA_TNUMBER })) {
            return 0;
        }

        const int32_t area = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1));
        C3Vector pos;
        pos.x = static_cast<float>(CoA::Script::CheckNumber(L, 2));
        pos.y = static_cast<float>(CoA::Script::CheckNumber(L, 3));
        pos.z = static_cast<float>(CoA::Script::CheckNumber(L, 4));
        float x = 0.0f;
        float y = 0.0f;
        CGWorldMap::GetWorldPosition(pos, area, &x, &y, nullptr, false, 0);
        CoA::Script::PushNum(L, x);
        CoA::Script::PushNum(L, y);
        return 2;
    }

    // FUN_10a3a5f0: map (x, y) clamped to [0, 1] -> world, returned as (y-axis, x-axis).
    int GetWorldPosition(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TNUMBER, LUA_TNUMBER, LUA_TNUMBER })) {
            return 0;
        }

        const int32_t area = CoA::Script::ToInt(CoA::Script::CheckNumber(L, 1));
        const float mx = std::min(std::max(static_cast<float>(CoA::Script::CheckNumber(L, 2)), 0.0f), 1.0f);
        const float my = std::min(std::max(static_cast<float>(CoA::Script::CheckNumber(L, 3)), 0.0f), 1.0f);
        const WorldMapAreaRec* row = g_worldMapAreaDB.GetRecord(area - 1);

        if (!row) {
            lua_pushnil(L);
            return 1;
        }

        const float wx = (row->m_locRight - row->m_locLeft) * mx + row->m_locLeft;
        CoA::Script::PushNum(L, my * (row->m_locBottom - row->m_locTop) + row->m_locTop);
        CoA::Script::PushNum(L, wx);
        return 2;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_WorldMap", "GetMapFileByAreaID", GetMapFileByAreaID),
        COA_NATIVE("C_WorldMap", "GetMapFileByZoneID", GetMapFileByZoneID),
        COA_NATIVE("C_WorldMap", "GetMapIDByAreaID", GetMapIDByAreaID),
        COA_NATIVE("C_WorldMap", "GetMapIDByZoneID", GetMapIDByZoneID),
        COA_NATIVE("C_WorldMap", "GetMapPosition", GetMapPosition),
        COA_NATIVE("C_WorldMap", "GetWorldPosition", GetWorldPosition),
        COA_NATIVE("C_WorldMap", "GetZoneIDByAreaID", GetZoneIDByAreaID),
    };
}

void CoA::WorldMapClient::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
