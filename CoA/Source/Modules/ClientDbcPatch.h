#ifndef COA_SOURCE_MODULES_CLIENTDBCPATCH_H
#define COA_SOURCE_MODULES_CLIENTDBCPATCH_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

class CDataStore;

// AscClientDbcPatch.cpp: the SMSG_PATCH_* handlers that patch the CLIENT's own WowClientDB tables (installers
// FUN_10228d30 / FUN_10229156 / FUN_1022958a). The wire reader and the index insert are here; the per-table writers
// (ClientDbcPatch.generated.inc, Tools/coa/gen_client_dbc_patch.py) and the handlers are in ClientDbcPatchClient.cpp.
namespace CoA::ClientDbcPatch {
    // The wire record as the handler reads it, item by item: b / h / d one value each, q two dwords, x four, _ a skipped
    // byte, L sixteen u32-length locale strings of which slot 0 is kept, p a byte packed into the current value (| closes
    // it), B a u32 of which only the low byte is kept (four pack into one value), s a C string, z a u32-length string.
    // Strings are copied into their own allocation and the value is the pointer; an overwritten row's old strings leak,
    // as in the original.
    std::vector<uint64_t> ReadValues(const char* program, CDataStore* msg);

    // What the 0x934 SKILL_LINE_ABILITY patch tells the spell API's spell -> ability map (AscSpellApi FUN_103266c0 /
    // FUN_10328a30); nothing until that module is ported.
    struct Deps {
        void (*skillLineAbilityErase)(uint32_t spell) = nullptr;
        void (*skillLineAbilityUpdate)(uint32_t spell) = nullptr;
    };

    Deps& Dependencies();

    // AscClientDbc::Insert: a new row's id inside [min, max] takes its slot; outside, the index is reallocated to cover
    // it and the old rows are copied -- except the old maximum, which the original's copy loop stops short of.
    template <class DB, class Row>
    void Insert(DB& db, int32_t id, Row* row, void* (*alloc)(size_t), void (*release)(void*)) {
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

        for (int32_t i = oldMin; i < oldMax; ++i) {   // the old maximum is not copied (original bug)
            if (db.m_recordsById[i - oldMin]) {
                index[i - lo] = db.m_recordsById[i - oldMin];
            }
        }

        index[id - lo] = row;
        release(db.m_recordsById);
        db.m_recordsById = index;
        db.m_minID = lo;
        db.m_maxID = hi;
    }
}

#endif
