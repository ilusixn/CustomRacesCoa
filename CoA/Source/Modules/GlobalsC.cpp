// GLOBAL bindings, batch C (port of AscGlobalsC.cpp). Ported so far: the glue-state natives.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"

namespace {
    // The ScreenLocations.dbc row's path (+4), or nil.
    int GetScreenLocationInfo(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        CoA::Dbc::Table& table = CoA::Dbc::Get("DBFilesClient\\ScreenLocations.dbc");

        if (const uint8_t* row = table.Row(id)) {
            CoA::Script::PushStr(L, table.Str(row, 4));
        } else {
            lua_pushnil(L);
        }

        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetScreenLocationInfo", GetScreenLocationInfo),
    };
}

void CoA::GlobalsC::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
