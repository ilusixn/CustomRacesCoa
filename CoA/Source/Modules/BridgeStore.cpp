// AscBridgeStore.cpp -- see BridgeStore.h.
//
//   Loader (FUN_101fd240 Creature, FUN_101fded0 Quest, and their siblings): WDBC with the expected field count and
//   row size -> CreateTable(row size, count) -> rows packed (a locstring becomes its locale slot's offset), tracking
//   the largest / smallest id -> string block via 0x68 at offset 0 -> records via 0x66 -> server index 0x6A(table, 0).
//   Any failure resets the store.
//
//   Cache (FUN_100bf700): at most 0x400 records and 0x200000 bytes, evicted least-recently-used first; a (field
//   offset, key) -> record index of at most 0x1000 entries, trimmed oldest first and purged of a record when it is
//   evicted.
#include "CoA/Source/Modules/BridgeStore.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/MemoryBridge.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <list>
#include <unordered_set>

namespace {
    using CoA::BridgeStore::CreatureRecord;
    using CoA::BridgeStore::VanityRecord;
    using ItemAddonRecord = CoA::ItemAddon::Record;
    namespace Bridge = CoA::MemoryBridge;

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

    char* Copy(const char* s, size_t n) {
        char* out = static_cast<char*>(malloc(n + 1));
        memcpy(out, s, n);
        out[n] = '\0';
        return out;
    }

    uint32_t Len(const char* s) {
        return s ? static_cast<uint32_t>(strlen(s)) + 1 : 0;
    }

    // A file row as table-row dwords: n > 0 copies n dwords, kLoc keeps one locstring's locale slot.
    constexpr int kLoc = -1;

    // ---- the records -------------------------------------------------------------------------------------------
    // Per record: its table row (the original's 32-bit record, string fields as string-block offsets), the file it
    // loads from, the string fields a reply carries (DAT_10BDB458 / DAT_10BDEDCC / DAT_10D3C154; Quest's and the
    // small tables' lists are never filled), and what a cached copy costs against the 2 MB budget.
    template <class Record> struct Traits;

    template <> struct Traits<CreatureRecord> {
        static constexpr uint32_t kRowSize = 0x1C;
        static constexpr uint32_t kFileFields = 0x17;
        static constexpr uint32_t kFileRowSize = 0x5C;
        static constexpr const char* kPath = "DBFilesClient\\Creature.dbc";
        static const std::vector<int>& Plan() { static const std::vector<int> p = { 2, kLoc, 4 }; return p; }
        static const std::vector<uint32_t>& StringFields() { static const std::vector<uint32_t> f = { 8 }; return f; }
        static const char*& Slot(CreatureRecord& r, size_t) { return r.name; }
        static uint32_t Id(const CreatureRecord& r) { return r.id; }
        static uint32_t Cost(const CreatureRecord& r) { return 0x1C + Len(r.name); }   // FUN_100c5c40

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
        static constexpr const char* kPath = "DBFilesClient\\Quest.dbc";
        static const std::vector<int>& Plan() { static const std::vector<int> p = { 0x1D }; return p; }
        static const std::vector<uint32_t>& StringFields() { static const std::vector<uint32_t> f; return f; }
        static const char*& Slot(QuestRecord&, size_t) { static const char* none = nullptr; return none; }
        static uint32_t Id(const QuestRecord& r) { return r.f[0]; }
        static uint32_t Cost(const QuestRecord&) { return 0x74; }   // FUN_101e6da0
        static void FromRow(const uint8_t* row, QuestRecord& r) { memcpy(r.f, row, sizeof(r.f)); }
        static void ToRow(const QuestRecord& r, uint8_t* row) { memcpy(row, r.f, sizeof(r.f)); }
    };

    template <> struct Traits<SmallRecord> {
        static constexpr uint32_t kRowSize = 0xC;
        static constexpr uint32_t kFileFields = 3;
        static constexpr uint32_t kFileRowSize = 0xC;
        static const std::vector<int>& Plan() { static const std::vector<int> p = { 3 }; return p; }
        static const std::vector<uint32_t>& StringFields() { static const std::vector<uint32_t> f; return f; }
        static const char*& Slot(SmallRecord&, size_t) { static const char* none = nullptr; return none; }
        static uint32_t Id(const SmallRecord& r) { return r.f[0]; }
        static uint32_t Cost(const SmallRecord&) { return 0xC; }   // FUN_101e6c20
        static void FromRow(const uint8_t* row, SmallRecord& r) { memcpy(r.f, row, sizeof(r.f)); }
        static void ToRow(const SmallRecord& r, uint8_t* row) { memcpy(row, r.f, sizeof(r.f)); }
    };

