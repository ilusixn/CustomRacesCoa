#ifndef COA_SOURCE_MODULES_CAFILTER_H
#define COA_SOURCE_MODULES_CAFILTER_H

// AscCAFilter.cpp: the character advancement browser filters (four klib filterable containers over
// CharacterAdvancement.dbc) and their natives. CAFilter.cpp has the design notes.
#include "CoA/Source/Modules/CA.h"
#include <string>

namespace CoA::CAFilter {
    // FUN_101ca1d0, FUN_101c4360, FUN_101ca500 on mgr +0x190: the recent / upcoming entry sets of the class categories.
    void RebuildRecentSets();
    void ResetEditor();                  // BuildCreator +0x228
    void ResetEntries();                 // mgr +0x2F8
    void ResetCategories(bool classes);  // mgr +0x28 (false) / +0x190 (true)
    void TagTypesPatched();              // SMSG 0x6C0: FUN_10217220 rebuilds its tag type -> stats table

    // For C_Wildcard's entry browser: FUN_101c1ce0 with the gates, FUN_1014e100 over +0x60.
    bool RowMatchesQuery(CoA::CA::Row r, const std::string& lowerQuery);
    int RowQualityIndex(CoA::CA::Row r);

    void Init();    // from CoA::CA::Init
    void Reset();   // tests: every container and the tag-type table back to their start
}

#endif
