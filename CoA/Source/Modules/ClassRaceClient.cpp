// The DLL's lookups over the client's ChrClasses / ChrRaces tables: GetClassInfo (AscCAMgr.cpp, handler_GetClassInfo
// 0x100D8FA0), GetClassInfoByName / GetRaceInfo (AscGlobalsE.cpp), GetRaceInfoByID (AscGlobalsC.cpp).
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include <string>

namespace {
    // First row (min..max) whose name equals `name` exactly.
    template <class T, class Field>
    const T* FindByName(const WowClientDB<T>& db, Field field, const std::string& name) {
        for (int32_t id = db.m_minID; id <= db.m_maxID; ++id) {
            if (const T* row = db.GetRecord(id)) {
                const char* value = row->*field;

                if (value && name == value) {
                    return row;
                }
            }
        }

        return nullptr;
    }

    // ChrClasses: name (+0x10), file name (+0x1C), id; three nils for an unknown class.
    int GetClassInfo(lua_State* L) {
        uint32_t cls;

        if (!CoA::Script::ReadNumber(L, cls)) {
            return 0;
        }

        const ChrClassesRec* row = g_chrClassesDB.GetRecord(static_cast<int32_t>(cls));

        if (!row) {
            return CoA::Script::PushNils(L, 3);
        }

        lua_pushstring(L, row->m_name ? row->m_name : "");
        lua_pushstring(L, row->m_filename ? row->m_filename : "");
        CoA::Script::PushInt(L, row->m_id);
        return 3;
    }

    // ChrClasses by name (+0x10): name, file name (+0x1C), id.
    int GetClassInfoByName(lua_State* L) {
        std::string name;

        if (!CoA::Script::ReadString(L, name)) {
            return 0;
        }

        const ChrClassesRec* row = FindByName(g_chrClassesDB, &ChrClassesRec::m_name, name);

        if (!row) {
            return CoA::Script::PushNils(L, 3);
        }

        CoA::Script::PushStr(L, row->m_name);
        CoA::Script::PushStr(L, row->m_filename);
        lua_pushinteger(L, row->m_id);
        return 3;
    }

    // ChrRaces by client file string (+0x2C): name (+0x38), the file string, id.
    int GetRaceInfo(lua_State* L) {
        std::string name;

        if (!CoA::Script::ReadString(L, name)) {
            return 0;
        }

        const ChrRacesRec* row = FindByName(g_chrRacesDB, &ChrRacesRec::m_clientFileString, name);

        if (!row) {
            return CoA::Script::PushNils(L, 3);
        }

        CoA::Script::PushStr(L, row->m_name);
        CoA::Script::PushStr(L, row->m_clientFileString);
        lua_pushinteger(L, row->m_id);
        return 3;
    }

    // ChrRaces: name (+0x38), client file string (+0x2C), id.
    int GetRaceInfoByID(lua_State* L) {
        uint32_t id;

        if (!CoA::Script::ReadNumber(L, id)) {
            return 0;
        }

        const ChrRacesRec* row = g_chrRacesDB.GetRecord(static_cast<int32_t>(id));

        if (!row) {
            return CoA::Script::PushNils(L, 3);
        }

        CoA::Script::PushStr(L, row->m_name);
        CoA::Script::PushStr(L, row->m_clientFileString);
        lua_pushinteger(L, row->m_id);
        return 3;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetClassInfo", GetClassInfo),
        COA_NATIVE("", "GetClassInfoByName", GetClassInfoByName),
        COA_NATIVE("", "GetRaceInfo", GetRaceInfo),
        COA_NATIVE("", "GetRaceInfoByID", GetRaceInfoByID),
    };
}

void CoA::ClassRaceClient::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
