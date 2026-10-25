#ifndef COA_SOURCE_MODULES_CAFILTER_H
#define COA_SOURCE_MODULES_CAFILTER_H

// AscCAFilter.cpp: the character advancement browser filters. Plan 7c ports the module; until then these entry points
// (what the CA core calls into) do nothing, which is what the original does before any browser was opened.
namespace CoA::CAFilter {
    void RebuildRecentSets();            // FUN_101ca1d0, FUN_101c4360, FUN_101ca500 on mgr +0x190
    void ResetEditor();                  // BuildCreator +0x228
    void ResetEntries();                 // mgr +0x2F8
    void ResetCategories(bool recent);   // mgr +0x28 (false) / +0x190 (true)
}

#endif