    template <> struct Traits<ItemAddonRecord> {
        static constexpr uint32_t kRowSize = 0x40;
        static constexpr uint32_t kFileFields = 0x30;
        static constexpr uint32_t kFileRowSize = 0xC0;
        static constexpr const char* kPath = "DBFilesClient\\ItemAddon.dbc";
        static const std::vector<int>& Plan() { static const std::vector<int> p = { 2, kLoc, kLoc, 12 }; return p; }
        static const std::vector<uint32_t>& StringFields() { static const std::vector<uint32_t> f = { 8, 0xC }; return f; }
        static const char*& Slot(ItemAddonRecord& r, size_t k) { return k == 0 ? r.name : r.description; }
        static uint32_t Id(const ItemAddonRecord& r) { return r.rowId; }
        static uint32_t Cost(const ItemAddonRecord& r) { return 0x40 + Len(r.name) + Len(r.description); }   // FUN_101a59b0

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
        static constexpr const char* kPath = "DBFilesClient\\VanityCollection.dbc";
        // FUN_1021a230: columns 0..41 in order, two locstrings, column 76.
        static const std::vector<int>& Plan() { static const std::vector<int> p = { 0x2A, kLoc, kLoc, 1 }; return p; }
        static const std::vector<uint32_t>& StringFields() { static const std::vector<uint32_t> f = { 0xC, 0xA8, 0xAC }; return f; }
        static const char*& Slot(VanityRecord& r, size_t k) { return r.strings[k]; }
        static uint32_t Id(const VanityRecord& r) { return r.f[0]; }
        static uint32_t Cost(const VanityRecord& r) { return 0xB4 + Len(r.strings[0]) + Len(r.strings[1]) + Len(r.strings[2]); }   // FUN_1032e760

        static void FromRow(const uint8_t* row, VanityRecord& r) {
            memcpy(r.f, row, sizeof(r.f));
            r.strings[0] = r.strings[1] = r.strings[2] = nullptr;
        }

        static void ToRow(const VanityRecord& r, uint8_t* row) {
            memcpy(row, r.f, sizeof(r.f));
        }
    };

    template <class Record> void Release(Record& r) {
        for (size_t k = 0; k < Traits<Record>::StringFields().size(); ++k) {
            free(const_cast<char*>(Traits<Record>::Slot(r, k)));
        }
    }

    // ---- reply readers -----------------------------------------------------------------------------------------
    // The original's readers throw on a short reply (FUN_100be440 / FUN_100be490); here the read fails.
    struct Reply {
        const std::vector<uint8_t>& b;
        size_t pos = 0;
        bool ok = true;

        uint32_t U32() {
            if (b.size() - pos < 4) {
                this->ok = false;
                return 0;
            }

            uint32_t v;
            memcpy(&v, b.data() + pos, 4);
            pos += 4;
            return v;
        }

        uint8_t U8() {
            if (pos >= b.size()) {
                this->ok = false;
                return 0;
            }

            return b[pos++];
        }

        const uint8_t* Take(size_t n) {
            if (b.size() - pos < n) {
                this->ok = false;
                return nullptr;
            }

            const uint8_t* p = b.data() + pos;
            pos += n;
            return p;
        }
    };

    // FUN_100c2380: a record's MSTB string table, n x {u32 row, u32 field, u32 string offset, u32 data offset, u32
    // length}, then the data. False when the header is not MSTB 1 or an entry runs past the data.
    struct StringEntry {
        uint32_t row, field, stringOffset, offset, length;
    };

    struct StringTable {
        std::vector<StringEntry> entries;
        std::vector<char> data;
        std::unordered_map<uint64_t, const StringEntry*> byKey;   // (row << 32 | field)
    };

    bool ParseStrings(Reply& in, StringTable& t) {
        if (in.U32() != 0x4254534D || in.U32() != 1 || !in.ok) {
            return false;
        }

        t.entries.resize(in.U32());

        for (StringEntry& e : t.entries) {
            const uint8_t* p = in.Take(sizeof(StringEntry));

            if (!p) {
                return false;
            }

            memcpy(&e, p, sizeof(StringEntry));
        }

        uint32_t size = in.U32();
        const uint8_t* data = in.Take(size);

        if (!in.ok) {
            return false;
        }

        t.data.assign(data, data + size);

        for (const StringEntry& e : t.entries) {
            if (e.offset > t.data.size() || e.length > t.data.size() - e.offset) {
                return false;
            }

            t.byKey[(static_cast<uint64_t>(e.row) << 32) | e.field] = &e;
        }

        return true;
    }

