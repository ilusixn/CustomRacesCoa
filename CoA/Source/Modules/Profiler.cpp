#include "CoA/Source/Modules/Profiler.h"
#include "CoA/Source/Modules/Modules.h"
#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Source/Runtime/LuaReg.h"
#include "CoA/Source/Runtime/Packets.h"
#include "CoA/Source/Runtime/Script.h"
#include "CoA/Source/Runtime/Timer.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
    struct Record {
        uint32_t bytes = 0;
        uint32_t maxSize = 0;
        uint32_t minSize = 0xFFFFFFFF;
        uint32_t count = 0;
        uint64_t lastMs = 0;
        uint64_t firstMs = 0;
        float avgSize = 0;
        float packetsPerSec = 0;
        float bytesPerSec = 0;
        uint64_t totalUs = 0;
        uint64_t maxUs = 0;
        uint64_t minUs = 0xFFFFFFFFFFFFFFFFull;
        float avgUs = 0;
        bool timed = false;
    };

    // MSVC's std::hash<std::string> is the original's FNV-1a (FUN_102c2f30), so iteration order matches.
    std::unordered_map<std::string, Record> s_opcodes;   // 0x10BE3788
    bool s_opcodeLogging = false;                        // 0x10BE3778
    bool s_eventLogging = false;                         // 0x10BE3779 (no reader)
    uint32_t s_opcodeTimer = 0;                          // 0x10BE37D4
    uint32_t s_eventTimer = 0;                           // 0x10BE37D8
    int32_t s_currentOpcode = 0;                         // 0x10BE3728

    int64_t NowNs() {   // FUN_100b8230
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    uint64_t NowMs() {   // _Xtime_get_ticks() / 10000
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    }

    std::string Key(const char* prefix, int32_t opcode) {   // "{} (0x{:08X})"
        char buffer[160];
        snprintf(buffer, sizeof(buffer), "%s%s (0x%08X)", prefix, CoA::Packets::Name(static_cast<uint32_t>(opcode)), static_cast<uint32_t>(opcode));
        return buffer;
    }

    // FUN_102c45b0
    void CountPacket(Record& r, uint32_t size, uint64_t ms) {
        if (r.firstMs == 0) {
            r.firstMs = ms;
        }

        r.bytes += size;
        r.maxSize = std::max(r.maxSize, size);
        r.minSize = std::min(r.minSize, size);
        ++r.count;
        r.lastMs = ms;
        float bytes = static_cast<float>(static_cast<double>(r.bytes));
        float count = static_cast<float>(static_cast<double>(r.count));
        r.avgSize = bytes / count;
        float seconds = std::max(static_cast<float>(r.lastMs - r.firstMs) / 1000.0f, 1.0f);
        r.packetsPerSec = count / seconds;
        r.bytesPerSec = bytes / seconds;
    }

    // FUN_102c46a0
    void TimePacket(Record& r, uint64_t us) {
        r.timed = true;
        r.totalUs += us;
        r.maxUs = std::max(r.maxUs, us);
        r.minUs = std::min(r.minUs, us);
        r.avgUs = static_cast<float>(r.totalUs) / static_cast<float>(static_cast<double>(r.count));
    }

    // FUN_102d5a20
    void ResetOpcodes() {
        for (auto& e : s_opcodes) {
            e.second = Record {};
        }
    }

    // ---- formatting ---------------------------------------------------------------------------------------------
    std::string Shortest(float v) {   // fmt "{}" of a float
        char buffer[48];
        auto result = std::to_chars(buffer, buffer + sizeof(buffer), v);
        return std::string(buffer, result.ptr);
    }

    std::string Fixed(const char* format, double v) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), format, v);
        return buffer;
    }

    std::string FormatBytes(uint32_t n) {   // FUN_102d3360
        if (n < 0x400) {
            return std::to_string(n) + " B";
        }

        if (n < 0x100000) {
            return Fixed("%.2f KB", static_cast<float>(static_cast<double>(n)) * 0.0009765625f);
        }

        return Fixed("%.2f MB", static_cast<float>(static_cast<double>(n)) * 9.536743e-07f);
    }

    std::string Micros(uint64_t us) {   // FUN_102d3450
        if (us == 0xFFFFFFFFFFFFFFFFull) {
            return "N/A";
        }

        if (us < 1000) {
            return std::to_string(us) + " \xC2\xB5s";
        }

        if (us < 1000000) {
            return Fixed("%.2f ms", static_cast<float>(us) / 1000.0f);
        }

        return Fixed("%.2f s", static_cast<float>(us) / 1e+06f);
    }

    // "{:<80} {:>10} {:>10} {:>10} {:>10} {:>10}"
    std::string Columns(const std::string& a, const std::string& b, const std::string& c, const std::string& d, const std::string& e, const std::string& f) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "%-80s %10s %10s %10s %10s %10s", a.c_str(), b.c_str(), c.c_str(), d.c_str(), e.c_str(), f.c_str());
        return buffer;
    }

    void Trace(const std::string& line) {
        CoA::Logger::Write(CoA::Logger::Trace, line);
    }

    // FUN_102d2620: per-record warnings, then the block when there are any.
    void Warnings() {
        std::vector<std::string> out;

        for (const auto& e : s_opcodes) {
            const Record& r = e.second;
            std::vector<std::string> w;

            if (1048576.0f < r.bytesPerSec) {
                w.push_back("- High bandwidth usage: " + FormatBytes(static_cast<uint32_t>(static_cast<int64_t>(r.bytesPerSec))) + "/s");
            }

            if (0x500000 < r.bytes) {
                w.push_back("- High total bandwidth: " + FormatBytes(r.bytes));
            }

            if (1000.0f < r.packetsPerSec) {
                w.push_back(Fixed("- High packet rate: %.1f packets/s", r.packetsPerSec));
            }

            if (5000 < r.count) {
                w.push_back("- High total packet count: " + std::to_string(r.count) + " packets");
            }

            float maxSize = static_cast<float>(static_cast<double>(r.maxSize));

            if (r.avgSize + r.avgSize < maxSize) {
                w.push_back("- Packet size spike detected: max " + FormatBytes(r.maxSize) + " (" +
                    Shortest(maxSize / (r.avgSize <= 0.0f ? 1.0f : r.avgSize)) + "x average)");
            }

            if (r.timed && r.avgUs + r.avgUs < static_cast<float>(r.maxUs)) {
                w.push_back("- Performance spike detected: max " + Micros(r.maxUs) + " (" +
                    Shortest(static_cast<float>(r.maxUs) / (r.avgUs <= 0.0f ? 1.0f : r.avgUs)) + "x average)");
            }

            float avgSize = r.avgSize <= 0.0f ? 1.0f : r.avgSize;
            int32_t spread = static_cast<int32_t>(r.maxSize - r.minSize);
            float spreadF = static_cast<float>(static_cast<double>(static_cast<uint32_t>(spread)));

            if (5.0f < spreadF / avgSize) {
                w.push_back("- High packet size variance: " + FormatBytes(r.minSize) + " to " + FormatBytes(r.maxSize) + " (" +
                    Shortest(spreadF / avgSize) + "x difference)");
            }

            if (r.timed) {
                float avgUs = r.avgUs <= 0.0f ? 1.0f : r.avgUs;

                if (5.0f < static_cast<float>(r.maxUs) / avgUs) {
                    w.push_back("- High processing time variance: " + Micros(r.minUs) + " to " + Micros(r.maxUs) + " (" +
                        Shortest(static_cast<float>(r.maxUs) / avgUs) + "x difference)");
                }
            }

            if (!w.empty()) {
                out.push_back("\nWarnings for " + e.first + ":");
                out.insert(out.end(), w.begin(), w.end());
            }
        }

        if (out.empty()) {
            return;
        }

        std::string rule(0x50, '=');
        Trace(rule);
        Trace("=== Network Analysis Warnings ===");

        for (const std::string& line : out) {
            Trace(line);
        }

        Trace(rule);
    }

    // FUN_102d35a0
    void DumpOpcodes() {
        struct Row {
            std::string key;
            Record r;
        };

        std::vector<Row> rows;
        uint32_t bytes = 0;
        uint32_t count = 0;
        uint64_t timedUs = 0;
        uint64_t firstMs = 0xFFFFFFFFFFFFFFFFull;
        uint64_t lastMs = 0;

        for (const auto& e : s_opcodes) {
            const Record& r = e.second;

            if (!r.count) {
                continue;
            }

            rows.push_back({ e.first, r });
            bytes += r.bytes;
            count += r.count;

            if (r.timed) {
                timedUs += r.totalUs;
            }

            if (r.firstMs && r.firstMs <= firstMs) {
                firstMs = r.firstMs;
            }

            if (r.lastMs && r.lastMs >= lastMs) {
                lastMs = r.lastMs;
            }
        }

        if (rows.empty()) {
            ResetOpcodes();
            return;
        }

        std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.r.bytesPerSec > b.r.bytesPerSec; });

        float seconds = static_cast<float>(lastMs - firstMs) / 1000.0f;

        if (seconds <= 1.0f) {
            seconds = 1.0f;
        }

        float countF = static_cast<float>(static_cast<double>(count));
        float avgUs = count ? static_cast<float>(timedUs) / countF : 0.0f;
        float packetsPerSec = countF / seconds;
        float bytesPerSec = static_cast<float>(static_cast<double>(bytes)) / seconds;

        char clock[32];
        time_t now = time(nullptr);
        tm local;
        localtime_s(&local, &now);
        strftime(clock, sizeof(clock), "%H:%M:%S", &local);
        std::string header = std::string("=== Network Statistics (") + clock + ") ===";
        std::string rule(header.size(), '=');
        Trace(rule);
        Trace(header);
        Trace(rule);
        Trace(Fixed("Duration: %.1fs", seconds) + " | Bandwidth: " + FormatBytes(bytes) + " (" +
            FormatBytes(static_cast<uint32_t>(static_cast<int64_t>(bytesPerSec))) + "/s) | Packets: " + std::to_string(count) +
            Fixed(" (%.1f/s)", packetsPerSec) + " | Processing: " + Micros(timedUs) + " (avg: " +
            Micros(static_cast<uint64_t>(static_cast<int64_t>(avgUs))) + ")");
        Trace(rule);
        Trace(Columns("Opcode", "BW/s", "Total", "Pkts/s", "Count", "Avg Size"));
        Trace(Columns("", "Min Time", "Max Time", "Avg Time", "Total Time", "% Time"));

        for (const Row& row : rows) {
            const Record& r = row.r;
            std::string name = row.key;

            if (name.size() > 0x4D) {
                name = name.substr(0, 0x4A) + "...";
            }

            std::string first = Columns(name, FormatBytes(static_cast<uint32_t>(static_cast<int64_t>(r.bytesPerSec))), FormatBytes(r.bytes),
                Shortest(r.packetsPerSec), std::to_string(r.count), FormatBytes(static_cast<uint32_t>(static_cast<int64_t>(r.avgSize))));
            float pct = timedUs ? static_cast<float>(r.totalUs) * 100.0f / static_cast<float>(timedUs) : 0.0f;
            char second[512];
            snprintf(second, sizeof(second), "%-80s %10s %10s %10s %10s %9.1f%%", "",
                r.timed ? Micros(r.minUs).c_str() : "N/A", r.timed ? Micros(r.maxUs).c_str() : "N/A",
                r.timed ? Micros(static_cast<uint64_t>(static_cast<int64_t>(r.avgUs))).c_str() : "N/A",
                r.timed ? Micros(r.totalUs).c_str() : "N/A", pct);

            if (1048576.0f < r.bytesPerSec || 1000.0f < r.packetsPerSec || r.avgSize + r.avgSize < static_cast<float>(static_cast<double>(r.maxSize))) {
                first += " [!]";
            } else if (r.timed && r.avgUs + r.avgUs < static_cast<float>(r.maxUs)) {
                first += " [!]";
            }

            Trace(first);
            Trace(second);
        }

        Trace(rule);
        Warnings();
        ResetOpcodes();
    }

    // FUN_102d4ba0 over an always-empty map: nothing to write.
    void DumpEvents() {
    }

    int32_t OpcodeTimer(const void*, void*) {   // LAB_102d5cb0
        DumpOpcodes();
        CoA::Timer::Schedule(5000, OpcodeTimer, nullptr);
        return 0;
    }

    int32_t EventTimer(const void*, void*) {   // LAB_102d5cd0
        DumpEvents();
        CoA::Timer::Schedule(5000, EventTimer, nullptr);
        return 0;
    }

    // The shared body of Set{Opcode,SignalEvent}LoggingEnabled (FUN_100b8b40: {boolean, boolean}).
    bool SetLogging(lua_State* L, bool& flag) {
        if (!CoA::Script::ValidateInput(L, { LUA_TBOOLEAN, LUA_TBOOLEAN })) {
            return false;
        }

        bool on = lua_toboolean(L, 1) != 0;
        bool periodic = lua_toboolean(L, 2) != 0;
        flag = on;

        if (s_opcodeTimer) {
            CoA::Timer::Cancel(s_opcodeTimer, OpcodeTimer, nullptr);
            s_opcodeTimer = 0;
        }

        if (s_eventTimer) {
            CoA::Timer::Cancel(s_eventTimer, EventTimer, nullptr);
            s_eventTimer = 0;
        }

        if (periodic && on) {
            s_opcodeTimer = CoA::Timer::Schedule(5000, OpcodeTimer, nullptr);   // FUN_102d5c70
            s_eventTimer = CoA::Timer::Schedule(5000, EventTimer, nullptr);
        }

        return true;
    }

    int LogOpcodes(lua_State*) {
        DumpOpcodes();
        return 0;
    }

    int LogSignalEvents(lua_State*) {
        DumpEvents();
        return 0;
    }

    int ResetOpcodeLogging(lua_State*) {
        ResetOpcodes();
        return 0;
    }

    int ResetSignalEventLogging(lua_State*) {
        return 0;
    }

    int SetOpcodeLoggingEnabled(lua_State* L) {
        if (SetLogging(L, s_opcodeLogging)) {
            ResetOpcodes();
        }

        return 0;
    }

    int SetSignalEventLoggingEnabled(lua_State* L) {
        SetLogging(L, s_eventLogging);
        return 0;
    }

    const CoA::Binding s_bindings[] = {
        COA_NATIVE("", "LogOpcodes", LogOpcodes),
        COA_NATIVE("", "LogSignalEvents", LogSignalEvents),
        COA_NATIVE("", "ResetOpcodeLogging", ResetOpcodeLogging),
        COA_NATIVE("", "ResetSignalEventLogging", ResetSignalEventLogging),
        COA_NATIVE("", "SetOpcodeLoggingEnabled", SetOpcodeLoggingEnabled),
        COA_NATIVE("", "SetSignalEventLoggingEnabled", SetSignalEventLoggingEnabled),
    };
}

