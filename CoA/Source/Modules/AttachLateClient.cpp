// AscAttachLate.cpp's client half: AscCrashContext::Assert as the client's fatal error.
#include "CoA/Source/Modules/AttachLate.h"
#include "CoA/Source/Modules/Modules.h"
#include "Engine/Source/Storm/H/storm.h"

namespace {
    void Fatal(const char* text) {
        SErrDisplayAppFatal("%s", text);
    }
}

void CoA::AttachLateClient::Init() {
    CoA::AttachLate::SetFatalSink(Fatal);
}
