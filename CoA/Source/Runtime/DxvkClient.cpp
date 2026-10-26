// Dxvk's Win32 half: the client's command line, the exe's folder, LoadLibraryW, and the Extensions.log (open since
// CoA::Attach, which runs before main).
#include "CoA/Source/Runtime/Dxvk.h"
#include "CoA/Source/Runtime/Log.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include <windows.h>
#include <shellapi.h>
#include <cctype>
#include <cwctype>

// Named apart from CoA::Log (inside CoA::DxvkClient an unqualified Log would name the namespace).
namespace {
    // A bare flag ("-nodxvk", "/nodxvk", "--nodxvk"); Script::CommandLineArg only finds flags followed by a value.
    bool HasArg(const char* name) {
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

            const char* n = name;

            while (*arg && *n && towlower(*arg) == static_cast<wchar_t>(tolower(*n))) {
                ++arg;
                ++n;
            }

            found = !*arg && !*n;
        }

        LocalFree(argv);
        return found;
    }

    std::wstring ExeDir() {
        wchar_t path[MAX_PATH];
        DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring dir(path, n);
        size_t slash = dir.find_last_of(L"\\/");
        return slash == std::wstring::npos ? std::wstring(L".") : dir.substr(0, slash);
    }

    void* Load(const std::wstring& path, uint32_t& err) {
        HMODULE module = LoadLibraryW(path.c_str());
        err = module ? 0 : GetLastError();
        return module;
    }

    void WriteLog(const char* line) {
        CoA::Log::Printf("%s", line);
    }
}

void CoA::DxvkClient::Preload() {
    g_d3d9LibraryHook = CoA::Dxvk::D3d9Library;
    CoA::Dxvk::Preload({ HasArg, ExeDir, Load, WriteLog });
}
