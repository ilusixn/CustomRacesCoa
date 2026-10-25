// Small GLOBAL bindings over the client's caches, DBC containers and descriptors, plus the packets that feed them;
// port of AscGlobalsB.cpp. Ported so far: the glue-state natives.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Modules/RealmInfo.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Script.h"

namespace {
    // Realm-info object +0x40 then +0x18.
    int GetRealmMaintenance(lua_State* L) {
        const CoA::RealmInfo::Info& r = CoA::RealmInfo::Get();
        CoA::Script::PushBool(L, r.gates[0] != 0 && r.flag14 != 0);
        return 1;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "GetRealmMaintenance", GetRealmMaintenance),
    };
}

void CoA::GlobalsB::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
