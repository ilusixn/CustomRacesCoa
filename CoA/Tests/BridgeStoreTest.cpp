#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/MemoryBridge.h"
#include "CoA/Tests/Support.h"
#include "CoA/Tests/TestMain.h"
#include <cstring>
#include <string>

namespace {
    std::vector<uint32_t> Join(std::initializer_list<std::vector<uint32_t>> parts) {
        std::vector<uint32_t> out;

        for (const auto& part : parts) {
            out.insert(out.end(), part.begin(), part.end());
        }

        return out;
    }

    std::vector<uint32_t> Zeros(size_t n) {
        return std::vector<uint32_t>(n, 0);
    }

    bool s_badCreature = false;

    // Strings: 1 "Wolf", 6 "Bear", 11 "Sword", 17 "Sharp", 23 "art", 27 "Desc", 32 "More".
    const std::string kStrings = std::string("Wolf") + '\0' + "Bear" + '\0' + "Sword" + '\0' + "Sharp" + '\0' + "art" + '\0' + "Desc" + '\0' + "More" + '\0';

    std::vector<uint32_t> VanityRow(uint32_t id, uint32_t item) {
        std::vector<uint32_t> head = { id, item, 3, 23 };
        head.resize(0x2A, 0);
        head[0xC] = 0x20;
        return Join({ head, CoATest::Loc(27), CoATest::Loc(32), { 9000 + id } });
    }

    std::vector<uint32_t> QuestRow(uint32_t id, uint32_t group8, uint32_t group10) {
        std::vector<uint32_t> row = Zeros(0x1D);
        row[0] = id;
        row[2] = group8;
        row[4] = group10;
        return row;
    }

    bool ReadBridge(const char* path, std::vector<uint8_t>& out) {
        if (strstr(path, "Creature.dbc")) {
            std::vector<uint32_t> wolf = Join({ { 1, 100 }, CoATest::Loc(1), { 501, 0, 0, 7 } });
            std::vector<uint32_t> bear = Join({ { 2, 200 }, CoATest::Loc(6), { 502, 0, 0, 8 } });

            if (s_badCreature) {
                wolf.pop_back();
                bear.pop_back();
            }

            out = CoATest::Wdbc({ wolf, bear }, kStrings);
            return true;
        }

        if (strstr(path, "ItemAddon.dbc")) {
            std::vector<uint32_t> fields = Zeros(12);
            fields[0] = 4;
            fields[1] = 0x80;
            out = CoATest::Wdbc({ Join({ { 1, 3000 }, CoATest::Loc(11), CoATest::Loc(17), fields }) }, kStrings);
            return true;
        }

        if (strstr(path, "VanityCollection.dbc")) {
            // 76 columns in 77 dwords: columns 13..14 are one 8-byte field.
            out = CoATest::Wdbc({ VanityRow(10, 4000), VanityRow(11, 4001) }, kStrings, 0x4C);
            return true;
        }

        if (strstr(path, "Quest.dbc")) {
            out = CoATest::Wdbc({ QuestRow(30, 5, 0), QuestRow(31, 0, 0), QuestRow(32, 0, 5) }, kStrings);
            return true;
        }

        if (strstr(path, "MythicKeystones.dbc")) {
            out = CoATest::Wdbc({ { 60, 1, 2 }, { 61, 3, 4 } }, kStrings);
            return true;
        }

        if (strstr(path, "SpellAffect.dbc")) {
            out = CoATest::Wdbc({ { 1, 700, 11 }, { 2, 701, 12 }, { 3, 700, 13 } }, kStrings);
            return true;
        }

        return false;
    }

    void Load(bool badCreature = false) {
        s_badCreature = badCreature;
        CoA::Dbc::SetFileReader(ReadBridge);
        CoA::BridgeStore::Reset();
        CoA::MemoryBridge::Reset();
        CoA::BridgeStore::LoadTables();
        CoA::BridgeStore::BuildIndexes();
        CoA::Dbc::SetFileReader(nullptr);
    }
}

COA_TEST(BridgeStore_CreatureByEntry) {
    Load();
    const CoA::BridgeStore::CreatureRecord* bear = CoA::BridgeStore::CreatureByEntry(200);
    CHECK(bear && bear->id == 2 && bear->displayId == 502 && bear->f18 == 8);
    CHECK(bear && std::string(bear->name) == "Bear");
    CHECK(CoA::BridgeStore::CreatureByEntry(2) == nullptr);
    // A cached answer is the same record.
    CHECK(CoA::BridgeStore::CreatureByEntry(200) == bear);
}

COA_TEST(BridgeStore_WrongLayoutIsNotLoaded) {
    Load(true);
    CHECK(CoA::BridgeStore::CreatureByEntry(100) == nullptr);
    CHECK(CoA::BridgeStore::KeystoneById(60, std::vector<uint32_t>(3).data()));
}

