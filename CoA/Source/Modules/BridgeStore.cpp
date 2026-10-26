// AscBridgeStore.cpp -- see BridgeStore.h.
//
//   Loader (FUN_101fd240 Creature, FUN_101fded0 Quest, and their siblings): WDBC with the expected field count and
//   row size; each file row packed as the original's table row (a locstring becomes its locale slot's offset), tracking
//   the largest / smallest id, then made the x64 record with its strings pointing into the file's string block. Then
//   the id index (the original's 0x6A on field 0). Any failure leaves the store empty.
//
//   The original kept these rows in MMgr64.exe and read them back through a 0x400-record / 2 MB LRU cache. Here the
//   tables are the records themselves: a lookup is the first row (lowest index) holding the key in an indexed field,
//   as the helper's sorted index answered; a field the original never indexed answers nothing, as its query failed.
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>
#include <unordered_set>

namespace {
    using CoA::BridgeStore::CreatureRecord;
    using CoA::BridgeStore::VanityRecord;
    using ItemAddonRecord = CoA::ItemAddon::Record;

    struct QuestRecord {
        uint32_t f[0x1D];
    };

    // MythicKeystones.dbc / SpellAffect.dbc: 3 u32 (FUN_101b9760).
    struct SmallRecord {
        uint32_t f[3];
    };

    uint32_t U32(const uint8_t* p, uint32_t off) {
        uint32_t v;
        memcpy(&v, p + off, 4);
        return v;
    }

    void SetU32(uint8_t* p, uint32_t off, uint32_t v) {
        memcpy(p + off, &v, 4);
    }

    // A file row as table-row dwords: n > 0 copies n dwords, kLoc keeps one locstring's locale slot.
    constexpr int kLoc = -1;

    // ---- the records -------------------------------------------------------------------------------------------
    // Per record: the original's 32-bit table row (string fields as string-block offsets; the keys are read from it),
    // the file it loads from and the row offsets of its string fields.
    template <class Record> struct Traits;

    template <> struct Traits<CreatureRecord> {
        static constexpr uint32_t kRowSize = 0x1C;
        static constexpr uint32_t kFileFields = 0x17;
        static constexpr uint32_t kFileRowSize = 0x5C;
        static constexpr size_t kStrings = 1;
        static constexpr const char* kPath = "DBFilesClient\\Creature.dbc";
        static const std::vector<int>& Plan() { static const std::vector<int> p = { 2, kLoc, 4 }; return p; }
        static const std::vector<uint32_t>& StringFields() { static const std::vector<uint32_t> f = { 8 }; return f; }
        static const char*& Slot(CreatureRecord& r, size_t) { return r.name; }
        static uint32_t Id(const CreatureRecord& r) { return r.id; }

        static void FromRow(const uint8_t* row, CreatureRecord& r) {
            r = { U32(row, 0), U32(row, 4), nullptr, U32(row, 0xC), U32(row, 0x10), U32(row, 0x14), U32(row, 0x18) };
        }

        static void ToRow(const CreatureRecord& r, uint8_t* row) {
            const uint32_t v[7] = { r.id, r.entry, 0, r.displayId, r.f10, r.f14, r.f18 };
            memcpy(row, v, sizeof(v));
        }
    };

    template <> struct Traits<QuestRecord> {
        static constexpr uint32_t kRowSize = 0x74;
        static constexpr uint32_t kFileFields = 0x1D;
        static constexpr uint32_t kFileRowSize = 0x74;
        static constexpr size_t kStrings = 0;
        static constexpr const char* kPath = "DBFilesClient\\Quest.dbc";
        static const std::vector<int>& Plan() { static const std::vector<int> p = { 0x1D }; return p; }
        static const std::vector<uint32_t>& StringFields() { static const std::vector<uint32_t> f; return f; }
        static const char*& Slot(QuestRecord&, size_t) { static const char* none = nullptr; return none; }
        static uint32_t Id(const QuestRecord& r) { return r.f[0]; }
        static void FromRow(const uint8_t* row, QuestRecord& r) { memcpy(r.f, row, sizeof(r.f)); }
        static void ToRow(const QuestRecord& r, uint8_t* row) { memcpy(row, r.f, sizeof(r.f)); }
    };

