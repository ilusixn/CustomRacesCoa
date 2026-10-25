#include "CoA/Source/Runtime/Events.h"
#include "CoA/Source/Runtime/Log.h"
#include <cstring>
#include <iterator>
#include <vector>

namespace {
    // Every name the original DLL registers through FUN_102783b0.
    const char* const s_eventNames[] = {
#include "CoA/Source/Runtime/Events.generated.inc"
    };

    // List 0x10BE2E74: appended to the glue table (FUN_102783c0 / FUN_10278e70 names).
    const char* const s_glueEventNames[] = {
        "CHARACTER_ACTIVATE_RESULT",
        "CHARACTER_DEACTIVATE_RESULT",
        "CHARACTER_LIST_UPDATED",
        "CHARACTER_CREATE_SELECTION_CHANGED",
        "CHARACTER_CREATE_PREVIEW_CHANGED",
    };

    std::vector<const char*> s_custom;   // DataContainer::m_customFrameEventData
    std::vector<const char*> s_glue;
    std::vector<const char*> s_world;    // DataContainer::m_frameEvent

    CoA::Events::SignalSink s_sink = nullptr;
}

void CoA::Events::SetSignalSink(CoA::Events::SignalSink sink) {
    s_sink = sink;
}

void CoA::Events::Signal(const char* name, const char* format, ...) {
    if (!s_sink) {
        return;
    }

    va_list args;
    va_start(args, format);
    s_sink(name, format, args);
    va_end(args);
}

void CoA::Events::Queue(const char* name) {
    s_custom.push_back(name);
}

bool CoA::Events::Fires(const char* name) {
    for (const char* known : s_eventNames) {
        if (strcmp(known, name) == 0) {
            return true;
        }
    }

    for (const char* known : s_glueEventNames) {
        if (strcmp(known, name) == 0) {
            return true;
        }
    }

    CoA::Log::Printf("Events: %s is not a known DLL event", name);
    return false;
}

void CoA::Events::RegisterNames() {
    for (const char* name : s_eventNames) {
        CoA::Events::Queue(name);
    }

    CoA::Log::Printf("Events: %u custom event names queued for the client event table", static_cast<unsigned>(std::size(s_eventNames)));
}

void CoA::Events::ResetQueue() {
    s_custom.clear();
}

const char** CoA::Events::ExtendList(const char** names, uint32_t& count) {
    if (count == 0x29) {
        s_glue.assign(names, names + count);
        s_glue.insert(s_glue.end(), std::begin(s_glueEventNames), std::end(s_glueEventNames));
        count = static_cast<uint32_t>(s_glue.size());
        return s_glue.data();
    }

    if (count == 0x2D2) {
        // DataContainer::SetupFrameEventVector: the stock list, then the queued customs.
        s_world.assign(names, names + count);
        s_world.insert(s_world.end(), s_custom.begin(), s_custom.end());
        count = static_cast<uint32_t>(s_world.size());
        return s_world.data();
    }

    return names;
}
