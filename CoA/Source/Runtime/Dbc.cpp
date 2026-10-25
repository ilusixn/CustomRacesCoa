#include "CoA/Source/Runtime/Dbc.h"
#include "CoA/Source/Runtime/Log.h"
#include <map>

namespace {
    // Tables whose row parser collapses localized strings (the reconstruction's tools/dbc_layouts.py).
    struct Layout {
        const char* path;
        const char* spec;
    };

    const Layout s_layouts[] = {
#include "CoA/Source/Runtime/DbcLayouts.generated.inc"
    };

    CoA::Dbc::FileReader s_reader = nullptr;
    uint32_t (*s_localeSlot)() = nullptr;

    // Separators compare equal: a table keeps whichever spelling first asked for it.
    const char* LayoutFor(const std::string& path) {
        for (const Layout& layout : s_layouts) {
            size_t i = 0;

            for (; i < path.size() && layout.path[i]; ++i) {
                char a = path[i] == '/' ? '\\' : path[i];
                char b = layout.path[i] == '/' ? '\\' : layout.path[i];

                if (a != b) {
                    break;
                }
            }

            if (i == path.size() && !layout.path[i]) {
                return layout.spec;
            }
        }

        return nullptr;
    }

    bool ReadAll(const std::string& path, std::vector<uint8_t>& out) {
        return s_reader && s_reader(path.c_str(), out);
    }
}

void CoA::Dbc::SetFileReader(CoA::Dbc::FileReader reader) {
    s_reader = reader;
}

void CoA::Dbc::SetLocaleSlot(uint32_t (*slot)()) {
    s_localeSlot = slot;
}

CoA::Dbc::Table::Table(const char* path) : m_path(path) {
}

void CoA::Dbc::Table::Load() {
    if (this->m_tried) {
        return;
    }

    this->m_tried = true;
    std::vector<uint8_t> file;

    if (!ReadAll(this->m_path, file)) {
        // The archives do not treat '/' and '\' alike, so the other spelling is tried before giving up.
        std::string other(this->m_path);

        for (char& c : other) {
            c = c == '/' ? '\\' : c == '\\' ? '/' : c;
        }

        if (!ReadAll(other, file)) {
            // Not cached as a failure: some DBCs live in archives mounted later, so a later call tries again.
            if (!this->m_openLogged) {
                CoA::Log::Printf("Unable to open %s (will retry)", this->m_path.c_str());
            }

            this->m_openLogged = true;
            this->m_tried = false;
            return;
        }
    }

    uint32_t header[5] = {};
    bool ok = file.size() >= sizeof(header);

    if (ok) {
        memcpy(header, file.data(), sizeof(header));
        ok = header[0] == 0x43424457;
    }

    uint32_t cols = ok ? header[2] : 0;
    uint32_t stringSize = ok ? header[4] : 0;
    this->m_count = ok ? header[1] : 0;
    this->m_rowSize = ok ? header[3] : 0;

    if (ok && this->m_count == 0) {
        CoA::Log::Printf("%s has no records", this->m_path.c_str());
    }

    if (ok && this->m_count) {
        size_t rowBytes = static_cast<size_t>(this->m_count) * this->m_rowSize;
        ok = file.size() >= sizeof(header) + rowBytes + stringSize;

        if (ok) {
            auto rows = file.begin() + sizeof(header);
            this->m_rows.assign(rows, rows + rowBytes);
            this->m_strings.assign(rows + rowBytes, rows + rowBytes + stringSize);
            this->m_strings.push_back('\0');
        }
    }

    if (!ok || !this->m_count || this->m_rowSize < 4) {
        if (!ok) {
            CoA::Log::Printf("%s: malformed WDBC", this->m_path.c_str());
        }

        this->m_count = 0;
        return;
    }

    // The DLL's struct is the file row with each localized block (16 locale offsets + flags) reduced to the offset
    // for this client's locale. Byte fields ('b') are packed in the file but the struct keeps natural alignment.
    if (const char* spec = LayoutFor(this->m_path)) {
        uint32_t structSize = 0;

        for (const char* c = spec; *c; ++c) {
            structSize = (*c == 'b') ? structSize + 1 : ((structSize + 3) & ~3u) + 4;
        }

        structSize = (structSize + 3) & ~3u;
        std::vector<uint8_t> converted(static_cast<size_t>(this->m_count) * structSize);
        uint32_t slot = s_localeSlot ? s_localeSlot() : 0;

        for (uint32_t i = 0; i < this->m_count; ++i) {
            const uint8_t* src = &this->m_rows[static_cast<size_t>(i) * this->m_rowSize];
            uint8_t* row = &converted[static_cast<size_t>(i) * structSize];
            uint32_t at = 0;

            for (const char* c = spec; *c; ++c) {
                if (*c == 'b') {
                    row[at++] = *src++;
                    continue;
                }

                at = (at + 3) & ~3u;

                if (*c == 'L') {
                    memcpy(row + at, src + slot * 4, 4);
                    src += 68;
                } else {
                    memcpy(row + at, src, 4);
                    src += 4;
                }

                at += 4;
            }
        }

        this->m_rows.swap(converted);
        this->m_rowSize = structSize;
    }

    // FUN_101feb50: maxId starts at 0, minId at 0x0FFFFFFF.
    this->m_max = 0;
    this->m_min = 0x0FFFFFFF;

    for (uint32_t i = 0; i < this->m_count; ++i) {
        uint32_t id = U32(&this->m_rows[static_cast<size_t>(i) * this->m_rowSize], 0);
        this->m_max = id > this->m_max ? id : this->m_max;
        this->m_min = id < this->m_min ? id : this->m_min;
    }

    this->m_index.assign(static_cast<size_t>(this->m_max - this->m_min) + 1, nullptr);

    for (uint32_t i = 0; i < this->m_count; ++i) {
        const uint8_t* row = &this->m_rows[static_cast<size_t>(i) * this->m_rowSize];
        this->m_index[U32(row, 0) - this->m_min] = row;
    }

    this->m_ok = true;
    CoA::Log::Printf("Dbc: %s %u rows, %u cols, ids %u..%u", this->m_path.c_str(), this->m_count, cols, this->m_min, this->m_max);
}

