#ifndef COA_SOURCE_RUNTIME_EVENTS_H
#define COA_SOURCE_RUNTIME_EVENTS_H

#include <cstdarg>
#include <cstdint>
#include <vector>

// The DLL's FrameXML events (port of AscRuntime's event code and the scaffold's DataContainer event list).
namespace CoA::Events {
    // DataContainer::RegisterCustomEvent: a name for the world event table; called from module Init.
    void Queue(const char* name);
    // AscRuntime::RegisterEventNames: queues the original DLL's 303 names (Events.generated.inc).
    void RegisterNames();
    // Tests only.
    void ResetQueue();
    // The queued custom names (DataContainer::m_customFrameEventData, 0x10BE2E58), in queue order.
    const std::vector<const char*>& CustomNames();

    // FUN_10278b50, the DLL's detour on FrameScript_CreateEvents (0x81B5F0). The client's two calls are told apart
    // by their count: 0x29 (glue) gets the five glue customs appended, 0x2D2 (world) the queued customs; any other
    // list passes through. Returns the list to register and updates count.
    const char** ExtendList(const char** names, uint32_t& count);

    // FUN_10278c90: the event's index in the client's table (hash first, name compare to confirm), or -1.
    int32_t Id(const char* name);
    // The event name at an index of the client's table, or nullptr.
    const char* Name(int32_t id);
    // AscRuntime::Signal: FrameScript_SignalEvent (0x81B530) when the client knows the name. Goes to the installed
    // sink (EventsClient.cpp's InstallClient; tests install a recorder); nothing happens without one.
    void Signal(const char* name, const char* format, ...);
    using SignalSink = void (*)(const char* name, const char* format, va_list args);
    void SetSignalSink(SignalSink sink);
    // EventsClient.cpp: the FrameScript sink.
    void InstallClient();
}

namespace CoA::Events {
    // A module declares an event it fires (the names themselves are all queued by RegisterNames). Logs a name the
    // client table will not have; returns whether it is known.
    bool Fires(const char* name);
}

// Declares an event a module fires; Tools/coa/inventory.py counts these.
#define COA_EVENT(name) CoA::Events::Fires(name)

#endif