    // ---- the cache (FUN_100bf700 and friends) ------------------------------------------------------------------
    template <class Record> class RecordCache {
    public:
        // FUN_100c3910
        Record* Get(uint32_t index) {
            auto it = this->m_records.find(index);

            if (it == this->m_records.end()) {
                return nullptr;
            }

            this->m_lru.splice(this->m_lru.begin(), this->m_lru, it->second.lru);
            return it->second.record;
        }

        // FUN_100c5c40: takes ownership; nullptr when the record was evicted straight away.
        Record* Insert(uint32_t index, Record* record) {
            this->EvictIndex(index);
            uint32_t cost = Traits<Record>::Cost(*record);
            this->m_lru.push_front(index);
            this->m_bytes += cost;
            this->m_records[index] = Slot{ record, cost, this->m_lru.begin() };

            while ((kMaxRecords < this->m_records.size() || kMaxBytes < this->m_bytes) && !this->m_lru.empty()) {
                auto victim = this->m_records.find(this->m_lru.back());

                if (victim == this->m_records.end()) {
                    this->m_lru.pop_back();
                } else {
                    this->Evict(victim);
                }
            }

            auto it = this->m_records.find(index);
            return it == this->m_records.end() ? nullptr : it->second.record;
        }

        // FUN_100c19c0: only for a cached record; a key already present moves to the front.
        void Index(uint32_t field, uint32_t key, uint32_t index) {
            if (this->m_records.find(index) == this->m_records.end()) {
                return;
            }

            uint64_t k = IndexKey(field, key);
            auto old = this->m_index.find(k);

            if (old != this->m_index.end()) {
                this->Unindex(old);
            }

            this->m_indexLru.push_front(k);
            this->m_index[k] = IndexSlot{ index, this->m_indexLru.begin() };

            while (kMaxIndex < this->m_index.size() && !this->m_indexLru.empty()) {   // FUN_100cba60
                auto it = this->m_index.find(this->m_indexLru.back());

                if (it == this->m_index.end()) {
                    this->m_indexLru.pop_back();
                } else {
                    this->Unindex(it);
                }
            }
        }

        // The index probe at the top of FUN_100b8cc0 / FUN_101d0bd0: a stale key is dropped.
        Record* FindIndexed(uint32_t field, uint32_t key) {
            auto it = this->m_index.find(IndexKey(field, key));

            if (it != this->m_index.end()) {
                this->m_indexLru.splice(this->m_indexLru.begin(), this->m_indexLru, it->second.lru);

                if (Record* r = this->Get(it->second.index)) {
                    return r;
                }

                this->Unindex(it);
            }

            return nullptr;
        }

        // FUN_101dbdc0
        void EvictIndex(uint32_t index) {
            auto it = this->m_records.find(index);

            if (it != this->m_records.end()) {
                this->Evict(it);
            }
        }

        // FUN_10209860's walk: the first cached record with this id.
        void EvictId(uint32_t id) {
            for (auto it = this->m_records.begin(); it != this->m_records.end(); ++it) {
                if (Traits<Record>::Id(*it->second.record) == id) {
                    this->Evict(it);
                    return;
                }
            }
        }

        void Clear() {
            while (!this->m_records.empty()) {
                this->Evict(this->m_records.begin());
            }

            this->m_lru.clear();
            this->m_index.clear();
            this->m_indexLru.clear();
            this->m_bytes = 0;
        }

    private:
        static constexpr size_t kMaxRecords = 0x400;
        static constexpr size_t kMaxBytes = 0x200000;
        static constexpr size_t kMaxIndex = 0x1000;

        struct Slot {
            Record* record;
            uint32_t cost;
            std::list<uint32_t>::iterator lru;
        };

        struct IndexSlot {
            uint32_t index;
            std::list<uint64_t>::iterator lru;
        };

        using RecordIt = typename std::unordered_map<uint32_t, Slot>::iterator;
        using IndexIt = typename std::unordered_map<uint64_t, IndexSlot>::iterator;

        static uint64_t IndexKey(uint32_t field, uint32_t key) {
            return (static_cast<uint64_t>(key) << 32) | field;
        }

        void Unindex(IndexIt it) {
            this->m_indexLru.erase(it->second.lru);
            this->m_index.erase(it);
        }

        // FUN_100c25c0
        void Evict(RecordIt it) {
            uint32_t index = it->first;
            Release(*it->second.record);
            delete it->second.record;
            this->m_bytes -= std::min<size_t>(it->second.cost, this->m_bytes);
            this->m_lru.erase(it->second.lru);
            this->m_records.erase(it);

            for (auto i = this->m_index.begin(); i != this->m_index.end();) {
                if (i->second.index == index) {
                    this->m_indexLru.erase(i->second.lru);
                    i = this->m_index.erase(i);
                } else {
                    ++i;
                }
            }
        }

        std::unordered_map<uint32_t, Slot> m_records;
        std::list<uint32_t> m_lru;
        std::unordered_map<uint64_t, IndexSlot> m_index;
        std::list<uint64_t> m_indexLru;
        size_t m_bytes = 0;
    };

