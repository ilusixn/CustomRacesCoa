// Warcraft 2 style cursor, toggled in-game with /wc2cursor horde|human|off (see esteria_options.lua).
// The client has no software-rendered pointer: it uses the real Windows cursor, set by Wow.exe via user32!SetCursor.
// We patch Wow.exe's own import-table entry for SetCursor so every call it makes routes through ours first; the
// text-edit (I-beam) cursor is left alone by comparing against LoadCursor(nullptr, IDC_IBEAM), everything else is
// swapped while a style is active. The toggle itself is read from the account's own Config.wtf (SetCVar persists
// there), polled once a second by a background thread - no exe patching beyond the one IAT entry is needed.
#include <atomic>
#include <algorithm>
#include <mutex>
#include <thread>
#include <psapi.h>
#include <d3d9.h>
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "gdi32.lib")

namespace CursorSwap
{
    using SetCursorFn = HCURSOR(WINAPI*)(HCURSOR);
    SetCursorFn RealSetCursor = nullptr;
    std::atomic<int> Style{0}; // 0 off, 1 horde (orc), 2 alliance (human)
    HCURSOR Cursors[3] = {nullptr, nullptr, nullptr};
    HCURSOR IBeam = nullptr;
    // the plain idle arrow is the single HCURSOR the game sets far more than any other (constant re-assertion on
    // every mouse move over open ground); learned at runtime instead of hardcoded, so action-cursors (attack sword
    // over an enemy, speech bubble over an NPC, ...), the text I-beam, and SetCursor(NULL) (hides it - used while
    // turning the camera with the right mouse button held) are left completely alone and keep working normally.
    constexpr int TRACKED = 8;
    HCURSOR Seen[TRACKED] = {};
    std::atomic<int> SeenCount[TRACKED] = {};
    std::mutex SeenLock;
    HCURSOR BaseArrow = nullptr;
    HCURSOR SysArrow = nullptr;
    std::atomic<HCURSOR> LastApplied{nullptr};
    std::atomic<long long> HookCalls{0};
    std::atomic<long long> Overrides{0};
    HWND GameWindow();
    void SyncClassCursor(HCURSOR cursor);
    void EnsureSubclass(HWND hwnd);
    bool IsIdleCursor(HCURSOR cursor);

    std::wstring ExeDirectory()
    {
        wchar_t path[MAX_PATH] = {};
        if (!GetModuleFileNameW(nullptr, path, MAX_PATH))
            return L"";
        if (wchar_t* slash = wcsrchr(path, L'\\'))
            slash[1] = 0;
        return path;
    }

    // learns BaseArrow from the stream of SetCursor calls: whichever distinct non-NULL, non-IBeam handle has been
    // seen far more often than any other (the idle arrow is re-asserted on essentially every mouse-move event,
    // while a contextual action-cursor only appears while actually hovering that one NPC/enemy/node)
    void Learn(HCURSOR cursor)
    {
        std::lock_guard<std::mutex> lock(SeenLock);
        int slot = -1;
        for (int i = 0; i < TRACKED; ++i)
        {
            if (Seen[i] == cursor)
            {
                slot = i;
                break;
            }
            if (!Seen[i] && slot < 0)
                slot = i;
        }
        if (cursor == Cursors[1] || cursor == Cursors[2])
            return;
        if (slot < 0)
            return; // table full of other distinct cursors: don't guess, leave BaseArrow as already learned
        Seen[slot] = cursor;
        int const count = ++SeenCount[slot];
        int total = 0;
        for (int i = 0; i < TRACKED; ++i)
            total += SeenCount[i].load(std::memory_order_relaxed);
        if (count > total / 2 && count >= 8)
            BaseArrow = cursor;
    }