    template <> struct Traits<SmallRecord> {
        static constexpr uint32_t kRowSize = 0xC;
        static constexpr uint32_t kFileFields = 3;
        static constexpr uint32_t kFileRowSize = 0xC;
        static constexpr size_t kStrings = 0;
        static const std::vector<int>& Plan() { static const std::vector<int> p = { 3 }; return p; }
        static const std::vector<uint32_t>& StringFields() { static const std::vector<uint32_t> f; return f; }
        static const char*& Slot(SmallRecord&, size_t) { static const char* none = nullptr; return none; }
        static uint32_t Id(const SmallRecord& r) { return r.f[0]; }
        static void FromRow(const uint8_t* row, SmallRecord& r) { memcpy(r.f, row, sizeof(r.f)); }
        static void ToRow(const SmallRecord& r, uint8_t* row) { memcpy(row, r.f, sizeof(r.f)); }
    };

    template <> struct Traits<ItemAddonRecord> {
        static constexpr uint32_t kRowSize = 0x40;
        static constexpr uint32_t kFileFields = 0x30;
        static constexpr uint32_t kFileRowSize = 0xC0;
        static constexpr size_t kStrings = 2;
        static constexpr const char* kPath = "DBFilesClient\\ItemAddon.dbc";
        static const std::vector<int>& Plan() { static const std::vector<int> p = { 2, kLoc, kLoc, 12 }; return p; }
        static const std::vector<uint32_t>& StringFields() { static const std::vector<uint32_t> f = { 8, 0xC }; return f; }
        static const char*& Slot(ItemAddonRecord& r, size_t k) { return k == 0 ? r.name : r.description; }
        static uint32_t Id(const ItemAddonRecord& r) { return r.rowId; }

        static void FromRow(const uint8_t* row, ItemAddonRecord& r) {
            r.rowId = U32(row, 0);
            r.id = U32(row, 4);
            r.name = nullptr;
            r.description = nullptr;
            memcpy(r.fields, row + 0x10, sizeof(r.fields));
        }

        static void ToRow(const ItemAddonRecord& r, uint8_t* row) {
            SetU32(row, 0, r.rowId);
            SetU32(row, 4, r.id);
            SetU32(row, 8, 0);
            SetU32(row, 0xC, 0);
            memcpy(row + 0x10, r.fields, sizeof(r.fields));
        }
    };

    template <> struct Traits<VanityRecord> {
        static constexpr uint32_t kRowSize = 0xB4;
        // 76 columns: 13..14 are one 8-byte field.
        static constexpr uint32_t kFileFields = 0x4C;
        static constexpr uint32_t kFileRowSize = 0x134;
        static constexpr size_t kStrings = 3;
        static constexpr const char* kPath = "DBFilesClient\\VanityCollection.dbc";
        // FUN_1021a230: columns 0..41 in order, two locstrings, column 76.
        static const std::vector<int>& Plan() { static const std::vector<int> p = { 0x2A, kLoc, kLoc, 1 }; return p; }
        static const std::vector<uint32_t>& StringFields() { static const std::vector<uint32_t> f = { 0xC, 0xA8, 0xAC }; return f; }
        static const char*& Slot(VanityRecord& r, size_t k) { return r.strings[k]; }
        static uint32_t Id(const VanityRecord& r) { return r.f[0]; }

        static void FromRow(const uint8_t* row, VanityRecord& r) {
            memcpy(r.f, row, sizeof(r.f));
            r.strings[0] = r.strings[1] = r.strings[2] = nullptr;
        }

        static void ToRow(const VanityRecord& r, uint8_t* row) {
            memcpy(row, r.f, sizeof(r.f));
        }
    };

    // ---- stores ------------------------------------------------------------------------------------------------
    // A row: the record, and the strings a patch gave it (a loaded row's strings point into the store's block).
    template <class Record> struct Row {
        Record record;
        std::string own[Traits<Record>::kStrings ? Traits<Record>::kStrings : 1];
    };

