// LuaDBC (port of AscLuaDbc.cpp) -- scripts describe a custom DBC's columns and read it from the MPQs:
//   local dbc = CreateDBC("Foo.dbc")          -- userdata, metatable "LuaDBC" from the registry
//   SetInt32(dbc, "A") SetUInt32 SetFloat SetString SetLocalizedString   -- column types i u f s l
//   Load(dbc)  GetRecord(dbc, id)  GetRecordByColumn(dbc, col, v)  GetRecordsByColumn(dbc, col, v)
// Only CreateDBC is global; the rest are methods of the "LuaDBC" metatable. Transcribed from FUN_1030c720 (CreateDBC),
// FUN_1030e730/eb20/e830/ec20/ea20 (Set*), handler_Load, FUN_1030ca70 (GetRecord), FUN_1030cdd0 /
// handler_GetRecordsByColumn. Schemas live in a map keyed by name and are never freed; CreateDBC on a known name resets
// its columns.
//
// Kept from the original: a localized column stores 16 values but a record's values are indexed by column number, so a
// column after an "l" reads the wrong slots; duplicate record ids append their values to the first record. Record
// iteration is file order. The file is read through CoA::Dbc's file reader, value by value as the original reads it.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
    // DAT_10b5b670; the 16th slot is NULL and keys as "".
    const char* const s_locales[16] = { "enUS", "enGB", "koKR", "frFR", "deDE", "enCN", "zhCN", "enTW", "esES", "esMX", "ruRU",
        "ptPT", "ptBR", "itIT", "Unk", "" };

    struct Column {
        std::string name;
        std::string type;
    };

    struct Record {
        uint32_t id = 0;
        std::vector<uint32_t> values;
    };

    struct Schema {
        bool created = false;
        std::string path;
        std::vector<Column> columns;
        std::vector<Record> records;
        std::unordered_map<uint32_t, size_t> index;
        std::vector<char> strings;
        uint32_t columnCount = 0;   // values per record after the id
    };

    std::unordered_map<std::string, Schema*>& Schemas() {
        static std::unordered_map<std::string, Schema*> schemas;
        return schemas;
    }

    // The original dereferences without a check; a non-userdata self returns nothing here instead.
    Schema* Self(lua_State* L) {
        Schema** userdata = static_cast<Schema**>(lua_touserdata(L, 1));
        return userdata ? *userdata : nullptr;
    }

    uint32_t Value(const Record& r, size_t i) {
        return i < r.values.size() ? r.values[i] : 0;
    }

    const char* Str(const Schema* s, uint32_t off) {
        return off < s->strings.size() ? s->strings.data() + off : "";
    }

    int CreateDBC(lua_State* L) {
        std::string name = CoA::Script::CheckString(L, 1);
        Schema*& s = Schemas()[name];

        if (!s) {
            s = new Schema();
        }

        if (s->created) {
            s->created = false;
            s->columns.clear();
            s->columnCount = 0;
            s->strings.clear();
        }

        s->path = "DBFilesClient\\" + name;
        s->created = true;
        *static_cast<Schema**>(lua_newuserdata(L, sizeof(Schema*))) = s;
        lua_pushstring(L, "LuaDBC");
        lua_rawget(L, LUA_REGISTRYINDEX);

        if (lua_type(L, -1) == LUA_TTABLE) {
            lua_setmetatable(L, -2);
        } else {
            lua_settop(L, -3);
            lua_pushnil(L);
        }

        return 1;
    }

    int AddColumn(lua_State* L, const char* type, uint32_t width, bool rejectId) {
        Schema* s = Self(L);

        if (!s) {
            return 0;
        }

        std::string name = CoA::Script::CheckString(L, 2);

        if (rejectId && name == "ID") {
            std::string message = s->path + " has duplicate column name ID";
            luaL_error(L, message.c_str());
            return 0;
        }

        s->columns.push_back({ name, type });
        s->columnCount += width;
        return 0;
    }

    int SetInt32(lua_State* L) {
        return AddColumn(L, "i", 1, true);
    }

    int SetUInt32(lua_State* L) {
        return AddColumn(L, "u", 1, true);
    }

    int SetFloat(lua_State* L) {
        return AddColumn(L, "f", 1, false);
    }

    int SetString(lua_State* L) {
        return AddColumn(L, "s", 1, false);
    }

    int SetLocalizedString(lua_State* L) {
        return AddColumn(L, "l", 17, false);
    }

    int LoadError(lua_State* L, const char* format, const std::string& path) {
        char buffer[512];
        _snprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, path.c_str());
        luaL_error(L, buffer);
        return 0;
    }

    // handler_Load: WDBC header, then per record an id and each column's values (one u32, or 16 + a discarded flags
    // word for "l"), then the string table. Errors are raised with luaL_error.
    int Load(lua_State* L) {
        Schema* s = Self(L);

        if (!s) {
            return 0;
        }

        s->records.clear();
        s->index.clear();
        std::vector<uint8_t> file;

        if (!CoA::Dbc::ReadFile(s->path.c_str(), file)) {
            return LoadError(L, "Unable to open %s", s->path);
        }

        size_t at = 0;
        auto read = [&](void* p, uint32_t n) {
            if (file.size() - at < n) {
                return false;
            }

            memcpy(p, file.data() + at, n);
            at += n;
            return true;
        };

        uint32_t signature = 0;
        uint32_t count = 0;
        uint32_t columns = 0;
        uint32_t rowSize = 0;
        uint32_t stringSize = 0;

        if (!read(&signature, 4)) {
            return LoadError(L, "Unable to read signature from %s", s->path);
        }

        if (signature != 0x43424457) {
            return LoadError(L, "Invalid signature to open %s", s->path);
        }

        if (!read(&count, 4)) {
            return LoadError(L, "Unable to read record count from %s", s->path);
        }

        if (count == 0) {
            return 0;
        }

        if (!read(&columns, 4)) {
            return LoadError(L, "Unable to read column count from %s", s->path);
        }

        if (columns != s->columnCount + 1) {
            char buffer[512];
            _snprintf_s(buffer, sizeof(buffer), _TRUNCATE, "%s has wrong number of columns (found %i, expected %i)", s->path.c_str(),
                static_cast<int>(columns), static_cast<int>(s->columnCount + 1));
            luaL_error(L, buffer);
            return 0;
        }

        if (!read(&rowSize, 4)) {
            return LoadError(L, "Unable to read row size from %s", s->path);
        }

        if (!read(&stringSize, 4)) {
            return LoadError(L, "Unable to string size from %s", s->path);
        }

        for (uint32_t r = 0; r < count; ++r) {
            uint32_t id = 0;
            read(&id, 4);
            auto it = s->index.find(id);

            if (it == s->index.end()) {
                it = s->index.emplace(id, s->records.size()).first;
                s->records.push_back(Record { id, {} });
            }

            Record& record = s->records[it->second];

            for (const Column& column : s->columns) {
                char type = column.type.size() == 1 ? column.type[0] : 0;

                if (type == 'i' || type == 'u' || type == 'f' || type == 's') {
                    uint32_t v = 0;
                    read(&v, 4);
                    record.values.push_back(v);
                } else if (type == 'l') {
                    for (int j = 0; j < 16; ++j) {
                        uint32_t v = 0;
                        read(&v, 4);
                        record.values.push_back(v);
                    }

                    uint32_t flags = 0;
                    read(&flags, 4);
                }
            }
        }

        s->strings.assign(stringSize + 1, '\0');

        if (!read(s->strings.data(), stringSize)) {
            return LoadError(L, "%s: Cannot read string table", s->path);
        }

        return 0;
    }

    // { ID = id, [column] = value, ... } -- FUN_1030ca70's record table, shared by the three getters.
    void PushRecord(lua_State* L, const Schema* s, const Record& r) {
        lua_createtable(L, 0, 0);
        lua_pushstring(L, "ID");
        lua_pushinteger(L, static_cast<int>(r.id));
        lua_settable(L, -3);

        for (size_t i = 0; i < s->columns.size(); ++i) {
            const Column& column = s->columns[i];
            CoA::Script::PushStr(L, column.name.c_str());

            switch (column.type.size() == 1 ? column.type[0] : 0) {
                case 'i':
                case 'u':
                    lua_pushinteger(L, static_cast<int>(Value(r, i)));
                    break;
                case 'f': {
                    uint32_t bits = Value(r, i);
                    float f;
                    memcpy(&f, &bits, 4);
                    lua_pushnumber(L, f);
                    break;
                }
                case 's':
                    lua_pushstring(L, Str(s, Value(r, i)));
                    break;
                case 'l':
                    lua_createtable(L, 0, 0);

                    for (size_t j = 0; j < 16; ++j) {
                        lua_pushstring(L, s_locales[j]);
                        lua_pushstring(L, Str(s, Value(r, i + j)));
                        lua_settable(L, -3);
                    }

                    break;
                default:
                    lua_pushnil(L);
                    break;
            }

            lua_settable(L, -3);
        }
    }

    int GetRecord(lua_State* L) {
        Schema* s = Self(L);

        if (!s) {
            return 0;
        }

        uint32_t id = static_cast<uint32_t>(CoA::Script::ToInt(lua_tonumber(L, 2)));
        auto it = s->index.find(id);

        if (it == s->index.end()) {
            lua_pushnil(L);
            return 1;
        }

        PushRecord(L, s, s->records[it->second]);
        return 1;
    }

    // Matching records for (column, value): numbers compare by the column's type ("i" as int, "u" against the
    // truncated number, "f" as float) on the first column of that name; strings compare the string-table text on the
    // last column of that name.
    std::vector<const Record*> Matches(lua_State* L, const Schema* s, bool firstOnly) {
        std::vector<const Record*> out;
        std::string name = CoA::Script::CheckString(L, 2);

        if (lua_isnumber(L, 3)) {
            double number = lua_tonumber(L, 3);
            int32_t asUnsigned = CoA::Script::ToInt(number);
            size_t c = 0;

            while (c < s->columns.size() && s->columns[c].name != name) {
                ++c;
            }

            if (c == s->columns.size()) {
                return out;
            }

            const std::string& type = s->columns[c].type;

            for (const Record& r : s->records) {
                uint32_t v = Value(r, c);
                float f;
                memcpy(&f, &v, 4);
                bool hit = (type == "i" && static_cast<int32_t>(v) == static_cast<int32_t>(number)) ||
                    (type == "u" && static_cast<int32_t>(v) == asUnsigned) || (type == "f" && f == static_cast<float>(number));

                if (hit) {
                    out.push_back(&r);

                    if (firstOnly) {
                        break;
                    }
                }
            }
        } else if (lua_isstring(L, 3)) {
            std::string value = CoA::Script::CheckString(L, 3);
            size_t c = static_cast<size_t>(-1);

            for (size_t i = 0; i < s->columns.size(); ++i) {
                if (s->columns[i].name == name) {
                    c = i;
                }
            }

            if (c == static_cast<size_t>(-1)) {
                return out;
            }

            for (const Record& r : s->records) {
                if (value == Str(s, Value(r, c))) {
                    out.push_back(&r);

                    if (firstOnly) {
                        break;
                    }
                }
            }
        }

        return out;
    }

    int GetRecordByColumn(lua_State* L) {
        Schema* s = Self(L);

        if (!s) {
            return 0;
        }

        std::vector<const Record*> matches = Matches(L, s, true);

        if (matches.empty()) {
            lua_pushnil(L);
        } else {
            PushRecord(L, s, *matches[0]);
        }

        return 1;
    }

    int GetRecordsByColumn(lua_State* L) {
        Schema* s = Self(L);

        if (!s) {
            return 0;
        }

        std::vector<const Record*> matches = Matches(L, s, false);

        if (matches.empty()) {
            lua_pushnil(L);
            return 1;
        }

        for (const Record* r : matches) {
            PushRecord(L, s, *r);
        }

        return static_cast<int>(matches.size());
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "CreateDBC", CreateDBC),
        COA_NATIVE("@LuaDBC", "GetRecord", GetRecord),
        COA_NATIVE("@LuaDBC", "GetRecordByColumn", GetRecordByColumn),
        COA_NATIVE("@LuaDBC", "GetRecordsByColumn", GetRecordsByColumn),
        COA_NATIVE("@LuaDBC", "Load", Load),
        COA_NATIVE("@LuaDBC", "SetFloat", SetFloat),
        COA_NATIVE("@LuaDBC", "SetInt32", SetInt32),
        COA_NATIVE("@LuaDBC", "SetLocalizedString", SetLocalizedString),
        COA_NATIVE("@LuaDBC", "SetString", SetString),
        COA_NATIVE("@LuaDBC", "SetUInt32", SetUInt32),
    };
}

void CoA::LuaDbc::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
