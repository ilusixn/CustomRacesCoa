#include "CoA/Source/Runtime/MemoryBridge.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>
#include <string>
#include <vector>

namespace {
    // A table of rows {id, key, stringOffset}; strings block "\0alpha\0beta\0".
    uint32_t MakeTable() {
        uint32_t table = CoA::MemoryBridge::CreateTable(12, 3);
        const char strings[] = "\0alpha\0beta";
        CoA::MemoryBridge::Command68(table, 0, strings, sizeof(strings));
        const uint32_t rows[] = { 10, 7, 1, 11, 9, 7, 12, 7, 0 };
        CoA::MemoryBridge::Command66(table, 0, rows, sizeof(rows));
        return table;
    }

    uint32_t U32(const std::vector<uint8_t>& b, size_t& pos) {
        uint32_t v;
        memcpy(&v, b.data() + pos, 4);
        pos += 4;
        return v;
    }
}

COA_TEST(MemoryBridge_HandlesStartAtOneAndAreNotReused) {
    CoA::MemoryBridge::Reset();
    CHECK(CoA::MemoryBridge::CreateTable(4, 0) == 1);
    CHECK(CoA::MemoryBridge::CreateTable(0, 3) == 0);
    CHECK(CoA::MemoryBridge::DestroyTable(1));
    CHECK(CoA::MemoryBridge::CreateTable(4, 1) == 2);
    CHECK(!CoA::MemoryBridge::DestroyTable(0));
    CHECK(!CoA::MemoryBridge::Command6A(0, 0));
}

COA_TEST(MemoryBridge_IndexNeededAndDroppedByWrites) {
    CoA::MemoryBridge::Reset();
    uint32_t table = MakeTable();
    std::vector<uint32_t> hits;
    CHECK(!CoA::MemoryBridge::Command6B(table, 4, 7, hits));
    CHECK(CoA::MemoryBridge::Command6A(table, 4));
    CHECK(CoA::MemoryBridge::Command6B(table, 4, 7, hits));
    CHECK((hits == std::vector<uint32_t>{ 0, 2 }));
    CHECK(CoA::MemoryBridge::Command6B(table, 4, 99, hits) && hits.empty());
    // A record write drops every index.
    const uint32_t row[] = { 13, 7, 0 };
    CHECK(CoA::MemoryBridge::Command66(table, 12, row, sizeof(row)));
    CHECK(!CoA::MemoryBridge::Command6B(table, 4, 7, hits));
    // A field that does not fit the record is refused.
    CHECK(!CoA::MemoryBridge::Command6A(table, 9));
}

COA_TEST(MemoryBridge_ReadCStringIncludesNul) {
    CoA::MemoryBridge::Reset();
    uint32_t table = MakeTable();
    std::string text;
    CHECK(CoA::MemoryBridge::Command6C(table, 1, text));
    CHECK(text == std::string("alpha", 6));
    CHECK(CoA::MemoryBridge::Command6C(table, 0, text) && text == std::string(1, '\0'));
    CHECK(!CoA::MemoryBridge::Command6C(table, 100, text));
}

COA_TEST(MemoryBridge_StringBlockGrowsZeroFilled) {
    CoA::MemoryBridge::Reset();
    uint32_t table = CoA::MemoryBridge::CreateTable(4, 1);
    CHECK(CoA::MemoryBridge::Command68(table, 4, "x", 2));
    std::string text;
    CHECK(CoA::MemoryBridge::Command6C(table, 0, text) && text == std::string(1, '\0'));
    CHECK(CoA::MemoryBridge::Command6C(table, 4, text) && text == std::string("x", 2));
}