    template <class Record> struct Store {
        bool loaded = false;
        uint32_t maxId = 0xFFFFFFFF;
        uint32_t minId = 0xFFFFFFF;
        std::vector<char> block;   // the file's string block
        size_t terminated = 0;     // offsets below this reach a NUL inside the block
        std::deque<Row<Record>> rows;   // stable addresses: rows are only appended
        // field offset -> key -> row indexes in row order (the original's server indexes, 0x6A)
        std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::vector<uint32_t>>> indexes;

        uint32_t Count() const {
            return static_cast<uint32_t>(this->rows.size());
        }
    };

    Store<CreatureRecord> s_creature;
    Store<QuestRecord> s_quest;
    Store<ItemAddonRecord> s_itemAddon;
    Store<VanityRecord> s_vanity;
    Store<SmallRecord> s_keystone;      // MythicKeystones
    Store<SmallRecord> s_spellAffect;   // SpellAffect

    // FUN_102121e0: the fields a quest group can be named in.
    const uint32_t kQuestGroupFields[] = { 8, 0xC, 0x10 };

    // FUN_101daee0 / FUN_101db1b0
    template <class Record> void Reset(Store<Record>& s) {
        s = Store<Record>();
    }

    // A string-block offset as the original's reads resolve it: past the block, or not NUL-terminated inside it, is "".
    template <class Record> const char* BlockString(const Store<Record>& s, uint32_t offset) {
        return offset < s.terminated ? s.block.data() + offset : "";
    }

    template <class Record> uint32_t Key(const Record& r, uint32_t field) {
        uint8_t row[Traits<Record>::kRowSize];
        Traits<Record>::ToRow(r, row);
        return U32(row, field);
    }

    // 0x6A: an index on a u32 field of a loaded table.
    template <class Record> void AddIndex(Store<Record>& s, uint32_t field) {
        if (!s.loaded || field > Traits<Record>::kRowSize - 4) {
            return;
        }

        auto& index = s.indexes[field];
        index.clear();

        for (uint32_t i = 0; i < s.Count(); ++i) {
            index[Key(s.rows[i].record, field)].push_back(i);
        }
    }

    // 0x6B: every row whose indexed field holds key, in row order; false when the field has no index.
    template <class Record> bool RowsWith(const Store<Record>& s, uint32_t field, uint32_t key, const std::vector<uint32_t>*& out) {
        static const std::vector<uint32_t> none;
        auto index = s.indexes.find(field);

        if (index == s.indexes.end()) {
            return false;
        }

        auto it = index->second.find(key);
        out = it == index->second.end() ? &none : &it->second;
        return true;
    }

    // FUN_101cf4c0 and siblings: the first row whose field holds key.
    template <class Record> bool FirstRow(const Store<Record>& s, uint32_t field, uint32_t key, uint32_t& index) {
        const std::vector<uint32_t>* rows = nullptr;

        if (!RowsWith(s, field, key, rows) || rows->empty()) {
            return false;
        }

        index = rows->front();
        return true;
    }

    // FUN_100b8cc0 / FUN_101a2690 / FUN_103298f0 (key field +4) and FUN_101d0bd0 / FUN_101d0500 / FUN_10329f60 (id,
    // field 0): the record whose `field` is `key`.
    template <class Record> const Record* Lookup(Store<Record>& s, uint32_t field, uint32_t key) {
        uint32_t index = 0;
        return FirstRow(s, field, key, index) ? &s.rows[index].record : nullptr;
    }

    template <class Record> const Record* LookupByKey(Store<Record>& s, uint32_t key) {
        return Lookup(s, 4, key);
    }

    template <class Record> const Record* LookupById(Store<Record>& s, uint32_t id) {
        return Lookup(s, 0, id);
    }

