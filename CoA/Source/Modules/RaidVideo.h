#ifndef COA_SOURCE_MODULES_RAIDVIDEO_H
#define COA_SOURCE_MODULES_RAIDVIDEO_H

#include <cstddef>
#include <cstdint>
#include <string>

// Raid video settings (port of AscRaidVideo.cpp, installer FUN_102dc960): eleven graphics CVars with a second value
// applied inside raids and battlegrounds while useRaidVideoSettings is on. RaidVideoClient.cpp holds the callback,
// the tick on every second SMSG_INITIAL_SPELLS, the glue-screen restore and the Config.wtf writer.
namespace CoA::RaidVideo {
    struct Entry {
        const char* base;
        const char* raidName;
        const char* raidDefault;
    };
    const Entry& Setting(size_t index);   // 0..10
    constexpr size_t kCount = 11;

    // FUN_102dcdb0's name handling: "<name>_raid" -> "<name>" (the first "_raid" removed); "" without one.
    std::string BaseName(const std::string& name);
}

#endif
