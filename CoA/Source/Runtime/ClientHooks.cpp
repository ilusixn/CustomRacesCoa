#include "CoA/Source/Runtime/ClientHooks.h"
#include "CoA/Source/Runtime/Logger.h"
#include <cstdio>

namespace {
    const char* s_lastSignalEvent = nullptr;
    bool s_initialSpellsFlag = false;
}

bool CoA::ClientHooks::AllowSignalEvent(int32_t eventId, lua_State* L, uint32_t registeredCount, bool hasEventData,
    bool hasEventObject, const char* name) {
    if (!L || !hasEventObject) {
        char line[256];
        snprintf(line, sizeof(line),
            "Blocked invalid FrameScript signal event at 0x0081AA00: eventId=%d, registeredEventCount=%u, "
            "luaState=0x%x, hasEventData=%s, hasEventObject=%s",
            eventId, registeredCount, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(L)), hasEventData ? "true" : "false",
            hasEventObject ? "true" : "false");
        CoA::Logger::Write(CoA::Logger::Fatal, line);
        return false;
    }

    for (const auto& entry : CoA::ClientHooks::stockEvents) {
        if (entry.first == eventId) {
            entry.second(L);
        }
    }

    if (name && *name) {
        s_lastSignalEvent = name;
    }

    return true;
}

const char* CoA::ClientHooks::LastSignalEvent() {
    return s_lastSignalEvent;
}

void CoA::ClientHooks::InitialSpellsReceived() {
    if (s_initialSpellsFlag) {
        CoA::ClientHooks::initialSpellsAlternate.Run();
        s_initialSpellsFlag = false;
    } else {
        s_initialSpellsFlag = true;
    }
}

void CoA::ClientHooks::Reset() {
    s_lastSignalEvent = nullptr;
    s_initialSpellsFlag = false;
    CoA::ClientHooks::stockEvents.clear();
    CoA::ClientHooks::initialSpellsAlternate.Clear();
    CoA::ClientHooks::addAuraEffect.Clear();
    CoA::ClientHooks::removeEffect.Clear();
    CoA::ClientHooks::addKnownSpell = nullptr;
    CoA::ClientHooks::afterAddKnownSpell.Clear();
}