int64_t CoA::Profiler::Received(int32_t opcode, uint32_t size) {
    s_currentOpcode = opcode;

    if (!s_opcodeLogging) {
        return 0;
    }

    int64_t start = NowNs();
    CountPacket(s_opcodes[Key("", opcode)], size, NowMs());
    return start;
}

void CoA::Profiler::Handled(int32_t opcode, int64_t start) {
    if (start) {
        TimePacket(s_opcodes[Key("", opcode)], static_cast<uint64_t>((NowNs() - start) / 1000));
    }
}

int64_t CoA::Profiler::SendStarted(int32_t opcode, uint32_t size) {
    if (!s_opcodeLogging) {
        return 0;
    }

    int64_t start = NowNs();
    CountPacket(s_opcodes[Key("SENT ", opcode)], size, NowMs());
    return start;
}

void CoA::Profiler::SendFinished(int32_t opcode, int64_t start) {
    if (start) {
        TimePacket(s_opcodes[Key("SENT ", opcode)], static_cast<uint64_t>((NowNs() - start) / 1000));
    }
}

int32_t CoA::Profiler::CurrentOpcode() {
    return s_currentOpcode;
}

uint32_t CoA::Profiler::Count(int32_t opcode) {
    auto it = s_opcodes.find(Key("", opcode));
    return it != s_opcodes.end() ? it->second.count : 0;
}

uint32_t CoA::Profiler::Bytes(int32_t opcode) {
    auto it = s_opcodes.find(Key("", opcode));
    return it != s_opcodes.end() ? it->second.bytes : 0;
}

void CoA::Profiler::Reset() {
    s_opcodes.clear();
    s_opcodeLogging = false;
    s_eventLogging = false;
    s_opcodeTimer = 0;
    s_eventTimer = 0;
    s_currentOpcode = 0;
}

void CoA::Profiler::Init() {
    CoA::LuaReg::AddModule(s_bindings, sizeof(s_bindings) / sizeof(s_bindings[0]));
}