COA_TEST(BridgeStore_Quests) {
    Load();
    const uint8_t* quest = CoA::BridgeStore::QuestById(31);
    CHECK(quest && CoA::Dbc::Table::U32(quest, 0) == 31);
    std::vector<const uint8_t*> group = CoA::BridgeStore::QuestsInGroup(5);
    CHECK(group.size() == 2);
    CHECK(group.size() == 2 && CoA::Dbc::Table::U32(group[0], 0) == 30 && CoA::Dbc::Table::U32(group[1], 0) == 32);
    CHECK(CoA::BridgeStore::QuestsInGroup(6).empty());
}

COA_TEST(BridgeStore_VanityStrings) {
    Load();
    CHECK(CoA::BridgeStore::VanityCount() == 2);
    const uint32_t indexes[] = { 1, 5 };
    std::vector<CoA::BridgeStore::VanityEntry> entries;
    CHECK(CoA::BridgeStore::VanityRecords(indexes, 2, entries));
    CHECK(entries.size() == 2 && entries[0].found && !entries[1].found);
    const CoA::BridgeStore::VanityRecord& row = entries[0].record;
    CHECK(row.f[0] == 11 && row.f[1] == 4001 && row.f[0xC] == 0x20 && row.f[0x2C] == 9011);
    CHECK(std::string(row.Str(3)) == "art" && std::string(row.Str(0x2A)) == "Desc" && std::string(row.Str(0x2B)) == "More");
    CHECK(row.Str(4) == nullptr);
    // The row no longer holding the item: looked up by item.
    const CoA::BridgeStore::VanityRecord* other = CoA::BridgeStore::VanityAt(1, 4000);
    CHECK(other && other->f[0] == 10);
}

COA_TEST(BridgeStore_KeystonePatchTouchesTheCacheOnly) {
    Load();
    uint32_t out[3] = {};
    CHECK(CoA::BridgeStore::KeystoneById(61, out) && out[1] == 3 && out[2] == 4);
    const uint32_t changed[3] = { 61, 9, 9 };
    CoA::BridgeStore::PatchKeystone(changed);
    CHECK(CoA::BridgeStore::KeystoneById(61, out) && out[1] == 9);
    // A new keystone needs PatchRecord, which the server refuses.
    const uint32_t added[3] = { 62, 1, 1 };
    CoA::BridgeStore::PatchKeystone(added);
    CHECK(!CoA::BridgeStore::KeystoneById(62, out));
}

COA_TEST(BridgeStore_SpellAffects) {
    Load();
    auto affects = CoA::BridgeStore::SpellAffectsFor(700);
    CHECK(affects.size() == 2);
    CHECK(affects.size() == 2 && affects[0][2] == 11 && affects[1][2] == 13);
    CHECK(CoA::BridgeStore::SpellAffectsFor(702).empty());
}

COA_TEST(BridgeStore_ItemAddonInfo) {
    Load();
    CoA::ItemAddon::Info info = CoA::ItemAddon::GetInfo(3000);
    CHECK(info.name == "Sword" && info.description == "Sharp" && info.quality == 4 && info.flags == 0x80);
    const CoA::ItemAddon::Record* record = CoA::ItemAddon::Find(3000);
    CHECK(record && record->rowId == 1 && std::string(record->name) == "Sword");
    CHECK(CoA::ItemAddon::GetInfo(1).name.empty());
}

COA_TEST(BridgeStore_CreaturePatchOverwritesTheCachedRecord) {
    Load();
    CoATest::Bytes known;
    known.U32(100).U32(0).U32(777).U32(1).U32(2).U32(3).Name("Dire Wolf");
    CoATest::Deliver(CoA::BridgeStore::OnPatchCreature, 0x567, known);
    const CoA::BridgeStore::CreatureRecord* wolf = CoA::BridgeStore::CreatureByEntry(100);
    CHECK(wolf && wolf->displayId == 777 && wolf->id == 0 && wolf->f18 == 3 && std::string(wolf->name) == "Dire Wolf");

    // An unknown entry would be appended through PatchRecord: refused.
    CoATest::Bytes unknown;
    unknown.U32(300).U32(0).U32(1).U32(1).U32(1).U32(1).Name("Cat");
    CoATest::Deliver(CoA::BridgeStore::OnPatchCreature, 0x567, unknown);
    CHECK(CoA::BridgeStore::CreatureByEntry(300) == nullptr);
}

COA_TEST(BridgeStore_QuestPatchIsRefused) {
    Load();
    CoATest::Bytes row;

    for (uint32_t v : QuestRow(31, 5, 0)) {
        row.U32(v);
    }

    CoATest::Deliver(CoA::BridgeStore::OnPatchQuest, 0x56F, row);
    CHECK(CoA::BridgeStore::QuestsInGroup(5).size() == 2);
}
