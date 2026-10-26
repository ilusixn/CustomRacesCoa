#include "CoA/Source/Runtime/MemoryBridge.h"
#include <algorithm>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace {
    using CoA::MemoryBridge::Message;

    // MMgr64.exe sub_1400258A0: a serialized reply over 0xFF000 bytes (48 + blob) is refused with result 8.
    constexpr size_t kMaxReplyBlob = 0xFEFD0;
    // The client's BatchedQuery (FUN_102b35a0) sends at most this many keys at once.
    constexpr size_t kMaxBatch = 0x3FBF3;
    constexpr uint32_t kMstr = 0x5254534D;
    constexpr uint32_t kMstb = 0x4254534D;

    struct Table {
        uint32_t recordSize = 0;
        uint32_t count = 0;
        std::vector<uint8_t> records;
        std::vector<uint8_t> strings;
        // field -> {value, byte offset}, sorted by (value, offset).
        std::unordered_map<uint32_t, std::vector<std::pair<uint32_t, uint32_t>>> indexes;
    };

    struct Store {
        std::mutex lock;
        std::unordered_map<uint32_t, Table> tables;
        uint32_t next = 1;
    };

    Store& Instance() {
        static Store store;
        return store;
    }

    struct Underflow {};

    struct Cursor {
        const std::vector<uint8_t>& blob;
        size_t pos = 0;

        uint32_t U32() {
            if (blob.size() - pos < 4) {
                throw Underflow();
            }

            uint32_t value;
            memcpy(&value, blob.data() + pos, 4);
            pos += 4;
            return value;
        }

        std::vector<uint32_t> List() {
            uint32_t n = U32();

            if ((blob.size() - pos) / 4 < n) {
                throw Underflow();
            }

            std::vector<uint32_t> out(n);

            for (uint32_t& value : out) {
                value = U32();
            }

            return out;
        }
    };

    void Put(std::vector<uint8_t>& out, uint32_t value) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&value);
        out.insert(out.end(), p, p + 4);
    }

    void PutList(std::vector<uint8_t>& out, const std::vector<uint32_t>& values) {
        Put(out, static_cast<uint32_t>(values.size()));

        for (uint32_t value : values) {
            Put(out, value);
        }
    }

    bool ValidField(const Table& table, uint32_t field) {
        return field <= table.recordSize && table.recordSize - field >= 4;
    }

    uint32_t FieldAt(const Table& table, size_t row, uint32_t field) {
        uint32_t value;
        memcpy(&value, table.records.data() + row * table.recordSize + field, 4);
        return value;
    }

    // sub_14001F240: offset 0, past the end or unterminated is "\0".
    std::pair<const uint8_t*, uint32_t> MstbString(const Table& table, uint32_t offset) {
        static const uint8_t nul = 0;

        if (offset != 0 && offset < table.strings.size()) {
            const uint8_t* begin = table.strings.data() + offset;
            const uint8_t* end = table.strings.data() + table.strings.size();
            const uint8_t* zero = std::find(begin, end, 0);

            if (zero != end) {
                return { begin, static_cast<uint32_t>(zero - begin + 1) };
            }
        }

        return { &nul, 1 };
    }

    // sub_140026690: one record's strings.
    void PutMstb(std::vector<uint8_t>& out, const Table& table, uint32_t row, const std::vector<uint32_t>& extra) {
        std::vector<uint8_t> entries;
        std::vector<uint8_t> data;
        uint32_t n = 0;

        for (uint32_t field : extra) {
            if (!ValidField(table, field)) {
                continue;
            }

            uint32_t offset = FieldAt(table, row, field);
            auto text = MstbString(table, offset);
            Put(entries, row);
            Put(entries, field);
            Put(entries, offset);
            Put(entries, static_cast<uint32_t>(data.size()));
            Put(entries, text.second);
            data.insert(data.end(), text.first, text.first + text.second);
            ++n;
        }

        Put(out, kMstb);
        Put(out, 1);
        Put(out, n);
        out.insert(out.end(), entries.begin(), entries.end());
        Put(out, static_cast<uint32_t>(data.size()));
        out.insert(out.end(), data.begin(), data.end());
    }

    // sub_14001E790: the optional extras after the key list; false is result 0xFFFFFFFF.
    bool ParseExtras(Cursor& cursor, std::vector<uint32_t>& extra) {
        if (cursor.pos == cursor.blob.size()) {
            return true;
        }

        uint32_t magic = cursor.U32();
        uint32_t version = cursor.U32();

        if (magic != kMstr || version != 1) {
            return false;
        }

        extra = cursor.List();
        return cursor.pos == cursor.blob.size();
    }

    Table* Find(Store& store, uint32_t handle) {
        auto it = store.tables.find(handle);
        return it == store.tables.end() ? nullptr : &it->second;
    }

    void PutRecord(std::vector<uint8_t>& out, const Table& table, uint32_t row) {
        const uint8_t* p = table.records.data() + static_cast<size_t>(row) * table.recordSize;
        out.insert(out.end(), p, p + table.recordSize);
    }

    // sub_1400226E0: the table commands; the reply's result.
    uint32_t Handle(Store& store, const Message& request, Message& reply) {
        using namespace CoA::MemoryBridge;

        switch (request.cmd) {
        case 0x64: {
            uint32_t recordSize = static_cast<uint32_t>(request.b);

            if (recordSize == 0) {
                return Failed;
            }

            Table& table = store.tables[store.next];
            table.recordSize = recordSize;
            table.count = request.c;
            table.records.assign(static_cast<size_t>(recordSize) * request.c, 0);
            reply.a = store.next++;
            return Success;
        }
        case 0x65:
            return store.tables.erase(request.a) ? Success : Failed;
        case 0x66:
        case 0x67:
        case 0x68:
        case 0x69: {
            Table* table = Find(store, request.a);

            if (!table) {
                return Failed;
            }

            bool strings = request.cmd >= 0x68;
            bool write = request.cmd == 0x66 || request.cmd == 0x68;
            std::vector<uint8_t>& bytes = strings ? table->strings : table->records;

            if (write && request.blob.size() < request.b) {
                return Failed;
            }

            if (request.cmd == 0x68) {
                // No upper bound: the block grows, zero-filled.
                uint64_t end = request.c + request.b;

                if (end > bytes.size()) {
                    bytes.resize(static_cast<size_t>(end), 0);
                }
            } else if (request.c > bytes.size() || request.b > bytes.size() - request.c) {
                return Failed;
            }

            if (write) {
                memcpy(bytes.data() + request.c, request.blob.data(), static_cast<size_t>(request.b));

                if (!strings) {
                    table->indexes.clear();
                }
            } else {
                reply.blob.assign(bytes.begin() + request.c, bytes.begin() + request.c + static_cast<size_t>(request.b));
            }

            return Success;
        }
        case 0x6A: {
            Table* table = Find(store, request.a);

            if (!table || !ValidField(*table, request.c)) {
                return Failed;
            }

            if (table->count && static_cast<uint64_t>(table->count - 1) * table->recordSize > 0xFFFFFFFF) {
                return Failed;
            }

            std::vector<std::pair<uint32_t, uint32_t>> index(table->count);

            for (uint32_t row = 0; row < table->count; ++row) {
                index[row] = { FieldAt(*table, row, request.c), row * table->recordSize };
            }

            std::sort(index.begin(), index.end());
            table->indexes[request.c] = std::move(index);
            return Success;
        }
        case 0x6B: {
            Table* table = Find(store, request.a);

            if (!table || !table->indexes.count(request.c)) {
                return Failed;
            }

            const auto& index = table->indexes[request.c];
            uint32_t key = static_cast<uint32_t>(request.b);
            auto first = std::lower_bound(index.begin(), index.end(), std::make_pair(key, 0u));
            std::vector<uint32_t> rows;

            for (auto it = first; it != index.end() && it->first == key; ++it) {
                rows.push_back(it->second / table->recordSize);
            }

            PutList(reply.blob, rows);
            reply.b = reply.blob.size();
            return Success;
        }
        case 0x6C: {
            Table* table = Find(store, request.a);

            if (!table || request.c >= table->strings.size()) {
                return Failed;
            }

            auto begin = table->strings.begin() + request.c;
            auto zero = std::find(begin, table->strings.end(), 0);

            if (zero == table->strings.end()) {
                return Failed;
            }

            reply.blob.assign(begin, zero + 1);
            reply.b = reply.blob.size();
            return Success;
        }
        case 0x6D:
        case 0x6F: {
            Cursor cursor{ request.blob };
            std::vector<uint32_t> keys = cursor.List();
            std::vector<uint32_t> extra;

            if (!ParseExtras(cursor, extra)) {
                return BadExtras;
            }

            Table* table = Find(store, request.a);

            if (!table || (request.cmd == 0x6D && !table->indexes.count(request.c))) {
                return Failed;
            }

            Put(reply.blob, static_cast<uint32_t>(keys.size()));

            for (uint32_t key : keys) {
                Put(reply.blob, key);
                bool found;
                uint32_t row = key;

                if (request.cmd == 0x6D) {
                    // The first (lowest-row) hit.
                    const auto& index = table->indexes[request.c];
                    auto it = std::lower_bound(index.begin(), index.end(), std::make_pair(key, 0u));
                    found = it != index.end() && it->first == key;
                    row = found ? it->second / table->recordSize : 0;
                } else {
                    found = key < table->count;
                }

                reply.blob.push_back(found ? 1 : 0);

                if (!found) {
                    continue;
                }

                if (request.cmd == 0x6D) {
                    Put(reply.blob, row);
                }

                PutRecord(reply.blob, *table, row);

                if (!extra.empty()) {
                    PutMstb(reply.blob, *table, row, extra);
                }
            }

            reply.b = reply.blob.size();
            return Success;
        }
        case 0x6E: {
            Cursor cursor{ request.blob };
            std::vector<uint32_t> offsets = cursor.List();
            Table* table = Find(store, request.a);

            if (!table) {
                return Failed;
            }

            Put(reply.blob, static_cast<uint32_t>(offsets.size()));

            for (uint32_t offset : offsets) {
                Put(reply.blob, offset);
                auto end = table->strings.end();
                auto zero = offset < table->strings.size() ? std::find(table->strings.begin() + offset, end, 0) : end;
                reply.blob.push_back(zero != end ? 1 : 0);

                if (zero != end) {
                    // The NUL is left out here, unlike 0x6C and MSTB.
                    Put(reply.blob, static_cast<uint32_t>(zero - (table->strings.begin() + offset)));
                    reply.blob.insert(reply.blob.end(), table->strings.begin() + offset, zero);
                }
            }

            reply.b = reply.blob.size();
            return Success;
        }
        case 0x73: {
            // PatchRecord. Not in MMgr64.exe (its dispatcher stops at 0x72, so every server patch of a bridged table
            // was dropped, in the original too); served here as the reconstruction now applies those patches (its
            // 3c8f196 pins them in the DLL's cache instead, the helper being fixed). One record at row `c`: an
            // existing row is overwritten, row `count` is appended. Built indexes are kept current (a 66 drops them,
            // but the client builds them once at load).
            Table* table = Find(store, request.a);

            if (!table || request.b != table->recordSize || request.blob.size() < request.b || request.c > table->count) {
                return Failed;
            }

            const uint32_t row = request.c;

            if (static_cast<uint64_t>(row) * table->recordSize > 0xFFFFFFFF) {
                return Failed;
            }

            const uint32_t offset = row * table->recordSize;
            const bool append = row == table->count;

            for (auto& kv : table->indexes) {
                auto& index = kv.second;

                if (!append) {
                    auto old = std::lower_bound(index.begin(), index.end(), std::make_pair(FieldAt(*table, row, kv.first), offset));

                    if (old != index.end() && old->second == offset) {
                        index.erase(old);
                    }
                }
            }

            if (append) {
                table->records.resize(table->records.size() + table->recordSize, 0);
                ++table->count;
            }

            memcpy(table->records.data() + offset, request.blob.data(), table->recordSize);

            for (auto& kv : table->indexes) {
                auto& index = kv.second;
                const std::pair<uint32_t, uint32_t> entry = { FieldAt(*table, row, kv.first), offset };
                index.insert(std::lower_bound(index.begin(), index.end(), entry), entry);
            }

            return Success;
        }
        default:
            // The projected reads 0x70..0x72 are not served; the client never sends them.
            return UnknownCommand;
        }
    }

    Message Request(uint32_t cmd, uint32_t a, uint64_t b, uint32_t c) {
        Message request;
        request.cmd = cmd;
        request.a = a;
        request.b = b;
        request.c = c;
        return request;
    }

    // FUN_102b47b0: 0x66 / 0x68 in chunks.
    bool Chunked(uint32_t cmd, uint32_t table, uint32_t offset, const void* src, uint32_t size) {
        if (size == 0) {
            return true;
        }

        if (!src || offset + size < offset) {
            return false;
        }

        const uint8_t* p = static_cast<const uint8_t*>(src);

        for (uint32_t done = 0; done < size;) {
            uint32_t chunk = std::min<uint32_t>(size - done, kMaxReplyBlob);
            Message request = Request(cmd, table, chunk, offset + done);
            request.blob.assign(p + done, p + done + chunk);

            if (CoA::MemoryBridge::Exchange(request).cmd != 0) {
                return false;
            }

            done += chunk;
        }

        return true;
    }

    // FUN_102b35a0: refused batches are retried at half their size; the replies' entries are joined.
    bool BatchedQuery(uint32_t cmd, uint32_t table, uint32_t c, const std::vector<uint32_t>& keys, std::vector<uint8_t>& out, const std::vector<uint32_t>& extra) {
        out.clear();
        uint32_t total = 0;
        std::vector<uint8_t> payload;
        size_t done = 0;
        size_t batch = std::min(keys.size(), kMaxBatch);

        while (done < keys.size()) {
            size_t take = std::min(keys.size() - done, batch);
            Message request = Request(cmd, table, 0, c);
            PutList(request.blob, std::vector<uint32_t>(keys.begin() + done, keys.begin() + done + take));

            if (!extra.empty()) {
                Put(request.blob, kMstr);
                Put(request.blob, 1);
                PutList(request.blob, extra);
            }

            request.b = request.blob.size();
            Message reply = CoA::MemoryBridge::Exchange(request);

            if (reply.cmd != 0) {
                if (take == 1) {
                    return false;
                }

                batch = std::max<size_t>(take >> 1, 1);
                continue;
            }

            if (reply.blob.size() < 4) {
                return false;
            }

            uint32_t n;
            memcpy(&n, reply.blob.data(), 4);

            if (n > ~total) {
                return false;
            }

            total += n;
            payload.insert(payload.end(), reply.blob.begin() + 4, reply.blob.end());
            done += take;
            batch = std::min(keys.size() - done, kMaxBatch);
        }

        Put(out, total);
        out.insert(out.end(), payload.begin(), payload.end());
        return true;
    }
}

