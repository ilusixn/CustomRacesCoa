#ifndef COA_SOURCE_RUNTIME_PACKETS_H
#define COA_SOURCE_RUNTIME_PACKETS_H

#include <cstdint>

class CDataStore;

// Server -> client packet routing for Ascension's opcodes (port of the original's installer FUN_102c3540 as
// transcribed in the scaffold's CNetClient.cpp). Opcodes above 0x55E live in the CoA table; at or below go to the
// stock NetClient table. NetClient::SetMessageHandler / ProcessMessage call in here.
namespace CoA::Packets {
    constexpr int32_t FIRST_CUSTOM = 0x55F;
    constexpr uint32_t STOCK_TABLE_SIZE = 0x51F;   // NetClient::m_handlers (NUM_MSG_TYPES)

    // 0x51F..0x55E: past the stock handler table, below the DLL's range. The 32-bit client indexes past its table
    // there; CoA64 treats these opcodes as unhandled (CoA/DIFFERENCES.md).
    bool InGap(uint32_t opcode);

    typedef int32_t (*Handler)(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);

    // FUN_102c4400's custom branch: insert or replace.
    void Set(uint32_t opcode, Handler handler, void* param);
    // FUN_102c3f80 (AscProfiler's hook on 0x631FC0 ResetHandler): the custom branch erases the entry.
    void Erase(uint32_t opcode);
    void Clear();

    // FUN_102c3ab0's custom branch. Returns false (and does nothing) for opcode < 0x55F. Otherwise calls the
    // handler and stores its result, or -- with no handler -- writes "Unhandled packet: <name> (<opcode>)" to the
    // Fatal channel, reads nothing and stores 1.
    bool Dispatch(int32_t opcode, uint32_t time, CDataStore* msg, int32_t& result);

    // FUN_102c4a90: the original's opcode name, or "unknown" past 0x9D4.
    const char* Name(uint32_t opcode);
}

// Registers a server packet handler; used inside a module's Init (FUN_102c4590 in the DLL). Tools/coa/inventory.py
// counts these.
#define COA_PACKET(opcode, fn) CoA::Packets::Set((opcode), (fn), nullptr)

#endif
