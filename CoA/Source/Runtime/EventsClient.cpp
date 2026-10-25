#include "CoA/Source/Runtime/Events.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Storm/H/storm.h"
#include <cstdarg>
#include <cstring>

// FUN_10278c90: hash first (SStrHashHT, 0x76F640), name compare to confirm.
int32_t CoA::Events::Id(const char* name) {
    uint32_t count = FrameScript_GetEventCount();

    if (!count) {
        return -1;
    }

    uint32_t hash = SStrHashHT(name);

    for (uint32_t i = 0; i < count; ++i) {
        FrameScript_EventObject* event = FrameScript_GetEvent(i);

        if (!event) {
            continue;
        }

        if (event->m_hashval == hash || std::strcmp(event->m_key.m_str, name) == 0) {
            return static_cast<int32_t>(i);
        }
    }

    return -1;
}

const char* CoA::Events::Name(int32_t id) {
    if (id < 0) {
        return nullptr;
    }

    FrameScript_EventObject* event = FrameScript_GetEvent(static_cast<uint32_t>(id));
    return event ? event->m_key.m_str : nullptr;
}

void CoA::Events::Signal(const char* name, const char* format, ...) {
    int32_t id = CoA::Events::Id(name);

    if (id < 0) {
        return;
    }

    va_list args;
    va_start(args, format);
    vFrameScript_SignalEvent(static_cast<uint32_t>(id), format, args);
    va_end(args);
}