    // Row `index` takes `r` (a null string reads as "", as the original's offset 0 does), and the indexes follow it.
    template <class Record> void SetRow(Store<Record>& s, uint32_t index, const Record& r) {
        Row<Record>& row = s.rows[index];

        for (auto& kv : s.indexes) {
            auto it = kv.second.find(Key(row.record, kv.first));

            if (it != kv.second.end()) {
                it->second.erase(std::remove(it->second.begin(), it->second.end(), index), it->second.end());

                if (it->second.empty()) {
                    kv.second.erase(it);
                }
            }
        }

        std::string text[Traits<Record>::kStrings ? Traits<Record>::kStrings : 1];
        Record copy = r;

        for (size_t k = 0; k < Traits<Record>::kStrings; ++k) {
            const char* src = Traits<Record>::Slot(copy, k);
            text[k] = src ? src : "";
        }

        row.record = copy;

        for (size_t k = 0; k < Traits<Record>::kStrings; ++k) {
            row.own[k] = std::move(text[k]);
            Traits<Record>::Slot(row.record, k) = row.own[k].c_str();
        }

        for (auto& kv : s.indexes) {
            std::vector<uint32_t>& rows = kv.second[Key(row.record, kv.first)];
            rows.insert(std::lower_bound(rows.begin(), rows.end(), index), index);
        }
    }

    // A new last row holding `r` (not yet in any index, so SetRow's removal finds nothing for it).
    template <class Record> void AppendRow(Store<Record>& s, const Record& r) {
        s.rows.emplace_back();
        SetRow(s, s.Count() - 1, r);
    }

    // FUN_101fd240 / FUN_101fded0
    template <class Record> void Load(Store<Record>& s, const char* path) {
        using T = Traits<Record>;
        Reset(s);
        auto start = std::chrono::steady_clock::now();
        auto ms = [&start]() { return static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()); };
        std::vector<uint8_t> file;
        bool read = CoA::Dbc::ReadFile(path, file);
        CoA::Log::Printf("BridgeStore: %s read %u bytes in %lld ms", path, static_cast<unsigned>(file.size()), ms());

        if (!read || file.size() < 20) {
            return;
        }

        uint32_t magic = U32(file.data(), 0);
        uint32_t count = U32(file.data(), 4);
        uint32_t stringSize = U32(file.data(), 0x10);

        if (magic != 0x43424457 || count == 0 || U32(file.data(), 8) != T::kFileFields || U32(file.data(), 0xC) != T::kFileRowSize ||
            file.size() - 20 < static_cast<uint64_t>(count) * T::kFileRowSize + stringSize) {
            return;
        }

        const char* strings = reinterpret_cast<const char*>(file.data()) + 20 + static_cast<size_t>(count) * T::kFileRowSize;
        s.block.assign(strings, strings + stringSize);

        for (size_t end = s.block.size(); end > 0; --end) {
            if (s.block[end - 1] == '\0') {
                s.terminated = end;
                break;
            }
        }

        s.maxId = 0;
        s.minId = 0xFFFFFFF;
        uint32_t slot = CoA::Dbc::LocaleSlot();
        uint8_t row[T::kRowSize];

        for (uint32_t i = 0; i < count; ++i) {
            const uint8_t* src = file.data() + 20 + static_cast<size_t>(i) * T::kFileRowSize;
            uint32_t at = 0;

            for (int part : T::Plan()) {
                if (part == kLoc) {
                    memcpy(row + at, src + slot * 4, 4);
                    src += 17 * 4;
                    at += 4;
                } else {
                    memcpy(row + at, src, part * 4);
                    src += part * 4;
                    at += part * 4;
                }
            }

            Row<Record>& r = s.rows.emplace_back();
            T::FromRow(row, r.record);

            for (size_t k = 0; k < T::kStrings; ++k) {
                T::Slot(r.record, k) = BlockString(s, U32(row, T::StringFields()[k]));
            }

            uint32_t id = U32(row, 0);
            s.maxId = std::max(s.maxId, id);
            s.minId = std::min(s.minId, id);
        }

