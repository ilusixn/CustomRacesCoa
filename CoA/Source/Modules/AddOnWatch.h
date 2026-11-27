#ifndef COA_SOURCE_MODULES_ADDONWATCH_H
#define COA_SOURCE_MODULES_ADDONWATCH_H

#include <cstdint>
#include <string>
#include <vector>

// Live addon list (CoA addition): which folder changes matter and when a burst has settled.
// See docs/2026-11-15-addon-watch-and-world-entry-notes.md.
namespace CoA::AddOnWatch {
    // The quiet time after the last relevant change before the rescan runs: extracting an addon writes many files.
    constexpr uint32_t kSettleMs = 500;

    // The addon a changed path (relative, '\' or '/') is about: only a top-level folder or a .toc directly in it,
    // so Lua/XML edits and the client's own .url / .pub writes never rescan the list ("" otherwise).
    std::string AddOnForChange(const char* relativePath);

    // A relevant change to the addon at now (ms).
    void Note(const std::string& name, uint32_t now);
    // The watch lost events (buffer overflow): every addon has to be rescanned.
    void NoteAll(uint32_t now);
    // Any other file change: the disk file hash goes stale, the list does not.
    void NoteFile(uint32_t now);
    // Whether changes settled at now; if so, takes the pending names (all = rescan everything; none = hash only).
    bool Take(uint32_t now, std::vector<std::string>& names, bool& all);
    // Tests only.
    void Reset();
}

#endif
