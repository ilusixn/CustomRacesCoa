// SMSG handlers that turn a packet straight into a UI event (port of AscEventPackets.cpp). Ported so far: the
// character select results.
//   0x75F / 0x760 (FUN_10190700 / FUN_10190a30) C string: its index in the ACTIVATE_CHARACTER_* (8) /
//                 DEACTIVATE_CHARACTER_* (7) names, else the Logger (channel 2) line "Unexpected Value: <s>" and the last
//                 (_FAILED); then CHARACTER_ACTIVATE_RESULT / CHARACTER_DEACTIVATE_RESULT("%b%s", index == 0, s)
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Read.h"
#include <string>

namespace {
    void CharacterResult(CDataStore* msg, const char* const* names, uint32_t count, const char* event) {
        std::string s = CoA::Read::Str(msg);
        uint32_t index = 0;

        while (index < count && s != names[index]) {
            ++index;
        }

        if (index == count) {
            CoA::Logger::Write(CoA::Logger::Debug, "Unexpected Value: " + s);
            index = count - 1;
        }

        CoA::Events::Signal(event, "%b%s", index == 0 ? 1u : 0u, s.c_str());
    }

    int32_t On75F(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        static const char* const names[8] = {
            "ACTIVATE_CHARACTER_OK",
            "ACTIVATE_CHARACTER_NOT_FOUND",
            "ACTIVATE_CHARACTER_NOT_OWNED",
            "ACTIVATE_CHARACTER_ALREADY_ACTIVE",
            "ACTIVATE_CHARACTER_TRANSFER_LOCKED",
            "ACTIVATE_CHARACTER_ONLINE",
            "ACTIVATE_CHARACTER_MAX_ACTIVE",
            "ACTIVATE_CHARACTER_FAILED",
        };

        CharacterResult(msg, names, 8, "CHARACTER_ACTIVATE_RESULT");
        return 1;
    }

    int32_t On760(void* param, uint32_t opcode, uint32_t time, CDataStore* msg) {
        static const char* const names[7] = {
            "DEACTIVATE_CHARACTER_OK",
            "DEACTIVATE_CHARACTER_NOT_FOUND",
            "DEACTIVATE_CHARACTER_NOT_OWNED",
            "DEACTIVATE_CHARACTER_ALREADY_INACTIVE",
            "DEACTIVATE_CHARACTER_TRANSFER_LOCKED",
            "DEACTIVATE_CHARACTER_ONLINE",
            "DEACTIVATE_CHARACTER_FAILED",
        };

        CharacterResult(msg, names, 7, "CHARACTER_DEACTIVATE_RESULT");
        return 1;
    }
}

void CoA::EventPackets::Init() {
    COA_PACKET(0x75F, On75F);
    COA_PACKET(0x760, On760);
    COA_EVENT("CHARACTER_ACTIVATE_RESULT");
    COA_EVENT("CHARACTER_DEACTIVATE_RESULT");
}
