#ifndef COA_SOURCE_MODULES_CLIENTDBCPATCH_H
#define COA_SOURCE_MODULES_CLIENTDBCPATCH_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

class CDataStore;

// AscClientDbcPatch.cpp: SMSG_PATCH_* for the CLIENT's own WowClientDB tables (FUN_10228d30 / FUN_10229156 / FUN_1022958a).
// Wire reader and index insert here; per-table writers (generated) and handlers in ClientDbcPatchClient.cpp.
namespace CoA::ClientDbcPatch {
    // The wire record, item by item; the program's letters are in docs/2026-11-03-client-dbc-patch-notes.md.
    std::vector<uint64_t> ReadValues(const char* program, CDataStore* msg);

    // What the 0x934 SKILL_LINE_ABILITY patch tells the spell API's spell -> ability map (AscSpellApi FUN_103266c0 /
    // FUN_10328a30); nothing until that module is ported.
    struct Deps {
        void (*skillLineAbilityErase)(uint32_t spell) = nullptr;
        void (*skillLineAbilityUpdate)(uint32_t spell) = nullptr;
    };

    Deps& Dependencies();

    // AscClientDbc::Insert, keeping the old maximum the original loses. ownsIndex is false when the index lives inside the
    // record block (WowClientDB_RLE): freeing it would end the x64 process. See the notes page above.
    template <class DB, class Row>
    void Insert(DB& db, int32_t id, Row* row, void* (*alloc)(size_t), void (*release)(void*), bool ownsIndex = true) {
        const int32_t oldMin = db.m_minID;
        const int32_t oldMax = db.m_maxID;

        if (id <= oldMax && id >= oldMin) {
            db.m_recordsById[id - oldMin] = row;
            return;
        }

        const int32_t lo = id < oldMin ? id : oldMin;
        const int32_t hi = oldMax < id ? id : oldMax;
        const size_t size = static_cast<size_t>(hi - lo + 1) * sizeof(db.m_recordsById[0]);
        auto index = static_cast<decltype(db.m_recordsById)>(alloc(size));
        memset(index, 0, size);

        for (int32_t i = oldMin; db.m_recordsById && i <= oldMax; ++i) {
            if (db.m_recordsById[i - oldMin]) {
                index[i - lo] = db.m_recordsById[i - oldMin];
            }
        }

        index[id - lo] = row;
        if (ownsIndex) {
            release(db.m_recordsById);
        }

        db.m_recordsById = index;
        db.m_minID = lo;
        db.m_maxID = hi;
    }
}

#endif
