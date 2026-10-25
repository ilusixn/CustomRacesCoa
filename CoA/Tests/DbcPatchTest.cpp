#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/ClientState.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/DbcPatch.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>

namespace {
    // GlobalStrings.dbc: {id, flags, key, value} rows; bit 0 = glue, bit 1 = world.
    bool ReadGlobalStrings(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "GlobalStrings.dbc") == nullptr) {
            return false;
        }

        const char strings[] = "\0GLUE_ONLY\0glue text\0WORLD_ONLY\0world text";
        uint32_t header[] = { 0x43424457, 2, 4, 16, sizeof(strings) };
        uint32_t rows[] = { 1, 1, 1, 11, 2, 2, 21, 32 };
        out.assign(reinterpret_cast<const uint8_t*>(header), reinterpret_cast<const uint8_t*>(header) + sizeof(header));
        out.insert(out.end(), reinterpret_cast<const uint8_t*>(rows), reinterpret_cast<const uint8_t*>(rows) + sizeof(rows));
        out.insert(out.end(), strings, strings + sizeof(strings));
        return true;
    }

    lua_State* s_state = nullptr;

    std::string Global(lua_State* L, const char* name) {
        lua_getglobal(L, name);
        std::string value = lua_isstring(L, -1) ? lua_tostring(L, -1) : "nil";
        lua_pop(L, 1);
        return value;
    }
}

COA_TEST(DbcPatch_RowImageThenStrings) {
    CoA::Packets::Clear();
    CoA::DbcPatch::Register(0x7F0, "DBFilesClient\\CoATestPatch.dbc", 0x0C, 1ull << 1, nullptr);
    CoATest::Bytes b;
    b.U32(42).U32(0xDEADBEEF).U32(7).Str("patched");
    CDataStore msg(b.data.data(), static_cast<uint32_t>(b.data.size()));
    int32_t result = 0;
    CHECK(CoA::Packets::Dispatch(0x7F0, 0, &msg, result));

    CoA::Dbc::Table& table = CoA::Dbc::Get("DBFilesClient\\CoATestPatch.dbc");
    const uint8_t* row = table.Row(42);
    CHECK(row != nullptr);
    CHECK(row && strcmp(table.Str(row, 4), "patched") == 0);
    CHECK(row && CoA::Dbc::Table::U32(row, 8) == 7);
}

COA_TEST(DbcPatch_ShortImageIsIgnored) {
    CoA::Packets::Clear();
    CoA::DbcPatch::Register(0x7F1, "DBFilesClient\\CoATestShort.dbc", 0x0C, 0, nullptr);
    CoATest::Bytes b;
    b.U32(9).U32(1);
    CDataStore msg(b.data.data(), static_cast<uint32_t>(b.data.size()));
    int32_t result = 0;
    CoA::Packets::Dispatch(0x7F1, 0, &msg, result);
    CHECK(CoA::Dbc::Get("DBFilesClient\\CoATestShort.dbc").Row(9) == nullptr);
}

COA_TEST(GlobalStrings_AppliedPerState) {
    CoA::Dbc::SetFileReader(ReadGlobalStrings);
    CoA::LuaReg::Reset();
    CoA::PatchData::Init();
    CoA::ClientState::SetMainLuaStateProbe([] { return s_state; });

    s_state = CoATest::NewState(true);
    CoA::LuaReg::RunGlueRegistered();
    CHECK(Global(s_state, "GLUE_ONLY") == "glue text");
    CHECK(Global(s_state, "WORLD_ONLY") == "nil");
    lua_close(s_state);

    s_state = CoATest::NewState(false);
    CoA::LuaReg::RunWorldRegistered();
    CHECK(Global(s_state, "GLUE_ONLY") == "nil");
    CHECK(Global(s_state, "WORLD_ONLY") == "world text");
    lua_close(s_state);

    s_state = nullptr;
    CoA::ClientState::SetMainLuaStateProbe(nullptr);
    CoA::Dbc::SetFileReader(nullptr);
}
