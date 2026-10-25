#ifndef COA_SOURCE_RUNTIME_PACKET_H
#define COA_SOURCE_RUNTIME_PACKET_H

#include "Engine/Source/Base/CDataStore.h"
#include <cstdint>

namespace CoA {
    // A client -> server packet, as every sender in the DLL inlines it (port of AscScript::Packet): CDataStore,
    // Put(u32 opcode), the fields, Finalize, ClientServices send. Little-endian.
    class Packet {
    public:
        explicit Packet(uint32_t opcode);
        Packet(const Packet&) = delete;
        Packet& operator=(const Packet&) = delete;

        Packet& Data(const void* data, uint32_t bytes);
        Packet& U8(uint8_t value) { return this->Data(&value, 1); }
        Packet& U16(uint16_t value) { return this->Data(&value, 2); }
        Packet& U32(uint32_t value) { return this->Data(&value, 4); }
        Packet& U64(uint64_t value) { return this->Data(&value, 8); }
        Packet& F32(float value) { return this->Data(&value, 4); }
        // PutString: the terminator included; nullptr is written as "".
        Packet& Str(const char* string);
        // FUN_100de790: a mask byte, then each non-zero byte of the guid, low byte first.
        Packet& PackedGuid(uint64_t guid);
        void Send();
        CDataStore* Store() { return &this->m_store; }

    private:
        CDataStore m_store;
    };
}

#endif
