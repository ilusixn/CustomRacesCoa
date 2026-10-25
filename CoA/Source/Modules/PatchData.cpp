// SMSG_PATCH_* handlers outside the generated table (port of AscPatchData.cpp). Ported so far: GlobalStrings.dbc --
// applied to each new Lua state after its natives (FUN_1025afb0 glue, FUN_1025af40 world) and patched by SMSG 0x676.
// The other registrations here (world tables whose callbacks feed unported modules) come with those modules.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/DbcPatch.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "Engine/Source/lua-5.1/src/lua.h"

namespace {
    CoA::Dbc::Table& GlobalStrings() {
        return CoA::Dbc::Get("DBFilesClient\\GlobalStrings.dbc");
    }

    // The client's current Lua state (FrameScript_GetContext): _G[key (+8)] = value (+0xC).
    void SetGlobal(lua_State* L, const uint8_t* row) {
        CoA::Dbc::Table& table = GlobalStrings();
        const char* value = table.Str(row, 0xC);
        lua_pushstring(L, value ? value : "");
        lua_setfield(L, LUA_GLOBALSINDEX, table.Str(row, 8));
    }

    // FUN_1025ad00's tail (SMSG 0x676): the row's flags (+4) pick the Lua states it applies to -- bit 1 in world,
    // bit 0 at glue -- and there the global is set in the current Lua state.
    void ApplyGlobalString(const uint8_t* row) {
        uint32_t flags = CoA::Dbc::Table::U32(row, 4);
        bool inWorld = CoA::ClientState::InWorld();

        if (!(inWorld ? (flags >> 1) & 1 : flags & 1)) {
            return;
        }

        if (lua_State* L = CoA::ClientState::MainLuaState()) {
            SetGlobal(L, row);
        }
    }

    // FUN_1025af40 (after the world natives) / FUN_1025afb0 (after the glue natives): every row whose flag (+4 bit 1 /
    // bit 0) names that Lua state.
    void ApplyGlobalStrings(uint32_t bit) {
        lua_State* L = CoA::ClientState::MainLuaState();

        if (!L) {
            return;
        }

        CoA::Dbc::Table& table = GlobalStrings();
        uint32_t count = table.Count();
        uint32_t applied = 0;

        for (uint32_t i = 0; i < count; ++i) {
            const uint8_t* row = table.RowAt(i);

            if (!row || !((CoA::Dbc::Table::U32(row, 4) >> bit) & 1)) {
                continue;
            }

            SetGlobal(L, row);
            ++applied;
        }

        CoA::Log::Printf("GlobalStrings.dbc: %u of %u rows applied to the %s state", applied, count, bit ? "world" : "glue");
    }

    void ApplyWorldGlobalStrings() {
        ApplyGlobalStrings(1);
    }

    void ApplyGlueGlobalStrings() {
        ApplyGlobalStrings(0);
    }
}

void CoA::PatchData::Init() {
    CoA::LuaReg::OnWorldRegistered(ApplyWorldGlobalStrings);
    CoA::LuaReg::OnGlueRegistered(ApplyGlueGlobalStrings);
    // 0x676 GLOBAL_STRINGS (FUN_1025ad00): {id, flags, key, value} with the two strings after the image.
    CoA::DbcPatch::Register(0x676, "DBFilesClient\\GlobalStrings.dbc", 0x10, (1ull << 2) | (1ull << 3), ApplyGlobalString);
}