CoA::MemoryBridge::Message CoA::MemoryBridge::Exchange(const Message& request) {
    Store& store = Instance();
    std::lock_guard<std::mutex> guard(store.lock);
    Message reply;
    reply.cmd = request.cmd;
    reply.a = request.a;
    reply.b = request.b;

    try {
        reply.result = Handle(store, request, reply);
    } catch (const Underflow&) {
        reply.result = InvalidMessage;
    }

    if (reply.result != Success) {
        Message refused;
        refused.cmd = 0xFFFFFFFF;
        refused.a = reply.a;
        refused.b = reply.b;
        refused.result = reply.result;
        return refused;
    }

    if (reply.blob.size() > kMaxReplyBlob) {
        Message refused;
        refused.cmd = 0xFFFFFFFF;
        refused.b = 48 + reply.blob.size();
        refused.result = OversizedMessage;
        return refused;
    }

    reply.cmd = 0;
    return reply;
}

void CoA::MemoryBridge::Reset() {
    Store& store = Instance();
    std::lock_guard<std::mutex> guard(store.lock);
    store.tables.clear();
    store.next = 1;
}

uint32_t CoA::MemoryBridge::CreateTable(uint32_t recordSize, uint32_t count) {
    Message reply = Exchange(Request(0x64, 0, recordSize, count));
    return reply.cmd != 0 ? 0 : reply.a;
}

