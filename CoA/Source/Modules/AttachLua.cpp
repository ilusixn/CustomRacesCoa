#include "CoA/Source/Modules/AttachLua.h"
#include <cstdio>
#include <ctime>

namespace {
    std::string s_combatLogName;   // 0x10BCCCFC
    bool s_cachesLoaded = false;   // 0x10D3DC6C
}

std::string CoA::AttachLua::CombatLogFileName(const SYSTEMTIME& time) {
    char stamp[0x50];
    snprintf(stamp, sizeof(stamp), "%04u-%02u-%02u-%02u.%02u.%02u", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute,
        time.wSecond);
    return std::string("Logs\\") + stamp + " WoWCombatLog.txt";
}

const char* CoA::AttachLua::NewCombatLogFileName() {
    SYSTEMTIME now;
    GetLocalTime(&now);
    s_combatLogName = CoA::AttachLua::CombatLogFileName(now);
    // The original puts() the strftime stamp.
    puts(s_combatLogName.substr(5, s_combatLogName.size() - 5 - 17).c_str());
    return s_combatLogName.c_str();
}

void CoA::AttachLua::ClientCacheVersionSet(uint32_t version) {
    if (!s_cachesLoaded) {
        CoA::AttachLua::loadCachesOnce.Run();
        s_cachesLoaded = true;
    }

    CoA::AttachLua::clientCacheVersion.Run(version);
}
