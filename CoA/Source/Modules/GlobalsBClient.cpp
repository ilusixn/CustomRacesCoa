// AscGlobalsB.cpp's client natives. Ported so far: the glue-state LoadWDBCache / SaveWDBCache.
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"

namespace {
    int LoadWDBCache(lua_State*) {   // DBCache_Initialize (0x635520)
        DBCache_Initialize();
        return 0;
    }

    int SaveWDBCache(lua_State*) {   // DBCache_Destroy (0x635540)
        DBCache_Destroy();
        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "LoadWDBCache", LoadWDBCache),
        COA_NATIVE("", "SaveWDBCache", SaveWDBCache),
    };
}

void CoA::GlobalsBClient::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