COA_TEST(MemoryBridge_IndexedBatchFirstHitWithStrings) {
    CoA::MemoryBridge::Reset();
    uint32_t table = MakeTable();
    CoA::MemoryBridge::Command6A(table, 4);
    std::vector<uint8_t> out;
    CHECK(CoA::MemoryBridge::Command6D(table, 4, { 7, 5 }, out, { 8 }));
    size_t pos = 0;
    CHECK(U32(out, pos) == 2);
    CHECK(U32(out, pos) == 7);
    CHECK(out[pos++] == 1);
    CHECK(U32(out, pos) == 0);   // the lowest row
    uint32_t record[3];
    memcpy(record, out.data() + pos, 12);
    pos += 12;
    CHECK(record[0] == 10 && record[2] == 1);
    // MSTB: header, one entry {row 0, field 8, string offset 1, data offset 0, length 6}, data "alpha\0".
    CHECK(U32(out, pos) == 0x4254534D);
    CHECK(U32(out, pos) == 1);
    CHECK(U32(out, pos) == 1);
    CHECK(U32(out, pos) == 0 && U32(out, pos) == 8 && U32(out, pos) == 1 && U32(out, pos) == 0 && U32(out, pos) == 6);
    CHECK(U32(out, pos) == 6);
    CHECK(memcmp(out.data() + pos, "alpha", 6) == 0);
    pos += 6;
    CHECK(U32(out, pos) == 5);
    CHECK(out[pos++] == 0);
    CHECK(pos == out.size());
}

COA_TEST(MemoryBridge_OffsetZeroStringIsOneNul) {
    CoA::MemoryBridge::Reset();
    uint32_t table = MakeTable();
    std::vector<uint8_t> out;
    // Row 2's string field is 0.
    CHECK(CoA::MemoryBridge::Command6F(table, { 2, 9 }, out, { 8 }));
    size_t pos = 0;
    CHECK(U32(out, pos) == 2);
    CHECK(U32(out, pos) == 2 && out[pos++] == 1);
    pos += 12;
    CHECK(U32(out, pos) == 0x4254534D && U32(out, pos) == 1 && U32(out, pos) == 1);
    CHECK(U32(out, pos) == 2 && U32(out, pos) == 8 && U32(out, pos) == 0 && U32(out, pos) == 0 && U32(out, pos) == 1);
    CHECK(U32(out, pos) == 1 && out[pos++] == 0);
    CHECK(U32(out, pos) == 9 && out[pos++] == 0);
    CHECK(pos == out.size());
}

// PatchRecord (not in MMgr64.exe): overwrites a row or appends at row `count`, keeping built indexes current; a row
// past the end or a record of the wrong size is refused.
COA_TEST(MemoryBridge_PatchRecord) {
    CoA::MemoryBridge::Reset();
    uint32_t table = MakeTable();
    CHECK(CoA::MemoryBridge::Command6A(table, 0));
    CHECK(CoA::MemoryBridge::Command6A(table, 4));
    const uint32_t row[] = { 99, 9, 0 };
    CHECK(!CoA::MemoryBridge::Command73(table, 4, row, sizeof(row)));
    CHECK(!CoA::MemoryBridge::Command73(table, 0, row, 8));
    CHECK(CoA::MemoryBridge::Command73(table, 0, row, sizeof(row)));
    std::vector<uint8_t> out;
    CoA::MemoryBridge::Command6F(table, { 0 }, out, {});
    uint32_t id;
    memcpy(&id, out.data() + 9, 4);
    CHECK(id == 99);
    std::vector<uint32_t> hits;
    CHECK(CoA::MemoryBridge::Command6B(table, 0, 10, hits) && hits.empty());
    CHECK(CoA::MemoryBridge::Command6B(table, 0, 99, hits) && (hits == std::vector<uint32_t>{ 0 }));
    CHECK(CoA::MemoryBridge::Command6B(table, 4, 7, hits) && (hits == std::vector<uint32_t>{ 2 }));
    CHECK(CoA::MemoryBridge::Command6B(table, 4, 9, hits) && (hits == std::vector<uint32_t>{ 0, 1 }));

    const uint32_t added[] = { 13, 7, 0 };
    CHECK(CoA::MemoryBridge::Command73(table, 3, added, sizeof(added)));
    CHECK(CoA::MemoryBridge::Command6B(table, 0, 13, hits) && (hits == std::vector<uint32_t>{ 3 }));
    CHECK(CoA::MemoryBridge::Command6B(table, 4, 7, hits) && (hits == std::vector<uint32_t>{ 2, 3 }));
    CHECK(CoA::MemoryBridge::Command6F(table, { 3 }, out, {}));
    memcpy(&id, out.data() + 9, 4);
    CHECK(id == 13);
}

