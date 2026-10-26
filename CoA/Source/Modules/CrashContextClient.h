#ifndef COA_SOURCE_MODULES_CRASHCONTEXTCLIENT_H
#define COA_SOURCE_MODULES_CRASHCONTEXTCLIENT_H

#include <cstdint>

// AscCrashContext.cpp's client half: the Engine recorder pointers and the crash-text writer.
namespace CoA::CrashContextClient {
    // FUN_1019e5f0 over 0x682200, after GxLogCrashInfo: appends the report to the client's crash text.
    void AppendReport(char* buffer, uint32_t size);

    void Init();
}

#endif