        s.loaded = true;
        AddIndex(s, 0);
        CoA::Log::Printf("BridgeStore: %s %u rows in %lld ms", path, count, ms());
    }

    // FUN_102094b0 / FUN_10209860 / FUN_102096d0 / FUN_102099a0 / FUN_1032eae0: overwrite `existing`'s row with `r`,
    // or append `r` (refused when its id is already in the table). MMgr64.exe refused the row write (0x73), so in the
    // original a patch never took effect; here it does.
    template <class Record> bool PatchRecord(Store<Record>& s, const Record* existing, const Record& r) {
        using T = Traits<Record>;

        if (!s.loaded) {
            return false;
        }

        uint32_t index = 0;

        if (!existing) {
            if (FirstRow(s, 0, T::Id(r), index)) {
                return false;
            }

            AppendRow(s, r);
        } else {
            if (!FirstRow(s, 0, T::Id(*existing), index)) {
                return false;
            }

            SetRow(s, index, r);
        }

        s.minId = std::min(s.minId, T::Id(r));
        s.maxId = std::max(s.maxId, T::Id(r));
        return true;
    }
}

const CoA::BridgeStore::CreatureRecord* CoA::BridgeStore::CreatureByEntry(uint32_t entry) {
    return LookupByKey(s_creature, entry);
}

const uint8_t* CoA::BridgeStore::QuestById(uint32_t id) {
    const QuestRecord* r = LookupById(s_quest, id);
    return r ? reinterpret_cast<const uint8_t*>(r->f) : nullptr;
}

// FUN_102123e0 with FUN_102127c0: the rows whose group field holds `group` (sorted, unique), in row order; empty when a
// group field has no index.
std::vector<const uint8_t*> CoA::BridgeStore::QuestsInGroup(uint32_t group) {
    std::vector<const uint8_t*> out;
    std::vector<uint32_t> indexes;

    for (uint32_t field : kQuestGroupFields) {
        const std::vector<uint32_t>* rows = nullptr;

        if (!RowsWith(s_quest, field, group, rows)) {
            return out;
        }

        indexes.insert(indexes.end(), rows->begin(), rows->end());
    }

    std::sort(indexes.begin(), indexes.end());
    indexes.erase(std::unique(indexes.begin(), indexes.end()), indexes.end());

    for (uint32_t index : indexes) {
        out.push_back(reinterpret_cast<const uint8_t*>(s_quest.rows[index].record.f));
    }

    return out;
}

// FUN_10209860 (with FUN_101e2a30's lookup of the current record).
bool CoA::BridgeStore::PatchQuest(const uint8_t* row) {
    QuestRecord r;
    memcpy(r.f, row, sizeof(r.f));
    const QuestRecord* found = LookupById(s_quest, r.f[0]);
    QuestRecord existing;

    if (found) {
        existing = *found;
    }

    return PatchRecord(s_quest, found ? &existing : nullptr, r);
}

uint32_t CoA::BridgeStore::VanityCount() {
    return s_vanity.Count();
}

// FUN_1032ec60's store part: the record with this id (FUN_10329f60), then FUN_1032eae0.
bool CoA::BridgeStore::PatchVanity(const VanityRecord& r) {
    const VanityRecord* found = LookupById(s_vanity, r.f[0]);
    VanityRecord existing;

    if (found) {
        existing = *found;
    }

    return PatchRecord(s_vanity, found ? &existing : nullptr, r);
}

// FUN_1032dc10: {index, found, record copy} for each index, in order.
bool CoA::BridgeStore::VanityRecords(const uint32_t* indexes, uint32_t n, std::vector<VanityEntry>& out) {
    out.clear();

    for (uint32_t i = 0; i < n; ++i) {
        VanityEntry v = {};
        v.index = indexes[i];

        if (v.index < s_vanity.Count()) {
            v.record = s_vanity.rows[v.index].record;
            v.found = true;
        }

        out.push_back(v);
    }

    return true;
}

// FUN_1032e090: the record at `index` while it still holds `itemId`, else the record for `itemId`.
const CoA::BridgeStore::VanityRecord* CoA::BridgeStore::VanityAt(uint32_t index, uint32_t itemId) {
    if (index < s_vanity.Count() && s_vanity.rows[index].record.f[1] == itemId) {
        return &s_vanity.rows[index].record;
    }

    return LookupByKey(s_vanity, itemId);
}

