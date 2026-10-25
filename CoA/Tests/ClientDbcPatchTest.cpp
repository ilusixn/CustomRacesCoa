#include "CoA/Source/Modules/ClientDbcPatch.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
    std::string Str(uint64_t v) {
        return reinterpret_cast<const char*>(static_cast<uintptr_t>(v));
    }

    // A WowClientDB's index fields, as Insert uses them.
    struct FakeDb {
        int32_t m_minID = 0;
        int32_t m_maxID = 0;
        int* * m_recordsById = nullptr;
    };

    void* Alloc(size_t n) {
        return malloc(n);
    }

    void Free(void* p) {
        free(p);
    }
}

// The wire program: b h d as dwords, q as two, x as four, _ skips a byte, L keeps locale slot 0 of 16 sized strings,
// p packs bytes until |, B packs the low bytes of four u32s, s a C string, z a sized string.
COA_TEST(ClientDbcPatch_ReadValues) {
    CoATest::Bytes b;
    b.U8(7).U16(0x1234).U32(0xDEADBEEF).U64(0x0000000200000001ull);
    b.U32(1).U32(2).U32(3).U32(4);
    b.U8(0xEE);

    for (int i = 0; i < 16; ++i) {
        const std::string s = i == 0 ? "enUS" : "x" + std::to_string(i);
        b.Name(s.c_str());
    }

    b.U8(0x11).U8(0x22);
    b.U32(0x1FF).U32(0x2AA).U32(0x3BB).U32(0x4CC);
    b.Str("cstr").Name("sized");
    CDataStore msg(b.data.data(), static_cast<uint32_t>(b.data.size()));

    const std::vector<uint64_t> v = CoA::ClientDbcPatch::ReadValues("bhdqx_Lpp|BBBBsz", &msg);
    CHECK(v.size() == 14);
    CHECK(v[0] == 7 && v[1] == 0x1234 && v[2] == 0xDEADBEEF && v[3] == 1 && v[4] == 2);
    CHECK(v[5] == 1 && v[6] == 2 && v[7] == 3 && v[8] == 4);
    CHECK(Str(v[9]) == "enUS");
    CHECK(v[10] == 0x2211);
    CHECK(v[11] == 0xCCBBAAFF);
    CHECK(Str(v[12]) == "cstr");
    CHECK(Str(v[13]) == "sized");
    CHECK(msg.m_read == msg.m_size);
}

// A new id inside the index replaces the slot; outside it the index grows to cover it -- and, as in the original, the
// old maximum's row is not copied across.
COA_TEST(ClientDbcPatch_InsertGrowsIndexDroppingOldMax) {
    int a = 1;
    int b = 2;
    int c = 3;
    int d = 4;
    FakeDb db;
    db.m_minID = 10;
    db.m_maxID = 12;
    db.m_recordsById = static_cast<int**>(Alloc(3 * sizeof(int*)));
    db.m_recordsById[0] = &a;
    db.m_recordsById[1] = nullptr;
    db.m_recordsById[2] = &b;

    CoA::ClientDbcPatch::Insert(db, 11, &c, Alloc, Free);
    CHECK(db.m_minID == 10 && db.m_maxID == 12 && db.m_recordsById[1] == &c);

    CoA::ClientDbcPatch::Insert(db, 15, &d, Alloc, Free);
    CHECK(db.m_minID == 10 && db.m_maxID == 15);
    CHECK(db.m_recordsById[0] == &a && db.m_recordsById[1] == &c);
    CHECK(db.m_recordsById[2] == nullptr);   // id 12, the old maximum: lost
    CHECK(db.m_recordsById[5] == &d);

    CoA::ClientDbcPatch::Insert(db, 8, &b, Alloc, Free);
    CHECK(db.m_minID == 8 && db.m_maxID == 15);
    CHECK(db.m_recordsById[0] == &b && db.m_recordsById[2] == &a && db.m_recordsById[3] == &c);
    CHECK(db.m_recordsById[7] == nullptr);   // id 15, the old maximum again
    Free(db.m_recordsById);
}

// An RLE table's index lives inside its record block: a growing insert must not free it (review fix; the original's
// free of that interior pointer is silent on its 32-bit heap, fatal on the x64 one).
COA_TEST(ClientDbcPatch_InsertKeepsSharedIndexBlock) {
    static int s_released = 0;
    int a = 1;
    int d = 4;
    int** block = static_cast<int**>(Alloc(5 * sizeof(int*)));
    FakeDb db;
    db.m_minID = 10;
    db.m_maxID = 11;
    db.m_recordsById = block + 3;   // m_records[3] then the index
    db.m_recordsById[0] = &a;
    db.m_recordsById[1] = nullptr;

    s_released = 0;
    CoA::ClientDbcPatch::Insert(db, 14, &d, Alloc, [](void* p) { ++s_released; free(p); }, false);
    CHECK(s_released == 0);
    CHECK(db.m_minID == 10 && db.m_maxID == 14 && db.m_recordsById[0] == &a && db.m_recordsById[4] == &d);
    Free(db.m_recordsById);
    Free(block);
}
