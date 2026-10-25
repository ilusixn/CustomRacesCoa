#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Source/Runtime/Log.h"
#include <windows.h>
#include <cstdio>
#include <ctime>
#include <unordered_set>

// Port of AscLogger.cpp (reconstruction): Logger (FUN_100a06d0, static 0x10BDB1E8) keeps one stream per
// channel, opened truncated as Logs\<Channel>.txt (FUN_102917a0 / FUN_102912c0).

static const char* const s_channels[10] = { "Trace", "Info", "Debug", "Warning", "Error", "Fatal", "MissingFiles", "LUA", "LFG", "LargeAlloc" };   // 0x10B45448
static FILE* s_streams[10];
static std::unordered_set<std::string> s_seen[10];

void CoA::Logger::Reopen(uint32_t channel) {
    if (channel >= 10) {
        return;
    }

    if (s_streams[channel]) {
        fclose(s_streams[channel]);
        s_streams[channel] = nullptr;
    }

    CreateDirectoryA("Logs", nullptr);
    std::string path = std::string("Logs\\") + s_channels[channel] + ".txt";
    s_streams[channel] = fopen(path.c_str(), "w");
}

void CoA::Logger::Init() {
    for (uint32_t channel = 0; channel < 10; channel++) {
        CoA::Logger::Reopen(channel);
        s_seen[channel].clear();
    }
}

void CoA::Logger::Shutdown() {
    for (uint32_t channel = 0; channel < 10; channel++) {
        if (s_streams[channel]) {
            fclose(s_streams[channel]);
            s_streams[channel] = nullptr;
        }
    }
}

void CoA::Logger::Write(uint32_t channel, const std::string& line) {
    if (channel >= 10 || !s_streams[channel]) {
        return;
    }

    fputs(line.c_str(), s_streams[channel]);
    fputc('\n', s_streams[channel]);
    fflush(s_streams[channel]);

    // Reconstruction addition: mirrored to the developer log.
    CoA::Log::Printf("[C_Logger:%s] %s", s_channels[channel], line.c_str());
}

void CoA::Logger::WriteOnce(uint32_t channel, const std::string& line) {
    if (channel >= 10 || !s_streams[channel]) {
        return;
    }

    if (!s_seen[channel].insert(line).second) {
        return;
    }

    CoA::Logger::Write(channel, line);
}

// _Xtime_get_ticks (100 ns since 1970) split into seconds ("%Y-%m-%d %H:%M:%S", local time) and zero-padded
// milliseconds.
void CoA::Logger::WriteStamped(uint32_t channel, const std::string& line) {
    if (channel >= 10 || !s_streams[channel]) {
        return;
    }

    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t ticks = ((static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime) - 116444736000000000ull;
    __time64_t seconds = static_cast<__time64_t>(ticks / 10000000);
    unsigned ms = static_cast<unsigned>((ticks / 10000) % 1000);

    tm local;
    _localtime64_s(&local, &seconds);

    char stamp[32];
    size_t n = strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local);
    snprintf(stamp + n, sizeof(stamp) - n, ".%03u", ms);

    CoA::Logger::Write(channel, std::string("[") + stamp + "] " + line);
}
