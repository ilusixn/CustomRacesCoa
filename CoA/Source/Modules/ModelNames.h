#ifndef COA_SOURCE_MODULES_MODELNAMES_H
#define COA_SOURCE_MODULES_MODELNAMES_H

#include <cstdint>
#include <string>
#include <vector>

// AscModelNames.cpp: the model-path autocomplete -- PopulateModelNameCache (FUN_10a701b0) and GetModelNameCompletions
// (FUN_10a6ef50).
//
// PopulateModelNameCache() rebuilds the path list (0x10D3D73C) from four tables, each path once (deduplicated on the
// path as stored), '\' turned into '/': CreatureModelData (0xAD3500) +0x08, GameObjectDisplayInfo (0xAD3938) +0x04,
// SpellVisualEffectName (0xAD4A18) +0x08 for ids 0 .. max - 1 (the original's loops stop before the max id; kept),
// then the DLL's ItemDisplayInfoCollections.dbc +0x10 for ids min .. max. Returns 1 with nothing pushed (kept).
//
// GetModelNameCompletions(query, limit): the query splits on single spaces; tokens shorter than 3 characters are
// ignored, a token in double quotes ("...", at least 3 characters with the quotes) is a quoted term, the rest plain
// terms. A path scores one per term it contains (case-sensitive); with quoted terms at least one must match, and a path
// is kept when anything matched. Sorted by score, highest first (std::sort, not stable), cut to `limit`, returned as an
// array.
namespace CoA::ModelNames {
    struct Deps {
        // The three client tables' path columns in the original's order, ids 0 .. max - 1 (absent rows skipped); a
        // null path is passed as nullptr.
        void (*clientPaths)(void (*add)(const char* path, void* ctx), void* ctx) = nullptr;
    };

    Deps& Dependencies();

    void Populate();
    const std::vector<std::string>& Paths();
    std::vector<std::string> Completions(const std::string& query, uint32_t limit);

    void Init();
    // Tests only.
    void Reset();
}

#endif
