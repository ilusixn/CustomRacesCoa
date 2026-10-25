#ifndef COA_SOURCE_RUNTIME_READ_H
#define COA_SOURCE_RUNTIME_READ_H

#include "Engine/Source/Base/CDataStore.h"
#include <cstdint>
#include <string>
#include <vector>

// Reading DLL packet fields through the client's own CDataStore getters. The reconstruction reads m_buffer
// directly, which is the original's result for a well-formed packet; for a short one the getters fail instead of
// reading past the buffer (m_read becomes m_size + 1 and every later read fails, yielding 0 / "").
namespace CoA::Read {
    inline bool Failed(const CDataStore* msg) { return msg->m_read > msg->m_size; }
    inline uint32_t Remaining(const CDataStore* msg) { return Failed(msg) ? 0 : msg->m_size - msg->m_read; }

    uint8_t U8(CDataStore* msg);
    uint16_t U16(CDataStore* msg);
    uint32_t U32(CDataStore* msg);
    int32_t I32(CDataStore* msg);
    uint64_t U64(CDataStore* msg);
    float F32(CDataStore* msg);
    // A NUL-terminated string (CDataStore::GetString without a length cap).
    std::string Str(CDataStore* msg);
    // u32 length, then that many bytes (no terminator on the wire).
    std::string Name(CDataStore* msg);
    // CDataStore::GetString into a fixed buffer; out is "" on failure.
    void CStr(CDataStore* msg, char* out, uint32_t max);
    // n raw bytes; zero-filled on failure.
    void Data(CDataStore* msg, void* out, uint32_t n);
    // CompressedDataStore (FUN_10195a60): the rest of the packet is a u32 inflated size and zlib data, inflated with the
    // client's 0x778180 (0 = ok). The packet is consumed either way; false when fewer than 4 bytes are left or
    // inflating fails.
    bool Inflate(CDataStore* msg, std::vector<uint8_t>& out);
}

#endif
