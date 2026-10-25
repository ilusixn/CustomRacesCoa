#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>
#include <vector>

namespace {
    // A WDBC file: header, `ids.size()` rows of {id, value, string offset}, the string block.
    std::vector<uint8_t> MakeWdbc(const std::vector<uint32_t>& ids, uint32_t magic = 0x43424457) {
        const char strings[] = "\0first\0second";
        std::vector<uint32_t> words = { magic, static_cast<uint32_t>(ids.size()), 3, 12, sizeof(strings) };
        uint32_t i = 0;
        for (uint32_t id : ids) {
            words.push_back(id);
            words.push_back(100 + i);
            words.push_back(i == 0 ? 1u : 7u);
            ++i;
        }
        std::vector<uint8_t> out(words.size() * 4);
        memcpy(out.data(), words.data(), out.size());
        out.insert(out.end(), strings, strings + sizeof(strings));
        return out;
    }

    std::vector<uint8_t> s_file;

    bool ReadFake(const char* path, std::vector<uint8_t>& out) {
        if (strcmp(path, "DBFilesClient\Fake.dbc") != 0) {
            return false;
        }
        out = s_file;
        return true;
    }
}

COA_TEST(Dbc_ParsesWdbc) {
    s_file = MakeWdbc({ 9, 4 });
    CoA::Dbc::SetFileReader(ReadFake);
    CoA::Dbc::Table table("DBFilesClient\Fake.dbc");

    CHECK(table.Count() == 2);
    CHECK(table.MinId() == 4);
    CHECK(table.MaxId() == 9);
    const uint8_t* nine = table.Row(9);
    const uint8_t* four = table.Row(4);
    CHECK(nine && CoA::Dbc::Table::U32(nine, 4) == 100);
    CHECK(four && CoA::Dbc::Table::U32(four, 4) == 101);
    CHECK(nine && strcmp(table.Str(nine, 8), "first") == 0);
    CHECK(four && strcmp(table.Str(four, 8), "second") == 0);
    CHECK(table.Row(5) == nullptr);
    CHECK(table.Row(10) == nullptr);
    CHECK(table.RowAt(0) == nine);

    CoA::Dbc::SetFileReader(nullptr);
}

COA_TEST(Dbc_RejectsBadMagic) {
    s_file = MakeWdbc({ 1 }, 0x12345678);
    CoA::Dbc::SetFileReader(ReadFake);
    CoA::Dbc::Table table("DBFilesClient\Fake.dbc");
    CHECK(!table.Loaded());
    CHECK(table.Row(1) == nullptr);
    CoA::Dbc::SetFileReader(nullptr);
}

COA_TEST(Dbc_MissingFileRetriesLater) {
    CoA::Dbc::SetFileReader(ReadFake);
    s_file = MakeWdbc({ 3 });
    CoA::Dbc::Table table("DBFilesClient\Other.dbc");
    CHECK(table.Row(3) == nullptr);
    CoA::Dbc::SetFileReader(nullptr);
}

COA_TEST(Dbc_UpsertAddsAndPatchesInPlace) {
    s_file = MakeWdbc({ 2, 3 });
    CoA::Dbc::SetFileReader(ReadFake);
    CoA::Dbc::Table table("DBFilesClient\Fake.dbc");

    std::vector<uint8_t> row(12, 0);
    uint32_t id = 3;
    uint32_t value = 555;
    memcpy(row.data(), &id, 4);
    memcpy(row.data() + 4, &value, 4);
    table.Upsert(3, row);
    CHECK(CoA::Dbc::Table::U32(table.RowAt(1), 4) == 555);

    id = 20;
    memcpy(row.data(), &id, 4);
    table.Upsert(20, row);
    CHECK(table.Row(20) != nullptr);
    // The original's index growth copies min..max-1: the old maximum (3) is dropped.
    CHECK(table.Row(3) == nullptr);
    CHECK(table.Row(2) != nullptr);

    CoA::Dbc::SetFileReader(nullptr);
}