    HCURSOR WINAPI Hooked(HCURSOR cursor)
    {
        HookCalls.fetch_add(1, std::memory_order_relaxed);
        if (cursor == IBeam)
        {
            LastApplied.store(cursor, std::memory_order_relaxed);
            return RealSetCursor(cursor);
        }
        if (!cursor)
        {
            LastApplied.store(cursor, std::memory_order_relaxed);
            return RealSetCursor(cursor); // NULL hides it (left/right click camera turn)
        }
        Learn(cursor);
        int const style = Style.load(std::memory_order_relaxed);
        // Idle family only (learned arrow, window-class arrow, OCR_NORMAL). Sword / talk / loot / inspect stay.
        if (style && Cursors[style] && IsIdleCursor(cursor))
        {
            cursor = Cursors[style];
            Overrides.fetch_add(1, std::memory_order_relaxed);
        }
        LastApplied.store(cursor, std::memory_order_relaxed);
        HWND hwnd = GameWindow();
        if (hwnd)
            EnsureSubclass(hwnd);
        SyncClassCursor(cursor);
        return RealSetCursor(cursor);
    }

    // Patches every "SetCursor" import table entry it finds in the given module (there can be more than one: the
    // real cursor-setting code does not have to live in Wow.exe's own image - this client also loads a separate
    // legacy.dll, which has its own independent import table). Returns how many entries were patched.
    int PatchModuleImport(HMODULE module)
    {
        __try
        {
            auto base = reinterpret_cast<std::uint8_t*>(module);
            auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE)
                return 0;
            auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE)
                return 0;
            auto importDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            if (!importDir.VirtualAddress)
                return 0;
            auto descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + importDir.VirtualAddress);
            int patched = 0;
            for (; descriptor->Name; ++descriptor)
            {
                char const* moduleName = reinterpret_cast<char const*>(base + descriptor->Name);
                if (_stricmp(moduleName, "user32.dll") != 0)
                    continue;
                auto thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->FirstThunk);
                auto nameThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
                    base + (descriptor->OriginalFirstThunk ? descriptor->OriginalFirstThunk : descriptor->FirstThunk));
                for (; nameThunk->u1.AddressOfData; ++nameThunk, ++thunk)
                {
                    if (IMAGE_SNAP_BY_ORDINAL(nameThunk->u1.Ordinal))
                        continue;
                    auto byName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + nameThunk->u1.AddressOfData);
                    if (strcmp(byName->Name, "SetCursor") != 0)
                        continue;
                    if (!RealSetCursor)
                        RealSetCursor = reinterpret_cast<SetCursorFn>(thunk->u1.Function);
                    DWORD oldProtect = 0;
                    if (!VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProtect))
                        continue;
                    thunk->u1.Function = reinterpret_cast<std::uintptr_t>(&Hooked);
                    VirtualProtect(&thunk->u1.Function, sizeof(void*), oldProtect, &oldProtect);
                    ++patched;
                }
            }
            return patched;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return 0; // a module with an unusual/packed layout: skip it rather than crash the client
        }
    }

    int PatchImport()
    {
        if (!RealSetCursor)
            RealSetCursor = reinterpret_cast<SetCursorFn>(&SetCursor); // fallback if no import table entry is ever found
        int total = 0;
        HMODULE modules[256];
        DWORD needed = 0;
        if (EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed))
        {
            std::size_t count = std::min<std::size_t>(needed / sizeof(HMODULE), 256);
            for (std::size_t i = 0; i < count; ++i)
                total += PatchModuleImport(modules[i]);
        }
        else
        {
            total += PatchModuleImport(GetModuleHandleW(nullptr));
        }
        return total;
    }

    // After the D3D device is up the visible pointer is IDirect3DDevice9::SetCursorProperties
    // (hardware cursor). user32 SetCursor barely runs (debug: ~24 calls, 0 overrides) so GCLP
    // patching cannot stick. Hook the device vtable slot 10; leave action surfaces alone.
    using Create9Fn = IDirect3D9* (WINAPI*)(UINT);
    using CreateDeviceFn = HRESULT(WINAPI*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
    using SetCursorPropertiesFn = HRESULT(WINAPI*)(IDirect3DDevice9*, UINT, UINT, IDirect3DSurface9*);
    using ShowCursorD3DFn = BOOL(WINAPI*)(IDirect3DDevice9*, BOOL);
    Create9Fn RealCreate9 = nullptr;
    CreateDeviceFn RealCreateDevice = nullptr;
    SetCursorPropertiesFn RealSetCursorProperties = nullptr;
    ShowCursorD3DFn RealShowCursorD3D = nullptr;
    IDirect3DDevice9* CursorDevice = nullptr;
    IDirect3DSurface9* Wc2Surface[3] = {};
    UINT Wc2HotX[3] = {};
    UINT Wc2HotY[3] = {};
    unsigned long long IdleBitmapHash = 0;
    UINT IdleHotX = 0;
    UINT IdleHotY = 0;
    int IdleHotCount = 0;
    std::atomic<int> LastD3DIdle{1};
    std::atomic<long long> D3DCalls{0};
    std::atomic<long long> D3DOverrides{0};

    void PatchVtableSlot(void** table, int index, void* hook, void** saved)
    {
        if (!table || table[index] == hook)
            return;
        DWORD oldProtect = 0;
        if (!VirtualProtect(&table[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
            return;
        if (saved && !*saved)
            *saved = table[index];
        table[index] = hook;
        VirtualProtect(&table[index], sizeof(void*), oldProtect, &oldProtect);
    }

    unsigned long long HashSurface(IDirect3DSurface9* surface)
    {
        if (!surface)
            return 0;
        D3DSURFACE_DESC desc = {};
        if (FAILED(surface->GetDesc(&desc)))
            return reinterpret_cast<unsigned long long>(surface);
        D3DLOCKED_RECT locked = {};
        if (FAILED(surface->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
            return reinterpret_cast<unsigned long long>(surface);
        unsigned long long hash = 1469598103934665603ull;
        auto* rows = static_cast<unsigned char const*>(locked.pBits);
        UINT const widthBytes = desc.Width * 4;
        for (UINT y = 0; y < desc.Height; ++y)
        {
            unsigned char const* row = rows + y * locked.Pitch;
            for (UINT x = 0; x < widthBytes; ++x)
            {
                hash ^= row[x];
                hash *= 1099511628211ull;
            }
        }
        surface->UnlockRect();
        return hash;
    }

    bool MakeWc2Surface(IDirect3DDevice9* device, int style)
    {
        if (!device || !Cursors[style])
            return false;
        if (Wc2Surface[style])
        {
            Wc2Surface[style]->Release();
            Wc2Surface[style] = nullptr;
        }
        ICONINFO info = {};
        if (!GetIconInfo(Cursors[style], &info))
            return false;
        Wc2HotX[style] = info.xHotspot;
        Wc2HotY[style] = info.yHotspot;
        int const w = 32;
        int const h = 32;
        HRESULT made = device->CreateOffscreenPlainSurface(w, h, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &Wc2Surface[style], nullptr);
        if (FAILED(made) || !Wc2Surface[style])
        {
            if (info.hbmColor)
                DeleteObject(info.hbmColor);
            if (info.hbmMask)
                DeleteObject(info.hbmMask);
            return false;
        }
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HDC screen = GetDC(nullptr);
        HDC dc = CreateCompatibleDC(screen);
        HBITMAP dib = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        HGDIOBJ old = SelectObject(dc, dib);
        DrawIconEx(dc, 0, 0, Cursors[style], w, h, 0, nullptr, DI_NORMAL);
        D3DLOCKED_RECT locked = {};
        if (SUCCEEDED(Wc2Surface[style]->LockRect(&locked, nullptr, 0)) && bits)
        {
            auto* src = static_cast<unsigned char*>(bits);
            auto* dst = static_cast<unsigned char*>(locked.pBits);
            for (int y = 0; y < h; ++y)
                memcpy(dst + y * locked.Pitch, src + y * w * 4, w * 4);
            Wc2Surface[style]->UnlockRect();
        }
        SelectObject(dc, old);
        DeleteObject(dib);
        DeleteDC(dc);
        ReleaseDC(nullptr, screen);
        if (info.hbmColor)
            DeleteObject(info.hbmColor);
        if (info.hbmMask)
            DeleteObject(info.hbmMask);
        return Wc2Surface[style] != nullptr;
    }

    HRESULT WINAPI HookedSetCursorProperties(IDirect3DDevice9* device, UINT hotX, UINT hotY, IDirect3DSurface9* bitmap)
    {
        D3DCalls.fetch_add(1, std::memory_order_relaxed);
        CursorDevice = device;
        int const style = Style.load(std::memory_order_relaxed);
        // Bitmap hash, not hotspot: Point and Attack often share (0,0). Same hash = idle (re-upload);
        // a different hash is sword / talk / loot and must pass through.
        unsigned long long const hash = HashSurface(bitmap);
        if (!IdleBitmapHash && hash)
            IdleBitmapHash = hash;
        bool const idle = hash && hash == IdleBitmapHash;
        LastD3DIdle.store(idle ? 1 : 0, std::memory_order_relaxed);
        if (style && Cursors[style] && bitmap && RealSetCursorProperties && idle)
        {
            if (!Wc2Surface[style])
                MakeWc2Surface(device, style);
            if (Wc2Surface[style])
            {
                D3DOverrides.fetch_add(1, std::memory_order_relaxed);
                return RealSetCursorProperties(device, Wc2HotX[style], Wc2HotY[style], Wc2Surface[style]);
            }
        }
        return RealSetCursorProperties ? RealSetCursorProperties(device, hotX, hotY, bitmap)
            : device->SetCursorProperties(hotX, hotY, bitmap);
    }

    BOOL WINAPI HookedShowCursorD3D(IDirect3DDevice9* device, BOOL show)
    {
        return RealShowCursorD3D ? RealShowCursorD3D(device, show) : device->ShowCursor(show);
    }

    HRESULT WINAPI HookedCreateDevice(IDirect3D9* self, UINT adapter, D3DDEVTYPE type, HWND window, DWORD flags,
        D3DPRESENT_PARAMETERS* params, IDirect3DDevice9** device)
    {
        HRESULT result = RealCreateDevice(self, adapter, type, window, flags, params, device);
        if (SUCCEEDED(result) && device && *device)
        {
            void** table = *reinterpret_cast<void***>(*device);
            PatchVtableSlot(table, 10, reinterpret_cast<void*>(&HookedSetCursorProperties),
                reinterpret_cast<void**>(&RealSetCursorProperties));
            PatchVtableSlot(table, 12, reinterpret_cast<void*>(&HookedShowCursorD3D),
                reinterpret_cast<void**>(&RealShowCursorD3D));
            IdleBitmapHash = 0;
            for (int i = 0; i < 3; ++i)
            {
                if (Wc2Surface[i])
                {
                    Wc2Surface[i]->Release();
                    Wc2Surface[i] = nullptr;
                }
            }
        }
        return result;
    }

    IDirect3D9* WINAPI HookedCreate9(UINT sdk)
    {
        IDirect3D9* d3d = RealCreate9 ? RealCreate9(sdk) : nullptr;
        if (d3d)
        {
            void** table = *reinterpret_cast<void***>(d3d);
            PatchVtableSlot(table, 16, reinterpret_cast<void*>(&HookedCreateDevice),
                reinterpret_cast<void**>(&RealCreateDevice));
        }
        return d3d;
    }

    int PatchD3DImport(HMODULE module)
    {
        __try
        {
            auto base = reinterpret_cast<std::uint8_t*>(module);
            auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE)
                return 0;
            auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE)
                return 0;
            auto importDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            if (!importDir.VirtualAddress)
                return 0;
            auto descriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + importDir.VirtualAddress);
            int patched = 0;
            for (; descriptor->Name; ++descriptor)
            {
                char const* moduleName = reinterpret_cast<char const*>(base + descriptor->Name);
                if (_stricmp(moduleName, "d3d9.dll") != 0)
                    continue;
                auto thunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + descriptor->FirstThunk);
                auto nameThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
                    base + (descriptor->OriginalFirstThunk ? descriptor->OriginalFirstThunk : descriptor->FirstThunk));
                for (; nameThunk->u1.AddressOfData; ++nameThunk, ++thunk)
                {
                    if (IMAGE_SNAP_BY_ORDINAL(nameThunk->u1.Ordinal))
                        continue;
                    auto byName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + nameThunk->u1.AddressOfData);
                    if (strcmp(byName->Name, "Direct3DCreate9") != 0)
                        continue;
                    if (!RealCreate9)
                        RealCreate9 = reinterpret_cast<Create9Fn>(thunk->u1.Function);
                    DWORD oldProtect = 0;
                    if (!VirtualProtect(&thunk->u1.Function, sizeof(void*), PAGE_READWRITE, &oldProtect))
                        continue;
                    thunk->u1.Function = reinterpret_cast<std::uintptr_t>(&HookedCreate9);
                    VirtualProtect(&thunk->u1.Function, sizeof(void*), oldProtect, &oldProtect);
                    ++patched;
                }
            }
            return patched;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return 0;
        }
    }

    int PatchD3D()
    {
        int total = 0;
        HMODULE modules[256];
        DWORD needed = 0;
        if (EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed))
        {
            std::size_t count = std::min<std::size_t>(needed / sizeof(HMODULE), 256);
            for (std::size_t i = 0; i < count; ++i)
                total += PatchD3DImport(modules[i]);
        }
        if (!RealCreate9)
        {
            if (HMODULE d3d = GetModuleHandleW(L"d3d9.dll"))
                RealCreate9 = reinterpret_cast<Create9Fn>(GetProcAddress(d3d, "Direct3DCreate9"));
        }
        return total;
    }

    // Wow never IAT-imports Direct3DCreate9 (debug: d3d-iat=0), so wrap a dummy device from
    // the same d3d9.dll. IDirect3DDevice9 vtables are shared: patching slot 10 hooks the live game too.
    void BindViaDummyDevice()
    {
        if (RealSetCursorProperties)
            return;
        HMODULE d3dmod = GetModuleHandleW(L"d3d9.dll");
        if (!d3dmod)
            return;
        auto create = reinterpret_cast<Create9Fn>(GetProcAddress(d3dmod, "Direct3DCreate9"));
        if (!create)
            return;
        IDirect3D9* d3d = create(D3D_SDK_VERSION);
        if (!d3d)
            return;
        HWND hwnd = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 32, 32, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        D3DPRESENT_PARAMETERS pp = {};
        pp.Windowed = TRUE;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.hDeviceWindow = hwnd;
        pp.BackBufferWidth = 32;
        pp.BackBufferHeight = 32;
        pp.BackBufferFormat = D3DFMT_A8R8G8B8;
        IDirect3DDevice9* dummy = nullptr;
        HRESULT made = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dummy);
        if (FAILED(made) || !dummy)
            made = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, hwnd,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dummy);
        if (SUCCEEDED(made) && dummy)
        {
            void** table = *reinterpret_cast<void***>(dummy);
            PatchVtableSlot(table, 10, reinterpret_cast<void*>(&HookedSetCursorProperties),
                reinterpret_cast<void**>(&RealSetCursorProperties));
            PatchVtableSlot(table, 12, reinterpret_cast<void*>(&HookedShowCursorD3D),
                reinterpret_cast<void**>(&RealShowCursorD3D));
            dummy->Release();
        }
        d3d->Release();
        if (hwnd)
            DestroyWindow(hwnd);
    }

    // Besides SetCursor, Windows also resets the cursor to whatever is registered on the window CLASS
    // (GCLP_HCURSOR) on every WM_SETCURSOR the app doesn't fully swallow - which happens on basically every mouse
    // move. That silently reverts our SetCursor call a moment later without ever going through the hooked import,
    // which is exactly consistent with the hook's own counters proving it fires/overrides correctly while nothing
    // visibly changes. Patching the class cursor itself closes that path.
    HCURSOR OriginalClassCursor = nullptr;
    bool HaveOriginalClassCursor = false;
    HWND GameHwnd = nullptr;

    bool IsGameClass(wchar_t const* cls)
    {
        return wcsstr(cls, L"GxWindow") || wcsstr(cls, L"Warcraft") || wcsstr(cls, L"Ascension");
    }

    BOOL CALLBACK FindGameWindow(HWND hwnd, LPARAM)
    {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid != GetCurrentProcessId() || !IsWindowVisible(hwnd))
            return TRUE;
        wchar_t cls[80] = {};
        GetClassNameW(hwnd, cls, 80);
        if (IsGameClass(cls))
        {
            GameHwnd = hwnd;
            return FALSE;
        }
        return TRUE; // never keep splash/launcher: it is replaced ~1s later and would restore the stock cursor
    }

    HWND GameWindow()
    {
        if (GameHwnd && IsWindow(GameHwnd))
        {
            wchar_t cls[80] = {};
            GetClassNameW(GameHwnd, cls, 80);
            if (IsGameClass(cls))
                return GameHwnd;
        }
        GameHwnd = nullptr;
        EnumWindows(FindGameWindow, 0);
        return GameHwnd;
    }

    bool IsIdleCursor(HCURSOR cursor)
    {
        if (!cursor)
            return false;
        if (cursor == BaseArrow || cursor == SysArrow)
            return true;
        if (HaveOriginalClassCursor && cursor == OriginalClassCursor && OriginalClassCursor)
            return true;
        return false;
    }

    HWND SubclassedHwnd = nullptr;
    WNDPROC RealWndProc = nullptr;

    LRESULT CALLBACK GameWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_SETCURSOR && LOWORD(lParam) == HTCLIENT)
        {
            int const style = Style.load(std::memory_order_relaxed);
            if (style && Cursors[style])
            {
                HCURSOR want = LastApplied.load(std::memory_order_relaxed);
                if (!want)
                {
                    if (RealSetCursor)
                        RealSetCursor(nullptr);
                    return TRUE; // mouselook: stay hidden
                }
                if (IsIdleCursor(want) || want == Cursors[style])
                    want = Cursors[style];
                if (want != IBeam && RealSetCursor)
                    RealSetCursor(want);
                return TRUE;
            }
        }
        if (RealWndProc)
            return CallWindowProcW(RealWndProc, hwnd, msg, wParam, lParam);
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    void EnsureSubclass(HWND hwnd)
    {
        if (!hwnd || hwnd == SubclassedHwnd)
            return;
        if (SubclassedHwnd && RealWndProc && IsWindow(SubclassedHwnd))
            SetWindowLongPtrW(SubclassedHwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(RealWndProc));
        RealWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&GameWndProc)));
        SubclassedHwnd = hwnd;
    }

    // WM_SETCURSOR / DefWindowProc restores GCLP_HCURSOR on every mouse-move. Keep that handle in
    // lockstep with whatever we last passed to SetCursor: WC2 while idle, the game's own action
    // cursor (sword / talk / loot) while hovering, original class cursor when the style is off.
    void SyncClassCursor(HCURSOR cursor)
    {
        HWND hwnd = GameWindow();
        if (!hwnd)
            return;
        EnsureSubclass(hwnd);
        if (!HaveOriginalClassCursor)
        {
            OriginalClassCursor = reinterpret_cast<HCURSOR>(GetClassLongPtrW(hwnd, GCLP_HCURSOR));
            HaveOriginalClassCursor = true;
        }
        int const style = Style.load(std::memory_order_relaxed);
        if (style && Cursors[style] && IsIdleCursor(cursor))
            cursor = Cursors[style];
        if (!cursor)
            cursor = (style && Cursors[style]) ? Cursors[style] : OriginalClassCursor;
        if (cursor)
            SetClassLongPtrW(hwnd, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(cursor));
    }

    void PatchWindowClass(int style)
    {
        GameHwnd = nullptr; // the real Gx window appears after the splash; re-find every poll
        HWND hwnd = GameWindow();
        if (!hwnd)
            return;
        if (style == 0)
        {
            if (HaveOriginalClassCursor && OriginalClassCursor)
            {
                SetClassLongPtrW(hwnd, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(OriginalClassCursor));
                SetCursor(OriginalClassCursor);
            }
            return;
        }
        if (!Cursors[style])
            return;
        HCURSOR last = LastApplied.load(std::memory_order_relaxed);
        bool const action = last && last != Cursors[style] && last != IBeam && !IsIdleCursor(last);
        if (!action)
            SetClassLongPtrW(hwnd, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(Cursors[style]));
    }

    int StyleFromFile(std::wstring const& path)
    {
        FILE* file = nullptr;
        if (_wfopen_s(&file, path.c_str(), L"rb") || !file)
            return -1;
        char line[256];
        int found = -1;
        while (fgets(line, sizeof(line), file))
        {
            if (!strstr(line, "[\"style\"]"))
                continue;
            if (strstr(line, "wc2horde"))
                found = 1;
            else if (strstr(line, "wc2human"))
                found = 2;
            else
                found = 0;
        }
        fclose(file);
        return found;
    }

    // SetCVar is unusable here: this client rejects any custom CVar name ("Couldn't find CVar named ..."), so the
    // style is a normal addon SavedVariablesPerCharacter table instead, written to
    // WTF\Account\<account>\<realm>\<character>\SavedVariables\CoAWC2Cursor.lua on ReloadUI()/logout. We don't
    // know from here which account/character is active, so pick whichever such file was written most recently.
    int ReadConfiguredStyle()
    {
        std::wstring root = ExeDirectory() + L"WTF\\Account\\";
        WIN32_FIND_DATAW accountEntry;
        HANDLE accountHandle = FindFirstFileW((root + L"*").c_str(), &accountEntry);
        if (accountHandle == INVALID_HANDLE_VALUE)
            return 0;
        std::wstring bestPath;
        FILETIME bestTime = {};
        do
        {
            if (!(accountEntry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || accountEntry.cFileName[0] == L'.')
                continue;
            std::wstring accountDir = root + accountEntry.cFileName + L"\\";
            WIN32_FIND_DATAW realmEntry;
            HANDLE realmHandle = FindFirstFileW((accountDir + L"*").c_str(), &realmEntry);
            if (realmHandle == INVALID_HANDLE_VALUE)
                continue;
            do
            {
                if (!(realmEntry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || realmEntry.cFileName[0] == L'.')
                    continue;
                std::wstring realmDir = accountDir + realmEntry.cFileName + L"\\";
                WIN32_FIND_DATAW charEntry;
                HANDLE charHandle = FindFirstFileW((realmDir + L"*").c_str(), &charEntry);
                if (charHandle == INVALID_HANDLE_VALUE)
                    continue;
                do
                {
                    if (!(charEntry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || charEntry.cFileName[0] == L'.')
                        continue;
                    std::wstring candidate = realmDir + charEntry.cFileName + L"\\SavedVariables\\CoAWC2Cursor.lua";
                    WIN32_FIND_DATAW fileEntry;
                    HANDLE fileHandle = FindFirstFileW(candidate.c_str(), &fileEntry);
                    if (fileHandle == INVALID_HANDLE_VALUE)
                        continue;
                    if (bestPath.empty() || CompareFileTime(&fileEntry.ftLastWriteTime, &bestTime) > 0)
                    {
                        bestTime = fileEntry.ftLastWriteTime;
                        bestPath = candidate;
                    }
                    FindClose(fileHandle);
                } while (FindNextFileW(charHandle, &charEntry));
                FindClose(charHandle);
            } while (FindNextFileW(realmHandle, &realmEntry));
            FindClose(realmHandle);
        } while (FindNextFileW(accountHandle, &accountEntry));
        FindClose(accountHandle);
        if (bestPath.empty())
            return 0;
        int style = StyleFromFile(bestPath);
        return style < 0 ? 0 : style;
    }

    void WriteDebug(std::wstring const& text)
    {
        std::wstring path = ExeDirectory() + L"wc2cursor_debug.txt";
        FILE* file = nullptr;
        if (_wfopen_s(&file, path.c_str(), L"wb") || !file)
            return;
        std::wstring wrapped = text;
        fwrite(wrapped.c_str(), sizeof(wchar_t), wrapped.size(), file);
        fclose(file);
    }

    // Everything here runs on the background thread, never inside DllMain: EnumProcessModules can call back into
    // the loader (LoadLibrary) internally on some Windows versions, which deadlocks or silently kills the process
    // if done while DllMain is holding the loader lock. A first debug line is written as soon as this thread
    // actually starts, before any patch attempt, so "the file never even appears" and "it appears but says failed"
    // are distinguishable.
    void PollLoop()
    {
        WriteDebug(L"thread started, about to patch\n");
        PatchD3D();
        BindViaDummyDevice();
        for (int attempt = 0; attempt < 5 && !RealSetCursor; ++attempt)
        {
            if (!PatchImport())
                Sleep(200); // legacy.dll or whichever module owns this may still be finishing its own load
            PatchD3D();
        }
        int tick = 0;
        while (true)
        {
            int style = Style.load(std::memory_order_relaxed);
            if (tick % 20 == 0)
            {
                style = ReadConfiguredStyle();
                Style.store(style, std::memory_order_relaxed);
                int patched = PatchImport();
                int d3dPatched = PatchD3D();
                BindViaDummyDevice();
                wchar_t buf[384];
                swprintf_s(buf, L"poll ok, style=%d, patched-entries=%d, d3d-iat=%d, hooked-d3d=%d, hook-calls=%lld, overrides=%lld, "
                    L"d3d-cursor=%lld, d3d-overrides=%lld, base-arrow=0x%p, real-setcursor=0x%p\n",
                    style, patched, d3dPatched, RealSetCursorProperties ? 1 : 0, HookCalls.load(), Overrides.load(),
                    D3DCalls.load(), D3DOverrides.load(), BaseArrow, reinterpret_cast<void*>(RealSetCursor));
                WriteDebug(buf);
            }
            PatchWindowClass(style);
            ++tick;
            Sleep(50);
        }
    }

    // Called from the D3D proxy if it finds us after the device already exists.
    extern "C" __declspec(dllexport) void CursorSwap_BindDevice(IDirect3DDevice9* device)
    {
        if (!device)
            return;
        void** table = *reinterpret_cast<void***>(device);
        PatchVtableSlot(table, 10, reinterpret_cast<void*>(&HookedSetCursorProperties),
            reinterpret_cast<void**>(&RealSetCursorProperties));
        PatchVtableSlot(table, 12, reinterpret_cast<void*>(&HookedShowCursorD3D),
            reinterpret_cast<void**>(&RealShowCursorD3D));
    }

    void Init()
    {
        std::wstring dir = ExeDirectory();
        Cursors[1] = LoadCursorFromFileW((dir + L"wc2_orc.cur").c_str());
        Cursors[2] = LoadCursorFromFileW((dir + L"wc2_human.cur").c_str());
        IBeam = LoadCursorW(nullptr, MAKEINTRESOURCEW(32513)); // IDC_IBEAM, by numeric id (IDC_IBEAM itself is an ANSI literal here)
        SysArrow = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
        if (!Cursors[1] && !Cursors[2])
            return; // files not installed: nothing to hook, leave the stock cursor alone (no thread, no WriteDebug:
                     // this path runs inside DllMain, kept to the one safe, local, no-LoadLibrary call it already was)
        std::thread(PollLoop).detach();
    }
}