const uint8_t* CoA::Dbc::Table::Row(uint32_t id) {
    this->Load();

    if (!this->m_ok || id < this->m_min || id > this->m_max) {
        return nullptr;
    }

    return this->m_index[id - this->m_min];
}

uint32_t CoA::Dbc::Table::Count() {
    this->Load();
    return this->m_ok ? this->m_count : 0;
}

const uint8_t* CoA::Dbc::Table::RowAt(uint32_t index) {
    this->Load();
    return (this->m_ok && index < this->m_count) ? &this->m_rows[static_cast<size_t>(index) * this->m_rowSize] : nullptr;
}

uint32_t CoA::Dbc::Table::MinId() {
    this->Load();
    return this->m_min;
}

uint32_t CoA::Dbc::Table::MaxId() {
    this->Load();
    return this->m_max;
}

bool CoA::Dbc::Table::Loaded() {
    this->Load();
    return this->m_ok;
}

uint32_t CoA::Dbc::Table::RowSize() {
    this->Load();
    return this->m_rowSize;
}

const char* CoA::Dbc::Table::Str(const uint8_t* row, uint32_t off) {
    uint32_t offset = U32(row, off);
    return offset < this->m_strings.size() ? &this->m_strings[offset] : "";
}

uint32_t CoA::Dbc::Table::AddString(const char* s) {
    this->Load();
    uint32_t offset = static_cast<uint32_t>(this->m_strings.size());
    this->m_strings.insert(this->m_strings.end(), s, s + strlen(s) + 1);
    return offset;
}

// The original's patch handlers copy over an existing row where it lies (so file-order walks see the change) and
// insert a new one through FUN_10133ac0, which only touches the index. Growing the index there copies ids
// min..max-1: the row that held the old maximum is dropped (an original defect, kept).
void CoA::Dbc::Table::Upsert(uint32_t id, const std::vector<uint8_t>& row) {
    this->Load();

    if (const uint8_t* existing = this->Row(id)) {
        size_t n = row.size();

        for (const std::vector<uint8_t>* patched : this->m_patched) {
            if (patched->data() == existing) {
                n = n < patched->size() ? n : patched->size();
            }
        }

        if (existing >= this->m_rows.data() && existing < this->m_rows.data() + this->m_rows.size()) {
            n = n < this->m_rowSize ? n : this->m_rowSize;
        }

        if (n < row.size()) {
            CoA::Log::Printf("Dbc: %s row %u patched in place, %u of %u bytes fit", this->m_path.c_str(), id,
                static_cast<unsigned>(n), static_cast<unsigned>(row.size()));
        }

        memcpy(const_cast<uint8_t*>(existing), row.data(), n);
        return;
    }

    auto copy = new std::vector<uint8_t>(row);
    this->m_patched.push_back(copy);

    if (!this->m_ok) {
        this->m_ok = true;
        this->m_min = id;
        this->m_max = id;
        this->m_index.assign(1, nullptr);
    }

    if (id < this->m_min || id > this->m_max) {
        uint32_t lo = id < this->m_min ? id : this->m_min;
        uint32_t hi = id > this->m_max ? id : this->m_max;
        std::vector<const uint8_t*> grown(static_cast<size_t>(hi - lo) + 1, nullptr);

        for (uint32_t i = this->m_min; i < this->m_max; ++i) {
            grown[i - lo] = this->m_index[i - this->m_min];
        }

        this->m_index.swap(grown);
        this->m_min = lo;
        this->m_max = hi;
    }

    this->m_index[id - this->m_min] = copy->data();
}

CoA::Dbc::Table& CoA::Dbc::Get(const char* path) {
    static std::map<std::string, CoA::Dbc::Table*> tables;
    std::string key(path);

    for (char& c : key) {
        if (c == '/') {
            c = '\\';
        }
    }

    CoA::Dbc::Table*& table = tables[key];

    if (!table) {
        table = new CoA::Dbc::Table(path);
    }

    return *table;
}
