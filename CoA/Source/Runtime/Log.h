#ifndef COA_SOURCE_RUNTIME_LOG_H
#define COA_SOURCE_RUNTIME_LOG_H

// The reconstruction's own developer log (AscLog): Logs\Extensions.log + OutputDebugString. Not part of the
// original, so off by default; `-extlog <n>` with n != 0 turns it on.
namespace CoA::Log {
    bool Requested(const char* commandLine);
    void Init();
    bool Enabled();
    void Printf(const char* fmt, ...);
}

#endif