bool CoA::MemoryBridge::DestroyTable(uint32_t table) {
    return table != 0 && Exchange(Request(0x65, table, 0, 0)).cmd == 0;
}

bool CoA::MemoryBridge::Command66(uint32_t table, uint32_t offset, const void* src, uint32_t size) {
    return Chunked(0x66, table, offset, src, size);
}

bool CoA::MemoryBridge::Command68(uint32_t table, uint32_t offset, const void* src, uint32_t size) {
    return Chunked(0x68, table, offset, src, size);
}

bool CoA::MemoryBridge::Command6A(uint32_t table, uint32_t field) {
    return Exchange(Request(0x6A, table, 0, field)).cmd == 0;
}

bool CoA::MemoryBridge::Command6B(uint32_t table, uint32_t field, uint32_t key, std::vector<uint32_t>& out) {
    out.clear();
    Message reply = Exchange(Request(0x6B, table, key, field));

    if (reply.cmd != 0) {
        return false;
    }

    try {
        Cursor cursor{ reply.blob };
        out = cursor.List();
    } catch (const Underflow&) {
        return false;
    }

    return true;
}

bool CoA::MemoryBridge::Command6C(uint32_t table, uint32_t offset, std::string& out) {
    out.clear();
    Message reply = Exchange(Request(0x6C, table, 0, offset));

    if (reply.cmd != 0 || reply.blob.empty()) {
        return false;
    }

    out.assign(reply.blob.begin(), reply.blob.end());
    return out.back() == '\0';
}

bool CoA::MemoryBridge::Command73(uint32_t table, uint32_t index, const void* src, uint32_t size) {
    if (table == 0 || !src || size == 0) {
        return false;
    }

    Message request = Request(0x73, table, size, index);
    const uint8_t* p = static_cast<const uint8_t*>(src);
    request.blob.assign(p, p + size);
    return Exchange(request).cmd == 0;
}

bool CoA::MemoryBridge::Command6D(uint32_t table, uint32_t field, const std::vector<uint32_t>& keys, std::vector<uint8_t>& out, const std::vector<uint32_t>& extra) {
    return BatchedQuery(0x6D, table, field, keys, out, extra);
}

bool CoA::MemoryBridge::Command6E(uint32_t table, const std::vector<uint32_t>& offsets, std::vector<uint8_t>& out) {
    return BatchedQuery(0x6E, table, 0, offsets, out, {});
}

bool CoA::MemoryBridge::Command6F(uint32_t table, const std::vector<uint32_t>& indexes, std::vector<uint8_t>& out, const std::vector<uint32_t>& extra) {
    return BatchedQuery(0x6F, table, 0, indexes, out, extra);
}
