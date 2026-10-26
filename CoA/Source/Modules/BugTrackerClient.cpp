// AscBugTracker.cpp's client question: the clock (_time64, 0x10b195e0 in the DLL's CRT).
#include "CoA/Source/Modules/BugTracker.h"
#include "CoA/Source/Modules/Modules.h"
#include <ctime>

namespace {
    int64_t Now() {
        return static_cast<int64_t>(time(nullptr));
    }
}

void CoA::BugTrackerClient::Init() {
    CoA::BugTracker::Dependencies().now = Now;
}