bool CoA::BridgeStore::KeystoneById(uint32_t id, uint32_t out[3]) {
    const SmallRecord* r = LookupById(s_keystone, id);

    if (!r) {
        return false;
    }

    memcpy(out, r->f, sizeof(r->f));
    return true;
}

// SMSG 0x56D (FUN_101e24d0): a known keystone is overwritten. The original wrote only its cached copy, which an
// eviction reverted; here the row keeps it. An unknown key is appended (FUN_101da590).
void CoA::BridgeStore::PatchKeystone(const uint32_t rec[3]) {
    if (!s_keystone.loaded) {
        return;
    }

    SmallRecord r;
    memcpy(r.f, rec, sizeof(r.f));
    uint32_t index = 0;

    if (FirstRow(s_keystone, 0, rec[0], index)) {
        SetRow(s_keystone, index, r);
        return;
    }

    AppendRow(s_keystone, r);
    s_keystone.minId = std::min(s_keystone.minId, rec[0]);
    s_keystone.maxId = std::max(s_keystone.maxId, rec[0]);
}

// FUN_103234d0: the rows whose modifier field (+4) is `spell`, in row order.
std::vector<std::array<uint32_t, 3>> CoA::BridgeStore::SpellAffectsFor(uint32_t spell) {
    std::vector<std::array<uint32_t, 3>> out;
    const std::vector<uint32_t>* rows = nullptr;

    if (!RowsWith(s_spellAffect, 4, spell, rows)) {
        return out;
    }

    for (uint32_t index : *rows) {
        const SmallRecord& r = s_spellAffect.rows[index].record;
        out.push_back({ r.f[0], r.f[1], r.f[2] });
    }

    return out;
}

// FUN_101d3d20, bridged part: MythicKeystones and SpellAffect first (0x101D3D7A, 0x101D4338).
void CoA::BridgeStore::LoadTables() {
    Load(s_keystone, "DBFilesClient\\MythicKeystones.dbc");
    Load(s_spellAffect, "DBFilesClient\\SpellAffect.dbc");
    Load(s_creature, Traits<CreatureRecord>::kPath);
    Load(s_quest, Traits<QuestRecord>::kPath);
    Load(s_itemAddon, Traits<ItemAddonRecord>::kPath);
    Load(s_vanity, Traits<VanityRecord>::kPath);
}

// FUN_101d3d20 0x101D9BCD..0x101D9D59: the item key (+4) on ItemAddon, VanityCollection and Creature, SpellAffect's
// modifier field and the quest group fields (each only on a loaded table).
void CoA::BridgeStore::BuildIndexes() {
    AddIndex(s_itemAddon, 4);
    AddIndex(s_vanity, 4);
    AddIndex(s_creature, 4);
    AddIndex(s_spellAffect, 4);

    for (uint32_t field : kQuestGroupFields) {
        AddIndex(s_quest, field);
    }
}

void CoA::BridgeStore::Reset() {
    ::Reset(s_creature);
    ::Reset(s_quest);
    ::Reset(s_itemAddon);
    ::Reset(s_vanity);
    ::Reset(s_keystone);
    ::Reset(s_spellAffect);
}

// SMSG 0x56F (FUN_101e2a30): one whole Quest.dbc row.
int32_t CoA::BridgeStore::OnPatchQuest(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    uint8_t row[0x74];
    CoA::Read::Data(msg, row, sizeof(row));
    CoA::BridgeStore::PatchQuest(row);
    return 1;
}

// SMSG 0x569 (FUN_101e1800): u32 item, 8 bytes not read, 12 u32, u32-length name, u32-length description. The record
// keeps the existing row id, or takes the largest id + 1.
int32_t CoA::BridgeStore::OnPatchItemAddon(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    ItemAddonRecord r = {};
    r.id = CoA::Read::U32(msg);
    CoA::Read::U64(msg);

    for (uint32_t& f : r.fields) {
        f = CoA::Read::U32(msg);
    }

    std::string name = CoA::Read::Name(msg);
    std::string description = CoA::Read::Name(msg);
    r.name = name.c_str();
    r.description = description.c_str();
    const ItemAddonRecord* found = LookupByKey(s_itemAddon, r.id);
    ItemAddonRecord existing;

    if (found) {
        existing = *found;
    }

    r.rowId = found ? existing.rowId : s_itemAddon.maxId + 1;
    // On success the original refreshes its item caches (not transcribed; MMgr64.exe refused 0x73, so it never did).
    PatchRecord(s_itemAddon, found ? &existing : nullptr, r);
    return 1;
}

