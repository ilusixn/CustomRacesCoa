#include "CoA/Source/Runtime/Packet.h"

namespace {
    CoA::Packet::Sender s_sender = nullptr;
}

void CoA::Packet::SetSender(CoA::Packet::Sender sender) {
    s_sender = sender;
}

// Finalize (0x401130), then the sender.
void CoA::Packet::Send() {
    this->m_store.Finalize();

    if (s_sender) {
        s_sender(&this->m_store);
    }
}

// CDataStore ctor (0x401050), Put(u32 opcode) (0x47B0A0).
CoA::Packet::Packet(uint32_t opcode) {
    this->m_store.Put(opcode);
}

// PutData (0x47B1C0).
CoA::Packet& CoA::Packet::Data(const void* data, uint32_t bytes) {
    this->m_store.PutData(data, bytes);
    return *this;
}

// PutString (0x47B300).
CoA::Packet& CoA::Packet::Str(const char* string) {
    this->m_store.PutString(string ? string : "");
    return *this;
}

CoA::Packet& CoA::Packet::PackedGuid(uint64_t guid) {
    uint8_t buf[9] = {};
    uint32_t n = 1;

    for (int32_t i = 0; i < 8 && guid; ++i, guid >>= 8) {
        if (guid & 0xFF) {
            buf[0] |= static_cast<uint8_t>(1u << i);
            buf[n++] = static_cast<uint8_t>(guid & 0xFF);
        }
    }

    return this->Data(buf, n);
}

// Packet::Send is in PacketSend.cpp, so code that only builds packets does not link the client services.