    // ---- stores and the loader ---------------------------------------------------------------------------------
    template <class Record> struct Store {
        uint32_t loaded = 0;
        uint32_t count = 0;
        uint32_t maxId = 0xFFFFFFFF;
        uint32_t minId = 0xFFFFFFF;
        uint32_t table = 0;
        uint32_t stringSize = 0;
        RecordCache<Record> cache;
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
        if (s.table) {
            Bridge::DestroyTable(s.table);
        }

        s.table = 0;
        s.stringSize = 0;
        s.cache.Clear();
        s.loaded = 0;
        s.count = 0;
        s.maxId = 0xFFFFFFFF;
        s.minId = 0xFFFFFFF;
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

        s.table = Bridge::CreateTable(T::kRowSize, count);

        if (!s.table) {
            return;
        }

        s.count = count;
        s.stringSize = stringSize;
        s.maxId = 0;
        s.minId = 0xFFFFFFF;
        uint32_t slot = CoA::Dbc::LocaleSlot();
        std::vector<uint8_t> rows(static_cast<size_t>(count) * T::kRowSize);

        for (uint32_t i = 0; i < count; ++i) {
            const uint8_t* src = file.data() + 20 + static_cast<size_t>(i) * T::kFileRowSize;
            uint8_t* row = rows.data() + static_cast<size_t>(i) * T::kRowSize;
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

            uint32_t id = U32(row, 0);
            s.maxId = std::max(s.maxId, id);
            s.minId = std::min(s.minId, id);
        }

        const uint8_t* strings = file.data() + 20 + static_cast<size_t>(count) * T::kFileRowSize;

        if (stringSize && !Bridge::Command68(s.table, 0, strings, stringSize)) {
            Reset(s);
            return;
        }

        // Without a string block the string fields are 0.
        if (!stringSize) {
            for (uint32_t i = 0; i < count; ++i) {
                for (uint32_t field : T::StringFields()) {
                    SetU32(rows.data() + static_cast<size_t>(i) * T::kRowSize, field, 0);
                }
            }
        }

        if (!Bridge::Command66(s.table, 0, rows.data(), static_cast<uint32_t>(rows.size()))) {
            Reset(s);
            return;
        }

        Bridge::Command6A(s.table, 0);
        s.loaded = 1;
        CoA::Log::Printf("BridgeStore: %s %u rows in %lld ms", path, count, ms());
    }

    // ---- keyed lookup ------------------------------------------------------------------------------------------
    // FUN_100c18f0 -> FUN_100b8440 / FUN_101a3940: each string field in turn, from the reply's string table (entry
    // (row, field)), else fetched from the bridge (0x6C at the string-block offset the row holds). Every string is a
    // malloc'd copy. The first failed fetch frees the copies made so far and fails the record.
    template <class Record> bool ResolveStrings(Record& r, const uint8_t* row, const StringTable* strings, uint32_t index, uint32_t table) {
        std::vector<char*> made;
        const auto& fields = Traits<Record>::StringFields();

        for (size_t k = 0; k < fields.size(); ++k) {
            const char*& slot = Traits<Record>::Slot(r, k);

            if (strings) {
                auto it = strings->byKey.find((static_cast<uint64_t>(index) << 32) | fields[k]);

                if (it != strings->byKey.end()) {
                    const StringEntry& e = *it->second;
                    const char* src = strings->data.data() + e.offset;
                    size_t n = e.length && src[e.length - 1] == '\0' ? e.length - 1 : e.length;
                    made.push_back(Copy(src, n));
                    slot = made.back();
                    continue;
                }
            }

            std::string text;

            if (!Bridge::Command6C(table, U32(row, fields[k]), text)) {
                for (char* m : made) {
                    free(m);
                }

                for (size_t j = 0; j < fields.size(); ++j) {
                    Traits<Record>::Slot(r, j) = nullptr;
                }

                return false;
            }

            made.push_back(Copy(text.data(), text.size() - 1));
            slot = made.back();
        }

        return true;
    }

