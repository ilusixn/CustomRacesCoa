#include "CoA/Source/Runtime/Script.h"
#include <Windows.h>
#include <shellapi.h>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "shell32.lib")

const char* CoA::Script::CommandLineArg(const char* name) {
    // The DLL keeps argv as a vector<std::string> built once at startup.
    static std::vector<std::string> argv;
    static bool parsed = false;

    if (!parsed) {
        parsed = true;
        int argc = 0;

        if (LPWSTR* wide = CommandLineToArgvW(GetCommandLineW(), &argc)) {
            for (int i = 0; i < argc; ++i) {
                char buffer[1024];
                WideCharToMultiByte(CP_ACP, 0, wide[i], -1, buffer, sizeof(buffer), nullptr, nullptr);
                argv.emplace_back(buffer);
            }

            LocalFree(wide);
        }
    }

    for (size_t i = 1; i + 1 < argv.size(); ++i) {
        const char* arg = argv[i].c_str();

        if (*arg != '-' && *arg != '/') {
            continue;
        }

        ++arg;

        if (*arg == '-') {
            ++arg;
        }

        if (strcmp(arg, name) == 0) {
            return argv[i + 1].c_str();
        }
    }

    return nullptr;
}
