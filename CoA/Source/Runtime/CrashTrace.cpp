// Developer diagnostic, only with -extlog (not part of the original DLL): on the first access violations or a stack
// overflow, writes the faulting address and the exe return addresses found on the stack to Logs\CrashTrace.txt,
// using almost no stack and no CRT so it works where the client's own crash handler cannot (a stack overflow
// otherwise ends the process as a bare access violation). Offsets are exe-relative; resolve them with CoA64.map.
#include "CoA/Source/Runtime/Log.h"
#include <Windows.h>

namespace {
    HANDLE s_file = INVALID_HANDLE_VALUE;

    void Put(const char* s) {
        DWORD written;
        WriteFile(s_file, s, lstrlenA(s), &written, nullptr);
    }

    void PutHex(DWORD64 v) {
        char buffer[19];
        buffer[0] = '0';
        buffer[1] = 'x';

        for (int i = 0; i < 16; ++i) {
            int nibble = static_cast<int>((v >> (60 - 4 * i)) & 0xF);
            buffer[2 + i] = static_cast<char>(nibble < 10 ? '0' + nibble : 'a' + nibble - 10);
        }

        buffer[18] = '\0';
        Put(buffer);
    }

    void PutFrame(DWORD64 pc) {
        HMODULE module = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(pc), &module);
        bool exe = module == GetModuleHandleA(nullptr);
        Put(exe ? "  exe+" : "  abs ");
        PutHex(exe ? pc - reinterpret_cast<DWORD64>(module) : pc);
        Put("\r\n");
    }

    LONG WINAPI Trace(EXCEPTION_POINTERS* ep) {
        static int seen = 0;
        DWORD code = ep->ExceptionRecord->ExceptionCode;

        if ((code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_STACK_OVERFLOW) || seen >= 3) {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        ++seen;
        Put(code == EXCEPTION_STACK_OVERFLOW ? "STACK OVERFLOW at\r\n" : "ACCESS VIOLATION at\r\n");
        DWORD64 rsp = ep->ContextRecord->Rsp;
        PutFrame(ep->ContextRecord->Rip);
        Put("rsp ");
        PutHex(rsp);
        Put("\r\n");

        // A cheap unwind: the first 60 values on the stack that point into the exe.
        int found = 0;

        for (DWORD64 p = rsp; found < 60 && p < rsp + 0x20000; p += 8) {
            DWORD64 value = *reinterpret_cast<DWORD64*>(p);
            HMODULE module = nullptr;

            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCSTR>(value), &module) && module == GetModuleHandleA(nullptr)) {
                PutFrame(value);
                ++found;
            }
        }

        FlushFileBuffers(s_file);
        return EXCEPTION_CONTINUE_SEARCH;
    }
}

void CoA_InstallCrashTrace() {
    s_file = CreateFileA("Logs\\CrashTrace.txt", GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    ULONG guarantee = 0x10000;
    SetThreadStackGuarantee(&guarantee);
    CoA::Log::Printf("crash trace %s", AddVectoredExceptionHandler(1, Trace) ? "installed" : "NOT installed");
}