    // A reply's record, its strings resolved; nullptr on a short or malformed reply.
    template <class Record> Record* ReadRecord(Store<Record>& s, Reply& in, uint32_t index) {
        const uint8_t* row = in.Take(Traits<Record>::kRowSize);

        if (!row) {
            return nullptr;
        }

        Record r;
        Traits<Record>::FromRow(row, r);

        if (!Traits<Record>::StringFields().empty()) {
            StringTable strings;

            if (!ParseStrings(in, strings) || !ResolveStrings(r, row, &strings, index, s.table)) {
                return nullptr;
            }
        }

        return new Record(r);
    }

    // FUN_100b8cc0 / FUN_101a2690 / FUN_103298f0 (key field +4) and FUN_101d0bd0 / FUN_101d0500 / FUN_10329f60 (id,
    // field 0): the record whose `field` is `key`. 0x6D on that field with the record's string fields as extras;
    // unless the row is cached, the reply's record and string table are consumed exactly.
    template <class Record> const Record* Lookup(Store<Record>& s, uint32_t field, uint32_t key) {
        if (!s.table) {
            return nullptr;
        }

        if (Record* r = s.cache.FindIndexed(field, key)) {
            return r;
        }

        std::vector<uint8_t> out;

        if (!Bridge::Command6D(s.table, field, { key }, out, Traits<Record>::StringFields())) {
            return nullptr;
        }

        Reply in{ out };

        if (in.U32() == 0) {
            return nullptr;
        }

        uint32_t replyKey = in.U32();
        uint8_t found = in.U8();

        if (!in.ok || !found || replyKey != key) {
            return nullptr;
        }

        uint32_t index = in.U32();

        if (!in.ok || index >= s.count) {
            return nullptr;
        }

        if (Record* r = s.cache.Get(index)) {
            s.cache.Index(field, key, index);
            s.cache.Index(0, Traits<Record>::Id(*r), index);
            return r;
        }

        Record* r = ReadRecord(s, in, index);

        if (!r) {
            return nullptr;
        }

        if (in.pos != out.size()) {
            Release(*r);
            delete r;
            return nullptr;
        }

        Record* stored = s.cache.Insert(index, r);

        if (stored) {
            s.cache.Index(0, Traits<Record>::Id(*stored), index);
            s.cache.Index(field, key, index);
        }

        return stored;
    }

    template <class Record> const Record* LookupByKey(Store<Record>& s, uint32_t key) {
        return Lookup(s, 4, key);
    }

    template <class Record> const Record* LookupById(Store<Record>& s, uint32_t id) {
        return Lookup(s, 0, id);
    }

    // FUN_10212cd0 (Quest) / FUN_1032ef40 (Vanity): every index cached. Indexes past the count are skipped, cached
    // ones marked found, the rest go out as one 0x6F batch; the reply is consumed exactly. `found` is then refreshed
    // from the cache.
    template <class Record> bool FetchBatch(Store<Record>& s, std::vector<std::pair<uint32_t, bool>>& entries) {
        if (entries.empty() || s.count == 0 || !s.table) {
            return true;
        }

        std::vector<uint32_t> missing;

        for (auto& e : entries) {
            if (e.first < s.count) {
                if (s.cache.Get(e.first)) {
                    e.second = true;
                } else {
                    missing.push_back(e.first);
                }
            }
        }

        if (!missing.empty()) {
            std::vector<uint8_t> out;

            if (!Bridge::Command6F(s.table, missing, out, Traits<Record>::StringFields())) {
                return false;
            }

            Reply in{ out };
            uint32_t n = in.U32();

            for (uint32_t i = 0; i < n && in.ok; ++i) {
                uint32_t index = in.U32();

                if (!in.U8()) {
                    continue;
                }

                Record* r = ReadRecord(s, in, index);

                if (!r) {
                    return false;
                }

                if (Record* stored = s.cache.Insert(index, r)) {   // FUN_1020ac50 / FUN_1032e760
                    s.cache.Index(0, Traits<Record>::Id(*stored), index);
                }
            }

            if (!in.ok || in.pos != out.size()) {
                return false;
            }
        }

        for (auto& e : entries) {
            if (!e.second) {
                e.second = e.first < s.count && s.cache.Get(e.first) != nullptr;
            }
        }

        return true;
    }

    // FUN_101cf4c0 and siblings: the table index of the record whose id (+0) is `id` -- the first hit of 0x6B.
    template <class Record> bool IndexOf(Store<Record>& s, uint32_t id, uint32_t& index) {
        std::vector<uint32_t> hits;

        if (!s.table || !Bridge::Command6B(s.table, 0, id, hits) || hits.empty()) {
            return false;
        }

        index = hits[0];
        return true;
    }

