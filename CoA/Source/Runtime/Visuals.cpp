#include "CoA/Source/Runtime/Visuals.h"
#include "CoA/Source/Runtime/Log.h"
#include "CoA/Source/Runtime/Lumenveil.h"
#include "CoA/Source/Runtime/LuxVelum.h"
#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>

namespace {
    // Before the choice existed LuxVelum was the only mod; keep that answer when nothing says otherwise.
    constexpr unsigned kDefault = CoA::Visuals::LuxVelum;

    std::wstring ExeDir() {
        wchar_t path[MAX_PATH];
        DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring dir(path, n);
        size_t slash = dir.find_last_of(L"\\/");
        return slash == std::wstring::npos ? std::wstring(L".") : dir.substr(0, slash);
    }

    // LuxVelum's fogdata.bin is format version 4; Lumenveil's engine reads version 3 and says "invalid" otherwise.
    // Keep LuxVelum's file aside so the chosen mod finds (or does not find) its own data.
    bool LuxFogData(const std::wstring& path) {
        FILE* file = nullptr;

        if (_wfopen_s(&file, path.c_str(), L"rb") || !file) {
            return false;
        }

        char head[8] = {};
        size_t read = fread(head, 1, sizeof(head), file);
        fclose(file);
        unsigned version = 0;

        if (read == sizeof(head)) {
            memcpy(&version, head + 4, sizeof(version));
        }

        return memcmp(head, "VFD1", 4) == 0 && version == 4;
    }

    // Lumenveil's engine reads fogdata.bin format version 3, LuxVelum's is version 4: keep LuxVelum's file aside so
    // the chosen mod finds its own data (or none, and Lumenveil derives its layers).
    void SwapFogData(unsigned choice) {
        const std::wstring dir = ExeDir();
        const std::wstring active = dir + L"\\fogdata.bin";
        const std::wstring kept = dir + L"\\fogdata.luxvelum.bin";

        if (choice == CoA::Visuals::LuxVelum) {
            if (!LuxFogData(active) && LuxFogData(kept) && !CopyFileW(kept.c_str(), active.c_str(), FALSE)) {
                CoA::Log::Printf("Visuals: LuxVelum's fogdata.bin could not be put back");
            }

            return;
        }

        if (LuxFogData(active) && MoveFileExW(active.c_str(), kept.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            CoA::Log::Printf("Visuals: LuxVelum's fogdata.bin kept as fogdata.luxvelum.bin");
        }
    }


    bool Parse(unsigned& out, const wchar_t* text) {
        if (!text || !*text) {
            return false;
        }

        wchar_t* end = nullptr;
        unsigned long value = wcstoul(text, &end, 10);

        if (!end || *end || value > CoA::Visuals::LuxVelum) {
            return false;
        }

        out = static_cast<unsigned>(value);
        return true;
    }

    // -visuals <n>, -visuals=<n>, or the old -noluxvelum (stock).
    bool FromCommandLine(unsigned& out) {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

        if (!argv) {
            return false;
        }

        bool found = false;

        for (int i = 1; i < argc && !found; ++i) {
            const wchar_t* arg = argv[i];

            if (*arg != L'-' && *arg != L'/') {
                continue;
            }

            ++arg;

            if (*arg == L'-') {
                ++arg;
            }

            if (_wcsicmp(arg, L"noluxvelum") == 0) {
                out = CoA::Visuals::Stock;
                found = true;
                continue;
            }

            if (_wcsnicmp(arg, L"visuals", 7) != 0) {
                continue;
            }

            if (arg[7] == L'=') {
                found = Parse(out, arg + 8);
            } else if (!arg[7] && i + 1 < argc) {
                found = Parse(out, argv[i + 1]);
            }
        }

        LocalFree(argv);
        return found;
    }

    bool FromFile(unsigned& out) {
        const std::wstring path = ExeDir() + L"\\Visuals.txt";
        FILE* file = nullptr;

        if (_wfopen_s(&file, path.c_str(), L"rb") || !file) {
            return false;
        }

        char text[32] = {};
        size_t read = fread(text, 1, sizeof(text) - 1, file);
        fclose(file);

        if (!read) {
            return false;
        }

        wchar_t wide[32] = {};
        MultiByteToWideChar(CP_ACP, 0, text, static_cast<int>(read), wide, 31);
        return Parse(out, wide);
    }
}

unsigned CoA::Visuals::Current() {
    unsigned choice = 0;

    if (FromCommandLine(choice) || FromFile(choice)) {
        return choice;
    }

    return kDefault;
}

bool CoA::Visuals::Set(unsigned choice) {
    if (choice > LuxVelum) {
        return false;
    }

    const std::wstring path = ExeDir() + L"\\Visuals.txt";
    FILE* file = nullptr;

    if (_wfopen_s(&file, path.c_str(), L"wb") || !file) {
        return false;
    }

    fprintf(file, "%u\n", choice);
    fclose(file);
    return true;
}

void CoA::Visuals::Start() {
    unsigned const choice = Current();
    SwapFogData(choice);

    switch (choice) {
        case Lumenveil:
            CoA::Lumenveil::Load();
            return;
        case LuxVelum:
            CoA::LuxVelum::Load();
            return;
        default:
            CoA::Log::Printf("Visuals: stock (graphics mods off)");
            return;
    }
}



