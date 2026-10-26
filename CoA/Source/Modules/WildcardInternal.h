#ifndef COA_SOURCE_MODULES_WILDCARDINTERNAL_H
#define COA_SOURCE_MODULES_WILDCARDINTERNAL_H

// What WildcardRolls.cpp (the mode manager), WildcardSets.cpp (the desired / undesired lists and their browsers) and
// WildcardRapid.cpp (the rapid-rolling session) share: the reconstruction's cross-file declarations. Not for other
// modules.
#include "CoA/Source/Modules/CA.h"
#include "CoA/Source/Modules/Wildcard.h"
#include <cstdint>
#include <string>
#include <vector>

namespace CoA::Wildcard::detail {
    // FUN_10a2f5e0: a STOP_RAPID_ROLLING_* name's index; any other string logs "Unexpected Value: <s>" to the Error
    // channel and answers 1 (STOP_RAPID_ROLLING_UNKNOWN).
    uint32_t StopCodeIndex(const std::string& s);
    extern const char* const kStopCodes[7];   // 0x10B607D4
    // The CA_UNLEARN_* name's index (the inline loops of FUN_10a30dd0 / FUN_10a35c20 over 0x10B204C8); any other string
    // answers 1 (CA_UNLEARN_UNKNOWN), without a log.
    uint32_t UnlearnResultIndex(const std::string& s);

    bool WildcardMode();   // FUN_1031ef10 bit 6

    // The client questions, through Deps (unset: no player, nothing counted, tick 0).
    bool HasPlayer();
    uint32_t TickCount();

    // Each file's part of Init / Reset.
    void InitRolls();
    void InitSets();
    void InitRapid();
    void ResetRolls();
    void ResetSets();
    void ResetRapid();
}

namespace CoA::WildcardSets {
    size_t DesiredCount();
    size_t UndesiredCount();
    // FUN_10a333d0's walks over the desired list: an entry flagged ability (+5) / talent (+6).
    bool HasDesiredWith(bool ability);
    // FUN_10a35270's second half (a spec change): both lists emptied, one event each when anything went.
    void ClearForSpecChange();
    // The session snapshot of the desired list (FUN_10a36d00's walk).
    void DesiredForRoll(std::vector<uint32_t>& ids, std::vector<uint32_t>& tags);

    struct Selection {
        uint32_t id;
        uint8_t kind;
    };

    bool KindIndex(const std::string& name, uint8_t& kind);   // FUN_10a29980
    // FUN_10a368b0's list swap.
    void ApplySelections(const std::vector<Selection>& desired, const std::vector<Selection>& undesired);

    struct Next {
        uint32_t id;
        uint8_t kind;
        bool ability;
        bool talent;
    };

    // FUN_10a344c0: the newest undesired CA entry `enough` accepts, else the newest one with a row.
    bool NewestUnlearnable(Next& out, bool (*enough)(const Next&));
}

#endif