    // FUN_10209ae0: the string block starts with one NUL; written first when the block is empty.
    template <class Record> bool EnsureStringBlock(Store<Record>& s) {
        if (s.stringSize != 0) {
            return true;
        }

        const char nul = 0;

        if (!Bridge::Command68(s.table, 0, &nul, 1)) {
            return false;
        }

        s.stringSize = 1;
        return true;
    }

    // FUN_101d3190 / FUN_1032bb10 / FUN_102094b0: each non-null string appended to the string block, its offset into
    // the row. A null string stays 0. The first failure fails the patch.
    template <class Record> bool AppendStrings(Store<Record>& s, Record& r, uint8_t* row) {
        const auto& fields = Traits<Record>::StringFields();

        for (size_t k = 0; k < fields.size(); ++k) {
            const char* text = Traits<Record>::Slot(r, k);
            SetU32(row, fields[k], 0);

            if (!text) {
                continue;
            }

            if (!EnsureStringBlock(s)) {
                return false;
            }

            uint32_t len = static_cast<uint32_t>(strlen(text)) + 1;

            if (len > ~s.stringSize) {
                return false;
            }

            uint32_t offset = s.stringSize;

            if (!Bridge::Command68(s.table, offset, text, len)) {
                return false;
            }

            s.stringSize += len;
            SetU32(row, fields[k], offset);
        }

        return true;
    }

    // FUN_102094b0 / FUN_10209860 / FUN_102096d0 / FUN_102099a0 / FUN_1032eae0: overwrite `existing`'s row with `r`,
    // or append `r` (refused when its id is already in the table). Strings go to the string block, then 0x73 writes
    // the row. MMgr64.exe refuses 0x73, so in the original a patch never took effect; the in-process server applies it
    // (MemoryBridge.h). The bookkeeping below is the original's.
    template <class Record> bool PatchRecord(Store<Record>& s, const Record* existing, Record r) {
        using T = Traits<Record>;

        if (!s.table) {
            return false;
        }

        uint32_t index = 0;
        uint32_t oldId = 0;

        if (!existing) {
            if (IndexOf(s, T::Id(r), index)) {
                return false;
            }

            index = s.count;
        } else {
            oldId = T::Id(*existing);

            if (!IndexOf(s, oldId, index)) {
                return false;
            }
        }

        uint8_t row[T::kRowSize];
        T::ToRow(r, row);

        if (!AppendStrings(s, r, row) || !Bridge::Command73(s.table, index, row, sizeof(row))) {
            return false;
        }

        s.cache.EvictIndex(index);

        if (!existing) {
            ++s.count;
        } else if (oldId != T::Id(r)) {
            s.cache.EvictId(oldId);
        }

        s.minId = std::min(s.minId, T::Id(r));
        s.maxId = std::max(s.maxId, T::Id(r));
        return true;
    }

    // FUN_1020d820: 0x6D on the ItemAddon table by item (no string fields): the rows, string fields still offsets.
    bool ReadItemAddonRows(const std::vector<uint32_t>& items, std::unordered_map<uint32_t, std::vector<uint8_t>>& out) {
        out.clear();

        if (items.empty()) {
            return true;
        }

        if (!s_itemAddon.table) {
            return false;
        }

        std::vector<uint8_t> reply;

        if (!Bridge::Command6D(s_itemAddon.table, 4, items, reply, {})) {
            return false;
        }

        Reply in{ reply };
        uint32_t n = in.U32();

        for (uint32_t i = 0; i < n && in.ok; ++i) {
            uint32_t item = in.U32();

            if (!in.U8()) {
                continue;
            }

            if (in.U32() >= s_itemAddon.count) {
                return false;
            }

            const uint8_t* row = in.Take(0x40);

            if (!row) {
                return false;
            }

            out[item].assign(row, row + 0x40);
        }

        return in.ok;
    }

    // FUN_1020e490: 0x6E over string-block offsets.
    bool ReadItemAddonStrings(const std::vector<uint32_t>& offsets, std::unordered_map<uint32_t, std::string>& out) {
        out.clear();

        if (offsets.empty()) {
            return true;
        }

        if (!s_itemAddon.table) {
            return false;
        }

        std::vector<uint8_t> reply;

        if (!Bridge::Command6E(s_itemAddon.table, offsets, reply)) {
            return false;
        }

        Reply in{ reply };
        uint32_t n = in.U32();

        for (uint32_t i = 0; i < n && in.ok; ++i) {
            uint32_t offset = in.U32();

            if (!in.U8()) {
                continue;
            }

            uint32_t len = in.U32();
            const uint8_t* text = in.Take(len);

            if (!text) {
                return false;
            }

            out[offset].assign(reinterpret_cast<const char*>(text), len);
        }

        return in.ok;
    }
}

