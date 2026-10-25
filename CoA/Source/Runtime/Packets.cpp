#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Logger.h"
#include <cstdio>
#include <unordered_map>

namespace {
    // CNetClientCustomPacket in the DLL's data container (sDC.GetPacketHandlerMap()).
    struct Entry {
        CoA::Packets::Handler handler;
        void* param;
    };

    std::unordered_map<uint32_t, Entry>& Map() {
        static std::unordered_map<uint32_t, Entry> map;
        return map;
    }

    // FUN_102c4a90's jump table (0x102C7AF0, 0x9D5 entries).
    const char* const s_opcodeNames[] = {
#include "CoA/Source/Runtime/OpcodeNames.generated.inc"
    };
}

bool CoA::Packets::InGap(uint32_t opcode) {
    return opcode >= CoA::Packets::STOCK_TABLE_SIZE && opcode < static_cast<uint32_t>(CoA::Packets::FIRST_CUSTOM);
}

void CoA::Packets::Set(uint32_t opcode, Handler handler, void* param) {
    Map().insert_or_assign(opcode, Entry { handler, param });
}

void CoA::Packets::Erase(uint32_t opcode) {
    Map().erase(opcode);
}

void CoA::Packets::Clear() {
    Map().clear();
}

bool CoA::Packets::Dispatch(int32_t opcode, uint32_t time, CDataStore* msg, int32_t& result) {
    if (opcode < CoA::Packets::FIRST_CUSTOM) {
        return false;
    }

    auto it = Map().find(static_cast<uint32_t>(opcode));

    if (it == Map().end()) {
        // AscProfiler::Unhandled: nothing is read.
        char line[160];
        snprintf(line, sizeof(line), "Unhandled packet: %s (%u)", CoA::Packets::Name(static_cast<uint32_t>(opcode)), static_cast<uint32_t>(opcode));
        CoA::Logger::Write(CoA::Logger::Fatal, line);
        result = 1;
        return true;
    }

    result = it->second.handler(it->second.param, static_cast<uint32_t>(opcode), time, msg);
    return true;
}

const char* CoA::Packets::Name(uint32_t opcode) {
    static_assert(sizeof(s_opcodeNames) / sizeof(s_opcodeNames[0]) == 0x9D5, "opcode name table");
    return opcode < 0x9D5 ? s_opcodeNames[opcode] : "unknown";
}
