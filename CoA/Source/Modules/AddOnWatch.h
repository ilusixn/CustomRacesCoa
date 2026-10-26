#ifndef COA_SOURCE_MODULES_ADDONWATCH_H
#define COA_SOURCE_MODULES_ADDONWATCH_H

#include <cstdint>
#include <string>
#include <vector>

// Live addon list (CoA addition, not in the original DLL): AddOnWatchClient.cpp watches Interface\AddOns\ (and
// -addonspath) for changes and has the client rescan just the addons that changed, so an addon added, removed or
// edited while the client runs shows up in the addon list without a restart. This half is the platform-free part:
// which changes matter and when a burst of them has settled.
namespace CoA::AddOnWatch {
    // The quiet time after the last relevant change before the rescan runs: extracting an addon writes many files.
    constexpr uint32_t kSettleMs = 500;

    // The addon a change notification path (relative to the watched folder, '\' or '/') is about, or "" when the
    // change cannot affect the list: only the top-level folder itself and a .toc directly inside it count, so
    // Lua/XML edits and the client's own .url / .pub writes never trigger a rescan.
    std::string AddOnForChange(const char* relativePath);

    // A relevant change to the addon at now (ms).
    void Note(const std::string& name, uint32_t now);
    // The watch lost events (buffer overflow): every addon has to be rescanned.
    void NoteAll(uint32_t now);
    // Whether a rescan is due at now; if so, takes the pending names (all = rescan everything).
    bool Take(uint32_t now, std::vector<std::string>& names, bool& all);
    // Tests only.
    void Reset();
}

#endif
