// C_ClassInfo.GetAllSpecs (FUN_100d8800) / GetSpecInfo (FUN_100dac60) over the DLL's ChrSpecs.dbc (port of
// AscSpecs.cpp).
//
// Row (layout d x29, L, L, d, d): +0 id, +4 class, +8 spec, +0xC SpecFilename, +0x10..+0x1C Cloth/Leather/Mail/Plate,
// +0x20..+0x28 primary stats, +0x2C..+0x40 MeleeDPS/RangedDPS/CasterDPS/Tank/Healer/Support, +0x44 DifficultyRating,
// +0x48..+0x50 primary resources, +0x54..+0x5C secondary resources, +0x60..+0x68 example spells, +0x6C PassiveSpell,
// +0x70 PassiveID, +0x74 Name, +0x78 Description, +0x7C SortOrder, +0x80 LootSpecID. Class/spec match is exact.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>
#include <vector>

namespace {
    CoA::Dbc::Table& Specs() {
        return CoA::Dbc::Get("DBFilesClient\\ChrSpecs.dbc");
    }

    int GetAllSpecs(lua_State* L) {
        std::string cls;

        if (!CoA::Script::ReadString(L, cls)) {
            return 0;
        }

        std::vector<const char*> specs;
        CoA::Dbc::Table& table = Specs();

        for (uint32_t id = table.MinId(); table.Loaded() && id <= table.MaxId(); ++id) {
            if (const uint8_t* row = table.Row(id)) {
                if (cls == table.Str(row, 4)) {
                    specs.push_back(table.Str(row, 8));
                }
            }
        }

        lua_createtable(L, 0, static_cast<int>(specs.size()));
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < specs.size(); ++i) {
            CoA::Script::PushNum(L, i + 1);
            CoA::Script::PushStr(L, specs[i]);
            lua_settable(L, -3);
        }

        return 1;
    }

    void SetStr(lua_State* L, const char* key, const char* value) {
        lua_pushstring(L, key);
        CoA::Script::PushStr(L, value);
        lua_settable(L, -3);
    }

    void SetBool(lua_State* L, const char* key, bool value) {
        lua_pushstring(L, key);
        CoA::Script::PushBool(L, value);
        lua_settable(L, -3);
    }

    void SetNum(lua_State* L, const char* key, int32_t value) {
        lua_pushstring(L, key);
        CoA::Script::PushNum(L, value);
        lua_settable(L, -3);
    }

    // Three string columns as an array, with every entry equal to `none` removed (FUN_100d12d0).
    void SetStrList(lua_State* L, CoA::Dbc::Table& table, const uint8_t* row, const char* key, uint32_t off, const char* none) {
        std::vector<std::string> values;

        for (uint32_t i = 0; i < 3; ++i) {
            std::string s = table.Str(row, off + i * 4);

            if (s != none) {
                values.push_back(s);
            }
        }

        lua_pushstring(L, key);
        lua_createtable(L, 0, 0);
        lua_checkstack(L, 2);

        for (uint32_t i = 0; i < values.size(); ++i) {
            CoA::Script::PushNum(L, i + 1);
            CoA::Script::PushStr(L, values[i].c_str());
            lua_settable(L, -3);
        }

        lua_settable(L, -3);
    }

    int GetSpecInfo(lua_State* L) {
        if (!CoA::Script::ValidateInput(L, { LUA_TSTRING, LUA_TSTRING })) {
            return 0;
        }

        std::string cls = CoA::Script::CheckString(L, 1);
        std::string spec = CoA::Script::CheckString(L, 2);
        CoA::Dbc::Table& table = Specs();

        for (uint32_t id = table.MinId(); table.Loaded() && id <= table.MaxId(); ++id) {
            const uint8_t* row = table.Row(id);

            if (!row || cls != table.Str(row, 4) || spec != table.Str(row, 8)) {
                continue;
            }

            auto I = [&](uint32_t off) { return CoA::Dbc::Table::I32(row, off); };
            lua_createtable(L, 0, 0);
            lua_checkstack(L, 2);
            SetNum(L, "ID", I(0));
            SetStr(L, "Class", table.Str(row, 4));
            SetStr(L, "Spec", table.Str(row, 8));
            SetStr(L, "SpecFilename", table.Str(row, 0xC));
            SetBool(L, "Cloth", I(0x10) != 0);
            SetBool(L, "Leather", I(0x14) != 0);
            SetBool(L, "Mail", I(0x18) != 0);
            SetBool(L, "Plate", I(0x1C) != 0);
            SetStrList(L, table, row, "PrimaryStats", 0x20, "None");
            SetBool(L, "MeleeDPS", I(0x2C) != 0);
            SetBool(L, "RangedDPS", I(0x30) != 0);
            SetBool(L, "CasterDPS", I(0x34) != 0);
            SetBool(L, "Tank", I(0x38) != 0);
            SetBool(L, "Healer", I(0x3C) != 0);
            SetBool(L, "Support", I(0x40) != 0);
            SetStr(L, "DifficultyRating", table.Str(row, 0x44));
            SetStrList(L, table, row, "PrimaryResources", 0x48, "NONE");
            SetStrList(L, table, row, "SecondaryResources", 0x54, "NONE");

            // FUN_100c0270 + remove(0): the three example spell ids, zeros dropped.
            std::vector<int32_t> spells;

            for (uint32_t off = 0x60; off <= 0x68; off += 4) {
                if (I(off) != 0) {
                    spells.push_back(I(off));
                }
            }

            lua_pushstring(L, "ExampleSpells");
            lua_createtable(L, 0, static_cast<int>(spells.size()));
            lua_checkstack(L, 2);

            for (uint32_t i = 0; i < spells.size(); ++i) {
                CoA::Script::PushNum(L, i + 1);
                CoA::Script::PushInt(L, spells[i]);
                lua_settable(L, -3);
            }

            lua_settable(L, -3);
            SetStr(L, "Name", table.Str(row, 0x74));
            SetStr(L, "Description", table.Str(row, 0x78));
            SetNum(L, "SortOrder", I(0x7C));
            SetNum(L, "PassiveSpell", I(0x6C));
            SetNum(L, "PassiveID", I(0x70));
            SetNum(L, "LootSpecID", I(0x80));
            return 1;
        }

        // The original pushes nil here and still returns 0.
        lua_pushnil(L);
        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("C_ClassInfo", "GetAllSpecs", GetAllSpecs),
        COA_NATIVE("C_ClassInfo", "GetSpecInfo", GetSpecInfo),
    };
}

void CoA::Specs::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
