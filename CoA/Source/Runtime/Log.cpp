#include "CoA/Source/Runtime/Log.h"
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// Port of AscLog.cpp (reconstruction).

static FILE* s_file;
static bool s_on;

// "-extlog <n>" (or /extlog, --extlog), value parsed like -autologin / -renderdebug: atoi(n) != 0.
bool CoA::Log::Requested(const char* commandLine) {
    const char* p = commandLine;
    char token[256];
    bool want = false;

    while (p && *p) {
        while (*p == ' ' || *p == '\t') {
            ++p;
        }

        if (!*p) {
            break;
        }

        size_t n = 0;
        bool quoted = false;

        for (; *p && (quoted || (*p != ' ' && *p != '\t')); ++p) {
            if (*p == '"') {
                quoted = !quoted;
            } else if (n + 1 < sizeof(token)) {
                token[n++] = *p;
            }
        }

        token[n] = 0;

        if (want) {
            return atoi(token) != 0;
        }

        const char* t = token;

        if (*t == '-' || *t == '/') {
            ++t;

            if (*t == '-') {
                ++t;
            }

            want = _stricmp(t, "extlog") == 0;
        }
    }

    return false;
}

void CoA::Log::Init() {
    s_on = Requested(GetCommandLineA());

    if (!s_on) {
        return;
    }

    CreateDirectoryA("Logs", nullptr);
    s_file = fopen("Logs\\Extensions.log", "a");
    Printf("=== CoA64 attached, pid %lu ===", GetCurrentProcessId());
}

bool CoA::Log::Enabled() {
    return s_on;
}

void CoA::Log::Printf(const char* fmt, ...) {
    if (!s_on) {
        return;
    }

    char buf[9216];
    va_list ap;
    va_start(ap, fmt);
    int n = _vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);

    if (n < 0) {
        n = static_cast<int>(sizeof(buf)) - 2;
    }

    buf[n] = 0;

    char line[2100];
    time_t t = time(nullptr);
    struct tm* tm = localtime(&t);
    _snprintf(line, sizeof(line), "%02d:%02d:%02d [Ext] %s\n", tm->tm_hour, tm->tm_min, tm->tm_sec, buf);
    OutputDebugStringA(line);

    if (s_file) {
        fputs(line, s_file);
        fflush(s_file);
    }
}
