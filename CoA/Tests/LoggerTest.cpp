#include "CoA/Source/Runtime/Logger.h"
#include "CoA/Tests/TestMain.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace {
    // Runs the logger in a fresh temp directory, since it writes Logs\<Channel>.txt relative to the cwd.
    struct TempCwd {
        char previous[MAX_PATH];
        char path[MAX_PATH];

        TempCwd() {
            GetCurrentDirectoryA(MAX_PATH, previous);
            char base[MAX_PATH];
            GetTempPathA(MAX_PATH, base);
            snprintf(path, sizeof(path), "%sCoATests_%lu_%lu", base, GetCurrentProcessId(), GetTickCount());
            CreateDirectoryA(path, nullptr);
            SetCurrentDirectoryA(path);
        }

        ~TempCwd() {
            CoA::Logger::Shutdown();
            SetCurrentDirectoryA(previous);
        }
    };

    std::string ReadAll(const char* file) {
        std::string text;
        FILE* f = fopen(file, "rb");

        if (!f) {
            return text;
        }

        char buf[512];
        size_t n;

        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
            text.append(buf, n);
        }

        fclose(f);
        return text;
    }
}

COA_TEST(Logger_WriteGoesToTheChannelFile) {
    TempCwd cwd;
    CoA::Logger::Init();
    CoA::Logger::Write(CoA::Logger::Fatal, "Unhandled packet: SMSG_TEST (1503)");
    // text-mode stream, as the original's ofstream: lines end in CRLF on disk
    CHECK(ReadAll("Logs\\Fatal.txt") == "Unhandled packet: SMSG_TEST (1503)\r\n");
    CHECK(ReadAll("Logs\\Error.txt").empty());
}

COA_TEST(Logger_WriteOnceDropsRepeats) {
    TempCwd cwd;
    CoA::Logger::Init();
    CoA::Logger::WriteOnce(CoA::Logger::MissingFiles, "a.blp");
    CoA::Logger::WriteOnce(CoA::Logger::MissingFiles, "a.blp");
    CoA::Logger::WriteOnce(CoA::Logger::MissingFiles, "b.blp");
    CHECK(ReadAll("Logs\\MissingFiles.txt") == "a.blp\r\nb.blp\r\n");
}

COA_TEST(Logger_InitTruncatesAndChannelOutOfRangeIsDropped) {
    TempCwd cwd;
    CoA::Logger::Init();
    CoA::Logger::Write(CoA::Logger::Info, "old");
    CoA::Logger::Init();
    CoA::Logger::Write(10, "nowhere");
    CHECK(ReadAll("Logs\\Info.txt").empty());
}

COA_TEST(Logger_WriteStampedHasMillisecondStamp) {
    TempCwd cwd;
    CoA::Logger::Init();
    CoA::Logger::WriteStamped(CoA::Logger::Debug, "tick");
    std::string text = ReadAll("Logs\\Debug.txt");
    // "[YYYY-MM-DD HH:MM:SS.mmm] tick\r\n"
    CHECK(text.size() == 32);

    if (text.size() == 32) {
        CHECK(text[0] == '[' && text[24] == ']' && text[20] == '.');
        CHECK(text.substr(26) == "tick\r\n");
    }
}
