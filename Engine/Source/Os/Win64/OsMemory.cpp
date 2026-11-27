#if defined(WOWDECOMP_ARCH_64)

#include <cstdint>
#include <windows.h>
#include "Engine/Source/Os/OsMemory.h"

static const uintptr_t s_nearRange = 0x60000000;

void* OsAllocNear(const void* anchor, size_t size) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    uintptr_t granularity = info.dwAllocationGranularity;
    uintptr_t bytes = (size + granularity - 1) & ~(granularity - 1);
    uintptr_t center = reinterpret_cast<uintptr_t>(anchor);
    uintptr_t minimum = reinterpret_cast<uintptr_t>(info.lpMinimumApplicationAddress);
    uintptr_t maximum = reinterpret_cast<uintptr_t>(info.lpMaximumApplicationAddress);

    if (center > minimum + s_nearRange) {
        minimum = center - s_nearRange;
    }

    if (center + s_nearRange < maximum) {
        maximum = center + s_nearRange;
    }

    // CoA: let the kernel search the window in one call; the VirtualQuery scan below froze the client on animation loads.
    // The end address must be the last byte of a page (an unaligned end makes the call fail), so align both ends. If it fails, scan.
    {
        typedef PVOID (WINAPI* VirtualAlloc2Fn)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
        static VirtualAlloc2Fn virtualAlloc2 = [] {
            HMODULE module = GetModuleHandleW(L"kernelbase.dll");
            return module ? reinterpret_cast<VirtualAlloc2Fn>(GetProcAddress(module, "VirtualAlloc2")) : nullptr;
        }();

        if (virtualAlloc2) {
            uintptr_t const pageMask = ~(static_cast<uintptr_t>(info.dwPageSize) - 1);
            uintptr_t const lows[2] = {center > minimum ? center : minimum, minimum};
            uintptr_t const highs[2] = {maximum, center < maximum ? center : maximum};

            for (int pass = 0; pass < 2; ++pass) {
                uintptr_t low = (lows[pass] + granularity - 1) & ~(granularity - 1);
                uintptr_t high = (highs[pass] & pageMask) - 1;

                if (low + bytes > high + 1) {
                    continue;
                }

                MEM_ADDRESS_REQUIREMENTS requirements = {};
                requirements.LowestStartingAddress = reinterpret_cast<PVOID>(low);
                requirements.HighestEndingAddress = reinterpret_cast<PVOID>(high);
                requirements.Alignment = 0;
                MEM_EXTENDED_PARAMETER parameter = {};
                parameter.Type = MemExtendedParameterAddressRequirements;
                parameter.Pointer = &requirements;
                void* ptr = virtualAlloc2(GetCurrentProcess(), nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, &parameter, 1);

                if (ptr) {
                    return ptr;
                }
            }
        }
    }

    MEMORY_BASIC_INFORMATION region;
    // CoA: resume the upward scan where the last allocation ended instead of walking the whole fragmented window from the anchor every time
    // (that froze the client on animation loads); a miss falls back to a full scan from the anchor.
    static uintptr_t s_cursor = 0;
    uintptr_t address = (s_cursor > center && s_cursor < maximum) ? s_cursor : center;
    bool resumed = address != center;

    for (;;) {
        while (address < maximum && VirtualQuery(reinterpret_cast<void*>(address), &region, sizeof(region))) {
            uintptr_t base = reinterpret_cast<uintptr_t>(region.BaseAddress);
            uintptr_t end = base + region.RegionSize;

            if (region.State == MEM_FREE) {
                uintptr_t candidate = ((base > address ? base : address) + granularity - 1) & ~(granularity - 1);

                if (candidate + bytes <= end && candidate + bytes <= maximum) {
                    void* ptr = VirtualAlloc(reinterpret_cast<void*>(candidate), bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

                    if (ptr) {
                        s_cursor = candidate + bytes;
                        return ptr;
                    }
                }
            }

            address = end;
        }

        if (!resumed) {
            break;
        }

        resumed = false;
        address = center;
    }

    address = center;

    while (address > minimum && VirtualQuery(reinterpret_cast<void*>(address - 1), &region, sizeof(region))) {
        uintptr_t base = reinterpret_cast<uintptr_t>(region.BaseAddress);
        uintptr_t end = base + region.RegionSize;

        if (end > address) {
            end = address;
        }

        if (region.State == MEM_FREE && end - base >= bytes) {
            uintptr_t candidate = (end - bytes) & ~(granularity - 1);

            if (candidate >= base && candidate >= minimum) {
                void* ptr = VirtualAlloc(reinterpret_cast<void*>(candidate), bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

                if (ptr) {
                    return ptr;
                }
            }
        }

        address = base;
    }

    return nullptr;
}

void OsFreeNear(void* ptr) {
    if (ptr) {
        VirtualFree(ptr, 0, MEM_RELEASE);
    }
}

// CoA diagnostics: COA_HEAPCHECK=1 -> HeapValidate at labelled points, first corruption logged to Logs/HeapCheck.log with the last events.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
namespace {
    struct HeapEvent { char text[200]; DWORD tick; };
    HeapEvent s_heapEvents[96];
    unsigned s_heapEventCount = 0;
    int s_heapCheckMode = -1;
    bool s_heapReported = false;
    unsigned s_heapCheckCalls = 0;
}

void CoaHeapNote(const char* what, const char* detail) {
    if (s_heapCheckMode < 0) {
        char* e = getenv("COA_HEAPCHECK");
        s_heapCheckMode = (e && *e == '1') ? 1 : 0;
    }
    if (!s_heapCheckMode || s_heapReported) {
        return;
    }
    HeapEvent& ev = s_heapEvents[s_heapEventCount++ % 96];
    _snprintf_s(ev.text, sizeof(ev.text), _TRUNCATE, "%s %s", what, detail ? detail : "");
    ev.tick = GetTickCount();
}

void CoaHeapCheck(const char* label) {
    if (s_heapCheckMode < 0) {
        char* e = getenv("COA_HEAPCHECK");
        s_heapCheckMode = (e && *e == '1') ? 1 : 0;
    }
    if (!s_heapCheckMode || s_heapReported) {
        return;
    }
    ++s_heapCheckCalls;
    HANDLE heaps[2] = {GetProcessHeap(), reinterpret_cast<HANDLE>(_get_heap_handle())};
    bool bad = false;
    for (int i = 0; i < 2; ++i) {
        if (heaps[i] && (i == 0 || heaps[i] != heaps[0]) && !HeapValidate(heaps[i], 0, nullptr)) {
            bad = true;
        }
    }
    if (!bad) {
        return;
    }
    s_heapReported = true;
    FILE* f = nullptr;
    CreateDirectoryA("Logs", nullptr);
    fopen_s(&f, "Logs/HeapCheck.log", "a");
    if (!f) {
        return;
    }
    fprintf(f, "HEAP CORRUPT at '%s' (check #%u, tick %lu). Last events, oldest first:\n", label, s_heapCheckCalls, GetTickCount());
    unsigned n = s_heapEventCount < 96 ? s_heapEventCount : 96;
    for (unsigned i = 0; i < n; ++i) {
        HeapEvent& ev = s_heapEvents[(s_heapEventCount - n + i) % 96];
        fprintf(f, "  %lu  %s\n", ev.tick, ev.text);
    }
    fclose(f);
}

#endif
