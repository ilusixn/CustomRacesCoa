#ifndef COA_SOURCE_RUNTIME_LOGGER_H
#define COA_SOURCE_RUNTIME_LOGGER_H

#include <cstdint>
#include <string>

// The original's channel logger (C_Logger streams, port of AscLogger.cpp): one stream per channel, opened
// truncated as Logs\<Channel>.txt at attach. Always on. The Lua bindings (C_Logger.*) are ported with the
// module that registers them.
namespace CoA::Logger {
    enum Channel : uint32_t {
        Trace = 0,
        Info = 1,
        Debug = 2,
        Warning = 3,
        Error = 4,
        Fatal = 5,
        MissingFiles = 6,
        LUA = 7,
        LFG = 8,
        LargeAlloc = 9,
    };

    void Init();
    void Shutdown();
    // FUN_102912c0: reopen one channel's file, truncated (ClearLUA).
    void Reopen(uint32_t channel);
    // FUN_10293640: the line and a newline; dropped when the channel has no stream.
    void Write(uint32_t channel, const std::string& line);
    // FUN_10293aa0: Write, each distinct line once per channel.
    void WriteOnce(uint32_t channel, const std::string& line);
    // FUN_102936d0: "[YYYY-MM-DD HH:MM:SS.mmm] <line>".
    void WriteStamped(uint32_t channel, const std::string& line);
}

#endif
