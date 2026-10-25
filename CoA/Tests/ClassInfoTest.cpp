#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstring>

namespace {
    // ChrSpecs.dbc, layout 29 d + L + L + d + d. Strings: 1 "MAGE", 6 "Fire", 11 "Frost", 17 "MageFire", 26 "None",
    // 31 "STR", 35 "NONE", 40 "MANA", 45 "Fire Mage", 55 "Burn things".
    std::vector<uint32_t> SpecRow(uint32_t id, uint32_t spec) {
        std::vector<uint32_t> row = { id, 1, spec, 17, 1, 0, 0, 0, 31, 26, 26, 0, 0, 1, 0, 0, 0, 1, 35, 40, 35, 35, 35, 35, 133, 0, 134, 0, 0 };
        std::vector<uint32_t> name = CoATest::Loc(45);
        std::vector<uint32_t> description = CoATest::Loc(55);
        row.insert(row.end(), name.begin(), name.end());
        row.insert(row.end(), description.begin(), description.end());
        row.push_back(id * 10);
        row.push_back(0);
        return row;
    }

    bool ReadTables(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "ChrSpecs.dbc")) {
            std::string strings = std::string("MAGE") + '\0' + "Fire" + '\0' + "Frost" + '\0' + "MageFire" + '\0' + "None" + '\0' +
                "STR" + '\0' + "NONE" + '\0' + "MANA" + '\0' + "Fire Mage" + '\0' + "Burn things";
            out = CoATest::Wdbc({ SpecRow(2, 6), SpecRow(4, 11) }, strings);
            return true;
        }

        if (strstr(path, "CoATestLua.dbc")) {
            out = CoATest::Wdbc({ { 5, 77, 1 }, { 9, 78, 6 } }, std::string("five") + '\0' + "nine");
            return true;
        }

        if (strstr(path, "AppearanceCategories.dbc")) {
            // Layout "ddLddddddd" (104-byte file rows) but 40-byte rows: dword 3 of row 7 is 111; reading the layout
            // would take that field from the next row's dword 9 (222).
            out = CoATest::Wdbc({ { 7, 0, 0, 111, 0, 0, 0, 0, 0, 0 }, { 8, 0, 0, 0, 0, 0, 0, 0, 0, 222 } }, "x");
            return true;
        }

        return false;
    }

    std::vector<std::string> Run(lua_State* L, const char* chunk) {
        std::vector<std::string> out;
        int base = lua_gettop(L);

        if (luaL_loadstring(L, chunk) != 0 || lua_pcall(L, 0, LUA_MULTRET, 0) != 0) {
            out.push_back(std::string("<error> ") + lua_tostring(L, -1));
            lua_settop(L, base);
            return out;
        }

        for (int i = base + 1; i <= lua_gettop(L); ++i) {
            out.push_back(lua_isnil(L, i) ? "nil" : lua_isboolean(L, i) ? (lua_toboolean(L, i) ? "true" : "false") : lua_tostring(L, i));
        }

        lua_settop(L, base);
        return out;
    }

    lua_State* Setup() {
        CoA::Dbc::SetFileReader(ReadTables);
        CoA::LuaReg::Reset();
        CoA::Specs::Init();
        CoA::LuaDbc::Init();
        return CoATest::NewState(true);
    }
}

COA_TEST(Dbc_LayoutWidthMismatchKeepsRawRows) {
    CoA::Dbc::SetFileReader(ReadTables);
    CoA::Dbc::Table table("DBFilesClient/AppearanceCategories.dbc");
    const uint8_t* row = table.Row(7);
    CHECK(row && CoA::Dbc::Table::U32(row, 0xC) == 111);
    CoA::Dbc::SetFileReader(nullptr);
}

COA_TEST(Specs_AllSpecsForClassInIdOrder) {
    lua_State* L = Setup();
    CHECK((Run(L, "local t = C_ClassInfo.GetAllSpecs('MAGE') return #t, t[1], t[2]") == std::vector<std::string>{ "2", "Fire", "Frost" }));
    CHECK((Run(L, "return #C_ClassInfo.GetAllSpecs('ROGUE')") == std::vector<std::string>{ "0" }));
    lua_close(L);
    CoA::Dbc::SetFileReader(nullptr);
}

COA_TEST(Specs_SpecInfoFields) {
    lua_State* L = Setup();
    CHECK((Run(L, "local s = C_ClassInfo.GetSpecInfo('MAGE', 'Fire') return s.ID, s.SpecFilename, s.Cloth, s.Leather, "
        "#s.PrimaryStats, s.PrimaryStats[1], #s.PrimaryResources, s.PrimaryResources[1], s.CasterDPS, s.Name, s.Description, "
        "#s.ExampleSpells, s.ExampleSpells[1], s.SortOrder")
        == std::vector<std::string>{ "2", "MageFire", "true", "false", "1", "STR", "1", "MANA", "true", "Fire Mage", "Burn things", "2", "133", "20" }));
    // Unknown spec: the original pushes nil but returns 0 values.
    CHECK(CoATest::Call(L, "C_ClassInfo", "GetSpecInfo", { "s:MAGE", "s:Arcane" }).empty());
    lua_close(L);
    CoA::Dbc::SetFileReader(nullptr);
}

COA_TEST(LuaDbc_CreateLoadQuery) {
    lua_State* L = Setup();
    CHECK((Run(L, "local d = CreateDBC('CoATestLua.dbc') d:SetUInt32('Value') d:SetString('Label') d:Load() "
        "local r = d:GetRecord(9) local c = d:GetRecordByColumn('Label', 'five') return r.ID, r.Value, r.Label, c.ID, d:GetRecord(6)")
        == std::vector<std::string>{ "9", "78", "nine", "5", "nil" }));
    CHECK((Run(L, "local d = CreateDBC('CoATestLua.dbc') d:SetUInt32('Value') local ok, err = pcall(d.Load, d) return ok, err")
        == std::vector<std::string>{ "false", "DBFilesClient\\CoATestLua.dbc has wrong number of columns (found 3, expected 2)" }));
    CHECK((Run(L, "local d = CreateDBC('Missing.dbc') local ok, err = pcall(d.Load, d) return ok, err")
        == std::vector<std::string>{ "false", "Unable to open DBFilesClient\\Missing.dbc" }));
    lua_close(L);
    CoA::Dbc::SetFileReader(nullptr);
}
