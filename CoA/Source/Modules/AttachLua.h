#ifndef COA_SOURCE_MODULES_ATTACHLUA_H
#define COA_SOURCE_MODULES_ATTACHLUA_H

#include "CoA/Source/Runtime/ClientHooks.h"
#include <cstdint>
#include <string>
#include <windows.h>

// AscAttachLua.cpp: the Lua functions and the packet handler the attach init detours or replaces (marked source
// edits in the decomp); what they compute without client objects lives here.
namespace CoA::AttachLua {
    // FUN_10a6ff20 (0x4FBD20 LoggingCombat): "Logs\<YYYY-MM-DD-HH.MM.SS> WoWCombatLog.txt".
    std::string CombatLogFileName(const SYSTEMTIME& time);
    // The name the client's combat log opens (kept alive here, as the DLL's 0x10BCCCFC), for the local time now. The
    // date part is also written to stdout, as the original's puts.
    const char* NewCombatLogFileName();

    // 0x635710 (SetClientCacheVersion), after: the item-stat and quest-addon stores (their modules register here) get
    // the version; each empties itself when it differs from its own. The itemstatcache.wdb load runs once first.
    inline CoA::ClientHooks::Hooks<> loadCachesOnce;
    inline CoA::ClientHooks::Hooks<uint32_t> clientCacheVersion;
    void ClientCacheVersionSet(uint32_t version);
}

#endif
