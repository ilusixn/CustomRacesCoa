#ifndef COA_SOURCE_RUNTIME_DBC_H
#define COA_SOURCE_RUNTIME_DBC_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// The DLL's custom-DBC tables (port of AscDbc: its own Addin/WowClientDB.h, a copy of Blizzard's).
//
// FUN_101e7c80 and its ~100 per-table siblings open "DBFilesClient/<Name>.dbc" through the client's SFile, check
// WDBC / column count / row size, then FUN_101feb50 keeps each row exactly as it is in the file, indexed by the id
// in its first dword. Bindings read rows at fixed byte offsets; a string column is resolved when read, from the
// same block (an absent string reads as ""). Tables load on first use.
namespace CoA::Dbc {
    class Table {
    public:
        explicit Table(const char* path);

        const uint8_t* Row(uint32_t id);       // nullptr if absent
        uint32_t Count();                      // container +0x08
        const uint8_t* RowAt(uint32_t index);  // index-th record in file order
        uint32_t MinId();
        uint32_t MaxId();
        bool Loaded();
        uint32_t RowSize();

        static uint32_t U32(const uint8_t* row, uint32_t off) { uint32_t v; memcpy(&v, row + off, 4); return v; }
        static int32_t I32(const uint8_t* row, uint32_t off) { int32_t v; memcpy(&v, row + off, 4); return v; }
        static float F32(const uint8_t* row, uint32_t off) { float v; memcpy(&v, row + off, 4); return v; }
        const char* Str(const uint8_t* row, uint32_t off);

        // FUN_10133ac0: point id at a caller-supplied row (the original's runtime DBC patching from server
        // packets). The row is copied and owned here; strings go through AddString.
        void Upsert(uint32_t id, const std::vector<uint8_t>& row);
        uint32_t AddString(const char* s);

    private:
        void Load();

        std::string m_path;
        bool m_tried = false;
        bool m_ok = false;
        bool m_openLogged = false;
        uint32_t m_count = 0;
        uint32_t m_rowSize = 0;
        uint32_t m_min = 0;
        uint32_t m_max = 0;
        std::vector<uint8_t> m_rows;
        std::vector<char> m_strings;
        std::vector<const uint8_t*> m_index;
        std::vector<std::vector<uint8_t>*> m_patched;
    };

    // One instance per file for the life of the process; '/' and '\' spellings are the same table.
    Table& Get(const char* path);

    // Reads a whole file from the client's archives (DbcClient.cpp installs SFile; tests install a fake).
    using FileReader = bool (*)(const char* path, std::vector<uint8_t>& out);
    void SetFileReader(FileReader reader);
    // The locale slot (0..15) a localized string column keeps (FUN_101b0c80); defaults to 0.
    void SetLocaleSlot(uint32_t (*slot)());

    // DbcClient.cpp: the SFile reader and the client's locale.
    void InstallClient();
}

#endif
