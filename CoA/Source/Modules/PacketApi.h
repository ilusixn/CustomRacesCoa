#ifndef COA_SOURCE_MODULES_PACKETAPI_H
#define COA_SOURCE_MODULES_PACKETAPI_H

#include <cstdint>

class CDataStore;

// AscPacketApi.cpp, the original's Lua packet API (0x1030f1b0..0x1030fdc0):
//
//   CreatePacket(opcode)      FUN_1030f620: a heap CDataStore with the u32 opcode written, as a userdata with the
//                             registry metatable "CDataStore" (nil without it). Never freed (no __gc), as shipped.
//   @CDataStore:Put*(v)       PutData of the 1 / 2 / 4-byte value (Int8 = UInt8, Int16 = UInt16), PutString with NUL.
//   @CDataStore:Get*()        the value at the read cursor, cursor advanced; GetUInt32 is GetInt32 (signed), GetGUID
//                             a u64 as "0x%016X" (0x74D0D0), GetString a NUL-terminated string.
//   @CDataStore:Send()        Finalize, then ClientServices::SendPacket (0x406F40): whatever the store holds.
//   RegisterPacket(op, fn)    FUN_1030fd10: fn into the registry (luaL_ref), the opcode routed to FUN_1030f410:
//                             above 0x55E the DLL's table (replacing), otherwise the client's SetMessageHandler.
//   ClearPacket(op)           FUN_1030f550: the function forgotten; the opcode stays routed.
//
// The original creates the "CDataStore" method table on the world state (FUN_1030ff20 from the world-registered list);
// here it is LuaReg's '@' table. The original's getters read past the packet end unchecked; here a getter with too
// few bytes left returns nothing and leaves the cursor (Ruling). A received packet's userdata is emptied after its
// handler returns, so a kept reference cannot reach the freed store (Ruling).
namespace CoA::PacketApi {
    using Handler = int32_t (*)(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);

    // The client half (PacketApiClient.cpp) installs these.
    struct Deps {
        void (*send)(CDataStore* msg) = nullptr;                          // 0x406F40 ClientServices::SendPacket
        void (*setStockHandler)(uint32_t opcode, Handler handler) = nullptr;   // FUN_102c4590(op, fn, 0): kept NetClient
    };

    Deps& Dependencies();

    // FUN_1030f410: the handler every RegisterPacket opcode is routed to; calls fn(opcode & 0xFFFF, packet) on the
    // client's main Lua state. Always 1.
    int32_t OnRegisteredPacket(void* param, uint32_t opcode, uint32_t time, CDataStore* msg);

    // Tests only.
    bool HasHandler(uint32_t opcode);
    void Reset();

    void Init();
}

#endif
