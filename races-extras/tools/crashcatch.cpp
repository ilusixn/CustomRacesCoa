// crashcatch <exe> [dir]: runs the game under the debugger and writes the faulting thread's stack (with source lines from the
// exe's pdb) to <dir>\CrashCatch.txt when it hits a fatal exception, including heap corruption (0xC0000374).
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#include <string>
#pragma comment(lib, "dbghelp.lib")

static void WriteStack(HANDLE proc, HANDLE thread, FILE* out) {
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_FULL;
    GetThreadContext(thread, &ctx);
    STACKFRAME64 f = {};
    f.AddrPC.Offset = ctx.Rip; f.AddrPC.Mode = AddrModeFlat;
    f.AddrFrame.Offset = ctx.Rbp; f.AddrFrame.Mode = AddrModeFlat;
    f.AddrStack.Offset = ctx.Rsp; f.AddrStack.Mode = AddrModeFlat;
    for (int i = 0; i < 60 && StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, thread, &f, &ctx, nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr); ++i) {
        char buf[sizeof(SYMBOL_INFO) + 512];
        SYMBOL_INFO* s = reinterpret_cast<SYMBOL_INFO*>(buf);
        s->SizeOfStruct = sizeof(SYMBOL_INFO); s->MaxNameLen = 511;
        DWORD64 d = 0; IMAGEHLP_LINE64 ln = { sizeof(ln) }; DWORD ld = 0;
        bool have = SymFromAddr(proc, f.AddrPC.Offset, &d, s);
        bool line = SymGetLineFromAddr64(proc, f.AddrPC.Offset, &ld, &ln);
        fprintf(out, "  %016llx %s+0x%llx %s:%lu\n", f.AddrPC.Offset, have ? s->Name : "?", d, line ? ln.FileName : "", line ? ln.LineNumber : 0);
    }
}

static DWORD64 g_base = 0;
int main(int argc, char** argv) {
    if (argc < 2) return 2;
    std::string dir = argc > 2 ? argv[2] : ".";
    SetEnvironmentVariableA("_NO_DEBUG_HEAP", "1");
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    std::string cmd = std::string("\"") + argv[1] + "\"";
    if (!CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, FALSE, DEBUG_ONLY_THIS_PROCESS, nullptr, dir.c_str(), &si, &pi)) {
        printf("cannot start %s (%lu)\n", argv[1], GetLastError());
        return 1;
    }
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(pi.hProcess, "C:/CoA-Build/coa64/bin/Win", FALSE);
    DEBUG_EVENT ev;
    bool first = true;
    for (;;) {
        if (!WaitForDebugEvent(&ev, INFINITE)) break;
        DWORD status = DBG_CONTINUE;
        if (ev.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT) {
            g_base = reinterpret_cast<DWORD64>(ev.u.CreateProcessInfo.lpBaseOfImage);
            SymLoadModule64(pi.hProcess, ev.u.CreateProcessInfo.hFile, argv[1], "CoA64", reinterpret_cast<DWORD64>(ev.u.CreateProcessInfo.lpBaseOfImage), 0);
            if (ev.u.CreateProcessInfo.hFile) CloseHandle(ev.u.CreateProcessInfo.hFile);
        } else if (ev.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
            DWORD code = ev.u.Exception.ExceptionRecord.ExceptionCode;
            if (ev.u.Exception.dwFirstChance && code != 0x80000003 && code != 0x4001000A && code != 0x406D1388) {
                std::string lp = dir + "\\CrashCatch.txt";
                if (FILE* lo = fopen(lp.c_str(), "a")) { fprintf(lo, "exception 0x%08lX at %p (first chance)\n", code, ev.u.Exception.ExceptionRecord.ExceptionAddress); fclose(lo); }
            }
            bool fatal = code == 0xC0000374 || code == 0xC0000005 || code == 0xC0000409 || code == 0xC00000FD || code == 0xC0000094;
            if (fatal) {
                HANDLE th = OpenThread(THREAD_ALL_ACCESS, FALSE, ev.dwThreadId);
                std::string path = dir + "\\CrashCatch.txt";
                FILE* out = fopen(path.c_str(), "a");
                if (out && th) {
                    fprintf(out, "=== exception 0x%08lX at %p (first chance %lu), thread %lu, image base %llx\n", code, ev.u.Exception.ExceptionRecord.ExceptionAddress, ev.u.Exception.dwFirstChance, ev.dwThreadId, g_base);
                    if (code == 0xC0000005 && ev.u.Exception.ExceptionRecord.NumberParameters >= 2)
                        fprintf(out, "access type %llu address %llx\n", ev.u.Exception.ExceptionRecord.ExceptionInformation[0], ev.u.Exception.ExceptionRecord.ExceptionInformation[1]);
                    WriteStack(pi.hProcess, th, out);
                    fprintf(out, "\n");
                    fclose(out);
                }
                if (th) CloseHandle(th);
                if (code == 0xC0000374 || !ev.u.Exception.dwFirstChance) { TerminateProcess(pi.hProcess, 1); break; }
            }
            status = DBG_EXCEPTION_NOT_HANDLED;
            if (first && code == EXCEPTION_BREAKPOINT) { first = false; status = DBG_CONTINUE; }
        } else if (ev.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) {
            std::string path = dir + "\\CrashCatch.txt";
            if (FILE* out = fopen(path.c_str(), "a")) { fprintf(out, "=== process exited, code 0x%08lX\n", ev.u.ExitProcess.dwExitCode); fclose(out); }
            break;
        } else if (ev.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT && ev.u.LoadDll.hFile) {
            CloseHandle(ev.u.LoadDll.hFile);
        }
        ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, status);
    }
    return 0;
}
