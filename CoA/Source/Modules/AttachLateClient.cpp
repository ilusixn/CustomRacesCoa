// AscAttachLate.cpp's client half: AscCrashContext::Assert (the report's "Assert Info", then the client's fatal error).
#include "CoA/Source/Modules/AttachLate.h"
#include "CoA/Source/Modules/CrashContext.h"
#include "CoA/Source/Modules/Modules.h"
#include "Engine/Source/Storm/H/storm.h"

namespace {
    void Fatal(const char* text) {
        CoA::CrashContext::Assert(text, nullptr, 0);
    }
}

void CoA::AttachLateClient::Init() {
    CoA::AttachLate::SetFatalSink(Fatal);
}
