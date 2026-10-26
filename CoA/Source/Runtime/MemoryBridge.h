#ifndef COA_SOURCE_RUNTIME_MEMORYBRIDGE_H
#define COA_SOURCE_RUNTIME_MEMORYBRIDGE_H

#include <cstdint>
#include <string>
#include <vector>

// The DLL's MemoryBridge (AscMemoryBridge.cpp) with its MMgr64.exe table server run in-process. The 32-bit DLL kept
// the large tables in a 64-bit helper process; the 64-bit client needs no helper, but the server's semantics are
// kept as they are (protocol 3): result codes and the 0xFEFD0-byte reply limit. One deliberate difference: PatchRecord
// (0x73), an unknown command to MMgr64.exe, is served, so server patches of bridged rows take effect (as the
// reconstruction now applies them). Only the table commands exist; the memory commands served the start-up self-test
// alone.
namespace CoA::MemoryBridge {
    // One request or reply. A reply's cmd is 0 on success, 0xFFFFFFFF on failure, and result says why.
    struct Message {
        uint32_t cmd = 0;
        uint32_t a = 0;
        uint64_t b = 0;
        uint32_t c = 0;
        std::vector<uint8_t> blob;
        uint32_t result = 0;
    };

    enum Result : uint32_t {
        Success = 0,
        Failed = 1,
        InvalidMessage = 2,
        OversizedMessage = 8,
        UnknownCommand = 10,
        BadExtras = 0xFFFFFFFF,
    };

    // The server: one command.
    Message Exchange(const Message& request);
    // Every table gone and the handle counter back at 1 (tests).
    void Reset();

    // The client wrappers, as the DLL calls them.
    uint32_t CreateTable(uint32_t recordSize, uint32_t count);
    bool DestroyTable(uint32_t table);
    // Records / strings in 0xFEFD0-byte chunks from a byte offset.
    bool Command66(uint32_t table, uint32_t offset, const void* src, uint32_t size);
    bool Command68(uint32_t table, uint32_t offset, const void* src, uint32_t size);
    // An index on a u32 field; needed by 6B and 6D, dropped by every 66.
    bool Command6A(uint32_t table, uint32_t field);
    // Every row whose field is key, in row order.
    bool Command6B(uint32_t table, uint32_t field, uint32_t key, std::vector<uint32_t>& out);
    // The string at offset, NUL included.
    bool Command6C(uint32_t table, uint32_t offset, std::string& out);
    // PatchRecord: one record at `index` (< count: overwritten, == count: appended); built indexes follow it.
    bool Command73(uint32_t table, uint32_t index, const void* src, uint32_t size);
    // Batched queries: out is u32 n and the per-key entries of every accepted batch. A refused batch (the reply over
    // the limit) is retried at half its size.
    bool Command6D(uint32_t table, uint32_t field, const std::vector<uint32_t>& keys, std::vector<uint8_t>& out, const std::vector<uint32_t>& extra);
    bool Command6E(uint32_t table, const std::vector<uint32_t>& offsets, std::vector<uint8_t>& out);
    bool Command6F(uint32_t table, const std::vector<uint32_t>& indexes, std::vector<uint8_t>& out, const std::vector<uint32_t>& extra);
}

#endif