// SMSG 0x567 (FUN_101e14f0): u32 entry, 4 bytes not read, 4 u32, u32-length name. A known entry is overwritten, its id
// becoming 0 (the original wrote only its cached copy, which an eviction reverted; here the row keeps it). An unknown
// entry becomes a record with the next id, appended.
int32_t CoA::BridgeStore::OnPatchCreature(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    uint32_t entry = CoA::Read::U32(msg);
    CoA::Read::U32(msg);
    uint32_t f[4];

    for (uint32_t& v : f) {
        v = CoA::Read::U32(msg);
    }

    std::string name = CoA::Read::Name(msg);
    uint32_t index = 0;

    if (FirstRow(s_creature, 4, entry, index)) {
        SetRow(s_creature, index, CreatureRecord{ 0, entry, name.c_str(), f[0], f[1], f[2], f[3] });
        return 1;
    }

    CreatureRecord r = { s_creature.maxId + 1, entry, name.c_str(), f[0], f[1], f[2], f[3] };
    PatchRecord(s_creature, static_cast<const CreatureRecord*>(nullptr), r);
    return 1;
}

// SMSG 0x574 (FUN_101e3790): 3 u32; patches the SpellAffect record with that id, else appends it.
int32_t CoA::BridgeStore::OnPatchSpellAffect(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    SmallRecord r;

    for (uint32_t& v : r.f) {
        v = CoA::Read::U32(msg);
    }

    const SmallRecord* found = LookupById(s_spellAffect, r.f[0]);
    SmallRecord existing;

    if (found) {
        existing = *found;
    }

    PatchRecord(s_spellAffect, found ? &existing : nullptr, r);
    return 1;
}

void CoA::BridgeStore::Init() {
    COA_PACKET(0x56F, CoA::BridgeStore::OnPatchQuest);
    COA_PACKET(0x569, CoA::BridgeStore::OnPatchItemAddon);
    COA_PACKET(0x567, CoA::BridgeStore::OnPatchCreature);
    COA_PACKET(0x574, CoA::BridgeStore::OnPatchSpellAffect);
}

const CoA::ItemAddon::Record* CoA::ItemAddon::Find(uint32_t itemId) {
    return LookupByKey(s_itemAddon, itemId);
}

// FUN_1020ef10: the unique items' records by item (+4); false when the table has no item index.
bool CoA::ItemAddon::FetchInfo(const uint32_t* items, uint32_t n, std::unordered_map<uint32_t, Info>& out) {
    out.clear();

    if (n == 0) {
        return true;
    }

    if (s_itemAddon.indexes.find(4) == s_itemAddon.indexes.end()) {
        return false;
    }

    std::unordered_set<uint32_t> seen;

    for (uint32_t i = 0; i < n; ++i) {
        if (!seen.insert(items[i]).second) {
            continue;
        }

        if (const Record* r = Find(items[i])) {
            Info& info = out[items[i]];   // FUN_1020df00
            info.name = r->name;
            info.description = r->description;
            info.quality = r->fields[0];
            info.flags = r->fields[1];
        }
    }

    return true;
}

CoA::ItemAddon::Info CoA::ItemAddon::GetInfo(uint32_t itemId) {
    std::unordered_map<uint32_t, Info> batch;

    if (FetchInfo(&itemId, 1, batch)) {
        auto it = batch.find(itemId);

        if (it != batch.end()) {
            return it->second;
        }
    }

    Info info;

    if (const Record* r = Find(itemId)) {
        info.name = r->name ? r->name : "";
        info.description = r->description ? r->description : "";
        info.quality = r->fields[0];
        info.flags = r->fields[1];
    }

    return info;
}