const CoA::BridgeStore::CreatureRecord* CoA::BridgeStore::CreatureByEntry(uint32_t entry) {
    return LookupByKey(s_creature, entry);
}

const uint8_t* CoA::BridgeStore::QuestById(uint32_t id) {
    const QuestRecord* r = LookupById(s_quest, id);
    return r ? reinterpret_cast<const uint8_t*>(r->f) : nullptr;
}

// FUN_102123e0 with FUN_102127c0: the indexes whose group field holds `group` (sorted, unique), fetched, in index
// order; empty when any query fails.
std::vector<const uint8_t*> CoA::BridgeStore::QuestsInGroup(uint32_t group) {
    std::vector<const uint8_t*> out;

    if (!s_quest.table) {
        return out;
    }

    std::vector<uint32_t> indexes;

    for (uint32_t field : kQuestGroupFields) {
        std::vector<uint32_t> hits;

        if (!Bridge::Command6B(s_quest.table, field, group, hits)) {
            return out;
        }

        indexes.insert(indexes.end(), hits.begin(), hits.end());
    }

    std::sort(indexes.begin(), indexes.end());
    indexes.erase(std::unique(indexes.begin(), indexes.end()), indexes.end());
    std::vector<std::pair<uint32_t, bool>> entries;

    for (uint32_t index : indexes) {
        entries.emplace_back(index, false);
    }

    if (!FetchBatch(s_quest, entries)) {
        return out;
    }

    for (uint32_t index : indexes) {
        if (const QuestRecord* r = s_quest.cache.Get(index)) {
            out.push_back(reinterpret_cast<const uint8_t*>(r->f));
        }
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
    return s_vanity.count;
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

// FUN_1032dc10: {index, found, record copy} for each index, in order; false when the batch fails.
bool CoA::BridgeStore::VanityRecords(const uint32_t* indexes, uint32_t n, std::vector<VanityEntry>& out) {
    out.clear();
    std::vector<std::pair<uint32_t, bool>> entries;

    for (uint32_t i = 0; i < n; ++i) {
        entries.emplace_back(indexes[i], false);
    }

    if (!FetchBatch(s_vanity, entries)) {
        return false;
    }

    for (const auto& e : entries) {
        VanityEntry v = {};
        v.index = e.first;

        if (e.second) {
            if (const VanityRecord* r = s_vanity.cache.Get(e.first)) {
                v.record = *r;
                v.found = true;
            }
        }

        out.push_back(v);
    }

    return true;
}

// FUN_1032e090: the record at `index` while it still holds `itemId`, else the record for `itemId`.
const CoA::BridgeStore::VanityRecord* CoA::BridgeStore::VanityAt(uint32_t index, uint32_t itemId) {
    if (index < s_vanity.count && s_vanity.table) {
        const VanityRecord* r = s_vanity.cache.Get(index);

        if (!r) {
            std::vector<std::pair<uint32_t, bool>> one = { { index, false } };

            if (FetchBatch(s_vanity, one)) {
                r = s_vanity.cache.Get(index);
            }
        }

        if (r && r->f[1] == itemId) {
            return r;
        }
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

// SMSG 0x56D (FUN_101e24d0): a known keystone is overwritten in its CACHED record only (the table keeps the old
// row; eviction brings it back). An unknown key is appended (FUN_101da590): refused when field 0 already holds the
// key, else 0x73 at the end of the table (refused by MMgr64.exe, applied by the in-process server).
void CoA::BridgeStore::PatchKeystone(const uint32_t rec[3]) {
    if (SmallRecord* r = const_cast<SmallRecord*>(LookupById(s_keystone, rec[0]))) {
        memcpy(r->f, rec, sizeof(r->f));
        return;
    }

    if (!s_keystone.table) {
        return;
    }

    std::vector<uint32_t> hits;

    if (Bridge::Command6B(s_keystone.table, 0, rec[0], hits) && !hits.empty()) {
        return;
    }

    if (!Bridge::Command73(s_keystone.table, s_keystone.count, rec, 0xC)) {
        return;
    }

    s_keystone.cache.EvictIndex(s_keystone.count);
    ++s_keystone.count;
    s_keystone.minId = std::min(s_keystone.minId, rec[0]);
    s_keystone.maxId = std::max(s_keystone.maxId, rec[0]);
}

// FUN_103234d0: 0x6B on the modifier field for `spell`, 0x6F for the uncached rows, then each from the cache.
std::vector<std::array<uint32_t, 3>> CoA::BridgeStore::SpellAffectsFor(uint32_t spell) {
    std::vector<std::array<uint32_t, 3>> out;
    std::vector<uint32_t> indexes;

    if (!s_spellAffect.table || !Bridge::Command6B(s_spellAffect.table, 4, spell, indexes)) {
        return out;
    }

    std::vector<std::pair<uint32_t, bool>> entries;

    for (uint32_t index : indexes) {
        entries.emplace_back(index, false);
    }

    if (!FetchBatch(s_spellAffect, entries)) {
        return out;
    }

    for (uint32_t index : indexes) {
        if (const SmallRecord* r = s_spellAffect.cache.Get(index)) {
            out.push_back({ r->f[0], r->f[1], r->f[2] });
        }
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

// FUN_101d3d20 0x101D9BCD..0x101D9D59: the item key (+4) on ItemAddon, VanityCollection and Creature -- sent even when
// the table is 0, as the original -- then SpellAffect's modifier field and the quest group fields while their
// tables exist.
void CoA::BridgeStore::BuildIndexes() {
    Bridge::Command6A(s_itemAddon.table, 4);
    Bridge::Command6A(s_vanity.table, 4);
    Bridge::Command6A(s_creature.table, 4);

    if (s_spellAffect.table) {
        Bridge::Command6A(s_spellAffect.table, 4);
    }

    if (s_quest.table) {
        for (uint32_t field : kQuestGroupFields) {
            Bridge::Command6A(s_quest.table, field);
        }
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
    const ItemAddonRecord* existing = LookupByKey(s_itemAddon, r.id);
    r.rowId = existing ? existing->rowId : s_itemAddon.maxId + 1;
    // On success the original refreshes its item caches (not transcribed; MMgr64.exe refused 0x73, so it never did).
    PatchRecord(s_itemAddon, existing, r);
    return 1;
}

// SMSG 0x567 (FUN_101e14f0): u32 entry, 4 bytes not read, 4 u32, u32-length name. A known entry is overwritten in its
// CACHED record only, its id becoming 0. An unknown entry becomes a record with the next id, appended.
int32_t CoA::BridgeStore::OnPatchCreature(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
    uint32_t entry = CoA::Read::U32(msg);
    CoA::Read::U32(msg);
    uint32_t f[4];

    for (uint32_t& v : f) {
        v = CoA::Read::U32(msg);
    }

    std::string name = CoA::Read::Name(msg);

    if (CreatureRecord* r = const_cast<CreatureRecord*>(LookupByKey(s_creature, entry))) {
        free(const_cast<char*>(r->name));
        *r = { 0, entry, Copy(name.data(), name.size()), f[0], f[1], f[2], f[3] };
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

bool CoA::ItemAddon::FetchInfo(const uint32_t* items, uint32_t n, std::unordered_map<uint32_t, Info>& out) {
    out.clear();

    if (n == 0) {
        return true;
    }

    std::vector<uint32_t> unique;
    std::unordered_set<uint32_t> seen;

    for (uint32_t i = 0; i < n; ++i) {
        if (seen.insert(items[i]).second) {
            unique.push_back(items[i]);
        }
    }

    std::unordered_map<uint32_t, std::vector<uint8_t>> rows;

    if (!ReadItemAddonRows(unique, rows)) {
        return false;
    }

    std::vector<uint32_t> offsets;
    std::unordered_set<uint32_t> seenOffsets;

    for (const auto& r : rows) {
        for (uint32_t field : { 8u, 0xCu }) {
            uint32_t offset = U32(r.second.data(), field);

            if (seenOffsets.insert(offset).second) {
                offsets.push_back(offset);
            }
        }
    }

    std::unordered_map<uint32_t, std::string> strings;

    if (!ReadItemAddonStrings(offsets, strings)) {
        return false;
    }

    for (const auto& r : rows) {
        Info& info = out[r.first];   // FUN_1020df00
        auto name = strings.find(U32(r.second.data(), 8));
        auto description = strings.find(U32(r.second.data(), 0xC));
        info.name = name != strings.end() ? name->second : std::string();
        info.description = description != strings.end() ? description->second : std::string();
        info.quality = U32(r.second.data(), 0x10);
        info.flags = U32(r.second.data(), 0x14);
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
