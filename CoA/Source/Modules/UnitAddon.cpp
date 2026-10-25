// AscUnitAddon.cpp (see UnitAddon.h).
#include "CoA/Source/Modules/UnitAddon.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/ObjectAddon.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <vector>

namespace {
    CoA::UnitAddon::Deps s_deps;

    bool TokenGuid(const char* token, uint64_t& guid) {
        guid = 0;
        return s_deps.tokenGuid && s_deps.tokenGuid(token, guid);
    }

    uint64_t ActivePlayerGuid() {
        return s_deps.activePlayerGuid ? s_deps.activePlayerGuid() : 0;
    }

    std::vector<CoA::ObjectAddon::Field>& UnitFields(const char* token) {
        uint64_t guid = 0;
        TokenGuid(token, guid);
        return CoA::ObjectAddon::Unit(guid);
    }

    // FUN_100d9a30
    int GetGloryPoints(lua_State* L) {
        std::string token;
        double v = 0.0;

        if (CoA::Script::ReadString(L, token)) {
            v = static_cast<int32_t>(UnitFields(token.c_str())[35].value);
        }

        CoA::Script::PushNum(L, v);
        return 1;
    }

    // FUN_100dde70
    int UnitAverageItemLevel(lua_State* L) {
        std::string token;
        double v = 0.0;

        if (CoA::Script::ReadString(L, token)) {
            v = CoA::ObjectAddon::AsFloat(UnitFields(token.c_str())[5]);
        }

        CoA::Script::PushNum(L, v);
        return 1;
    }

    // FUN_10321c30 / d30 / e60: lua_isstring gate, then nothing at all for an unknown token.
    bool AltPowerUnit(lua_State* L, uint64_t& guid) {
        if (!lua_isstring(L, 1)) {
            return false;
        }

        const std::string token = CoA::Script::CheckString(L, 1);
        return TokenGuid(token.c_str(), guid);
    }

    int UnitAlternativePower(lua_State* L) {
        uint64_t guid;

        if (!AltPowerUnit(L, guid)) {
            return 0;
        }

        CoA::Script::PushInt(L, static_cast<int32_t>(CoA::ObjectAddon::Unit(guid)[1].value));
        return 1;
    }

    int UnitAlternativePowerMax(lua_State* L) {
        uint64_t guid;

        if (!AltPowerUnit(L, guid)) {
            return 0;
        }

        const uint32_t type = CoA::ObjectAddon::Unit(guid)[0].value;
        int32_t max = 0;

        if (type != 0) {
            if (const uint8_t* row = CoA::Dbc::Get("DBFilesClient\\SpellAlternativePowerType.dbc").Row(type)) {
                max = CoA::Dbc::Table::I32(row, 8);
            }
        }

        CoA::Script::PushInt(L, max);
        return 1;
    }

    int UnitAlternativePowerType(lua_State* L) {
        uint64_t guid;

        if (!AltPowerUnit(L, guid)) {
            return 0;
        }

        const int32_t type = static_cast<int32_t>(CoA::ObjectAddon::Unit(guid)[0].value);

        if (type == 0) {
            lua_pushnil(L);
        } else {
            CoA::Script::PushInt(L, type);
        }

        return 1;
    }

    // handler_GetPrestigeLevel: the active player's fields 2 and 3.
    int GetPrestigeLevel(lua_State* L) {
        std::vector<CoA::ObjectAddon::Field>& f = CoA::ObjectAddon::Unit(ActivePlayerGuid());
        CoA::Script::PushNum(L, static_cast<int32_t>(f[2].value));
        CoA::Script::PushNum(L, static_cast<int32_t>(CoA::ObjectAddon::Unit(ActivePlayerGuid())[3].value));
        return 2;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetGloryPoints", GetGloryPoints),
        COA_NATIVE("", "UnitAverageItemLevel", UnitAverageItemLevel),
        COA_NATIVE("", "UnitAlternativePower", UnitAlternativePower),
        COA_NATIVE("", "UnitAlternativePowerMax", UnitAlternativePowerMax),
        COA_NATIVE("", "UnitAlternativePowerType", UnitAlternativePowerType),
        COA_NATIVE("", "GetPrestigeLevel", GetPrestigeLevel),
    };
}

CoA::UnitAddon::Deps& CoA::UnitAddon::Dependencies() {
    return s_deps;
}

void CoA::UnitAddon::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
