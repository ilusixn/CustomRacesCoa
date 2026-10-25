#include "CoA/Source/Modules/AttachMisc.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Source/Runtime/Packet.h"
#include <cstdio>
#include <cstring>

const char* CoA::AttachMisc::CVarSetValue(const char* name, const char* value) {
    return name && strcmp(name, "scriptProfile") == 0 ? "0" : value;
}

const char* CoA::AttachMisc::NameplateMessage(const char* name, const char* value) {
    if (!name || strcmp(name, "nameplateShowEnemies") != 0) {
        return nullptr;
    }

    return value && strcmp(value, "0") == 0 ? "NAMEPLATES_MESSAGE_ALL_OFF" : "NAMEPLATES_MESSAGE_ALL_ON_AUTO";
}

bool CoA::AttachMisc::AllowFormattedSignal(int32_t eventId, uint32_t registeredCount, bool hasEventData, bool hasEventObject,
    const void* format, const void* va) {
    if (hasEventData && eventId >= 0 && static_cast<uint32_t>(eventId) < registeredCount && hasEventObject) {
        return true;
    }

    char line[256];
    snprintf(line, sizeof(line),
        "Blocked invalid FrameScript signal event at 0x0081AC90: eventId=%d, registeredEventCount=%u, format=0x%x, "
        "vaList=0x%X, hasEventData=%s, hasEventObject=false",
        eventId, registeredCount, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(format)),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(va)), hasEventData ? "true" : "false");
    CoA::Logger::Write(CoA::Logger::Fatal, line);
    return false;
}

void CoA::AttachMisc::LoginVerifyWorld() {
    CoA::Packet(0x53B).Send();   // FUN_100e08b0
    CoA::AttachMisc::loginVerifyWorld.Run();
}

bool CoA::AttachMisc::StopsSwing(uint32_t attributes, uint32_t attributesEx, uint32_t attributesEx2, int32_t reason) {
    return ((attributes & 0x404) || ((attributesEx >> 9) & 1) || ((attributesEx2 >> 0x14) & 1)) && reason == 0x55;
}
