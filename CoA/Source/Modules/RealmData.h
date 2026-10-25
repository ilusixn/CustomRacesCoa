#ifndef COA_SOURCE_MODULES_REALMDATA_H
#define COA_SOURCE_MODULES_REALMDATA_H

#include <cstdint>
#include <string>
#include <vector>

// Realm data hot-swap -- the original's per-realm data directory (port of AscRealmData.cpp; module init
// FUN_102f8930). A realm can run on its own data: Data\<path>\listarchive names MPQs (one per line) opened from
// Data\<path>\ on top of the base archives at priorities 0x200, 0x201, ... The path comes from SMSG_REALM_INFO's first
// string, from SetDataPath(path) (glue), from the "realmdata <path>" console command, or from -realmdata on the command
// line (initial path only). The switch itself, the client hooks and the always-on changes (the per-realm WDB cache
// directory, -hd 0) are in RealmDataClient.cpp.
namespace CoA::RealmData {
    // The archive names in <dir>\listarchive, one per line, in order; empty when there is none.
    std::vector<std::string> ListArchive(const std::string& dir);
    // The build number in <dir>buildinfo (the original's path: no separator), default 12340.
    uint32_t BuildNumber(const std::string& dir);
    // FUN_102fb520: "Cache/WDB/<locale>/<realm name>".
    std::string CachePath(const char* locale, const char* realm);

    // FUN_102f8920.
    const std::string& CurrentPath();
    // FUN_102fac20's test: not when `path` is current, or already the pending switch.
    bool NeedsSwitch(const std::string& path);
    // DAT_10be357d: Data\patch-Q.MPQ was opened.
    bool HdPatchLoaded();

    // The switch state (0x10BCC0A8..0x10BCC0F0), driven by RealmDataClient.cpp.
    struct State {
        std::string path;        // 0x10BCC0A8
        std::string pending;     // 0x10BCC0F0
        bool active = false;     // 0x10BCC0D8
        bool finished = false;   // 0x10BCC0D9
        bool hdPatch = false;    // 0x10BE357D
    };

    State& GetState();

    // Tests only: the current path, no switch pending.
    void ResetState(const std::string& path);
}

#endif
