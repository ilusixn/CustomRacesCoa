// Small GLOBAL bindings over the client's caches, DBC containers and descriptors, plus the packets that feed them;
// port of AscGlobalsB.cpp. Ported so far: the glue-state natives.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include "CoA/Source/Runtime/Script.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <string>

namespace {
    // SMSG 0x76F CHARACTER_SELECTION_SORT_ORDER (FUN_100d3a00): a C string.
    std::string s_sortOrder;      // DAT_10bc94ac
    bool s_sortOrderSet = false;  // DAT_10bdb5fe

    int32_t OnSortOrder(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        s_sortOrder = CoA::Read::Str(msg);
        s_sortOrderSet = true;
        return 1;
    }

    int GetCharacterSelectionSortOrder(lua_State* L) {
        if (!s_sortOrderSet) {
            lua_pushnil(L);
        } else {
            CoA::Script::PushStr(L, s_sortOrder.c_str());
        }

        return 1;
    }

    // Realm-info object +0x40 then +0x18.
    int GetRealmMaintenance(lua_State* L) {
        const CoA::RealmInfo::Info& r = CoA::RealmInfo::Get();
        CoA::Script::PushBool(L, r.gates[0] != 0 && r.flag14 != 0);
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetRealmMaintenance", GetRealmMaintenance),
        COA_NATIVE("", "GetCharacterSelectionSortOrder", GetCharacterSelectionSortOrder),
    };
}

void CoA::GlobalsB::Init() {
    COA_PACKET(0x76F, OnSortOrder);
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
