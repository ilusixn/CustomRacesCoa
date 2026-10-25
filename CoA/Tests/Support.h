#ifndef COA_TESTS_SUPPORT_H
#define COA_TESTS_SUPPORT_H

// Shared test helpers: packet bytes, a recording event sink, and calling natives through a real Lua state.

#include "Engine/Source/Base/CDataStore.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

struct lua_State;

namespace CoATest {
    // Runs the rest of a test in a fresh temporary working directory (with a WTF folder); restores the old one.
    struct TempCwd {
        TempCwd();
        ~TempCwd();
        char previous[260];
        char dir[260];
    };

    // Little-endian packet payload builder.
    class Bytes {
    public:
        Bytes& U8(uint8_t v) { return this->Raw(&v, 1); }
        Bytes& U16(uint16_t v) { return this->Raw(&v, 2); }
        Bytes& U32(uint32_t v) { return this->Raw(&v, 4); }
        Bytes& I32(int32_t v) { return this->Raw(&v, 4); }
        Bytes& U64(uint64_t v) { return this->Raw(&v, 8); }
        Bytes& F32(float v) { return this->Raw(&v, 4); }
        Bytes& Str(const char* s) { return this->Raw(s, static_cast<uint32_t>(strlen(s) + 1)); }
        // u32 length + bytes, no terminator.
        Bytes& Name(const char* s) { this->U32(static_cast<uint32_t>(strlen(s))); return this->Raw(s, static_cast<uint32_t>(strlen(s))); }
        Bytes& Raw(const void* p, uint32_t n) {
            const uint8_t* b = static_cast<const uint8_t*>(p);
            this->data.insert(this->data.end(), b, b + n);
            return *this;
        }

        std::vector<uint8_t> data;
    };

    // A WDBC file: each row is its dwords (all rows the same length), then the string block (offset 0 is "").
    // columns: the header's field count when it is not one per dword (a 64-bit column).
    std::vector<uint8_t> Wdbc(const std::vector<std::vector<uint32_t>>& rows, const std::string& strings, uint32_t columns = 0);
    // A localized-string block of a WDBC row: the offset in all 16 locale slots, then the flags dword.
    std::vector<uint32_t> Loc(uint32_t offset);

    // Feeds a payload to a handler with the CoA::Packets::Handler signature.
    int32_t Deliver(int32_t (*handler)(void*, uint32_t, uint32_t, CDataStore*), uint32_t opcode, Bytes& bytes, CDataStore** out = nullptr);

    // Every CoA::Events::Signal while installed: "NAME" or "NAME:arg1,arg2" (%s %u %d %b %f formatted).
    void RecordSignals();
    void StopRecordingSignals();
    std::vector<std::string>& Signals();

    // Every CoA::Packet::Send while installed: the finalized bytes (u32 opcode first).
    void RecordPackets();
    void StopRecordingPackets();
    std::vector<std::vector<uint8_t>>& SentPackets();
    // A u32 opcode followed by the bytes.
    std::vector<uint8_t> Expected(uint32_t opcode, const Bytes& payload);

    // Inits RealmInfo, Account, Config, GameEvents and the realm getters of Misc / GlobalsB / GlobalsD.
    void InitRealmModules();

    // A fresh Lua state with the base library; natives registered for the glue (true) or world (false) state.
    lua_State* NewState(bool glue);
    // Calls ns.name (ns "" = global) with the given number/string arguments; returns the results as strings
    // ("nil", "true"/"false", numbers with %g, strings as-is, "table").
    std::vector<std::string> Call(lua_State* L, const char* ns, const char* name, const std::vector<std::string>& args = {});
}

#endif
