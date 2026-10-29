// Debug aid: every access violation writes its module-relative address, the faulting address and a scan of the stack for return
// addresses to Logs\last_av.txt, because the client's own crash report sometimes dies while it prints the call stack. Resolve the
// numbers with the build's CoA64.map (bin\Win). The file is overwritten by each access violation.
#include <windows.h>
#include <cstdio>
#include <cstdint>

namespace {
    LONG CALLBACK Trace(EXCEPTION_POINTERS* info) {
        if (info->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        IMAGE_NT_HEADERS* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
        const std::uintptr_t size = nt->OptionalHeader.SizeOfImage;
        FILE* file = nullptr;

        if (fopen_s(&file, "Logs\last_av.txt", "w") || !file) {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        const auto rip = static_cast<std::uintptr_t>(info->ContextRecord->Rip);
        fprintf(file, "AV at rva 0x%llx (rip %p) reading/writing %p (type %llu); base %p\n", static_cast<unsigned long long>(rip - base),
            reinterpret_cast<void*>(rip), reinterpret_cast<void*>(info->ExceptionRecord->ExceptionInformation[1]),
            static_cast<unsigned long long>(info->ExceptionRecord->ExceptionInformation[0]), reinterpret_cast<void*>(base));
        const auto* sp = reinterpret_cast<const std::uintptr_t*>(info->ContextRecord->Rsp);
        int found = 0;

        for (int i = 0; i < 768 && found < 40; ++i) {
            std::uintptr_t value = 0;

            if (!ReadProcessMemory(GetCurrentProcess(), sp + i, &value, sizeof(value), nullptr)) {
                break;
            }

            if (value > base && value < base + size) {
                fprintf(file, "  stack[%d] rva 0x%llx\n", i, static_cast<unsigned long long>(value - base));
                ++found;
            }
        }

        fclose(file);
        return EXCEPTION_CONTINUE_SEARCH;
    }
}

namespace CoA::Esteria {
    void InstallCrashTrace() {
        AddVectoredExceptionHandler(0, Trace);
    }
}