COA_TEST(MemoryBridge_OversizedRepliesAreSplit) {
    CoA::MemoryBridge::Reset();
    // 40 rows of 64 KB: one reply would be far over the 0xFF000-byte limit.
    uint32_t table = CoA::MemoryBridge::CreateTable(0x10000, 40);
    std::vector<uint32_t> indexes;

    for (uint32_t i = 0; i < 40; ++i) {
        indexes.push_back(i);
    }

    std::vector<uint8_t> out;
    CHECK(CoA::MemoryBridge::Command6F(table, indexes, out, {}));
    size_t pos = 0;
    CHECK(U32(out, pos) == 40);
    CHECK(out.size() == 4 + 40 * (5 + 0x10000));
}

COA_TEST(MemoryBridge_ResultCodes) {
    CoA::MemoryBridge::Reset();
    uint32_t table = MakeTable();
    CoA::MemoryBridge::Command6A(table, 4);
    CoA::MemoryBridge::Message request;
    request.cmd = 0x70;
    request.a = table;
    CoA::MemoryBridge::Message reply = CoA::MemoryBridge::Exchange(request);
    CHECK(reply.cmd == 0xFFFFFFFF && reply.result == CoA::MemoryBridge::UnknownCommand);
    request.cmd = 0x73;   // no record
    reply = CoA::MemoryBridge::Exchange(request);
    CHECK(reply.cmd == 0xFFFFFFFF && reply.result == CoA::MemoryBridge::Failed);

    // Keys {7}, then a header that is not MSTR.
    request.cmd = 0x6D;
    request.c = 4;
    request.blob = { 1, 0, 0, 0, 7, 0, 0, 0, 'X', 'S', 'T', 'R', 1, 0, 0, 0 };
    reply = CoA::MemoryBridge::Exchange(request);
    CHECK(reply.cmd == 0xFFFFFFFF && reply.result == CoA::MemoryBridge::BadExtras && reply.blob.empty());
    // Bytes after the extras.
    request.blob = { 1, 0, 0, 0, 7, 0, 0, 0, 'M', 'S', 'T', 'R', 1, 0, 0, 0, 0, 0, 0, 0, 9 };
    CHECK(CoA::MemoryBridge::Exchange(request).result == CoA::MemoryBridge::BadExtras);
    // Two keys announced, one sent.
    request.blob = { 2, 0, 0, 0, 7, 0, 0, 0 };
    CHECK(CoA::MemoryBridge::Exchange(request).result == CoA::MemoryBridge::InvalidMessage);
    // No such table.
    request.a = 99;
    request.blob = { 1, 0, 0, 0, 7, 0, 0, 0 };
    reply = CoA::MemoryBridge::Exchange(request);
    CHECK(reply.cmd == 0xFFFFFFFF && reply.result == CoA::MemoryBridge::Failed);

    // A reply over 0xFEFD0 bytes.
    uint32_t big = CoA::MemoryBridge::CreateTable(0x10000, 16);
    request.cmd = 0x6F;
    request.a = big;
    request.blob = { 16, 0, 0, 0 };

    for (uint8_t i = 0; i < 16; ++i) {
        request.blob.insert(request.blob.end(), { i, 0, 0, 0 });
    }

    reply = CoA::MemoryBridge::Exchange(request);
    CHECK(reply.cmd == 0xFFFFFFFF && reply.result == CoA::MemoryBridge::OversizedMessage && reply.blob.empty());
}

COA_TEST(MemoryBridge_CStringBatchExcludesNul) {
    CoA::MemoryBridge::Reset();
    uint32_t table = MakeTable();
    std::vector<uint8_t> out;
    CHECK(CoA::MemoryBridge::Command6E(table, { 7, 0, 50 }, out));
    size_t pos = 0;
    CHECK(U32(out, pos) == 3);
    CHECK(U32(out, pos) == 7 && out[pos++] == 1 && U32(out, pos) == 4);
    CHECK(memcmp(out.data() + pos, "beta", 4) == 0);
    pos += 4;
    CHECK(U32(out, pos) == 0 && out[pos++] == 1 && U32(out, pos) == 0);
    CHECK(U32(out, pos) == 50 && out[pos++] == 0);
    CHECK(pos == out.size());
}
